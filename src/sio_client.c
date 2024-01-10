

#include <sio_client.h>
#include <utility.h>
#include <string.h>

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
    client->last_sent_pong = 0;

    client->_server_session_id = NULL;
    client->handshake_client = NULL;
    client->alloc_auth_body_cb = config->alloc_auth_body_cb;

    // all of the clients need to be null

    client->polling_client = NULL;
    client->posting_client = NULL;
    client->handshake_client = NULL;

    sio_client_map[slot] = client;

    xSemaphoreGive(client->client_lock);

    ESP_LOGD(TAG, "inited client %d @ %p", slot, client);

    return (sio_client_id_t)slot;
}

esp_err_t sio_client_close(sio_client_id_t clientId)
{
    sio_client_t *client = sio_client_get_and_lock(clientId);

    switch (client->status)
    {
    case SIO_CLIENT_INITED:
    case SIO_CLIENT_STATUS_CLOSED:
    case SIO_CLIENT_CLOSING:
        ESP_LOGI(TAG, "Client %d in state %d closing not necessary, or in progress",
                 clientId, client->status);
        break;

    case SIO_CLIENT_STATUS_HANDSHAKING:
    case SIO_CLIENT_STATUS_CONNECTED:
        // handshaking is controlled by a flag, so we set to closed and wait
        // for the sio_handshake to finish or fail
        // and now send the close packet to the server since we were connected
        client->status = SIO_CLIENT_CLOSING;
        unlockClient(client);

        // send close packet, this may fail if the sio_handshake failed as well but that is ok
        Packet_t *p = (Packet_t *)calloc(1, sizeof(Packet_t));
        p->data = calloc(1, 2);
        p->len = 2;
        setEioType(p, EIO_PACKET_CLOSE);

        sio_send_packet(clientId, p);

        client = sio_client_get_and_lock(clientId);

        break;

    default:
        break;
    }

    client->status = SIO_CLIENT_STATUS_CLOSED;

    ESP_LOGI(TAG, "Client %d in state %d closed",
             clientId, client->status);

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

    if (client->status != SIO_CLIENT_STATUS_CLOSED)
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

void sio_client_print_status(const sio_client_id_t clientId)
{
    if (!sio_client_exists(clientId))
    {
        ESP_LOGE(TAG, "Client %d does not exist", clientId);
        return;
    }

    sio_client_t *client = sio_client_get_and_lock(clientId);

    ESP_LOGI(TAG, "Client %d status: %d, last sent pong: %s",
             clientId, client->status, asctime(localtime(&client->last_sent_pong)));

    unlockClient(client);
}
/// ---- runtime Locking

bool sio_client_exists(const sio_client_id_t clientId)
{

    if (sio_client_map == NULL)
    {
        // nothing exists
        return false;
    }

    if (clientId >= SIO_MAX_PARALLEL_SOCKETS || clientId < 0)
    {
        return false;
    }

    return sio_client_map[clientId] != NULL;
}

void unlockClient(sio_client_t *client)
{
    ESP_LOGD(TAG, "Unlocking client %d", client->client_id);
    xSemaphoreGive(client->client_lock);
}

void lockClient(sio_client_t *client)
{
    ESP_LOGD(TAG, "Locking client %p", client);
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
            unlockClient(sio_client_map[clientId]);
            return false;
        }
        else
        {
            return true;
        }
    }
}