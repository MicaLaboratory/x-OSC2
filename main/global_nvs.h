#ifndef GLOBAL_NVS_IMPL_H
#define GLOBAL_NVS_IMPL_H

#include "NVS_Helper_Funcs.h"
#include <stdbool.h>
#include "esp_err.h"
// #include <
// #include <cstddef>
// #include <cstring>

#define PINCOUNT 28

typedef enum
{
    AP = 1,
    STA,
    AP_MODE_END
} AP_Mode;

// #ifndef GPIO_State
// #define GPIO_State
typedef enum
{
    GPIO_OFF,
    GPIO_ANALOGUE,
    GPIO_DIGITAL
} GPIO_State;
// #endif
typedef enum
{
    GPIO_OUTPUT,
    GPIO_INPUT,
} GPIO_IO;

typedef struct
{
    AP_Mode network_mode;
    const char *ap_ssid;
    const char *ap_password;
    const char *sta_ssid;
    const char *sta_password;
} Network_Setttings_t;

typedef struct
{
    const char *remote_ip;
    uint16_t remote_port;
    const char *local_ip;
    uint16_t local_port;
    bool bundle;
    bool address_prefix;
} Osc_Settings_t;

typedef struct
{
    uint32_t gpio_rate;
    GPIO_State pin_mode[PINCOUNT];
    GPIO_IO pin_io[PINCOUNT];
} GPIO_Settings_t;

typedef struct
{
    Network_Setttings_t net_settings;
    Osc_Settings_t osc_settings;
    GPIO_Settings_t gpio_settings;
} NVS_Global;

typedef enum
{
    FIELD_U32,
    FIELD_U16,
    FIELD_BOOL,
    FIELD_ENUM,
    FIELD_STR
} FieldType;

typedef struct
{
    const char *name; // full path string
    size_t offset;    // offsetof(NVS_Global, ...)
    size_t size;      // sizeof(field)
    FieldType type;   // type enum
} FieldDesc;

static const FieldDesc g_fields[] = {

    /* -------------------------------
       Network Settings
       ------------------------------- */
    {"net_settings.network_mode",
     offsetof(NVS_Global, net_settings.network_mode),
     sizeof(((NVS_Global *)0)->net_settings.network_mode),
     FIELD_ENUM},
    {"net_settings.ap_ssid",
     offsetof(NVS_Global, net_settings.ap_ssid),
     sizeof(((NVS_Global *)0)->net_settings.ap_ssid),
     FIELD_STR},
    {"net_settings.ap_password",
     offsetof(NVS_Global, net_settings.ap_password),
     sizeof(((NVS_Global *)0)->net_settings.ap_password),
     FIELD_STR},
    {"net_settings.sta_ssid",
     offsetof(NVS_Global, net_settings.sta_ssid),
     sizeof(((NVS_Global *)0)->net_settings.sta_ssid),
     FIELD_STR},
    {"net_settings.sta_password",
     offsetof(NVS_Global, net_settings.sta_password),
     sizeof(((NVS_Global *)0)->net_settings.sta_password),
     FIELD_STR},

    /* -------------------------------
       OSC Settings
       ------------------------------- */
    {"osc_settings.remote_ip",
     offsetof(NVS_Global, osc_settings.remote_ip),
     sizeof(((NVS_Global *)0)->osc_settings.remote_ip),
     FIELD_STR},
    {"osc_settings.remote_port",
     offsetof(NVS_Global, osc_settings.remote_port),
     sizeof(((NVS_Global *)0)->osc_settings.remote_port),
     FIELD_U16},
    {"osc_settings.local_ip",
     offsetof(NVS_Global, osc_settings.local_ip),
     sizeof(((NVS_Global *)0)->osc_settings.local_ip),
     FIELD_STR},
    {"osc_settings.local_port",
     offsetof(NVS_Global, osc_settings.local_port),
     sizeof(((NVS_Global *)0)->osc_settings.local_port),
     FIELD_U16},
    {"osc_settings.bundle",
     offsetof(NVS_Global, osc_settings.bundle),
     sizeof(((NVS_Global *)0)->osc_settings.bundle),
     FIELD_BOOL},
    {"osc_settings.address_prefix",
     offsetof(NVS_Global, osc_settings.address_prefix),
     sizeof(((NVS_Global *)0)->osc_settings.address_prefix),
     FIELD_BOOL},

    /* -------------------------------
       GPIO Settings
       ------------------------------- */
    {"gpio_settings.gpio_rate",
     offsetof(NVS_Global, gpio_settings.gpio_rate),
     sizeof(((NVS_Global *)0)->gpio_settings.gpio_rate),
     FIELD_U32},
};

static inline bool nvs_update(NVS_Global *nvs, const char *field_name, const void *value)
{
    for (size_t i = 0; i < sizeof(g_fields) / sizeof(g_fields[0]); i++)
    {
        const FieldDesc *fd = &g_fields[i];

        if (strcmp(fd->name, field_name) != 0)
            continue;

        void *target = (uint8_t *)nvs + fd->offset;

        switch (fd->type)
        {
        case FIELD_U32:
            ESP_ERROR_CHECK(nvs_save_value("GLOBAL",field_name,NVS_TYPE_U32,value));
            *(uint32_t *)target = *(const uint32_t *)value;
            return true;

        case FIELD_U16:
            ESP_ERROR_CHECK(nvs_save_value("GLOBAL",field_name,NVS_TYPE_U32,value));
            *(uint16_t *)target = *(const uint16_t *)value;
            return true;

        case FIELD_BOOL:
            ESP_ERROR_CHECK(nvs_save_value("GLOBAL",field_name,NVS_TYPE_U8,value));
            *(bool *)target = *(const bool *)value;
            return true;

        case FIELD_ENUM:
            ESP_ERROR_CHECK(nvs_save_value("GLOBAL",field_name,NVS_TYPE_I8,value));
            *(uint32_t *)target = *(const uint32_t *)value;
            return true;

        case FIELD_STR:
            ESP_ERROR_CHECK(nvs_save_value("GLOBAL",field_name,NVS_TYPE_STR,value));
            *(const char **)target = (const char *)value;
            return true;
        }
    }

    return false; // field not found
}

#endif