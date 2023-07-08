
#include "utility.h"
#include <esp_assert.h>

static const char *TAG = "[sio:util]";
static const char token_charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

void freeIfNotNull(void **ptr)
{
    if ((*ptr) != NULL)
    {
        free((*ptr));
        (*ptr) = NULL;
    }
}

// allocate new random token string on heap
char *alloc_random_string(const size_t length)
{
    char *randomString = (char *)malloc(length + 1);

    if (randomString != NULL)
    {
        for (int n = 0; n < length; n++)
        {
            randomString[n] = token_charset[rand() % sizeof(token_charset) - 1];
        }

        randomString[length] = '\0';
    }
    else
    {
        assert(false && "Out of memory");
    }

    return randomString;
}

#if false

char *util_str_cat(char *destination, char *source)
{
    while (*destination)
        destination++;
    while ((*destination++ = *source++))
        ;

    return --destination;
}

esp_err_t util_substr(
    char *substr,
    char *source,
    size_t *source_len,
    int start,
    int end)
{
    ESP_LOGW(SIO_UTIL_TAG, "source: %s", source);
    ESP_LOGW(SIO_UTIL_TAG, "source_len: %d", *source_len);

    int j = 0;
    for (int i = 0; i < *source_len; i = i + 1)
    {
        if ((i >= start) & (i <= end))
        {
            substr[j] = source[i];
            j = j + 1;
        }
    }

    // null terminate destination string
    // if (*source_len != end) {
    //     *substr = '\0';
    // }

    return ESP_OK;
}



char *util_extract_json(char *pc_buffer)
{
    // Empty?
    int32_t i_strlen = strlen(pc_buffer);
    if (i_strlen <= 0)
    {
        ESP_LOGW(SIO_UTIL_TAG, "util_extract_json(): EMPTY STRING!");
        return NULL;
    }

    // Trim left
    bool b_found_trim_pos = false;
    for (int32_t i = 0; i < i_strlen; i++)
    {
        if (pc_buffer[i] == '[' || pc_buffer[i] == '{')
        {
            b_found_trim_pos = true;
            pc_buffer = &pc_buffer[i];
            break;
        }
    }
    if (!b_found_trim_pos)
    {
        ESP_LOGW(SIO_UTIL_TAG, "util_extract_json(): TRIM LEFT FAIL!");
        return NULL;
    }
    i_strlen = strlen(pc_buffer);

    // Trim right
    b_found_trim_pos = false;
    for (int32_t i = i_strlen - 1; i >= 0; i--)
    {
        if (pc_buffer[i] == ']' || pc_buffer[i] == '}')
        {
            b_found_trim_pos = true;
            pc_buffer[i + 1] = (char)NULL; // Terminate
            break;
        }
    }
    if (!b_found_trim_pos)
    {
        ESP_LOGW(SIO_UTIL_TAG, "util_extract_json(): TRIM RIGHT FAIL!");
        return NULL;
    }
    i_strlen = strlen(pc_buffer);

    return pc_buffer;
}

#endif