
#include <internal/sio_packet.h>
#include <utility.h>

#include <esp_log.h>
#include <sio_client.h>

const char *TAG = "[sio_packet]";
const char *empty_str = "";

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
        ESP_LOGW(TAG, "Unknown packet type %d %s", packet->eio_type, packet->data);
        break;
    }
}

void free_packet(Packet_t **packet_p_p)
{
    Packet_t *packet_p = *packet_p_p;

    if (packet_p->data != NULL)
    {
        free(packet_p->data);
        packet_p->data = NULL;
    }

    free(packet_p);
    *packet_p_p = NULL;
}

int get_array_size(PacketPointerArray_t arr_p)
{
    if (arr_p == NULL)
    {
        return 0;
    }

    int i = 0;
    while (arr_p[i] != NULL)
    {
        i++;
    }
    return i;
}

void free_packet_arr(PacketPointerArray_t *arr_p)
{
    PacketPointerArray_t arr = *arr_p;

    // print_packet_arr(arr);

    int i = 0;
    while (arr[i] != NULL)
    {
        Packet_t *p = arr[i];
        free_packet(&p);
        i++;
    }
    free(arr);
    *arr_p = NULL;
}

Packet_t *alloc_message(const char *json_str, const char *event_str)
{
    if (json_str == NULL)
    {
        json_str = empty_str;
    }

    Packet_t *packet = calloc(1, sizeof(Packet_t));
    if (packet == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for packet");
        return NULL;
    }

    packet->eio_type = EIO_PACKET_MESSAGE;
    packet->sio_type = SIO_PACKET_EVENT;

    if (event_str == NULL)
    {

        packet->len = 2 + strlen(json_str);
        packet->data = calloc(1, packet->len + 1);

        sprintf(packet->data, "42%s", json_str);
    }
    else
    {
        // Events attach something before the json and make it an array
        // 42["event",json]

        packet->len = 2 + strlen("[\"") + strlen(event_str) + strlen("\",") + strlen(json_str) + strlen("]");
        packet->data = calloc(1, packet->len + 1);

        sprintf(packet->data, "42[\"%s\",%s]", event_str, json_str);
    }
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
    ESP_LOGI(TAG, "Packet: %p EIO:%d SIO:%d len:%d  -- %s",
             packet, packet->eio_type, packet->sio_type, packet->len,
             packet->data);
}

void print_packet_arr(PacketPointerArray_t arr)
{

    if (arr == NULL)
    {
        ESP_LOGW(TAG, "Not printing null packet arr");
        return;
    }

    ESP_LOGI(TAG, "Packet array: %p", arr);

    int i = 0;
    while (arr[i] != NULL)
    {
        print_packet(arr[i]);
        i++;
    }
}