
#include <internal/sio_packet.h>
#include <utility.h>

#include <esp_log.h>

const char *TAG = "[sio_packet]";

void parse_packet(Packet_t *packet)
{

    if (packet->data == NULL)
    {
        ESP_LOGE(TAG, "Packet data is NULL");
        return;
    }

    if (packet->len < 1)
    {
        ESP_LOGE(TAG, "Packet length is less than 1");
        return;
    }

    packet->eio_type = (eio_packet_t)(packet->data[0] - '0');
    packet->sio_type = SIO_PACKET_NONE;

    if (packet->eio_type == EIO_PACKET_MESSAGE)
    {
        packet->sio_type = (sio_packet_t)(packet->data[1] - '0');
        packet->json_start = packet->data + 2;
    }
    else
    {
        packet->json_start = packet->data + 1;
    }
}

void free_packet(Packet_t *packet)
{

    freeIfNotNull(packet->data);
    freeIfNotNull(packet);
}