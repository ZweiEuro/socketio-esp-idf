#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <string.h>

#include "esp_types.h"
#include "esp_log.h"
#include "esp_err.h"

    char *alloc_random_string(const size_t length);
    void freeIfNotNull(void **ptr);

    // undef

    char *util_str_cat(char *destination, char *source);

    esp_err_t util_substr(
        char *substr,
        char *source,
        size_t *source_len,
        int start,
        int end);

    char *util_extract_json(char *pcBuffer);

#ifdef __cplusplus
}
#endif