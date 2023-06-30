#include <sio_client.h>
#include <sio_types.h>
#include <internal/sio_packet.h>
#include <internal/task_functions.h>
#include <utility.h>
#include <cJSON.h>

#include <esp_log.h>
static const char *TAG = "[sio_socketio]";

ESP_EVENT_DEFINE_BASE(SIO_EVENT);

esp_err_t handshake(sio_client_t *client);
esp_err_t handshake_polling(sio_client_t *client);
esp_err_t handshake_websocket(sio_client_t *client);

esp_err_t sio_send_packet_polling(sio_client_t *client, const Packet_t *packet);
esp_err_t sio_send_packet_websocket(sio_client_t *client, const Packet_t *packet);

char *alloc_post_url(const sio_client_t *client);
char *alloc_handshake_get_url(const sio_client_t *client);

esp_err_t sio_client_begin(const sio_client_id_t clientId)
{

    sio_client_t *client = sio_client_get_and_lock(clientId);
    esp_err_t handshake_result = handshake(client);

    if (handshake_result == ESP_OK)
    {
        ESP_LOGI(TAG, "Connected to %s", client->server_address);
    }
    else
    {
        ESP_LOGI(TAG, "Failed to connect to %s %s", client->server_address, esp_err_to_name(handshake_result));
    }
    unlockClient(client);
    return handshake_result;
}

// handshake
esp_err_t handshake(sio_client_t *client)
{

    if (client->transport == SIO_TRANSPORT_WEBSOCKETS)
    {
        return handshake_websocket(client);
    }
    else if (client->transport == SIO_TRANSPORT_POLLING)
    {
        return handshake_polling(client);
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t handshake_polling(sio_client_t *client)
{
    if (client->polling_client_running)
    {
        ESP_LOGE(TAG, "Polling client already running, close it properly first");
        return ESP_FAIL;
    }

    static Packet_t *packet = NULL;
    packet = NULL;
    { // scope for first url without session id

        char *url = alloc_handshake_get_url(client);

        if (client->handshake_client == NULL)
        {

            // Form the request URL

            ESP_LOGD(TAG, "Handshake URL: >%s< len:%d", url, strlen(url));

            esp_http_client_config_t config = {
                .url = url,
                .event_handler = http_client_polling_get_handler,
                .user_data = &packet,
                .disable_auto_redirect = true,
                .method = HTTP_METHOD_GET,
            };
            client->handshake_client = esp_http_client_init(&config);

            if (client->handshake_client == NULL)
            {
                ESP_LOGE(TAG, "Failed to initialize HTTP client");
                return ESP_FAIL;
            }
        }
        esp_http_client_set_url(client->handshake_client, url);
        esp_http_client_set_method(client->handshake_client, HTTP_METHOD_GET);
        esp_http_client_set_header(client->handshake_client, "Content-Type", "text/html");
        esp_http_client_set_header(client->handshake_client, "Accept", "text/plain");

        freeIfNotNull(url);
    }

    esp_err_t err = esp_http_client_perform(client->handshake_client);
    if (err != ESP_OK || packet == NULL)
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s, packet pointer %p ", esp_err_to_name(err), packet);
        esp_http_client_cleanup(client->handshake_client);
        client->handshake_client = NULL;

        sio_event_data_t event_data = {
            .client_id = client->client_id,
            .packet = NULL};

        esp_event_post(SIO_EVENT, SIO_EVENT_CONNECT_ERROR, &event_data, sizeof(sio_event_data_t), pdMS_TO_TICKS(50));

        return err;
    }

    // parse the packet to get out session id and reconnect stuff etc

    cJSON *json = cJSON_Parse(packet->json_start);

    if (json == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", cJSON_GetErrorPtr());
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }

        unlockClient(client);
        sio_event_data_t event_data = {
            .client_id = client->client_id,
            .packet = packet};

        esp_event_post(SIO_EVENT, SIO_EVENT_CONNECT_ERROR, &event_data, sizeof(sio_event_data_t), pdMS_TO_TICKS(50));

        return ESP_FAIL;
    }

    client->server_session_id = strdup(cJSON_GetObjectItemCaseSensitive(json, "sid")->valuestring);
    client->server_ping_interval_ms = cJSON_GetObjectItem(json, "pingInterval")->valueint;
    client->server_ping_timeout_ms = cJSON_GetObjectItem(json, "pingTimeout")->valueint;

    cJSON_Delete(json);
    // send back the ok with the new url

    // Post an OK, or rather the auth message

    const char *auth_data = client->alloc_auth_body_cb == NULL ? "" : client->alloc_auth_body_cb(client);

    unlockClient(client);

    Packet_t *ack_pack = alloc_packet(client->client_id, auth_data, strlen(auth_data));
    setSioType(ack_pack, SIO_PACKET_CONNECT);
    esp_err_t ret = sio_send_packet_polling(client, ack_pack);
    free_packet(ack_pack);

    if (ret == ESP_OK)
    {

        // reuse not possible since user_data can't be set after init.
        // newer idf version can do that but not the 5.0.2 (current stable)
        esp_http_client_cleanup(client->handshake_client);
        client->handshake_client = NULL;

        client->polling_client_running = true;
        xTaskCreate(&sio_polling_task, "sio_polling", 4096, (void *)&client->client_id, 6, NULL);

        sio_event_data_t event_data = {
            .client_id = client->client_id,
            .packet = packet};

        esp_event_post(SIO_EVENT, SIO_EVENT_CONNECTED, &event_data, sizeof(sio_event_data_t), pdMS_TO_TICKS(50));

        // Tranfer the handler over to the polling service
        // start the polling service and throw events
    }

    return ret;
}

esp_err_t handshake_websocket(sio_client_t *client)
{
    assert(false && "Not implemented");
}

// sending

esp_err_t sio_send_string(const sio_client_id_t clientId, const char *data, size_t len)
{
    Packet_t *p = alloc_packet(clientId, data, len);
    esp_err_t ret = sio_send_packet(clientId, p);
    free_packet(p);
    return ret;
}

esp_err_t sio_send_packet(const sio_client_id_t clientId, const Packet_t *packet)
{
    sio_client_t *client = sio_client_get_and_lock(clientId);

    if (client->server_session_id == NULL)
    {
        ESP_LOGE(TAG, "Server session id not set, was this client initialized?");
        unlockClient(client);

        return ESP_FAIL;
    }
    esp_err_t ret = ESP_FAIL;

    if (client->transport == SIO_TRANSPORT_WEBSOCKETS)
    {
        ret = sio_send_packet_websocket(client, packet);
    }
    else if (client->transport == SIO_TRANSPORT_POLLING)
    {
        ret = sio_send_packet_polling(client, packet);
    }
    else
    {
        ret = ESP_ERR_INVALID_ARG;
    }

    unlockClient(client);
    return ret;
}

// TODO: figure out why this is necessary,
// https://github.com/ZweiEuro/socketio-esp-idf/issues/1
#define REBUILD_CLIENT_POST 1

esp_err_t sio_send_packet_polling(sio_client_t *client, const Packet_t *packet)
{

    static Packet_t *response_packet = NULL;
    response_packet = NULL;
    { // scope for first url without session id

        char *url = alloc_post_url(client);

        if (client->posting_client == NULL)
        {

            // Form the request URL

            esp_http_client_config_t config = {
                .url = url,
                .event_handler = http_client_polling_post_handler,
                .user_data = &response_packet,
                .disable_auto_redirect = true,
                .method = HTTP_METHOD_POST,
            };
            client->posting_client = esp_http_client_init(&config);

            if (client->posting_client == NULL)
            {
                ESP_LOGE(TAG, "Failed to initialize HTTP client");
                return ESP_FAIL;
            }
        }
        esp_http_client_set_header(client->posting_client, "Content-Type", "text/plain;charset=UTF-8");
        esp_http_client_set_header(client->posting_client, "Accept", "*/*");
        esp_http_client_set_method(client->posting_client, HTTP_METHOD_POST);
        esp_http_client_set_post_field(client->posting_client, packet->data, packet->len);

        esp_http_client_set_url(client->posting_client, url);

        freeIfNotNull(url);
    }

    esp_err_t err = esp_http_client_perform(client->posting_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        print_packet(packet);
#if REBUILD_CLIENT_POST
        esp_http_client_cleanup(client->posting_client);
        client->posting_client = NULL;
#endif
        return err;
    }

    // allocate posting user if not present
    if (response_packet != NULL && response_packet->eio_type == EIO_PACKET_OK_SERVER)
    {
        ESP_LOGI(TAG, "Response packet: %s, ok from server, sent succesful", response_packet->data);

        free_packet(response_packet);
#if REBUILD_CLIENT_POST
        esp_http_client_cleanup(client->posting_client);
        client->posting_client = NULL;
#endif
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Response packet: >%s<, not ok from server, sent failed", response_packet->data);
    }

#if REBUILD_CLIENT_POST
    esp_http_client_cleanup(client->posting_client);
    client->posting_client = NULL;
#endif
    return ESP_FAIL;
}

esp_err_t sio_send_packet_websocket(sio_client_t *client, const Packet_t *packet)
{
    assert(false && "Not implemented");
    return ESP_OK;
}

// close

void sio_client_close(sio_client_id_t clientId)
{

    Packet_t *p = (Packet_t *)calloc(1, sizeof(Packet_t));
    p->data = calloc(1, 2);
    p->len = 2;
    setEioType(p, EIO_PACKET_CLOSE);

    // close the listener and wait for it to close
    sio_client_t *client = sio_client_get_and_lock(clientId);
    client->polling_client_running = false;
    unlockClient(client);
    // wait until the task has deleted itself
    while (client->polling_client != NULL)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    sio_send_packet(clientId, p);
}

// util

char *alloc_handshake_get_url(const sio_client_t *client)
{

    char *token = alloc_random_string(SIO_TOKEN_SIZE);
    size_t url_length =
        strlen(client->transport == SIO_TRANSPORT_POLLING ? SIO_TRANSPORT_POLLING_PROTO_STRING : SIO_TRANSPORT_WEBSOCKETS_PROTO_STRING) +
        strlen("://") +
        strlen(client->server_address) +
        strlen(client->sio_url_path) +
        strlen("/?EIO=X&transport=") +
        strlen(SIO_TRANSPORT_POLLING_STRING) +
        strlen("&t=") + strlen(token);

    char *url = calloc(1, url_length + 1);
    if (url == NULL)
    {
        assert(false && "Failed to allocate memory for handshake url");
        return NULL;
    }

    sprintf(
        url,
        "%s://%s%s/?EIO=%d&transport=%s&t=%s",
        SIO_TRANSPORT_POLLING_PROTO_STRING,
        client->server_address,
        client->sio_url_path,
        client->eio_version,
        SIO_TRANSPORT_POLLING_STRING,
        token);

    freeIfNotNull(token);
    return url;
}

char *alloc_post_url(const sio_client_t *client)
{
    if (client == NULL || client->server_session_id == NULL)
    {
        ESP_LOGE(TAG, "Server session id not set, was this client initialized? Client: %p", client);
        return NULL;
    }

    char *token = alloc_random_string(SIO_TOKEN_SIZE);
    size_t url_length =
        strlen(client->transport == SIO_TRANSPORT_POLLING ? SIO_TRANSPORT_POLLING_PROTO_STRING : SIO_TRANSPORT_WEBSOCKETS_PROTO_STRING) +
        strlen("://") +
        strlen(client->server_address) +
        strlen(client->sio_url_path) +
        strlen("/?EIO=X&transport=") +
        strlen(SIO_TRANSPORT_POLLING_STRING) +
        strlen("&t=") + strlen(token) +
        strlen("&sid=") + strlen(client->server_session_id);

    char *url = calloc(1, url_length + 1);

    if (url == NULL)
    {
        assert(false && "Failed to allocate memory for handshake url");
        return NULL;
    }

    sprintf(
        url,
        "%s://%s%s/?EIO=%d&transport=%s&t=%s&sid=%s",
        SIO_TRANSPORT_POLLING_PROTO_STRING,
        client->server_address,
        client->sio_url_path,
        client->eio_version,
        SIO_TRANSPORT_POLLING_STRING,
        token,
        client->server_session_id);

    freeIfNotNull(token);
    return url;
}

char *alloc_polling_get_url(const sio_client_t *client)
{
    return alloc_post_url(client);
}