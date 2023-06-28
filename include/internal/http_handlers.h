#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_http_client.h"

    typedef struct
    {
        char *data;
        size_t len;
    } http_response_t;

    esp_err_t http_client_polling_handler(esp_http_client_event_t *evt);

#ifdef __cplusplus
}
#endif