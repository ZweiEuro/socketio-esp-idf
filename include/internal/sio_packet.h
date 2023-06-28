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

    typedef struct Packet_t
    {
        eio_packet_t type;
        uint32_t socket_id;

        char *raw;  // raw gotten information
        char *data_start; // directly at the data
    } Packet;

    void parse(const char *raw_buffer, Packet *packet);

#ifdef __cplusplus
}
#endif