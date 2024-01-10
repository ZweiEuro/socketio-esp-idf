
#include <internal/task_functions.h>
#include <internal/sio_packet.h>
#include <http_polling_handlers.h>

#include <sio_client.h>
#include <sio_types.h>
#include <utility.h>
#include <esp_types.h>

static const char *TAG = "[SIO_TASK:polling]";

void sio_polling_task(void *pvParameters)
{
    sio_client_id_t clientId = (sio_client_id_t)pvParameters;

    static PacketPointerArray_t response_packets;

    // initializing polling task
    {

        sio_client_t *client = sio_client_get_and_lock(clientId);

        assert(client);
        assert(client->polling_client == NULL && "Polling client is not NULL");

        char *url = alloc_polling_get_url(client);

        esp_http_client_config_t config = {
            .url = url,
            .event_handler = http_client_polling_get_handler,
            .user_data = &response_packets,
            .disable_auto_redirect = true,
            .timeout_ms = (client->server_ping_interval_ms == 0 ? 5000 : (client->server_ping_interval_ms + client->server_ping_timeout_ms * 2))};
        client->polling_client = esp_http_client_init(&config);
        assert(client->polling_client != NULL && "Failed to init polling client");

        unlockClient(client);
        free(url);
    }

    ESP_LOGI(TAG, "Started polling task for client %d", clientId);

    while (true)
    {
        response_packets = NULL;
        sio_client_t *client = sio_client_get_and_lock(clientId);
        assert(client != NULL && "Client is NULL");
        sio_client_status_t currentStatus = client->status;
        unlockClient(client);

        if (currentStatus != SIO_CLIENT_STATUS_CONNECTED)
        {
            ESP_LOGI(TAG, "Stopping polling task, status is %d", client->status);
            goto end_ok;
        }

        esp_err_t err = esp_http_client_perform(client->polling_client);

        if (err != ESP_OK)
        {
            if (err == ESP_ERR_HTTP_EAGAIN)
            {
                ESP_LOGI(TAG, "HTTP request timed out. Ping timeout");
            }

            // todo: emit DISCONNECTED event on any fail
            int r = esp_http_client_get_errno(client->polling_client);
            ESP_LOGE(TAG, "HTTP POLLING GET request failed: %s %i", esp_err_to_name(err), r);
            goto end_error;
        }

        int http_response_status_code = esp_http_client_get_status_code(client->polling_client);
        int http_response_content_length = esp_http_client_get_content_length(client->polling_client);

        if (http_response_status_code != 200)
        {
            ESP_LOGW(TAG, "Polling HTTP request failed with status code %d", http_response_status_code);
            goto end_error;
        }
        if (http_response_content_length <= 0)
        {
            ESP_LOGW(TAG, "Polling HTTP request failed: No content returned.");
            goto end_error;
        }

        // go through all messages and handle all non message related messages

        for (int i = 0; i < get_array_size(response_packets); i++)
        {

            Packet_t *response_packet = response_packets[0];

            switch (response_packet->eio_type)
            {
            case EIO_PACKET_PING:
                // send pong back

                Packet_t *p = (Packet_t *)calloc(1, sizeof(Packet_t));
                p->data = (char *)calloc(1, 2);
                p->len = 2;
                setEioType(p, EIO_PACKET_PONG);
                esp_err_t ret = sio_send_packet(client->client_id, p);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to send PONG packet");
                }
                else
                {

                    sio_client_t *client = sio_client_get_and_lock(clientId);
                    time(&client->last_sent_pong);
                    unlockClient(client);
                }
                free_packet(&p);

                break;

            case EIO_PACKET_CLOSE:
                ESP_LOGI(TAG, "Received close packet");
                // we still want to send the close event here
                goto end_error;
                break;

            case EIO_PACKET_MESSAGE:
                // do nothing, will get forwarded
                break;

            default:
                ESP_LOGW(TAG, "unhandled packet type %d", response_packet->eio_type);
                break;
            }
        }

        if (get_array_size(response_packets) == 1 && response_packets[0]->eio_type != EIO_PACKET_MESSAGE)
        {
            // Single package and just ping
            continue;
        }

        ESP_LOGI(TAG, "Poller Received %d packets", get_array_size(response_packets));
        {
            sio_event_data_t event_data = {
                .client_id = clientId,
                .packets_pointer = response_packets,
                .len = get_array_size(response_packets)};

            esp_event_post(SIO_EVENT, SIO_EVENT_RECEIVED_MESSAGE, &event_data, sizeof(sio_event_data_t), pdMS_TO_TICKS(50));
        }
    }
end_error:
{
    sio_event_data_t event_data = {
        .client_id = clientId,
        .packets_pointer = NULL,
        .len = 0};

    esp_event_post(SIO_EVENT, SIO_EVENT_DISCONNECTED, &event_data, sizeof(sio_event_data_t), pdMS_TO_TICKS(50));
}
end_ok:

    sio_client_t *client = sio_client_get_and_lock(clientId);
    client->status = SIO_CLIENT_STATUS_CLOSED;
    esp_http_client_close(client->polling_client);
    esp_http_client_cleanup(client->polling_client);
    client->polling_client = NULL;

    unlockClient(client);

    vTaskDelete(NULL);
}