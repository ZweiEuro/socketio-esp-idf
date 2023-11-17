
#include <internal/sio_packet.h>
#include <utility.h>

#include <esp_log.h>

#include <sio_client.h>

const char *TAG = "[sio_packet]";
const char *empty_str = "";

static const char base64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// https://github.com/espressif/esp-idf/blob/8fc8f3f47997aadba21facabc66004c1d22de181/components/wpa_supplicant/src/utils/base64.c#L84C1-L150C2
static unsigned char *base64_gen_decode(const char *src, size_t len,
                                        size_t *out_len)

{
    const char *table = base64_table;
    unsigned char dtable[256], *out, *pos, block[4], tmp;
    size_t i, count, olen;
    int pad = 0;
    size_t extra_pad;

    memset(dtable, 0x80, 256);
    for (i = 0; i < sizeof(base64_table) - 1; i++)
        dtable[(unsigned char)table[i]] = (unsigned char)i;
    dtable['='] = 0;

    count = 0;
    for (i = 0; i < len; i++)
    {
        if (dtable[(unsigned char)src[i]] != 0x80)
            count++;
    }

    if (count == 0)
        return NULL;
    extra_pad = (4 - count % 4) % 4;

    olen = (count + extra_pad) / 4 * 3;
    pos = out = malloc(olen);
    if (out == NULL)
        return NULL;

    count = 0;
    for (i = 0; i < len + extra_pad; i++)
    {
        unsigned char val;

        if (i >= len)
            val = '=';
        else
            val = src[i];
        tmp = dtable[val];
        if (tmp == 0x80)
            continue;

        if (val == '=')
            pad++;
        block[count] = tmp;
        count++;
        if (count == 4)
        {
            *pos++ = (block[0] << 2) | (block[1] >> 4);
            *pos++ = (block[1] << 4) | (block[2] >> 2);
            *pos++ = (block[2] << 6) | block[3];
            count = 0;
            if (pad)
            {
                if (pad == 1)
                    pos--;
                else if (pad == 2)
                    pos -= 2;
                else
                {
                    /* Invalid padding */
                    free(out);
                    return NULL;
                }
                break;
            }
        }
    }

    *out_len = pos - out;
    return out;
}

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

    if (packet->data[0] == 'b')
    {
        packet->eio_type = EIO_PACKET_MESSAGE;
        packet->sio_type = SIO_PACKET_BINARY_EVENT;

        // Careful with this! binary data starts offset by 1 and is base64 encdoded

        char *decoded_b64 = (char *)base64_gen_decode(packet->data + 1, packet->len - 1, &packet->len);

        if (decoded_b64 == NULL)
        {
            ESP_LOGE(TAG, "Failed to decode base64 dataset from SIO");
            return;
        }
        free(packet->data);
        packet->data = decoded_b64;
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
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, packet->data, packet->len, ESP_LOG_WARN);
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