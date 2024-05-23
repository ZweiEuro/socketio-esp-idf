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

static const char *TAG = "[sio_handshake]";

static esp_http_client_handle_t sio_handshake_client = NULL;
static PacketPointerArray_t packets = NULL;
static SemaphoreHandle_t sio_handshake_lock = NULL;

esp_err_t sio_handshake(sio_client_t *client)
{

    assert(client != NULL && "Client is NULL");
    assert(sio_client_is_locked(client->client_id) && "Client is not locked");

    if (sio_handshake_lock == NULL)
    {
        sio_handshake_lock = xSemaphoreCreateMutex();
    }
    else
    {
        xSemaphoreTake(sio_handshake_lock, portMAX_DELAY);
    }

    esp_err_t err = ESP_FAIL;

    if (client->transport == SIO_TRANSPORT_WEBSOCKETS)
    {
        assert(false && "Not implemented");
    }
    else if (client->transport == SIO_TRANSPORT_POLLING)
    {
        err = handshake_polling(client);
    }

    if (err != ESP_OK)
    {
        // should any error occur, remove the http cleanup and NULL the client
        esp_http_client_cleanup(sio_handshake_client);
        sio_handshake_client = NULL;
    }

    if (packets != NULL)
    {
        free_packet_arr(&packets);
        packets = NULL;
    }

    xSemaphoreGive(sio_handshake_lock);

    return err;
}

esp_err_t send_auth_package_polling(sio_client_t *client, Packet_t *packet)
{
    assert(false && "Not implemented");
    return ESP_FAIL;
}

esp_err_t handshake_polling(sio_client_t *client)
{

    // scope for first url without session id

    esp_err_t err = ESP_FAIL;

    if (sio_handshake_client == NULL)
    {
        // if there is none, init a client

        char *url = alloc_handshake_get_url(client);

        esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_GET,
            .timeout_ms = 5000,
            .disable_auto_redirect = true,
            .event_handler = http_client_polling_get_handler,
            .user_data = &packets};

        sio_handshake_client = esp_http_client_init(&config);
        ESP_LOGI(TAG, "Handshake client init %p %s", sio_handshake_client, url);
        free(url);

        esp_http_client_set_header(sio_handshake_client, "Content-Type", "text/html");
        esp_http_client_set_header(sio_handshake_client, "Accept", "text/plain");

        assert(sio_handshake_client != NULL && "Failed to init http client");
    }
    else
    {
        // edit the url if the client exists
        char *url = alloc_handshake_get_url(client);
        esp_http_client_set_url(sio_handshake_client, url);
        free(url);
    }

    ESP_LOGI(TAG, "Sending sio_handshake status: %d", client->status);

    err = esp_http_client_perform(sio_handshake_client);

    esp_http_client_close(sio_handshake_client);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Handshake failed %s ", esp_err_to_name(err));
        return err;
    }

    { // scope for var declaration error after cleanup

        if (packets == NULL)
        {
            ESP_LOGE(TAG, "Did not receive any packets");
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
            assert(false && "not implemented");
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
