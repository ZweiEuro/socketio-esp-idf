
#pragma once

#include <sio_types.h>
#include <sio_client.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(SIO_EVENT);

// Init with default values
void sio_client_init(sio_client_t *client, const char *server_addr, const char *nspc);
void sio_client_destroy(sio_client_t *client);

// Polling Stuff

// Websocket Stuff