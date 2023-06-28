

#include <sio_client.h>
#include <utility.h>
#include <string.h>

uint32_t id_generator_ = 1;

void sio_client_init(sio_client_t *client, const char *server_addr, const char *nspc)
{
    client->client_id = id_generator_++;
    client->eio_version = EIO_VERSION;
    client->max_connect_retries = SIO_DEFAULT_MAX_CONN_RETRIES;
    client->retry_interval_ms = SIO_DEFAULT_RETRY_INTERVAL_MS;
    client->server_ping_interval_ms = 0;
    client->server_ping_timeout_ms = 0;
    client->transport = SIO_TRANSPORT_POLLING;

    client->server_address = strdup(server_addr);
    client->url_path = strdup(SIO_DEFAULT_URL_PATH);

    client->token = alloc_random_string(SIO_TOKEN_SIZE);
    client->server_session_id = NULL;

    client->nspc = strdup(nspc == NULL ? SIO_DEFAULT_NAMESPACE : nspc);

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

bool sio_client_is_inited(const sio_client_t *client)
{
    return client->token != NULL;
}