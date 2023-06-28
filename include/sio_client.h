#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <sio_types.h>

#define SIO_DEFAULT_EIO_VERSION CONFIG_SIO_DEFAULT_EIO_VERSION
#define SIO_DEFAULT_SIO_URL_PATH CONFIG_SIO_DEFAULT_SIO_URL_PATH

#define SIO_MAX_PARALLEL_SOCKETS CONFIG_SIO_MAX_PARALLEL_SOCKETS
#define SIO_DEFAULT_SIO_NAMESPACE CONFIG_SIO_DEFAULT_SIO_NAMESPACE

#define SIO_HTTP_RECV_BUFFER CONFIG_SIO_HTTP_RECV_BUFFER
#define SIO_DEFAULT_ESSAGE_QUEUE_SIZE CONFIG_SIO_DEFAULT_MESSAGE_QUEUE_SIZE

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
        uint8_t eio_version;          /* if 0 uses CONFIG_EIO_VERSION */
        uint16_t max_connect_retries; /* Maximum connection retry attempts, 0 = infinite */
        uint16_t retry_interval_ms;   /* Pause between retry attempts */
        sio_transport_t transport;    /* Preferred SocketIO transport */
        const char *server_address;   /* SocketIO server address with port (Excluding namespace)*/
        const char *sio_url_path;     /* SocketIO URL path, usually "/socket.io" */
        const char *nspc;             /* SocketIO namespace */

    } sio_client_config_t;

    typedef struct
    {
        sio_client_id_t client_id;
        uint8_t eio_version;

        const char *server_address;
        const char *sio_url_path;
        const char *nspc;
        sio_transport_t transport;

        uint16_t max_connect_retries;
        uint16_t retry_interval_ms;
        // after init

        // info gotten from the server
        uint16_t server_ping_interval_ms; /* Server-configured ping interval */
        uint16_t server_ping_timeout_ms;  /* Server-configured ping wait-timeout */

        char *server_session_id; /* SocketIO session ID */

    } sio_client_t;

    // Init with default values
    sio_client_id_t sio_client_init(const sio_client_config_t *config);
    void sio_client_destroy(const sio_client_id_t clientId);

    bool sio_client_is_inited(const sio_client_id_t clientId);

    // Risky
    sio_client_t *sio_client_get(const sio_client_id_t clientId);

#ifdef __cplusplus
}
#endif