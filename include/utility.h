#pragma once

#include <string.h>

#include "esp_types.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *SIO_UTIL_TAG = "[sio:util]";

char *alloc_random_string(const size_t length);
void freeIfNotNull(void *ptr);

char *util_str_cat(char *destination, char *source);

esp_err_t util_substr(
    char *substr,
    char *source,
    size_t *source_len,
    int start,
    int end);

void freeIfNotNull(void *ptr);
char *util_extract_json(char *pcBuffer);
