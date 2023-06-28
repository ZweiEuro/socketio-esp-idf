

#include <sio_client.h>
#include <utility.h>
#include <string.h>

static const char *TAG = "[sio_client]";

sio_client_t **sio_client_map = (sio_client_t **)NULL;

sio_client_id_t sio_client_init(const sio_client_config_t *config)
{

    if (sio_client_map == NULL)
    {
        sio_client_map = calloc(SIO_MAX_PARALLEL_SOCKETS, sizeof(sio_client_t *));
    }

    // some basic error checks
    if (config->server_address == NULL)
    {
        ESP_LOGE(TAG, "No server address provided");
        return -1;
    }

    // get open slot
    uint8_t slot = SIO_MAX_PARALLEL_SOCKETS;
    for (uint8_t i = 0; i < SIO_MAX_PARALLEL_SOCKETS; i++)
    {
        if (sio_client_map[i] == NULL)
        {
            slot = i;
            break;
        }
    }

    // check if none was free
    if (slot == SIO_MAX_PARALLEL_SOCKETS)
    {
        ESP_LOGE(TAG, "No slot available, clear a socket or increase the SIO_MAX_PARALLEL_SOCKETS define");
        return 0;
    }

    // copy from config everyting over
    sio_client_map[slot] = calloc(1, sizeof(sio_client_t));

    sio_client_t *client = sio_client_map[slot];

    client->client_id = slot;
    client->eio_version = config->eio_version == NULL ? SIO_DEFAULT_EIO_VERSION : config->eio_version;

    client->server_address = strdup(config->server_address);
    client->sio_url_path = strdup(config->sio_url_path == NULL ? SIO_DEFAULT_SIO_URL_PATH : config->sio_url_path);
    client->nspc = strdup(config->nspc == NULL ? SIO_DEFAULT_SIO_NAMESPACE : config->nspc);
    client->transport = config->transport;

    client->max_connect_retries = config->max_connect_retries;
    client->retry_interval_ms = config->retry_interval_ms;

    client->server_ping_interval_ms = 0;
    client->server_ping_timeout_ms = 0;

    client->server_session_id = NULL;

    return (sio_client_id_t)slot;
}

void sio_client_destroy(const sio_client_id_t clientId)
{
    if (!sio_client_is_inited(clientId))
    {
        return;
    }

    sio_client_t *client = sio_client_map[clientId];

    freeIfNotNull(client->server_address);
    freeIfNotNull(client->sio_url_path);
    freeIfNotNull(client->nspc);

    // could be allocated
    freeIfNotNull(client->server_session_id);
    freeIfNotNull(client);

    sio_client_map[clientId] = NULL;
    // if all of them are freed then free the map

    bool allFreed = true;
    for (uint8_t i = 0; i < SIO_MAX_PARALLEL_SOCKETS; i++)
    {
        if (sio_client_map[i] != NULL)
        {
            allFreed = false;
            break;
        }
    }

    if (allFreed)
    {
        freeIfNotNull(sio_client_map);
        sio_client_map = NULL;
    }
}

bool sio_client_is_inited(const sio_client_id_t clientId)
{
    return sio_client_map[clientId] != NULL;
}

sio_client_t *sio_client_get(const sio_client_id_t clientId)
{
    return sio_client_map[clientId];
}