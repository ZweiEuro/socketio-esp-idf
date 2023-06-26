// based on https://github.com/socketio/socket.io-client-cpp/blob/3.1.0/src/internal/sio_packet.cpp

//
//  sio_packet.cpp
//
//  Created by Melo Yao on 3/22/15.
//

#pragma once

#include <internal/sio_types.h>
#include <esp_types.h>

typedef struct Packet_t
{
    uint32_t pack_id;
    eio_packet_t type;
    uint32_t socket_id;

    uint8_t *raw;  // raw gotten information
    uint8_t *data; // directly at the data
} Packet;

void parse(const uint8_t *raw_buffer, Packet *packet);
