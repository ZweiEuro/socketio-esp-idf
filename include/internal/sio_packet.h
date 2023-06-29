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

    void parse_packet(Packet_t *packet);

    Packet_t *alloc_packet(const sio_client_id_t clientId, const char *data, size_t len);

    void free_packet(Packet_t *packet);

    // util

    void setEioType(Packet_t *packet, eio_packet_t type);
    void setSioType(Packet_t *packet, sio_packet_t type);

#ifdef __cplusplus
}
#endif