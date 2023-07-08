// based on https://github.com/socketio/socket.io-client-cpp/blob/3.1.0/src/internal/sio_packet.cpp

//
//  sio_packet.cpp
//
//  Created by Melo Yao on 3/22/15.
//

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <sio_types.h>
#include <esp_types.h>

    typedef struct
    {
        eio_packet_t eio_type;
        sio_packet_t sio_type;

        char *json_start; // pointer inside buffer pointing to the start of the data (start of the json)

        char *data; // raw data
        size_t len;
    } Packet_t;

    typedef Packet_t **PacketPointerArray_t;

    void parse_packet(Packet_t *packet_p);

    // locks internally
    Packet_t *alloc_message(const char *json_str, const char *event_str);

    int get_array_size(PacketPointerArray_t arr);

    void free_packet(Packet_t **packet_p_p);
    void free_packet_arr(PacketPointerArray_t *arr_p_p);

    void print_packet(const Packet_t *packet_p);
    void print_packet_arr(PacketPointerArray_t arr);
    // util

    void setEioType(Packet_t *packet, eio_packet_t type);
    void setSioType(Packet_t *packet, sio_packet_t type);

#ifdef __cplusplus
}
#endif