#pragma once
#include <esp_types.h>

// low level message
typedef enum
{
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
    SIO_EVENT_CONNECTED_HTTP,          /* SocketIO Client connected over HTTP */
    SIO_EVENT_CONNECTED_WS,            /* SocketIO Client connected over WebSockets */
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
    SIO_TRANSPORT_POLLING,   /* polling */
    SIO_TRANSPORT_WEBSOCKETS /* websockets */
} sio_transport_t;

typedef void (*sio_on_event_fptr_t)(const uint8_t *event_name, const uint8_t *message);

typedef struct
{
    uint32_t client_id;              /* Our internal id */
    uint8_t eio_version;             /* EngineIO protocol version */
    uint8_t max_connect_retries;     /* Maximum connection retry attempts */
    uint8_t retry_interval_ms;       /* Pause between retry attempts */
    uint8_t server_ping_interval_ms; /* Server-configured ping interval */
    uint8_t server_ping_timeout_ms;  /* Server-configured ping wait-timeout */
    sio_transport_t transport;       /* Preferred SocketIO transport */
    const uint8_t *server_address;   /* SocketIO server address with port */
    uint8_t *token;                  /* Random token for cache prevention */
    uint8_t *server_session_id;      /* SocketIO session ID */
    uint8_t *nspc;                   /* SocketIO namespace */

    sio_on_event_fptr_t *on_event; /* Function-pointer to user-defined function */
    size_t on_event_size;
} sio_client_t;
