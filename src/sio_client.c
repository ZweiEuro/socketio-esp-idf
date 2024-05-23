

#include <sio_client.h>
#include <utility.h>
#include <string.h>

#include <esp_debug_helpers.h>

static const char *TAG = "[sio_client]";

sio_client_t **sio_client_map = (sio_client_t **)NULL;

bool sio_client_exists(const sio_client_id_t clientId);

sio_client_id_t sio_client_init(const sio_client_config_t *config)
{

    if (sio_client_map == NULL)
    {
        sio_client_map = (sio_client_t **)calloc(SIO_MAX_PARALLEL_SOCKETS, sizeof(sio_client_t *));
        // set all pointers to null
        for (uint8_t i = 0; i < SIO_MAX_PARALLEL_SOCKETS; i++)
        {
            sio_client_map[i] = NULL;
        }
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
        return -1;
    }

    // copy from config everyting over

    sio_client_t *client = (sio_client_t *)calloc(1, sizeof(sio_client_t));

    client->client_id = slot;
    client->client_lock = xSemaphoreCreateBinary();

    client->status = SIO_CLIENT_INITED;

    assert(client->client_lock != NULL && "Could not create client lock");

    client->eio_version = config->eio_version == 0 ? SIO_DEFAULT_EIO_VERSION : config->eio_version;

    client->server_address = strdup(config->server_address);
    client->sio_url_path = strdup(config->sio_url_path == NULL ? SIO_DEFAULT_SIO_URL_PATH : config->sio_url_path);
    client->nspc = strdup(config->nspc == NULL ? SIO_DEFAULT_SIO_NAMESPACE : config->nspc);
    client->transport = config->transport;

    client->server_ping_interval_ms = 0;
    client->server_ping_timeout_ms = 0;

    client->_server_session_id = NULL;
    client->alloc_auth_body_cb = config->alloc_auth_body_cb;

    // all of the clients need to be null

    client->polling_client = NULL;

    sio_client_map[slot] = client;

    xSemaphoreGive(client->client_lock);

    ESP_LOGI(TAG, "inited client %d @ %p", slot, client);

    return (sio_client_id_t)slot;
}

/**
 * @brief
 *
 * @param clientId
 * @return esp_err_t
 */
esp_err_t sio_client_close(sio_client_id_t clientId)
{
    sio_client_t *client = sio_client_get_and_lock(clientId);

    if (client == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (client->status == SIO_CLIENT_CLOSED)
    {
        ESP_LOGI(TAG, "Client %d already closed", clientId);

        unlockClient(client);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Client %d in state %d closing anything that's up",
             clientId, client->status);

    if (client->polling_task_handle != NULL)
    {
        const eTaskState state = eTaskGetState(client->polling_task_handle);
        if (state != eDeleted && state != eInvalid)
        {
            // only delete it if it is still running, if it detected the error it might have closed itself
            ESP_LOGI(TAG, "Deleting polling task %p", client->polling_task_handle);
            vTaskDelete(client->polling_task_handle);
        }

        client->polling_task_handle = NULL;
    }

    if (client->polling_client != NULL)
    {
        esp_http_client_close(client->polling_client);
        esp_http_client_cleanup(client->polling_client);
        client->polling_client = NULL;
    }

    client->status = SIO_CLIENT_CLOSED;

    unlockClient(client);
    return ESP_OK;
}

void sio_client_destroy(sio_client_id_t clientId)
{
    if (!sio_client_exists(clientId))
    {
        return;
    }

    sio_client_t *client = sio_client_get_and_lock(clientId);

    if (client->status != SIO_CLIENT_CLOSED)
    {
        ESP_LOGW(TAG, "Closing client that is not  yet closed");
        unlockClient(client);
        sio_client_close(clientId);

        client = sio_client_get_and_lock(clientId);
    }

    freeIfNotNull(&client->server_address);
    freeIfNotNull(&client->sio_url_path);
    freeIfNotNull(&client->nspc);

    // could be allocated
    freeIfNotNull(&client->_server_session_id);

    // Remove the semaphore, cleanup all handlers
    vSemaphoreDelete(client->client_lock);

    free(client);
    client = NULL;
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
        free(sio_client_map);
        sio_client_map = NULL;
    }
}

/// ---- runtime Locking

bool sio_client_exists(const sio_client_id_t clientId)
{

    if (sio_client_map == NULL)
    {
        ESP_LOGW(TAG, "client does not exist");
        // nothing exists
        return false;
    }

    if (clientId >= SIO_MAX_PARALLEL_SOCKETS || clientId < 0)
    {
        return false;
    }

    return sio_client_map[clientId] != NULL;
}

#define DEBUG_LOCKING 0

void unlockClient(sio_client_t *client)
{
#if DEBUG_LOCKING

    ESP_LOGI(TAG, "Unlocking client %p", client);
    esp_backtrace_print(10);

#endif
    xSemaphoreGive(client->client_lock);
}

void lockClient(sio_client_t *client)
{

#if DEBUG_LOCKING
    if (sio_client_is_locked(client->client_id))
    {
        ESP_LOGW(TAG, "Client %p is already locked, waiting", client);
    }
    else
    {

        ESP_LOGI(TAG, "Locking client %p", client);
    }
    esp_backtrace_print(10);
#endif

    xSemaphoreTake(client->client_lock, portMAX_DELAY);
}

sio_client_t *sio_client_get_and_lock(const sio_client_id_t clientId)
{
    if (sio_client_exists(clientId))
    {
        lockClient(sio_client_map[clientId]);
        return sio_client_map[clientId];
    }
    else
    {
        return NULL;
    }
}

bool sio_client_is_locked(const sio_client_id_t clientId)
{

    if (!sio_client_exists(clientId))
    {
        return NULL;
    }
    else
    {
        if (xSemaphoreTake(sio_client_map[clientId]->client_lock, (TickType_t)0) == pdTRUE)
        {
            xSemaphoreGive(sio_client_map[clientId]->client_lock);
            return false;
        }
        else
        {
            return true;
        }
    }
}