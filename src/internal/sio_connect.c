#include <sio_client.h>
#include <sio_types.h>
#include <internal/sio_packet.h>
#include <internal/task_functions.h>
#include <utility.h>
#include <internal/sio_connect.h>

esp_err_t sio_connect(sio_client_t *client)
{
    assert(client->status == SIO_CLIENT_STATUS_HANDSHOOK && "Client did not sio_handshake?");

    esp_err_t err = ESP_FAIL;

    if (client->transport == SIO_TRANSPORT_WEBSOCKETS)
    {
        assert(false && "Websockets not implemented yet");
    }
    else if (client->transport == SIO_TRANSPORT_POLLING)
    {

        xTaskCreate(&sio_polling_task, "sio_polling", 4096, (void *)client->client_id, 6, NULL);
        err = ESP_OK;
    }

    if (err != ESP_OK)
    {
        client->status = SIO_CLIENT_STATUS_ERROR;
    }
    else
    {
        sio_event_data_t event_data = {
            .client_id = client->client_id,
            .packets_pointer = NULL,
            .len = 0};
        esp_event_post(SIO_EVENT, SIO_EVENT_CONNECTED, &event_data, sizeof(sio_event_data_t), pdMS_TO_TICKS(50));

        client->status = SIO_CLIENT_STATUS_CONNECTED;
    }

    return err;
}
