

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
    client->client_lock = xSemaphoreCreateBinary();

    assert(client->client_lock != NULL && "Could not create client lock");
    xSemaphoreGive(client->client_lock);

    client->eio_version = config->eio_version == 0 ? SIO_DEFAULT_EIO_VERSION : config->eio_version;

    client->server_address = strdup(config->server_address);
    client->sio_url_path = strdup(config->sio_url_path == NULL ? SIO_DEFAULT_SIO_URL_PATH : config->sio_url_path);
    client->nspc = strdup(config->nspc == NULL ? SIO_DEFAULT_SIO_NAMESPACE : config->nspc);
    client->transport = config->transport;

    client->max_connect_retries = config->max_connect_retries;
    client->retry_interval_ms = config->retry_interval_ms;

    client->server_ping_interval_ms = 0;
    client->server_ping_timeout_ms = 0;

    client->server_session_id = NULL;
    client->handshake_client = NULL;
    client->alloc_auth_body_cb = config->alloc_auth_body_cb;

    // all of the clients need to be null

    client->polling_client = NULL;
    client->posting_client = NULL;
    client->handshake_client = NULL;

    client->polling_client_running = false;

    return (sio_client_id_t)slot;
}

void sio_client_destroy(sio_client_id_t clientId)
{
    if (!sio_client_is_inited(clientId))
    {
        return;
    }

    sio_client_t *client = sio_client_map[clientId];

    if (client->polling_client_running)
    {
        ESP_LOGE(TAG, "Polling client is running, stop it first");
        return;
    }

    freeIfNotNull(client->server_address);
    freeIfNotNull(client->sio_url_path);
    freeIfNotNull(client->nspc);

    // could be allocated
    freeIfNotNull(client->server_session_id);

    // Remove the semaphore, cleanup all handlers
    vSemaphoreDelete(client->client_lock);
    if (client->polling_client != NULL)
    {
        ESP_ERROR_CHECK(esp_http_client_cleanup(client->polling_client));
    }
    if (client->posting_client != NULL)
    {
        ESP_ERROR_CHECK(esp_http_client_cleanup(client->posting_client));
    }
    if (client->handshake_client != NULL)
    {
        ESP_ERROR_CHECK(esp_http_client_cleanup(client->handshake_client));
    }

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

sio_client_t *sio_client_get_and_lock(const sio_client_id_t clientId)
{
    if (!sio_client_is_inited(clientId))
    {
        ESP_LOGW(TAG, "Client %d is not inited", clientId);
        return NULL;
    }
    else
    {
        lockClient(sio_client_map[clientId]);
        return sio_client_map[clientId];
    }
}

void unlockClient(sio_client_t *client)
{
    ESP_LOGW(TAG, "Unlocking client %d", client->client_id);
    xSemaphoreGive(client->client_lock);
}

void lockClient(sio_client_t *client)
{
    ESP_LOGW(TAG, "Locking client %d", client->client_id);
    xSemaphoreTake(client->client_lock, portMAX_DELAY);
}

bool sio_client_is_locked(const sio_client_id_t clientId)
{

    if (!sio_client_is_inited(clientId))
    {
        return NULL;
    }
    else
    {
        if (xSemaphoreTake(sio_client_map[clientId]->client_lock, (TickType_t)0) == pdTRUE)
        {
            unlockClient(sio_client_map[clientId]);
            return false;
        }
        else
        {
            return true;
        }
    }
}