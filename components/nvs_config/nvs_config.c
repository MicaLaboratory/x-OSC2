#include "nvs_config.h"
#include "nvs_flash.h"
#include "nvs.h" 
#include "esp_log.h"

// NVS 
void nvs_save_int(const char *namespace_name, const char *key, int32_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error opening NVS: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_i32(handle, key, value);
    if (err == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI("NVS", "Saved %s = %ld", key, value);
    } else {
        ESP_LOGE("NVS", "Failed to save %s: %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
}

int32_t nvs_load_int(const char *namespace_name, const char *key, int32_t default_value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW("NVS", "Namespace not found, using default");
        return default_value;
    }

    int32_t value = default_value;
    err = nvs_get_i32(handle, key, &value);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "Key %s not found, using default", key);
    } else if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error reading %s: %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return value;
}

void print_all_nvs_entries(const char *namespace) {
    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find("nvs", namespace, NVS_TYPE_ANY, &it);

    while (err == ESP_OK && it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        printf("Key: %s, Type: %d\n", info.key, info.type);

        // If you want to read the value:
        nvs_handle_t handle;
        if (nvs_open(namespace, NVS_READONLY, &handle) == ESP_OK) {
            if (info.type == NVS_TYPE_STR) {
                size_t len;
                nvs_get_str(handle, info.key, NULL, &len);
                char *value = malloc(len);
                if (value) {
                    nvs_get_str(handle, info.key, value, &len);
                    printf("  Value: %s\n", value);
                    free(value);
                }
            } else if (info.type == NVS_TYPE_I32) {
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