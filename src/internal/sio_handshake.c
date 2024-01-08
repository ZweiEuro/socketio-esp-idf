#include <sio_client.h>
#include <sio_types.h>
#include <internal/sio_packet.h>
#include <internal/task_functions.h>
#include <utility.h>
#include <cJSON.h>
#include <internal/sio_handshake.h>
#include <internal/sio_send.h>

#include <esp_log.h>

esp_err_t handshake_polling(sio_client_t *client);
esp_err_t handshake_websocket(sio_client_t *client);

static const char *TAG = "[sio_handshake]";

esp_err_t sio_handshake(sio_client_t *client)
{
    assert(client->status != SIO_CLIENT_STATUS_HANDSHAKING && "Client is already handshaking");
    client->status = SIO_CLIENT_STATUS_HANDSHAKING;

    esp_err_t err = ESP_FAIL;

    if (client->transport == SIO_TRANSPORT_WEBSOCKETS)
    {
        err = handshake_websocket(client);
    }
    else if (client->transport == SIO_TRANSPORT_POLLING)
    {
        err = handshake_polling(client);
    }

    if (err != ESP_OK)
    {
        client->status = SIO_CLIENT_STATUS_ERROR;

        ESP_LOGW(TAG, "Handshake failed, sending error event");
        sio_event_data_t event_data = {
            .client_id = client->client_id,
            .packets_pointer = NULL,
            .len = 0};
        esp_event_post(SIO_EVENT, SIO_EVENT_CONNECT_ERROR, &event_data, sizeof(sio_event_data_t), pdMS_TO_TICKS(50));
    }
    else
    {
        client->status = SIO_CLIENT_STATUS_HANDSHOOK;
    }

    return err;
}

esp_err_t handshake_polling(sio_client_t *client)
{

    const sio_client_id_t client_id = client->client_id;

    static PacketPointerArray_t packets;
    packets = NULL;
    // scope for first url without session id

    assert(client->handshake_client == NULL && "Handshake client already exists");

    {

        char *url = alloc_handshake_get_url(client);

        // Form the request URL

        esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_GET,
            .disable_auto_redirect = true,
            .event_handler = http_client_polling_get_handler,
            .user_data = &packets,
            .timeout_ms = 5000};

        client->handshake_client = esp_http_client_init(&config);

        assert(client->handshake_client != NULL && "Failed to init http client");

        esp_http_client_set_url(client->handshake_client, url);
        esp_http_client_set_method(client->handshake_client, HTTP_METHOD_GET);
        esp_http_client_set_header(client->handshake_client, "Content-Type", "text/html");
        esp_http_client_set_header(client->handshake_client, "Accept", "text/plain");
        free(url);
    }

    esp_err_t err = ESP_FAIL;
    {
    retry_handshake:
        sio_client_status_t client_status = client->status;
        esp_http_client_handle_t client_handshake_http_client = client->handshake_client;

        if (client_status != SIO_CLIENT_STATUS_HANDSHAKING)
        {
            ESP_LOGW(TAG, "Handshake cancelled, client status is %d", client_status);
            esp_http_client_close(client->handshake_client);

            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Sending sio_handshake");
        assert(client_handshake_http_client != NULL);

        // UNSAFE START
        unlockClient(client);
        client = NULL;
        err = esp_http_client_perform(client_handshake_http_client);
        client = sio_client_get_and_lock(client_id);
        // UNSAFE END

        if (err == ESP_ERR_HTTP_EAGAIN || err == ESP_ERR_HTTP_CONNECT)
        {
            ESP_LOGW(TAG, "Handshake failed, retrying: %s ", esp_err_to_name(err));
            goto retry_handshake;
        }

        esp_http_client_close(client_handshake_http_client);
        esp_http_client_cleanup(client_handshake_http_client);
        client->handshake_client = NULL;
    }
    { // scope for var declaration error after cleanup

        if (err != ESP_OK || packets == NULL)
        {
            ESP_LOGE(TAG, "HTTP GET request failed: %s, packets pointer %p ", esp_err_to_name(err), packets);
            return err;
        }

        // parse the packet to get out session id and reconnect stuff etc

        if (get_array_size(packets) != 1)
        {
            ESP_LOGE(TAG, "Expected 1 packet, got %d", get_array_size(packets));
            return ESP_FAIL;
        }

        Packet_t *packet = packets[0];

        if (packet->eio_type != EIO_PACKET_OPEN)
        {
            ESP_LOGE(TAG, "Expected open packet, got %d", packet->eio_type);
            return ESP_FAIL;
        }

        cJSON *json = cJSON_Parse(packet->json_start);

        if (json == NULL)
        {
            ESP_LOGE(TAG, "Failed to parse JSON: %s", cJSON_GetErrorPtr());
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL)
            {
                fprintf(stderr, "Error before: %s\n", error_ptr);
            }

            return ESP_FAIL;
        }
        else
        {
            char *json_printed = cJSON_Print(json);
            ESP_LOGI(TAG, "Handshake json %s", json_printed);
            free(json_printed);
        }

        client->_server_session_id = strdup(cJSON_GetObjectItemCaseSensitive(json, "sid")->valuestring);
        client->server_ping_interval_ms = cJSON_GetObjectItem(json, "pingInterval")->valueint;
        client->server_ping_timeout_ms = cJSON_GetObjectItem(json, "pingTimeout")->valueint;

        cJSON_Delete(json);
        // send back the ok with the new url

        // Post an OK, or rather the auth message

        const char *auth_data = client->alloc_auth_body_cb == NULL ? strdup("") : client->alloc_auth_body_cb(client);

        Packet_t *init_packet = alloc_message(auth_data, NULL);
        free((void *)auth_data);
        auth_data = NULL;

        setSioType(init_packet, SIO_PACKET_CONNECT);

        if (client->transport == SIO_TRANSPORT_POLLING)
        {
            err = sio_send_packet_polling(client, init_packet);
        }
        else if (client->transport == SIO_TRANSPORT_WEBSOCKETS)
        {
            err = sio_send_packet_websocket(client, init_packet);
        }
        else
        {
            assert(false && "Unknown transport");
        }

        ESP_LOGI(TAG, "free init packet");
        free_packet(&init_packet);
    }

    return err;
}

esp_err_t handshake_websocket(sio_client_t *client)
{
    assert(false && "Not implemented");
    return ESP_FAIL;
}
