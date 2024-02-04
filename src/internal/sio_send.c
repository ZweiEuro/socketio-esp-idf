#include <sio_types.h>
#include <internal/sio_packet.h>
#include <internal/sio_send.h>
#include <internal/task_functions.h>
#include <utility.h>
#include <cJSON.h>

#include <esp_log.h>
static const char *TAG = "[sio_socketio]";

ESP_EVENT_DEFINE_BASE(SIO_EVENT);

esp_err_t sio_send_string(const sio_client_id_t clientId, const char *data)
{
    ESP_LOGD(TAG, "Sending string: %s %d", data, strlen(data));

    Packet_t *p = alloc_message(data, "message");
    // print_packet(p);
    esp_err_t ret = sio_send_packet(clientId, p);
    free_packet(&p);
    return ret;
}

esp_err_t sio_send_packet(const sio_client_id_t clientId, const Packet_t *packet)
{
    sio_client_t *client = sio_client_get_and_lock(clientId);

    if (client->status != SIO_CLIENT_STATUS_CONNECTED &&
        client->status != SIO_CLIENT_CLOSING)
    {
        ESP_LOGE(TAG, "Client not in sendable state %d", client->status);
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
#define REBUILD_CLIENT_POST 0

esp_err_t sio_send_packet_polling(sio_client_t *client, const Packet_t *packet)
{
    static PacketPointerArray_t packets;
    packets = NULL;

    { // scope for first url without session id

        char *url = alloc_post_url(client);

        if (client->posting_client == NULL)
        {

            // Form the request URL

            esp_http_client_config_t config = {
                .url = url,
                .event_handler = http_client_polling_post_handler,
                .user_data = &packets,
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

        freeIfNotNull(&url);
    }

    esp_err_t err = esp_http_client_perform(client->posting_client);
    if (err != ESP_OK || packets == NULL)
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s response: %p ", esp_err_to_name(err), packets);
        goto cleanup;
    }

    if (get_array_size(packets) != 1)
    {
        ESP_LOGE(TAG, "Expected one 'ok' from server, got something else");
        goto cleanup;
    }

    // allocate posting user if not present
    if (packets[0]->eio_type == EIO_PACKET_OK_SERVER)
    {
        ESP_LOGD(TAG, "Ok from server response array %p", packets);
    }
    else
    {
        ESP_LOGE(TAG, "Not ok from server after send");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, packets[0]->data, packets[0]->len, ESP_LOG_ERROR);
    }

cleanup:
    if (packets != NULL)
    {
        free_packet_arr(&packets);
    }
    if (client->posting_client != NULL)
    {
#if REBUILD_CLIENT_POST
        esp_http_client_cleanup(client->posting_client);
        client->posting_client = NULL;
#else
        esp_http_client_close(client->posting_client);
#endif
    }

    return err;
}

esp_err_t sio_send_packet_websocket(sio_client_t *client, const Packet_t *packet)
{
    assert(false && "Not implemented");
    return ESP_OK;
}

bool sio_client_is_connected(sio_client_id_t clientId)
{
    sio_client_t *client = sio_client_get_and_lock(clientId);
    bool ret = client->status == SIO_CLIENT_STATUS_CONNECTED;
    unlockClient(client);
    return ret;
}
