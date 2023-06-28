#include <sio_socketio.h>
#include <sio_types.h>
#include <internal/http_handlers.h>
#include <internal/sio_packet.h>
#include <utility.h>

#include <esp_log.h>
static const char *TAG = "[sio_socketio]";

esp_err_t handshake(sio_client_t *client);
esp_err_t handshake_polling(sio_client_t *client);
esp_err_t handshake_websocket(sio_client_t *client);

char *alloc_connect_url(const sio_client_t *client);

esp_err_t sio_client_begin(const sio_client_id_t clientId)
{

    sio_client_t *client = sio_client_get(clientId);

    esp_err_t handshake_result = ESP_FAIL;
    uint16_t current_attempt = 1;

    while (client->max_connect_retries == 0 || current_attempt < client->max_connect_retries)
    {
        ESP_LOGI(TAG, "Connecting to %s attempt %d", client->server_address, current_attempt);

        handshake_result = handshake(client);
        if (handshake_result == ESP_OK)
        {
            return ESP_OK;
        }
        ESP_LOGI(TAG, "Handshake failed, retrying in %d ms", client->retry_interval_ms);
        vTaskDelay(client->retry_interval_ms / portTICK_PERIOD_MS);
        current_attempt++;
    }

    ESP_LOGI(TAG, "Failed to connect to %s %s", client->server_address, esp_err_to_name(handshake_result));

    return handshake_result;
}

esp_err_t handshake(sio_client_t *client)
{

    if (client->transport == SIO_TRANSPORT_WEBSOCKETS)
    {
        return handshake_websocket(client);
    }
    else if (client->transport == SIO_TRANSPORT_POLLING)
    {
        return handshake_polling(client);
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t handshake_polling(sio_client_t *client)
{

    // Form the request URL
    char *url = alloc_connect_url(client);

    ESP_LOGD(TAG, "Handshake URL: >%s< len:%d", url, strlen(url));

    Packet_t *packet = NULL;

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_client_polling_handler,
        .user_data = &packet,
        .disable_auto_redirect = true};
    esp_http_client_handle_t http_client = esp_http_client_init(&config);

    if (http_client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        freeIfNotNull(url);
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(http_client);
    if (err != ESP_OK || packet == NULL)
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s, packet pointer %p ", esp_err_to_name(err), packet);

        esp_http_client_cleanup(http_client);
        freeIfNotNull(url);
        return err;
    }

    int http_response_status_code = esp_http_client_get_status_code(http_client);
    int http_response_content_length = esp_http_client_get_content_length(http_client);
    ESP_LOGI(
        TAG, "HTTP GET Status = %d, content_length = %d",
        http_response_status_code,
        http_response_content_length);

    ESP_LOGI(TAG, "Packet: %d %d len %d \n%s\n%s",

             packet->eio_type, packet->sio_type, packet->len, packet->data, packet->json_start

    );

    esp_http_client_cleanup(http_client);
    freeIfNotNull(url);

    return ESP_OK;
}

esp_err_t handshake_websocket(sio_client_t *client)
{
    assert(false && "Not implemented");
}

// util

char *alloc_connect_url(const sio_client_t *client)
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

    // Form the request URL
    char *url = calloc(1, url_length + 1);
    sprintf(
        url,
        "%s://%s%s/?EIO=%d&transport=%s&t=%s",
        SIO_TRANSPORT_POLLING_PROTO_STRING,
        client->server_address,
        client->sio_url_path,
        client->eio_version,
        SIO_TRANSPORT_POLLING_STRING,
        token);

    freeIfNotNull(token);
    return url;
}