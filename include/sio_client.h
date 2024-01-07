#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <sio_types.h>
#include <internal/http_polling_handlers.h>
#include <internal/sio_packet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_event.h"

#define SIO_DEFAULT_EIO_VERSION CONFIG_SIO_DEFAULT_EIO_VERSION
#define SIO_DEFAULT_SIO_URL_PATH CONFIG_SIO_DEFAULT_SIO_URL_PATH

#define SIO_MAX_PARALLEL_SOCKETS CONFIG_SIO_MAX_PARALLEL_SOCKETS
#define SIO_DEFAULT_SIO_NAMESPACE CONFIG_SIO_DEFAULT_SIO_NAMESPACE

#define SIO_TRANSPORT_POLLING_STRING "polling"
#define SIO_TRANSPORT_POLLING_PROTO_STRING "http"

#define SIO_TRANSPORT_WEBSOCKETS_STRING "websockets"
#define SIO_TRANSPORT_WEBSOCKETS_PROTO_STRING "ws"

#define SIO_SID_SIZE 20
#define SIO_TOKEN_SIZE 7

#define MAX_HTTP_RECV_BUFFER 512

    typedef struct sio_client_t sio_client_t;

    typedef const char *(*sio_auth_body_fptr_t)(const struct sio_client_t *client);

    typedef struct
    {
        uint8_t eio_version;        /* if 0 uses CONFIG_EIO_VERSION */
        sio_transport_t transport;  /* Preferred SocketIO transport */
        const char *server_address; /* SocketIO server address with port (Excluding namespace)*/
        const char *sio_url_path;   /* SocketIO URL path, usually "/socket.io" */
        const char *nspc;           /* SocketIO namespace */

        sio_auth_body_fptr_t alloc_auth_body_cb; /* Callback to generate auth body, will be free'd after use */

    } sio_client_config_t;

    struct sio_client_t
    {
        sio_client_id_t client_id;
        SemaphoreHandle_t client_lock;

        sio_client_status_t status;

        uint8_t eio_version;

        char *server_address;
        char *sio_url_path;
        char *nspc;
        sio_transport_t transport;

        sio_auth_body_fptr_t alloc_auth_body_cb;

        // after init

        // info gotten from the server
        uint16_t server_ping_interval_ms; /* Server-configured ping interval */
        uint16_t server_ping_timeout_ms;  /* Server-configured ping wait-timeout */

        char *_server_session_id; /* SocketIO session ID */

        // used internally
        esp_http_client_handle_t handshake_client; /* Used to establish first connection*/
        esp_http_client_handle_t polling_client;   /* Used for continuous polling */
        esp_http_client_handle_t posting_client;   /* Used for posting messages */
    };

    ESP_EVENT_DECLARE_BASE(SIO_EVENT);

    esp_err_t sio_client_begin(const sio_client_id_t clientId);

    void unlockClient(sio_client_t *client);
    void lockClient(sio_client_t *client);

    // Init with default values
    sio_client_id_t sio_client_init(const sio_client_config_t *config);
    void sio_client_destroy(sio_client_id_t clientId);

    bool sio_client_is_connected(sio_client_id_t clientId);
    esp_err_t sio_client_close(const sio_client_id_t clientId);

    esp_err_t sio_send_packet(const sio_client_id_t clientId, const Packet_t *packet);
    esp_err_t sio_send_string(const sio_client_id_t clientId, const char *data);

    // locks the semaphore, get it first before doing
    // any writing else it will most certainly produce race conditions
    sio_client_t *sio_client_get_and_lock(const sio_client_id_t clientId);

    bool sio_client_is_locked(const sio_client_id_t clientId);

    char *alloc_polling_get_url(const sio_client_t *client);

    // Events:

    // Event struct
    typedef struct
    {
        sio_client_id_t client_id;
        PacketPointerArray_t packets_pointer;
        int len;
    } sio_event_data_t;

#ifdef __cplusplus
}
#endif