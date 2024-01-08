
#pragma once

#include <sio_client.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t sio_connect(sio_client_t *client);

#ifdef __cplusplus
}
#endif