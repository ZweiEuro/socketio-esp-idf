#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <sio_types.h>

#define EIO_VERSION CONFIG_EIO_VERSION
#define SIO_HTTP_RECV_BUFFER CONFIG_SIO_HTTP_RECV_BUFFER
#define SIO_DEFAULT_ESSAGE_QUEUE_SIZE CONFIG_SIO_DEFAULT_MESSAGE_QUEUE_SIZE

#define SIO_DEFAULT_NAMESPACE "/"
#define SIO_DEFAULT_URL_PATH "/socket.io"
#define SIO_DEFAULT_MAX_CONN_RETRIES 3
#define SIO_DEFAULT_RETRY_INTERVAL_MS 3000u

#define SIO_TRANSPORT_POLLING_STRING "polling"
#define SIO_TRANSPORT_POLLING_PROTO_STRING "http"

#define SIO_TRANSPORT_WEBSOCKETS_STRING "websockets"
#define SIO_TRANSPORT_WEBSOCKETS_PROTO_STRING "ws"

#define MAX_HTTP_RECV_BUFFER 512
#define SIO_SID_SIZE 20
#define SIO_TOKEN_SIZE 7
#define ASCII_RS ""
#define ASCII_RS_INDEX = 30

    typedef void (*sio_on_event_fptr_t)(const uint8_t *event_name, const uint8_t *message);

    typedef struct
    {
        uint32_t client_id;  /* internal id */
        uint8_t eio_version; /* EngineIO protocol version */

        uint16_t max_connect_retries; /* Maximum connection retry attempts, 0 = infinite */

        uint16_t retry_interval_ms; /* Pause between retry attempts */

        uint16_t server_ping_interval_ms; /* Server-configured ping interval */
        uint16_t server_ping_timeout_ms;  /* Server-configured ping wait-timeout */

        sio_transport_t transport; /* Preferred SocketIO transport */
        char *server_address;      /* SocketIO server address with port */

        char *url_path; /* SocketIO URL path */

        char *token;             /* Random token for cache prevention */
        char *server_session_id; /* SocketIO session ID */
        char *nspc;              /* SocketIO namespace */

        sio_on_event_fptr_t *on_event; /* Function-pointer to user-defined function */
        size_t on_event_size;
    } sio_client_t;

    typedef struct
    {
        uint32_t client_id;
        sio_client_t *client;

    } sio_client_map_t;

    // Init with default values
    void sio_client_init(sio_client_t *client, const char *server_addr, const char *nspc);
    void sio_client_destroy(sio_client_t *client);

    bool sio_client_is_inited(const sio_client_t *client);

#ifdef __cplusplus
}
#endif