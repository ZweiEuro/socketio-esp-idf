#include <sio_socketio.h>
#include <sio_types.h>
#include <internal/http_handlers.h>
#include <utility.h>

#include <esp_log.h>
static const char *TAG = "[sio_socketio]";

esp_err_t handshake(const sio_client_t *client);
esp_err_t handshake_polling(const sio_client_t *client);
esp_err_t handshake_websocket(const sio_client_t *client);

esp_err_t sio_client_begin(const sio_client_t *client)
{

    esp_err_t handshake_result = ESP_FAIL;
    uint16_t current_attempt = 1;

    while (current_attempt < client->max_connect_retries)
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

esp_err_t handshake(const sio_client_t *client)
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

esp_err_t handshake_polling(const sio_client_t *client)
{

    http_response_t response_data = {
        .data = NULL,
        .len = 0};

    size_t url_length =
        strlen(SIO_TRANSPORT_POLLING_PROTO_STRING) +
        strlen("://") +
        strlen(client->server_address) +
        strlen(client->url_path) +
        strlen("/?EIO=X&transport=") +
        strlen(SIO_TRANSPORT_POLLING_STRING) +
        strlen("&t=") + strlen(client->token);

    // Form the request URL
    char *url = calloc(1, url_length + 1);
    sprintf(
        url,
        "%s://%s%s/?EIO=%d&transport=%s&t=%s",
        SIO_TRANSPORT_POLLING_PROTO_STRING,
        client->server_address,
        client->url_path,
        EIO_VERSION,
        SIO_TRANSPORT_POLLING_STRING,
        client->token);

    ESP_LOGD(TAG, "Handshake URL: >%s< len:%d", url, url_length);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_client_polling_handler,
        .user_data = &response_data,
        .disable_auto_redirect = true};
    esp_http_client_handle_t http_client = esp_http_client_init(&config);

    if (http_client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        freeIfNotNull(url);
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(http_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));

        esp_http_client_cleanup(http_client);
        freeIfNotNull(url);
        return err;
    }

    int http_response_status_code = esp_http_client_get_status_code(http_client);
    int http_response_content_length = esp_http_client_get_content_length(http_client);
    ESP_LOGI(
        TAG, "HTTP GET Status = %d, content_length = %d, %s %d",
        http_response_status_code,
        http_response_content_length,
        response_data.data,
        response_data.len);

    esp_http_client_cleanup(http_client);
    freeIfNotNull(url);

    return ESP_OK;
}

esp_err_t handshake_websocket(const sio_client_t *client)
{
    assert(false && "Not implemented");
}