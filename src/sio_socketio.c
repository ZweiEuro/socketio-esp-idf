#include <sio_socketio.h>
#include <sio_types.h>
#include <internal/sio_packet.h>
#include <utility.h>
#include <cJSON.h>

#include <esp_log.h>
static const char *TAG = "[sio_socketio]";

esp_err_t handshake(sio_client_t *client);
esp_err_t handshake_polling(sio_client_t *client);
esp_err_t handshake_websocket(sio_client_t *client);

esp_err_t sio_send_packet_polling(sio_client_t *client, const Packet_t *packet);
esp_err_t sio_send_packet_websocket(sio_client_t *client, const Packet_t *packet);

char *alloc_handshake_get_url(const sio_client_t *client);

esp_err_t sio_client_begin(const sio_client_id_t clientId)
{

    sio_client_t *client = sio_client_get_and_lock(clientId);

    esp_err_t handshake_result = ESP_FAIL;
    uint16_t current_attempt = 1;

    while (client->max_connect_retries == 0 || current_attempt < client->max_connect_retries)
    {
        ESP_LOGI(TAG, "Connecting to %s attempt %d", client->server_address, current_attempt);

        handshake_result = handshake(client);
        if (handshake_result == ESP_OK)
        {
            handshake_result = ESP_OK;
            break;
        }
        ESP_LOGI(TAG, "Handshake failed, retrying in %d ms", client->retry_interval_ms);
        vTaskDelay(client->retry_interval_ms / portTICK_PERIOD_MS);
        current_attempt++;
    }

    ESP_LOGI(TAG, "Failed to connect to %s %s", client->server_address, esp_err_to_name(handshake_result));
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
    if (client->polling_client != NULL)
    {
        ESP_LOGE(TAG, "Polling client already initialized, close it properly first");
        return ESP_FAIL;
    }

    Packet_t *packet = NULL;
    { // scope for first url without session id

        char *url = alloc_handshake_get_url(client);

        if (client->handshake_client == NULL)
        {

            // Form the request URL

            ESP_LOGD(TAG, "Handshake URL: >%s< len:%d", url, strlen(url));

            esp_http_client_config_t config = {
                .url = url,
                .event_handler = http_client_polling_handler,
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
        else
        {
            esp_http_client_set_url(client->handshake_client, url);
            esp_http_client_set_method(client->handshake_client, HTTP_METHOD_GET);
            esp_http_client_delete_header(client->handshake_client, "Content-Type");
            esp_http_client_set_header(client->handshake_client, "Accept", "text/plain");
        }

        freeIfNotNull(url);
    }

    esp_err_t err = esp_http_client_perform(client->handshake_client);
    if (err != ESP_OK || packet == NULL)
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s, packet pointer %p ", esp_err_to_name(err), packet);
        return err;
    }

    int http_response_status_code = esp_http_client_get_status_code(client->handshake_client);
    int http_response_content_length = esp_http_client_get_content_length(client->handshake_client);
    ESP_LOGI(
        TAG, "HTTP GET Status = %d, content_length = %d",
        http_response_status_code,
        http_response_content_length);

    ESP_LOGI(TAG, "Packet: %d %d len %d \n%s\n%s",
             packet->eio_type, packet->sio_type, packet->len, packet->data, packet->json_start);

    // parse the packet to get out session id and reconnect stuff etc

    // example string: {"sid":"XX","upgrades":["websocket"],"pingInterval":20000,"pingTimeout":5000,"maxPayload":1000000}

    cJSON *json = cJSON_Parse(packet->json_start);
    if (json == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return ESP_FAIL;
    }

    client->server_session_id = cJSON_GetObjectItemCaseSensitive(json, "sid")->valuestring;
    client->server_ping_interval_ms = cJSON_GetObjectItem(json, "pingInterval")->valueint;
    client->server_ping_timeout_ms = cJSON_GetObjectItem(json, "pingTimeout")->valueint;

    free_packet(packet);
    // send back the ok with the new url

    // Post an OK, or rather the auth message

    const char *auth_data = client->alloc_auth_body_cb == NULL ? "" : client->alloc_auth_body_cb(client);

    unlockClient(client);

    Packet_t *p = alloc_packet(client->client_id, auth_data, strlen(auth_data));
    setSioType(p, SIO_PACKET_CONNECT);

    esp_err_t ret = sio_send_packet_polling(client->client_id, p);
    free_packet(p);

    return ESP_OK;
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

esp_err_t sio_send_packet_polling(sio_client_t *client, const Packet_t *packet)
{

    ESP_LOGI(TAG, "Sending package %s", packet->data);

    // allocate posting user if not present

    return ESP_OK;

    { // second url, with session id, and build auth body
        char *url = alloc_handshake_get_url(client);

        esp_http_client_set_url(client->handshake_client, url);
        esp_http_client_set_method(client->handshake_client, HTTP_METHOD_POST);
        esp_http_client_set_header(client->handshake_client, "Content-Type", "text/plain");
        esp_http_client_set_header(client->handshake_client, "Accept", "text/html");

        freeIfNotNull(url);
    }
}

esp_err_t sio_send_packet_websocket(sio_client_t *client, const Packet_t *packet)
{
    assert(false && "Not implemented");
    return ESP_OK;
}

// receiving

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