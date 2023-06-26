#pragma once

#include <sio_types.h>

typedef void (*sio_on_event_fptr_t)(const uint8_t *event_name, const uint8_t *message);

typedef struct
{
    uint32_t client_id;  /* Our internal id */
    uint8_t eio_version; /* EngineIO protocol version */

    uint16_t max_connect_retries; /* Maximum connection retry attempts */

    uint16_t retry_interval_ms;       /* Pause between retry attempts */
    uint16_t server_ping_interval_ms; /* Server-configured ping interval */
    uint16_t server_ping_timeout_ms;  /* Server-configured ping wait-timeout */

    sio_transport_t transport; /* Preferred SocketIO transport */
    char *server_address;      /* SocketIO server address with port */

    char *token;             /* Random token for cache prevention */
    char *server_session_id; /* SocketIO session ID */
    char *nspc;              /* SocketIO namespace */

    sio_on_event_fptr_t *on_event; /* Function-pointer to user-defined function */
    size_t on_event_size;
} sio_client_t;
