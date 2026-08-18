/**
 * @file NVS_Helper_Funcs.h
 * @author Ben Marples
 * @brief Helper API for persisting and loading application settings via ESP-IDF NVS.
 *
 * Provides a field-descriptor-driven mechanism (see FieldDesc / g_fields) for
 * reading and writing individual members of the NVS_Global settings struct
 * to/from non-volatile storage, along with dedicated helpers for GPIO pin
 * state (which is stored as blobs rather than scalar fields).
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

/** @brief Number of GPIO pins managed by the device, from Kconfig. */
#define PINCOUNT CONFIG_PINCOUNT

//------------------------------------------------------------------------------
// Structs 

/**
 * @brief Generic pointer/size pair used when storing or retrieving NVS blob values.
 */
typedef struct
{
    const void *data; /**< Pointer to the raw blob data. */
    size_t size;       /**< Size of the blob in bytes. */
} nvs_blob_t;

/**
 * @brief Identifies the underlying C type of a settings field, used by the
 *        generic get/set/populate routines to select the correct NVS accessor.
 */
typedef enum
{
    FIELD_U32,   /**< 32-bit unsigned integer field. */
    FIELD_U16,   /**< 16-bit unsigned integer field. */
    FIELD_BOOL,  /**< Boolean field (stored as u8 in NVS). */
    FIELD_ENUM,  /**< Enum field (stored as u32 in NVS). */
    FIELD_STR,   /**< Heap-allocated, null-terminated string field. */
    // FIELD_ARRAY_ENUM
} FieldType;

/**
 * @brief Wi-Fi network operating mode.
 */
typedef enum
{
    AP = 1,        /**< Device operates as a Wi-Fi access point. */
    STA,           /**< Device operates as a Wi-Fi station (client). */
    AP_MODE_END    /**< Sentinel marking the end of valid modes. */
} AP_Mode;

/**
 * @brief Operating mode of an individual GPIO pin.
 */
typedef enum
{
    GPIO_OFF,       /**< Pin is disabled. */
    GPIO_ANALOGUE,  /**< Pin is configured for analogue I/O. */
    GPIO_DIGITAL    /**< Pin is configured for digital I/O. */
} GPIO_State;

/**
 * @brief Direction of an individual GPIO pin.
 */
typedef enum
{
    GPIO_OUTPUT, /**< Pin is configured as an output. */
    GPIO_INPUT,  /**< Pin is configured as an input. */
} GPIO_IO;

/**
 * @brief Wi-Fi network configuration (AP and STA credentials/mode).
 */
typedef struct
{
    AP_Mode network_mode;       /**< Current network operating mode. */
    const char *ap_ssid;        /**< SSID used when operating as an access point. */
    const char *ap_password;    /**< Password used when operating as an access point. */
    const char *sta_ssid;       /**< SSID of the network to join in station mode. */
    const char *sta_password;   /**< Password of the network to join in station mode. */
} Network_Setttings_t;

/**
 * @brief OSC (Open Sound Control) transport configuration.
 */
typedef struct
{
    const char *remote_ip;   /**< Destination IP address for outgoing OSC messages. */
    uint16_t remote_port;    /**< Destination UDP port for outgoing OSC messages. */
    const char *local_ip;    /**< Local IP address to bind for incoming OSC messages. */
    uint16_t local_port;     /**< Local UDP port to bind for incoming OSC messages. */
    bool bundle;             /**< Whether outgoing messages are sent as OSC bundles. */
    bool address_prefix;     /**< Whether an address prefix is applied to OSC messages. */
} Osc_Settings_t;

/**
 * @brief GPIO subsystem configuration, including per-pin mode/direction.
 */
typedef struct
{
    uint32_t gpio_rate;              /**< Sampling/update rate for GPIO polling. */
    GPIO_State pin_mode[PINCOUNT];   /**< Per-pin operating mode. */
    GPIO_IO pin_io[PINCOUNT];        /**< Per-pin direction (input/output). */
} GPIO_Settings_t;

/**
 * @brief Top-level aggregate of all persisted application settings.
 */
typedef struct
{
    Network_Setttings_t net_settings; /**< Wi-Fi network settings. */
    Osc_Settings_t osc_settings;      /**< OSC transport settings. */
    GPIO_Settings_t gpio_settings;    /**< GPIO configuration settings. */
} NVS_Global;

/**
 * @brief Describes how a single NVS_Global field maps to NVS storage.
 *
 * Instances of this struct (see the internal g_fields table) drive the
 * generic nvs_update() / nvs_populate_value() logic, allowing fields to be
 * read or written by name without per-field boilerplate.
 */
typedef struct
{
    const char *name;          /**< Fully-qualified field name, e.g. "net_settings.ap_ssid". */
    const char *nvs_namespace; /**< NVS namespace the field is stored under. */
    const char *nvs_key;       /**< NVS key the field is stored under. */
    size_t offset;             /**< Byte offset of the field within NVS_Global. */
    size_t size;                /**< Size in bytes of the field within NVS_Global. */
    FieldType type;             /**< C type of the field, used to pick the right accessor. */
} FieldDesc;

//------------------------------------------------------------------------------
// Functions

/**
 * @brief Print every entry stored in a given NVS namespace to stdout.
 *
 * Iterates all entries in @p namespace via the NVS entry iterator API and
 * prints string and 32-bit integer values; other types are listed by key/type only.
 *
 * @param namespace NVS namespace to enumerate.
 */
void print_all_nvs_entries(const char *namespace);

/**
 * @brief Update a single named field in @p nvs and persist it to NVS.
 *
 * Looks up @p field_name in the internal field descriptor table, writes
 * @p value into both the in-memory NVS_Global struct and flash-backed NVS.
 *
 * @param nvs        Settings struct to update in place.
 * @param field_name Fully-qualified field name (see FieldDesc::name).
 * @param value      Pointer to the new value; its type must match the field's FieldType.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if @p field_name is unknown,
 *         or an ESP-IDF NVS error code on failure.
 */
esp_err_t nvs_update(NVS_Global *nvs, const char *field_name, const void *value);

/**
 * @brief Load a single named field from NVS into @p nvs, falling back to a default.
 *
 * @param nvs           Settings struct to populate.
 * @param field_name    Fully-qualified field name (see FieldDesc::name).
 * @param default_value Pointer to the default value to use if the field is
 *                       not present in NVS or on error.
 * @return ESP_OK if loaded from NVS, or the underlying error code if the
 *         default was used instead.
 */
esp_err_t nvs_populate_value(NVS_Global *nvs, const char *field_name, const void *default_value);

/**
 * @brief Load every known field into @p nvs from NVS, using @p defaults as fallback.
 *
 * @param nvs      Settings struct to populate.
 * @param defaults Struct containing default values for every field.
 * @return ESP_OK if all fields were processed successfully; the first
 *         non-OK error encountered otherwise.
 */
esp_err_t nvs_populate_all(NVS_Global *nvs, const NVS_Global *defaults);

/**
 * @brief Load a single scalar/string value from NVS, falling back to a default.
 *
 * @param namespace_name NVS namespace to read from.
 * @param key             NVS key to read.
 * @param type            Type of the value, selects the correct NVS accessor.
 * @param default_value   Pointer to the default value to use if the key is absent.
 * @param[out] out_value   Pointer to receive the loaded (or default) value.
 *                          For FIELD_STR, receives a newly heap-allocated string
 *                          that the caller is responsible for freeing.
 * @return ESP_OK on success; an ESP-IDF error code if the namespace/key could
 *         not be opened/read (in which case @p out_value is set to the default).
 */
esp_err_t nvs_load_value(const char *namespace_name, const char *key, FieldType type, const void *default_value, void *out_value);

/**
 * @brief Persist the per-pin GPIO mode array to NVS as a blob.
 *
 * @param modes Array of PINCOUNT GPIO_State values to save.
 * @return ESP_OK on success, or an ESP-IDF NVS error code on failure.
 */
esp_err_t nvs_save_gpio_modes(const GPIO_State *modes);

/**
 * @brief Load the per-pin GPIO mode array from NVS, falling back to defaults.
 *
 * @param[out] out_modes    Array of PINCOUNT GPIO_State entries to populate.
 * @param default_modes     Array of PINCOUNT default values used if NVS is
 *                           unavailable or the stored blob size doesn't match.
 * @return ESP_OK on success; an ESP-IDF error code otherwise (defaults applied).
 */
esp_err_t nvs_load_gpio_modes(GPIO_State *out_modes, const GPIO_State *default_modes);

/**
 * @brief Persist the per-pin GPIO direction array to NVS as a blob.
 *
 * @param ios Array of PINCOUNT GPIO_IO values to save.
 * @return ESP_OK on success, or an ESP-IDF NVS error code on failure.
 */
esp_err_t nvs_save_gpio_ios(const GPIO_IO *ios);

/**
 * @brief Load the per-pin GPIO direction array from NVS, falling back to defaults.
 *
 * @param[out] out_ios   Array of PINCOUNT GPIO_IO entries to populate.
 * @param default_ios     Array of PINCOUNT default values used if NVS is
 *                         unavailable or the stored blob size doesn't match.
 * @return ESP_OK on success; an ESP-IDF error code otherwise (defaults applied).
 */
esp_err_t nvs_load_gpio_ios(GPIO_IO *out_ios, const GPIO_IO *default_ios);

/**
 * @brief Persist both GPIO mode and direction arrays to NVS in one call.
 *
 * @param modes Array of PINCOUNT GPIO_State values to save.
 * @param ios   Array of PINCOUNT GPIO_IO values to save.
 * @return ESP_OK if both saves succeed; the first error encountered otherwise.
 */
esp_err_t nvs_save_gpio_pins(const GPIO_State *modes, const GPIO_IO *ios);

/**
 * @brief Load both GPIO mode and direction arrays from NVS in one call.
 *
 * @param[out] out_modes   Array of PINCOUNT GPIO_State entries to populate.
 * @param[out] out_ios     Array of PINCOUNT GPIO_IO entries to populate.
 * @param default_modes    Defaults used if the mode blob can't be loaded.
 * @param default_ios      Defaults used if the direction blob can't be loaded.
 * @return ESP_OK if both loads succeed; the first error encountered otherwise.
 */
esp_err_t nvs_load_gpio_pins(GPIO_State *out_modes, GPIO_IO *out_ios, const GPIO_State *default_modes, const GPIO_IO *default_ios);

#endif