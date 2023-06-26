
#pragma once

#include <sio_types.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"

#define EIO_VERSION CONFIG_EIO_VERSION
#define SIO_HTTP_RECV_BUFFER CONFIG_SIO_HTTP_RECV_BUFFER
#define SIO_DEFAULT_ESSAGE_QUEUE_SIZE CONFIG_SIO_DEFAULT_MESSAGE_QUEUE_SIZE

#define SIO_DEFAULT_NAMESPACE "/"
#define SIO_DEFAULT_URL_PATH "/socket.io"
#define SIO_DEFAULT_MAX_CONN_RETRIES 3
#define SIO_DEFAULT_RETRY_INTERVAL_MS 3000u

#define SIO_MAX_EVENT_BODY 512
#define SIO_MAX_URL_LENGTH 512

#define SIO_TRANSPORT_POLLING_STRING "polling"
#define SIO_TRANSPORT_POLLING_PROTO_STRING "http"

#define SIO_TRANSPORT_WEBSOCKETS_STRING "websockets"
#define SIO_TRANSPORT_WEBSOCKETS_PROTO_STRING "ws"

#define MAX_HTTP_RECV_BUFFER 512
#define SIO_SID_SIZE 20
#define SIO_TOKEN_SIZE 7
#define ASCII_RS ""
#define ASCII_RS_INDEX = 30

ESP_EVENT_DECLARE_BASE(SIO_EVENT);

// Init with default values
void sio_client_init(sio_client_t *client, const uint8_t *server_addr, const uint8_t *nspc);

// Polling Stuff

// Websocket Stuff