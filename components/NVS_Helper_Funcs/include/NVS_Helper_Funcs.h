
#ifndef NVS_HELPER_H
#define NVS_HELPER_H


#include <stdint.h>
#include "esp_err.h"
#include "nvs.h"
#include "sdkconfig.h"

#define PINCOUNT CONFIG_PINCOUNT

// typedef enum {
//     NVS_TYPE_I32,
//     NVS_TYPE_U32,
//     NVS_TYPE_I8,
//     NVS_TYPE_U8,
//     NVS_TYPE_STR,
//     NVS_TYPE_BLOB
// } nvs_value_type_t;

typedef struct {
    const void *data;
    size_t size;
} nvs_blob_t;

typedef enum
{
    FIELD_U32,
    FIELD_U16,
    FIELD_BOOL,
    FIELD_ENUM,
    FIELD_STR,
    // FIELD_ARRAY_ENUM
} FieldType;

typedef enum
{
    AP = 1,
    STA,
    AP_MODE_END
} AP_Mode;

typedef enum
{
    GPIO_OFF,
    GPIO_ANALOGUE,
    GPIO_DIGITAL
} GPIO_State;

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

typedef struct
{
    const char *name;
    size_t offset;
    size_t size;
    FieldType type;
} FieldDesc;


void print_all_nvs_entries(const char *namespace);

esp_err_t nvs_update(NVS_Global *nvs, const char *field_name, const void *value);
esp_err_t nvs_populate_value(NVS_Global *nvs, const char *field_name, const void *default_value);
esp_err_t nvs_populate_all(NVS_Global *nvs, const NVS_Global *defaults);
esp_err_t nvs_load_value(const char *namespace_name, const char *key, FieldType type, const void *default_value, void *out_value);


#endif