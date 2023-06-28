
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <sio_client.h>
#include "esp_event.h"

    ESP_EVENT_DECLARE_BASE(SIO_EVENT);

    esp_err_t sio_client_begin(const sio_client_id_t clientId);

#ifdef __cplusplus
}
#endif