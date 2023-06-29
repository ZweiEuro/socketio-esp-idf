#include <internal/http_handlers.h>
#include <internal/sio_packet.h>
#include <utility.h>
#include <sio_types.h>
#include <esp_assert.h>
#include <esp_log.h>
#include "esp_tls.h"

static const char *TAG = "[sio:http_handlers]";

esp_err_t http_client_polling_handler(esp_http_client_event_t *evt)
{
    Packet_t *response_data = *((Packet_t **)evt->user_data);

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

        if (!esp_http_client_is_chunked_response(evt->client))
        {

            if (response_data == NULL)
            {
                ESP_LOGD(TAG, "Allocating response data");
                response_data = (Packet_t *)calloc(1, sizeof(Packet_t));
                *((Packet_t **)evt->user_data) = response_data;

                response_data->data = (char *)calloc(1, esp_http_client_get_content_length(evt->client) + 1);

                response_data->len = 0;
                if (response_data->data == NULL)
                {
                    ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                    return ESP_FAIL;
                }
            }
            memcpy(response_data->data + response_data->len, evt->data, evt->data_len);

            response_data->len += evt->data_len;
        }
        else
        {
            ESP_LOGE(TAG, "Chunked encoding is not supported");
            return ESP_FAIL;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        // reponse data written into the user_data buffer
        if (response_data != NULL)
        {

            parse_packet(response_data);
        }

        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            if (response_data != NULL)
            {
                freeIfNotNull(response_data->data);
                freeIfNotNull(response_data);
            }
        }

        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;

    default:
        ESP_LOGE(TAG, "Unhandled event %d", evt->event_id);
        break;
    }
    return ESP_OK;
}