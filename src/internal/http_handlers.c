#include <internal/http_handlers.h>
#include <internal/sio_packet.h>
#include <utility.h>
#include <sio_types.h>
#include <esp_assert.h>
#include <esp_log.h>
#include "esp_tls.h"

static const char *TAG = "[sio:http_handlers]";

esp_err_t http_client_polling_get_handler(esp_http_client_event_t *evt)
{
    // TODO: This makes a race condition if the handler is used twice at the same time.
    // I don't think this happenes due to the esp implementation under the hood
    static char *recv_buffer = NULL;
    static size_t recv_length = 0;

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED with pointer %p", recv_buffer);
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

            if (recv_buffer == NULL)
            {
                int all_size = esp_http_client_get_content_length(evt->client) + 2;

                recv_buffer = (char *)calloc(1, all_size);
                recv_length = 0;

                // ESP_LOGI(TAG, "allocated %d at %p", all_size, recv_buffer);

                if (recv_buffer == NULL)
                {
                    ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                    return ESP_FAIL;
                }
            }
            memcpy((void *)recv_buffer + recv_length, evt->data, evt->data_len);
            recv_length += evt->data_len;
        }
        else
        {
            ESP_LOGD(TAG, "Chunked response %d %s", evt->data_len, (char *)evt->data);
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");

        // parse the data into packets, multi packet support
        if (recv_buffer != NULL && recv_length > 0)
        {
            ESP_LOGI(TAG, "Received %i bytes at %p of data %s",
                     recv_length, recv_buffer, (char *)recv_buffer);

            recv_buffer[recv_length] = ASCII_RS;
            recv_buffer[recv_length + 1] = '\0';

            PacketPointerArray_t response_arr = *((PacketPointerArray_t *)evt->user_data);

            if (response_arr != NULL)
            {
                ESP_LOGE(TAG, "User data is not null, this should not happen");
                goto freeBuffers;
            }

            ESP_LOGI(TAG, "Received %i bytes of data  destination for arr pointer %p, %s,",
                     recv_length, evt->user_data, (char *)recv_buffer);

            // count how many packets ( by scanning for ASCII_RS)
            // the '<=' is important so we take the end delimiter with us and know if there is a single packet
            int rs_count = 0;
            for (int i = 0; i <= recv_length; i++)
            {
                if (recv_buffer[i] == ASCII_RS)
                {
                    rs_count++;
                }
            }

            ESP_LOGD(TAG, "Found %i packets", rs_count);

            if (rs_count == 0)
            {
                // ???
                ESP_LOGW(TAG, "No packets found, creating empty packet");

                goto freeBuffers;
            }

            // allocate the response array of pointers
            response_arr = (PacketPointerArray_t)calloc(rs_count + 1, sizeof(Packet_t *));

            ESP_LOGW(TAG, "Allocated l:%d packets array %p", rs_count, response_arr);

            if (response_arr == NULL)
            {
                ESP_LOGE(TAG, "Failed alloc response array");
                goto freeBuffers;
            }

            // initially all of them are null, fill all of them up except the last which will stay null, indicating end

            char *packet_start = strtok(recv_buffer, ASCII_RS_STRING);
            for (int i = 0; i < rs_count; i++)
            {

                if (packet_start == NULL)
                {
                    ESP_LOGE(TAG, "Failed to parse packet");
                    goto freeBuffers;
                }

                Packet_t *new_packet_p = (Packet_t *)calloc(1, sizeof(Packet_t));

                ESP_LOGE(TAG, "Allocated packet %p", new_packet_p);

                new_packet_p->data = strdup(packet_start);
                new_packet_p->len = strlen(packet_start);
                parse_packet(new_packet_p);

                response_arr[i] = new_packet_p;

                packet_start = strtok(NULL, ASCII_RS_STRING);
            }
            *((PacketPointerArray_t *)evt->user_data) = response_arr;
            print_packet_arr(response_arr);
        }
    freeBuffers:
        if (recv_buffer != NULL)
        {
            // ESP_LOGI(TAG, "Freed %p", recv_buffer);
            free(recv_buffer);
        }
        recv_buffer = NULL;
        recv_length = 0;

        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGD(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGD(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            if (recv_buffer != NULL)
            {
                free(recv_buffer);
                recv_buffer = NULL;
                recv_length = 0;
            }
        }

        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;

    default:
        ESP_LOGE(TAG, "Unhandled event %d", evt->event_id);
        break;
    }
    return ESP_OK;
}

esp_err_t http_client_polling_post_handler(esp_http_client_event_t *evt) // Any will do fine, posting is not done with the handler, handler only handles receiving
{
    return http_client_polling_get_handler(evt);
}
