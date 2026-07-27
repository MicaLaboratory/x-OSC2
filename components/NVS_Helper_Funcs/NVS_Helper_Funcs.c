#include <stdio.h>
#include <inttypes.h> // for PRId32
#include "NVS_Helper_Funcs.h"

#include "esp_log.h"

// #include "../../main/global_nvs.h"

#include "NVS_Helper_Funcs.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "esp_err.h"

static const FieldDesc g_fields[] = {
    {"net_settings.network_mode", "net_settings", "network_mode",
     offsetof(NVS_Global, net_settings.network_mode),
     sizeof(((NVS_Global *)0)->net_settings.network_mode),
     FIELD_ENUM},

    {"net_settings.ap_ssid", "net_settings", "ap_ssid",
     offsetof(NVS_Global, net_settings.ap_ssid),
     sizeof(((NVS_Global *)0)->net_settings.ap_ssid),
     FIELD_STR},

    {"net_settings.ap_password", "net_settings", "ap_password",
     offsetof(NVS_Global, net_settings.ap_password),
     sizeof(((NVS_Global *)0)->net_settings.ap_password),
     FIELD_STR},

    {"net_settings.sta_ssid", "net_settings", "sta_ssid",
     offsetof(NVS_Global, net_settings.sta_ssid),
     sizeof(((NVS_Global *)0)->net_settings.sta_ssid),
     FIELD_STR},

    {"net_settings.sta_password", "net_settings", "sta_password",
     offsetof(NVS_Global, net_settings.sta_password),
     sizeof(((NVS_Global *)0)->net_settings.sta_password),
     FIELD_STR},

    {"osc_settings.remote_ip", "osc_settings", "remote_ip",
     offsetof(NVS_Global, osc_settings.remote_ip),
     sizeof(((NVS_Global *)0)->osc_settings.remote_ip),
     FIELD_STR},

    {"osc_settings.remote_port", "osc_settings", "remote_port",
     offsetof(NVS_Global, osc_settings.remote_port),
     sizeof(((NVS_Global *)0)->osc_settings.remote_port),
     FIELD_U16},

    {"osc_settings.local_ip", "osc_settings", "local_ip",
     offsetof(NVS_Global, osc_settings.local_ip),
     sizeof(((NVS_Global *)0)->osc_settings.local_ip),
     FIELD_STR},

    {"osc_settings.local_port", "osc_settings", "local_port",
     offsetof(NVS_Global, osc_settings.local_port),
     sizeof(((NVS_Global *)0)->osc_settings.local_port),
     FIELD_U16},

    {"osc_settings.bundle", "osc_settings", "bundle",
     offsetof(NVS_Global, osc_settings.bundle),
     sizeof(((NVS_Global *)0)->osc_settings.bundle),
     FIELD_BOOL},

    {"osc_settings.address_prefix", "osc_settings", "address_prefix",
     offsetof(NVS_Global, osc_settings.address_prefix),
     sizeof(((NVS_Global *)0)->osc_settings.address_prefix),
     FIELD_BOOL},

    {"gpio_settings.gpio_rate", "gpio_settings", "gpio_rate",
     offsetof(NVS_Global, gpio_settings.gpio_rate),
     sizeof(((NVS_Global *)0)->gpio_settings.gpio_rate),
     FIELD_U32},
};

// Functions

esp_err_t nvs_update(NVS_Global *nvs, const char *field_name, const void *value);
esp_err_t nvs_populate_value(NVS_Global *nvs, const char *field_name, const void *default_value);
esp_err_t nvs_populate_all(NVS_Global *nvs, const NVS_Global *defaults);
esp_err_t nvs_save_value(const char *namespace_name, const char *key, nvs_type_t type, const void *value);
esp_err_t nvs_load_value(const char *namespace_name, const char *key, FieldType type, const void *default_value, void *out_value);

//

static inline const void *get_default_value(const NVS_Global *defaults, const FieldDesc *fd)
{
    return (const uint8_t *)defaults + fd->offset;
}

esp_err_t nvs_update(NVS_Global *nvs, const char *field_name, const void *value)
{
    for (size_t i = 0; i < sizeof(g_fields) / sizeof(g_fields[0]); i++)
    {
        const FieldDesc *fd = &g_fields[i];
        esp_err_t err;

        if (strcmp(fd->name, field_name) != 0)
            continue;

        void *target = (uint8_t *)nvs + fd->offset;

        switch (fd->type)
        {
        case FIELD_U32:
            err = nvs_save_value(fd->nvs_namespace, fd->nvs_key, NVS_TYPE_U32, value);
            *(uint32_t *)target = *(const uint32_t *)value;
            return err;

        case FIELD_U16:
            err = nvs_save_value(fd->nvs_namespace, fd->nvs_key, NVS_TYPE_U16, value);
            *(uint16_t *)target = *(const uint16_t *)value;
            return err;

        case FIELD_BOOL:
            err = nvs_save_value(fd->nvs_namespace, fd->nvs_key, NVS_TYPE_U8, value);
            *(bool *)target = *(const bool *)value;
            return err;

        case FIELD_ENUM:
            err = nvs_save_value(fd->nvs_namespace, fd->nvs_key, NVS_TYPE_U32, value);
            *(uint32_t *)target = *(const uint32_t *)value;
            return err;

        case FIELD_STR:
            err = nvs_save_value(fd->nvs_namespace, fd->nvs_key, NVS_TYPE_STR, value);
            if (err != ESP_OK)
            {
                return err;
            }
            char *copy = strdup((const char *)value);
            if (copy != NULL)
            {
                free(*(char **)target);
                *(char **)target = copy;
            }
            else
            {
                err = ESP_ERR_NO_MEM;
            }
            return err;

        default:
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t nvs_populate_value(NVS_Global *nvs, const char *field_name, const void *default_value)
{
    for (size_t i = 0; i < sizeof(g_fields) / sizeof(g_fields[0]); i++)
    {
        const FieldDesc *fd = &g_fields[i];

        if (strcmp(fd->name, field_name) != 0)
            continue;

        void *target = (uint8_t *)nvs + fd->offset;
        esp_err_t err;

        switch (fd->type)
        {
        case FIELD_U32:
        case FIELD_ENUM:
        {
            uint32_t tmp;
            err = nvs_load_value(fd->nvs_namespace, fd->nvs_key, fd->type, default_value, &tmp);
            *(uint32_t *)target = (err == ESP_OK) ? tmp : *(const uint32_t *)default_value;
            return err;
        }

        case FIELD_U16:
        {
            uint16_t tmp;
            err = nvs_load_value(fd->nvs_namespace, fd->nvs_key, FIELD_U16, default_value, &tmp);
            *(uint16_t *)target = (err == ESP_OK) ? tmp : *(const uint16_t *)default_value;
            return err;
        }

        case FIELD_BOOL:
        {
            bool tmp;
            err = nvs_load_value(fd->nvs_namespace, fd->nvs_key, FIELD_BOOL, default_value, &tmp);
            *(bool *)target = (err == ESP_OK) ? tmp : *(const bool *)default_value;
            return err;
        }

        case FIELD_STR:
        {
            char *tmp = NULL;
            err = nvs_load_value(fd->nvs_namespace, fd->nvs_key, FIELD_STR, default_value, &tmp);

            if (err != ESP_OK)
            {
                char *dup = strdup(*(const char **)default_value);
                if (dup != NULL)
                {
                    free(*(char **)target);
                    *(char **)target = dup;
                }
                else
                {
                    err = ESP_ERR_NO_MEM;
                }
                return err;
            }

            free(*(char **)target);
            *(char **)target = tmp;

            return err;
        }

        default:
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t nvs_populate_all(NVS_Global *nvs, const NVS_Global *defaults)
{
    for (size_t i = 0; i < sizeof(g_fields) / sizeof(g_fields[0]); i++)
    {
        const FieldDesc *fd = &g_fields[i];
        const void *def = get_default_value(defaults, fd);

        esp_err_t err = nvs_populate_value(nvs, fd->name, def);
        if (err != ESP_OK)
        {
            return err;
        }
    }

    return ESP_OK;
}

// Saving INT

esp_err_t nvs_save_value(const char *namespace_name, const char *key, nvs_type_t type, const void *value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }
    switch (type)
    {
    case NVS_TYPE_I32:
        err = nvs_set_i32(handle, key, *(int32_t *)value);
        break;

    case NVS_TYPE_U32:
        err = nvs_set_u32(handle, key, *(uint32_t *)value);
        break;

    case NVS_TYPE_U16:
        err = nvs_set_u16(handle, key, *(uint16_t *)value);
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

esp_err_t nvs_load_value(const char *namespace_name, const char *key, FieldType type, const void *default_value, void *out_value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);

    if (err != ESP_OK)
    {
        switch (type)
        {
        case FIELD_U32:
        case FIELD_ENUM:
            *(uint32_t *)out_value = *(const uint32_t *)default_value;
            break;
        case FIELD_U16:
            *(uint16_t *)out_value = *(const uint16_t *)default_value;
            break;
        case FIELD_BOOL:
            *(bool *)out_value = *(const bool *)default_value;
            break;
        case FIELD_STR:
            *(char **)out_value = strdup(*(const char **)default_value);
            break;
        default:
            return ESP_ERR_INVALID_ARG;
        }
        return err;
    }

    switch (type)
    {
    case FIELD_U32:
    {
        uint32_t val = *(uint32_t *)default_value;
        err = nvs_get_u32(handle, key, &val);
        *(uint32_t *)out_value = val;
        break;
    }

    case FIELD_U16:
    {
        uint16_t val = *(uint16_t *)default_value;
        err = nvs_get_u16(handle, key, &val);
        *(uint16_t *)out_value = val;
        break;
    }

    case FIELD_BOOL:
    {
        bool val = *(bool *)default_value;
        err = nvs_get_u8(handle, key, (uint8_t *)&val);
        *(bool *)out_value = val;
        break;
    }

    case FIELD_ENUM:
    {
        uint32_t val = *(uint32_t *)default_value;
        err = nvs_get_u32(handle, key, &val);
        *(uint32_t *)out_value = val;
        break;
    }

    case FIELD_STR:
    {
        size_t len = 0;
        err = nvs_get_str(handle, key, NULL, &len);

        if (err != ESP_OK)
        {
            *(char **)out_value = strdup((const char *)default_value);
            break;
        }

        char *buf = malloc(len);
        if (!buf)
        {
            *(char **)out_value = strdup((const char *)default_value);
            break;
        }

        err = nvs_get_str(handle, key, buf, &len);
        if (err != ESP_OK)
        {
            free(buf);
            *(char **)out_value = strdup((const char *)default_value);
            break;
        }

        *(char **)out_value = buf;
        break;
    }

    default:
        err = ESP_ERR_INVALID_ARG;
        break;
    }

    nvs_close(handle);
    return err;
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
