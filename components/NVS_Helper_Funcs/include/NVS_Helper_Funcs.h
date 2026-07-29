
/**
 * @file NVS_Helper_Funcs.h
 * @author Ben Marples
 */

//------------------------------------------------------------------------------
#ifndef NVS_HELPER_H
#define NVS_HELPER_H

//------------------------------------------------------------------------------
// Includes
#include <stdint.h>
#include "esp_err.h"
#include "nvs.h"
#include "sdkconfig.h"


//------------------------------------------------------------------------------
// Defines
#define PINCOUNT CONFIG_PINCOUNT

//------------------------------------------------------------------------------
// Structs 
typedef struct
{
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
    const char *nvs_namespace;
    const char *nvs_key;
    size_t offset;
    size_t size;
    FieldType type;
} FieldDesc;

//------------------------------------------------------------------------------
// Functions
void print_all_nvs_entries(const char *namespace);

esp_err_t nvs_update(NVS_Global *nvs, const char *field_name, const void *value);
esp_err_t nvs_populate_value(NVS_Global *nvs, const char *field_name, const void *default_value);
esp_err_t nvs_populate_all(NVS_Global *nvs, const NVS_Global *defaults);
esp_err_t nvs_load_value(const char *namespace_name, const char *key, FieldType type, const void *default_value, void *out_value);
esp_err_t nvs_save_gpio_modes(const GPIO_State *modes);
esp_err_t nvs_load_gpio_modes(GPIO_State *out_modes, const GPIO_State *default_modes);
esp_err_t nvs_save_gpio_ios(const GPIO_IO *ios);
esp_err_t nvs_load_gpio_ios(GPIO_IO *out_ios, const GPIO_IO *default_ios);
esp_err_t nvs_save_gpio_pins(const GPIO_State *modes, const GPIO_IO *ios);
esp_err_t nvs_load_gpio_pins(GPIO_State *out_modes, GPIO_IO *out_ios, const GPIO_State *default_modes, const GPIO_IO *default_ios);

#endif