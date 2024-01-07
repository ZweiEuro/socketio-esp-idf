
#include "utility.h"
#include <esp_assert.h>

static const char *TAG = "[sio:util]";
static const char token_charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

void freeIfNotNull(void **ptr)
{
    if ((*ptr) != NULL)
    {
        free((*ptr));
        (*ptr) = NULL;
    }
}

// allocate new random token string on heap
char *alloc_random_string(const size_t length)
{
    char *randomString = (char *)malloc(length + 1);

    if (randomString != NULL)
    {
        for (int n = 0; n < length; n++)
        {
            randomString[n] = token_charset[rand() % sizeof(token_charset) - 1];
        }

        randomString[length] = '\0';
    }
    else
    {
        assert(false && "Out of memory");
    }

    return randomString;
}

// util

char *alloc_handshake_get_url(const sio_client_t *client)
{

    char *token = alloc_random_string(SIO_TOKEN_SIZE);
    size_t url_length =
        strlen(client->transport == SIO_TRANSPORT_POLLING ? SIO_TRANSPORT_POLLING_PROTO_STRING : SIO_TRANSPORT_WEBSOCKETS_PROTO_STRING) +
        strlen("://") +
        strlen(client->server_address) +
        strlen(client->sio_url_path) +
        strlen("/?EIO=X&transport=") +
        strlen(SIO_TRANSPORT_POLLING_STRING) +
        strlen("&t=") + strlen(token);

    char *url = (char *)calloc(1, url_length + 1);
    if (url == NULL)
    {
        assert(false && "Failed to allocate memory for handshake url");
        return NULL;
    }

    sprintf(
        url,
        "%s://%s%s/?EIO=%d&transport=%s&t=%s",
        SIO_TRANSPORT_POLLING_PROTO_STRING,
        client->server_address,
        client->sio_url_path,
        client->eio_version,
        SIO_TRANSPORT_POLLING_STRING,
        token);

    freeIfNotNull(&token);
    return url;
}

char *alloc_post_url(const sio_client_t *client)
{
    if (client == NULL || client->_server_session_id == NULL)
    {
        ESP_LOGE(TAG, "Server session id not set, was this client initialized? Client: %p", client);
        return NULL;
    }

    char *token = alloc_random_string(SIO_TOKEN_SIZE);
    size_t url_length =
        strlen(client->transport == SIO_TRANSPORT_POLLING ? SIO_TRANSPORT_POLLING_PROTO_STRING : SIO_TRANSPORT_WEBSOCKETS_PROTO_STRING) +
        strlen("://") +
        strlen(client->server_address) +
        strlen(client->sio_url_path) +
        strlen("/?EIO=X&transport=") +
        strlen(SIO_TRANSPORT_POLLING_STRING) +
        strlen("&t=") + strlen(token) +
        strlen("&sid=") + strlen(client->_server_session_id);

    char *url = (char *)calloc(1, url_length + 1);

    if (url == NULL)
    {
        assert(false && "Failed to allocate memory for handshake url");
        return NULL;
    }

    sprintf(
        url,
        "%s://%s%s/?EIO=%d&transport=%s&t=%s&sid=%s",
        SIO_TRANSPORT_POLLING_PROTO_STRING,
        client->server_address,
        client->sio_url_path,
        client->eio_version,
        SIO_TRANSPORT_POLLING_STRING,
        token,
        client->_server_session_id);

    freeIfNotNull(&token);
    return url;
}

char *alloc_polling_get_url(const sio_client_t *client)
{
    return alloc_post_url(client);
}