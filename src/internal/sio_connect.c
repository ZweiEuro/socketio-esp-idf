#include <sio_client.h>
#include <sio_types.h>
#include <internal/sio_packet.h>
#include <internal/task_functions.h>
#include <utility.h>
#include <internal/sio_connect.h>
#include <internal/sio_handshake.h>

static const char *TAG = __FILENAME__;

esp_err_t sio_connect(sio_client_t *client)
{

    assert(sio_client_is_locked(client->client_id) && "Client is not locked");

    // the client must not be connected yet
    if (client->status == SIO_CLIENT_STATUS_CONNECTED)
    {
        ESP_LOGW(TAG, "Client %d already connected", client->client_id);
        return ESP_ERR_INVALID_STATE;
    }

    // first perform a successful handshake
    esp_err_t err = sio_handshake(client);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Handshake succeeded for client %d", client->client_id);
    }
    else
    {
        ESP_LOGE(TAG, "Handshake failed for client %d %s",
                 client->client_id, esp_err_to_name(err));

        return err;
    }

    if (client->transport == SIO_TRANSPORT_WEBSOCKETS)
    {
        assert(false && "Websockets not implemented yet");
    }
    else if (client->transport == SIO_TRANSPORT_POLLING)
    {

        xTaskCreate(&sio_continuous_polling_task, "sio_polling", 4096, (void *)client->client_id, 6, &client->polling_task_handle);

        if (client->polling_task_handle == NULL)
        {
            ESP_LOGE(TAG, "Failed to create polling task");
            return ESP_FAIL;
        }
    }

    client->status = SIO_CLIENT_STATUS_CONNECTED;

    return err;
}
