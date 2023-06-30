
#include <internal/sio_packet.h>
#include <utility.h>

#include <esp_log.h>
#include <sio_client.h>

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

    if (packet->len == 2 && packet->data[0] == 'o' && packet->data[1] == 'k')
    {
        packet->eio_type = EIO_PACKET_OK_SERVER;
        packet->sio_type = SIO_PACKET_NONE;
        packet->json_start = NULL;
        return;
    }

    packet->eio_type = (eio_packet_t)(packet->data[0] - '0');
    packet->sio_type = SIO_PACKET_NONE;
    packet->json_start = NULL;

    if (packet->len <= 2)
    {
        ESP_LOGD(TAG, "Packet length is less than 2, single indicator");
        return;
    }

    switch (packet->eio_type)
    {
    case EIO_PACKET_OPEN:
        packet->json_start = packet->data + 1;
        break;

    case EIO_PACKET_MESSAGE:
        packet->sio_type = (sio_packet_t)(packet->data[1] - '0');
        // find the start of the json message, (the namespace might be in between but we just ignore it)

        for (int i = 2; i < packet->len; i++)
        {
            if (packet->data[i] == '{' || packet->data[i] == '[')
            {
                packet->json_start = packet->data + i;
                break;
            }
        }
        break;

    default:

        if (packet->data[1] == ASCII_RS)
        {
            packet->sio_type = SIO_PACKET_RS;
        }
        else
        {
            packet->sio_type = (sio_packet_t)(packet->data[1] - '0');
        }
        break;
    }
}

void free_packet(Packet_t *packet)
{

    freeIfNotNull(packet->data);
    freeIfNotNull(packet);
}

Packet_t *alloc_packet(const sio_client_id_t clientId, const char *data, size_t len)
{

    sio_client_t *client = sio_client_get_and_lock(clientId);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to get client");
        unlockClient(client);
        return NULL;
    }

    Packet_t *packet = calloc(1, sizeof(Packet_t));
    if (packet == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for packet");
        unlockClient(client);

        return NULL;
    }
    // we stick '44' at the start every time
    packet->len = 2; // 2 for the '44'
    if (strcmp(client->nspc, "/") != 0)
    {
        packet->len += strlen(client->nspc) + 2; // 2 for '/' and ',' at start and end
    }
    packet->len += len;
    packet->data = calloc(1, len + 1 + 2);

    if (packet->data == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for packet data");
        free_packet(packet);
        unlockClient(client);

        return NULL;
    }

    if (strcmp(client->nspc, "/") == 0)
    {
        sprintf(packet->data, "44%s", data);
    }
    else
    {
        sprintf(packet->data, "44/%s,%s", client->nspc, data);
    }

    parse_packet(packet);
    unlockClient(client);

    return packet;
}

void setEioType(Packet_t *packet, eio_packet_t type)
{
    packet->eio_type = type;
    packet->data[0] = type + '0';
}

void setSioType(Packet_t *packet, sio_packet_t type)
{
    if (packet->eio_type != EIO_PACKET_MESSAGE)
    {
        ESP_LOGE(TAG, "Packet is not a message packet, setting sio type is not allowed");
        return; // only message packets have a sio_type (see parse_packet)
    }
    packet->sio_type = type;
    packet->data[1] = type + '0';
}

void print_packet(const Packet_t *packet)
{
    ESP_LOGI(TAG, "Packet: EIO:%d SIO:%d len:%d\n%s",
             packet->eio_type, packet->sio_type, packet->len,
             packet->data);
}