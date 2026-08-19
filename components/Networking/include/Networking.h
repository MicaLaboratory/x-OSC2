/**
 * @file Networking.h
 * @author Ben Marples
 * @brief Networking utilities for Wi‑Fi configuration and OSC UDP transmission.
 *
 * This module provides high‑level helpers for:
 * - Initialising Wi‑Fi in either SoftAP or Station mode
 * - Applying configuration stored in NVS
 * - Sending OSC‑formatted UDP packets to a remote endpoint
 *
 * All functions are designed to be lightweight wrappers around ESP‑IDF
 * networking primitives, providing a clean API for the rest of the firmware.
 */

#ifndef NETWORKING_H
#define NETWORKING_H

//------------------------------------------------------------------------------
// Includes
#include <stdio.h>
#include <string.h>
#include "settings.h"
#include "NVS_Helper_Funcs.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"

//------------------------------------------------------------------------------
// Types

/**
 * @enum socket_err
 * @brief Error codes returned by UDP socket operations.
 *
 * These values indicate the result of a socket‑level operation inside
 * `udp_send_osc()`. They are intentionally minimal to keep error handling
 * lightweight.
 *
 * - SOCK_OK      — Operation succeeded
 * - SOCK_errno   — A standard errno‑based socket error occurred
 * - SOCK_BIND    — Failed to bind or configure the socket
 */
typedef enum
{
    SOCK_OK,      /**< Operation succeeded */
    SOCK_errno,   /**< errno‑based socket failure */
    SOCK_BIND     /**< Failure during socket bind/configuration */
} socket_err;

//------------------------------------------------------------------------------
// Function Declarations

/**
 * @brief Initialise networking subsystem and configure Wi‑Fi based on settings.
 *
 * This function selects either SoftAP or Station mode depending on the
 * configuration stored in NVS (`Osc_Settings_t`). It applies the appropriate
 * Wi‑Fi configuration, registers user‑provided callbacks, and prepares the
 * device for network communication.
 *
 * @param cfg Pointer to the loaded OSC and network settings structure.
 * @param callbacks_ Pointer to a structure containing Wi‑Fi event callbacks.
 *
 * @note This function must be called once during system startup, after NVS
 *       has been initialised.
 */
void networking_init(const Osc_Settings_t *cfg, const WirelessCallbacks *callbacks_);

/**
 * @brief Configure Wi‑Fi SoftAP mode using values stored in NVS.
 *
 * Populates a `wifi_config_t` structure with SSID, password, channel, and
 * security parameters retrieved from the global NVS context. This function
 * does not start Wi‑Fi; it only prepares the configuration.
 *
 * @param ctx Pointer to global NVS configuration context.
 * @param cfg Pointer to a `wifi_config_t` structure to be filled with AP settings.
 *
 * @return
 * - ESP_OK on success  
 * - ESP_ERR_INVALID_ARG if parameters are invalid  
 * - ESP_FAIL if configuration could not be loaded from NVS
 *
 * @note Call `esp_wifi_set_mode(WIFI_MODE_AP)` and `esp_wifi_start()` after this.
 */
esp_err_t wifi_configure_softap(const NVS_Global *ctx, wifi_config_t *cfg);

/**
 * @brief Configure Wi‑Fi Station mode using values stored in NVS.
 *
 * Loads SSID, password, scan behaviour, and authentication thresholds from NVS
 * and writes them into the provided `wifi_config_t` structure. This prepares
 * the device to connect to an external access point.
 *
 * @param ctx Pointer to global NVS configuration context.
 * @param cfg Pointer to a `wifi_config_t` structure to be filled with STA settings.
 *
 * @return
 * - ESP_OK on success  
 * - ESP_ERR_INVALID_ARG if parameters are invalid  
 * - ESP_FAIL if configuration could not be loaded from NVS
 *
 * @note Call `esp_wifi_set_mode(WIFI_MODE_STA)` and `esp_wifi_start()` after this.
 */
esp_err_t wifi_configure_station(const NVS_Global *ctx, wifi_config_t *cfg);

/**
 * @brief Send an OSC‑formatted UDP packet to the configured remote endpoint.
 *
 * Creates a UDP socket, sends the provided OSC message buffer, and closes the
 * socket immediately after transmission. This function is intended for
 * lightweight, fire‑and‑forget OSC messaging.
 *
 * @param contents Pointer to the OSC message buffer.
 * @param size Number of bytes to send.
 *
 * @return
 * - SOCK_OK on successful transmission  
 * - SOCK_errno if a socket error occurred (check errno)  
 * - SOCK_BIND if the socket could not be created or bound
 *
 * @warning This function performs a full socket open/send/close cycle each call.
 *          For high‑frequency OSC output, consider using a persistent socket.
 */
int udp_send_osc(const char *contents, const size_t size);

#endif // NETWORKING_H
