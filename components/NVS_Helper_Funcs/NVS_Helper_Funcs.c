#include <stdio.h>
#include <inttypes.h> // for PRId32
#include "NVS_Helper_Funcs.h"
#include "nvs.h"
#include "esp_log.h"

// Saving INT

esp_err_t nvs_save_value(const char *namespace_name, const char *key, nvs_value_type_t type, const void *value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    switch (type)
    {
    case NVS_TYPE_I32:
        err = nvs_set_i32(handle, key, *(int32_t *)value);
        break;

    case NVS_TYPE_U32:
        err = nvs_set_u32(handle, key, *(uint32_t *)value);
        break;

    case NVS_TYPE_I8:
        err = nvs_set_i8(handle, key, *(int8_t *)value);
        break;

    case NVS_TYPE_U8:
        err = nvs_set_u8(handle, key, *(uint8_t *)value);
        break;

    case NVS_TYPE_STR:
        err = nvs_set_str(handle, key, (const char *)value);
        break;

    case NVS_TYPE_BLOB:
        // You must pass a struct containing pointer + size
        // Example struct shown below
        {
            const nvs_blob_t *blob = (const nvs_blob_t *)value;
            err = nvs_set_blob(handle, key, blob->data, blob->size);
        }
        break;
        
    default:
        err = ESP_ERR_INVALID_ARG;
        break;
    }

    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

// Loading INT
int32_t nvs_load_int(const char *namespace_name, const char *key, int32_t default_value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);

    if (err != ESP_OK)
    {
        ESP_LOGW("NVS", "Namespace %s not found, using default", namespace_name);
        return default_value;
    }

    int32_t value = default_value;
    err = nvs_get_i32(handle, key, &value);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW("NVS", "Key %s not found, using default", key);
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE("NVS", "Error reading %s: %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return value;
}

// Loading STRING
char *nvs_load_str(const char *namespace_name, const char *key, const char *default_value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);

    if (err != ESP_OK)
    {
        ESP_LOGW("NVS", "Namespace %s not found, using default", namespace_name);
        return strdup(default_value);
    }

    size_t len = 0;
    err = nvs_get_str(handle, key, NULL, &len);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW("NVS", "Key %s not found, using default", key);
        nvs_close(handle);
        return strdup(default_value);
    }

    if (err != ESP_OK)
    {
        ESP_LOGE("NVS", "Error reading %s: %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return strdup(default_value);
    }

    char *value = malloc(len);
    if (!value)
    {
        ESP_LOGE("NVS", "Out of memory allocating %zu bytes", len);
        nvs_close(handle);
        return strdup(default_value);
    }

    err = nvs_get_str(handle, key, value, &len);
    if (err != ESP_OK)
    {
        ESP_LOGE("NVS", "Error reading %s: %s", key, esp_err_to_name(err));
        free(value);
        nvs_close(handle);
        return strdup(default_value);
    }

    nvs_close(handle);
    return value;
}

// Print all NVS entries
void print_all_nvs_entries(const char *namespace)
{
    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find("nvs", namespace, NVS_TYPE_ANY, &it);

    while (err == ESP_OK && it != NULL)
    {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        printf("Key: %s, Type: %d\n", info.key, info.type);

        nvs_handle_t handle;
        if (nvs_open(namespace, NVS_READONLY, &handle) == ESP_OK)
        {
            if (info.type == NVS_TYPE_STR)
            {
                size_t len;
                nvs_get_str(handle, info.key, NULL, &len);
                char *value = malloc(len);
                if (value)
                {
                    nvs_get_str(handle, info.key, value, &len);
                    printf("  Value: %s\n", value);
                    free(value);
                }
            }
            else if (info.type == NVS_TYPE_I32)
            {
                int32_t val;
                nvs_get_i32(handle, info.key, &val);
                printf("  Value: %" PRId32 "\n", val);
            }
            nvs_close(handle);
        }

        err = nvs_entry_next(&it);
    }

    nvs_release_iterator(it);
}
