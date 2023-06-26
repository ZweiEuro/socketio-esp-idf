

#include <sio_client.h>
#include <utility.h>
#include <string.h>

#define EIO_VERSION CONFIG_EIO_VERSION
#define SIO_HTTP_RECV_BUFFER CONFIG_SIO_HTTP_RECV_BUFFER
#define SIO_DEFAULT_ESSAGE_QUEUE_SIZE CONFIG_SIO_DEFAULT_MESSAGE_QUEUE_SIZE

#define SIO_DEFAULT_NAMESPACE "/"
#define SIO_DEFAULT_URL_PATH "/socket.io"
#define SIO_DEFAULT_MAX_CONN_RETRIES 3
#define SIO_DEFAULT_RETRY_INTERVAL_MS 3000u

#define SIO_MAX_EVENT_BODY 512
#define SIO_MAX_URL_LENGTH 512

#define SIO_TRANSPORT_POLLING_STRING "polling"
#define SIO_TRANSPORT_POLLING_PROTO_STRING "http"

#define SIO_TRANSPORT_WEBSOCKETS_STRING "websockets"
#define SIO_TRANSPORT_WEBSOCKETS_PROTO_STRING "ws"

#define MAX_HTTP_RECV_BUFFER 512
#define SIO_SID_SIZE 20
#define SIO_TOKEN_SIZE 7
#define ASCII_RS ""
#define ASCII_RS_INDEX = 30

uint32_t id_generator_ = 0;

void sio_client_init(sio_client_t *client, const char *server_addr, const char *nspc)
{
    client->client_id = id_generator_++;
    client->eio_version = EIO_VERSION;
    client->max_connect_retries = SIO_DEFAULT_MAX_CONN_RETRIES;
    client->retry_interval_ms = SIO_DEFAULT_RETRY_INTERVAL_MS;
    client->server_ping_interval_ms = 0;
    client->server_ping_timeout_ms = 0;
    client->transport = SIO_TRANSPORT_POLLING;

    strcpy(client->server_address, server_addr);

    client->token = alloc_random_string(SIO_TOKEN_SIZE);
    client->server_session_id = NULL;

    strcpy(client->nspc, nspc == NULL ? SIO_DEFAULT_NAMESPACE : nspc);

    client->on_event = NULL;
    client->on_event_size = 0;
}

void sio_client_destroy(sio_client_t *client)
{
    freeIfNotNull(client->server_address);
    freeIfNotNull(client->token);
    freeIfNotNull(client->server_session_id);
    freeIfNotNull(client->nspc);
    freeIfNotNull(client->on_event);
}
