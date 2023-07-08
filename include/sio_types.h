#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <esp_types.h>

    typedef int8_t sio_client_id_t;

    // low level message
    typedef enum
    {
        EIO_PACKET_NONE = -2,
        EIO_PACKET_OK_SERVER = -1,
        EIO_PACKET_OPEN = 0,
        EIO_PACKET_CLOSE,
        EIO_PACKET_PING,
        EIO_PACKET_PONG,
        EIO_PACKET_MESSAGE,
        EIO_PACKET_UPGRADE,
        EIO_PACKET_NOOP
    } eio_packet_t;

    // packets that talk about events
    typedef enum
    {
        SIO_PACKET_NONE = -1,
        SIO_PACKET_CONNECT = 0,
        SIO_PACKET_DISCONNECT,
        SIO_PACKET_EVENT,
        SIO_PACKET_ACK,
        SIO_PACKET_CONNECT_ERROR,
        SIO_PACKET_BINARY_EVENT,
        SIO_PACKET_BINARY_ACK
    } sio_packet_t;

    // events that are given to the outside system
    typedef enum
    {
        SIO_EVENT_READY = 0,               /* SocketIO Client ready */
        SIO_EVENT_CONNECTED,               /* SocketIO Client connected and got some kind of enginio response*/
        SIO_EVENT_RECEIVED_MESSAGE,        /* SocketIO Client received message */
        SIO_EVENT_CONNECT_ERROR,           /* SocketIO Client failed to connect */
        SIO_EVENT_UPGRADE_TRANSPORT_ERROR, /* SocketIO Client failed upgrade transport */
        SIO_EVENT_DISCONNECTED             /* SocketIO Client disconnected */
    } sio_event_t;

    typedef enum
    {
        SIO_CLIENT_DISCONNECTED = 0,
        SIO_CLIENT_CONNECTED,
    } sio_client_status_t;

    typedef enum
    {
        SIO_TRANSPORT_POLLING = 0, /* polling */
        SIO_TRANSPORT_WEBSOCKETS   /* websockets */
    } sio_transport_t;

    // http structs

#ifdef __cplusplus
}
#endif