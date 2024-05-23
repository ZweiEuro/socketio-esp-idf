#include <sio_client.h>
#include <sio_types.h>
#include <internal/sio_packet.h>
#include <internal/task_functions.h>
#include <internal/sio_handshake.h>
#include <internal/sio_connect.h>

#include <utility.h>
#include <cJSON.h>

#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include <esp_log.h>

bool inited = false;

TaskHandle_t sio_worker_handle = NULL;

EventGroupHandle_t wifi_event_group;

const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "SIO";

void sio_worker_task(void *pvParameters);

void sio_got_ip(void *arg, esp_event_base_t event_base,
                int32_t event_id, void *event_data)

{
    ESP_LOGI(TAG, "Wifi got new ip, restart worker");

    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
}

void sio_sta_lost(void *arg, esp_event_base_t event_base,
                  int32_t event_id, void *event_data)

{
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    // stop all clients
    ESP_LOGI(TAG, "Wifi disconnected, stop everything");
    for (sio_client_id_t clientId = 0; clientId < SIO_MAX_PARALLEL_SOCKETS; clientId++)
    {
        sio_client_close(clientId);
    }
}

// call this before first callin esp_wifi_start();
esp_err_t sio_init()
{

    if (inited)
    {
        return ESP_OK;
    }
    inited = true;

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &sio_got_ip, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &sio_sta_lost, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_STOP, &sio_sta_lost, NULL, NULL));

    // subscribe to all networking events and init all queues

    xTaskCreate(&sio_worker_task, "SIO_worker", 4096, NULL, 6, &sio_worker_handle);

    return ESP_OK;
}

void sio_worker_task(void *pvParameters)
{
    for (;;)
    {
        // wait for connection
        xEventGroupWaitBits(wifi_event_group,
                            WIFI_CONNECTED_BIT,
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);

        // go through all SIO clients and connect those that are waiting to be started

        for (sio_client_id_t client_index = 0; client_index < SIO_MAX_PARALLEL_SOCKETS; client_index++)
        {
            sio_client_t *client = sio_client_get_and_lock(client_index);

            if (client == NULL)
            {
                continue;
            }

            const sio_client_id_t clientId = client->client_id;

            if (client->status == SIO_CLIENT_STATUS_CONNECTED)
            {
                // ESP_LOGI(TAG, "Client %d already connected", clientId);
                unlockClient(client);
                continue;
            }

            if (client->status == SIO_CLIENT_ERROR)
            {
                ESP_LOGI(TAG, "Client %d in error state, closing", clientId);
                sio_client_close(clientId);
                unlockClient(client);
                continue;
            }

            // connect it
            esp_err_t err = sio_connect(client);

            // in either case the client is "done"
            unlockClient(client);

            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "Connect succeeded for client %d", client->client_id);

                sio_event_data_t event_data = {
                    .client_id = client->client_id,
                    .packets_pointer = NULL,
                    .len = 0};
                esp_event_post(SIO_EVENT, SIO_EVENT_CONNECTED, &event_data, sizeof(sio_event_data_t), pdMS_TO_TICKS(50));
            }
            else
            {
                ESP_LOGI(TAG, "Connect failed for client %d %s",
                         client->client_id, esp_err_to_name(err));

                sio_event_data_t event_data = {
                    .client_id = client_index,
                    .packets_pointer = NULL,
                    .len = 0};
                esp_event_post(SIO_EVENT, SIO_EVENT_CONNECT_ERROR, &event_data, sizeof(sio_event_data_t), pdMS_TO_TICKS(50));
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGE(TAG, "SIO worker task started");
    assert(false);
}
