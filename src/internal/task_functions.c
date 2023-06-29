
#include <internal/task_functions.h>
#include <internal/sio_packet.h>
#include <http_handlers.h>

#include <sio_client.h>
#include <sio_types.h>
#include <utility.h>
#include <esp_types.h>

static const char *TAG = "[SIO_TASK:polling]";

void sio_polling_task(void *pvParameters)
{
    sio_client_id_t *clientId = (sio_client_id_t *)pvParameters;
    Packet_t *response_packet = NULL;

    while (true)
    {
        sio_client_t *client = sio_client_get_and_lock(*clientId);
        {
            char *url = alloc_polling_get_url(client);

            if (client->polling_client == NULL)
            {
                esp_http_client_config_t config = {
                    .url = url,
                    .event_handler = http_client_polling_get_handler,
                    .user_data = &response_packet,
                    .disable_auto_redirect = true,
                    .timeout_ms = client->server_ping_timeout_ms * 2 * 1000,

                };
                client->polling_client = esp_http_client_init(&config);
            }
            else
            {
                esp_http_client_set_url(client->polling_client, url);
            }
            ESP_LOGI(TAG, "Polling URL: %s", url);
            freeIfNotNull(url);
        }
        unlockClient(client);
        esp_err_t err = esp_http_client_perform(client->polling_client);

        if (err != ESP_OK)
        {
            // todo: emit DISCONNECTED event on any fail
            int r = esp_http_client_get_errno(client->polling_client);
            ESP_LOGE(TAG, "HTTP POLLING GET request failed: %s %i", esp_err_to_name(err), r);
            break;
        }

        int http_response_status_code = esp_http_client_get_status_code(client->polling_client);
        int http_response_content_length = esp_http_client_get_content_length(client->polling_client);
        ESP_LOGI(
            TAG, "HTTP GET Status = %d, content_length = %d",
            http_response_status_code,
            http_response_content_length);

        if (http_response_status_code != 200)
        {
            ESP_LOGW(TAG, "Polling HTTP request failed with status code %d", http_response_status_code);
            break;
        }
        if (http_response_content_length <= 0)
        {
            ESP_LOGW(TAG, "Polling HTTP request failed: No content returned.");
            break;
        }

        print_packet(response_packet);
        ESP_LOG_BUFFER_HEX(TAG, response_packet->data, response_packet->len);

        if (response_packet->eio_type == EIO_PACKET_NOOP)
        {
            if (response_packet->sio_type == SIO_PACKET_RS)
            {
                ESP_LOGI(TAG, "Sio Separated package received");
                break;
            }
        }

        free_packet(response_packet);
        response_packet = NULL;

        // vTaskDelay(pdMS_TO_TICKS(sio_client->ping_interval_ms));
    }
    sio_client_t *client = sio_client_get_and_lock(*clientId);

    client->polling_client_running = false;
    esp_http_client_cleanup(client->polling_client);
    client->polling_client = NULL;

    unlockClient(client);

    vTaskDelete(NULL);
}