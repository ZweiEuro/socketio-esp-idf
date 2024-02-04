
#pragma once

#include <sio_client.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t sio_send_string(const sio_client_id_t clientId, const char *data);
    esp_err_t sio_send_packet(const sio_client_id_t clientId, const Packet_t *packet);

    // NOT THREAD SAVE
    esp_err_t sio_send_packet_polling(sio_client_t *client, const Packet_t *packet);
    // NOT THREAD SAVE
    esp_err_t sio_send_packet_websocket(sio_client_t *client, const Packet_t *packet);
#ifdef __cplusplus
}
#endif