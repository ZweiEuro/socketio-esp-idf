#include <sio_types.h>
#include <internal/sio_packet.h>
#include <internal/sio_send.h>
#include <internal/task_functions.h>
#include <utility.h>
#include <cJSON.h>

#include <esp_log.h>
static const char *TAG = "[sio_send]";

ESP_EVENT_DEFINE_BASE(SIO_EVENT);

static esp_http_client_handle_t sio_post_client = NULL;
static PacketPointerArray_t packets = NULL;
static SemaphoreHandle_t sio_post_lock = NULL;

// TODO: Use single global post client

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

    if (sio_post_lock == NULL)
    {
        sio_post_lock = xSemaphoreCreateMutex();
    }
    else
    {
        xSemaphoreTake(sio_post_lock, portMAX_DELAY);
    }

    sio_client_t *client = sio_client_get_and_lock(clientId);
    assert(client != NULL && "Client is NULL");

    assert(sio_client_is_locked(client->client_id) && "Client is not locked");

    esp_err_t ret = ESP_FAIL;

    if (client->transport == SIO_TRANSPORT_WEBSOCKETS)
    {
        assert(false && "Not implemented");
    }
    else if (client->transport == SIO_TRANSPORT_POLLING)
    {
        ret = sio_send_packet_polling(client, packet);
    }
    else
    {
        ret = ESP_ERR_INVALID_ARG;
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send packet %s closing post client", esp_err_to_name(ret));

        esp_http_client_cleanup(sio_post_client);
        esp_http_client_close(sio_post_client);
        sio_post_client = NULL;
    }

    unlockClient(client);
    xSemaphoreGive(sio_post_lock);
    return ret;
}

esp_err_t sio_send_packet_polling(sio_client_t *client, const Packet_t *packet)
{
    static PacketPointerArray_t packets;
    packets = NULL;

    assert(client != NULL && "Client is NULL");
    assert(sio_client_is_locked(client->client_id) && "Client is not locked");

    if (sio_post_client == NULL)
    {

        char *url = alloc_post_url(client);

        esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 5000,
            .disable_auto_redirect = true,
            .event_handler = http_client_polling_post_handler,
            .user_data = &packets};

        sio_post_client = esp_http_client_init(&config);

        ESP_LOGI(TAG, "Post client init %p %s", sio_post_client, url);
        free(url);

        esp_http_client_set_header(sio_post_client, "Content-Type", "text/plain;charset=UTF-8");
        esp_http_client_set_header(sio_post_client, "Accept", "*/*");

        assert(sio_post_client != NULL && "Failed to init http client");
    }
    else
    {
        char *url = alloc_post_url(client);
        esp_http_client_set_url(sio_post_client, url);
        free(url);
    }

    esp_http_client_set_post_field(sio_post_client, packet->data, packet->len);

    ESP_LOGI(TAG, "Sending packet %p", packet);

    esp_err_t err = esp_http_client_perform(sio_post_client);
    if (err != ESP_OK || packets == NULL)
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s response: %p did not get any response", esp_err_to_name(err), packets);

        return ESP_FAIL;
    }

    if (get_array_size(packets) != 1)
    {
        ESP_LOGE(TAG, "Expected one 'ok' from server, got something else");
        goto cleanup;
    }

    // allocate posting user if not present
    if (packets[0]->eio_type == EIO_PACKET_OK_SERVER)
    {
        ESP_LOGI(TAG, "Ok from server response array %p", packets);
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

    return err;
}

bool sio_client_is_connected(sio_client_id_t clientId)
{
    sio_client_t *client = sio_client_get_and_lock(clientId);
    bool ret = client->status == SIO_CLIENT_STATUS_CONNECTED;
    unlockClient(client);
    return ret;
}
