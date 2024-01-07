#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_http_client.h"

#define ASCII_RS ''
#define ASCII_RS_STRING ""
#define ASCII_RS_INDEX = 30

    esp_err_t http_client_polling_get_handler(esp_http_client_event_t *evt);

    esp_err_t http_client_polling_post_handler(esp_http_client_event_t *evt);

#ifdef __cplusplus
}
#endif