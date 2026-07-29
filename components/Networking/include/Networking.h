/**
 * @file Networking.h
 * @author Ben Marples
 */
#ifndef NETWORKING_H
#define NETWORKING_H

//------------------------------------------------------------------------------
// includes
#include <stdio.h>
#include <string.h>
#include "settings.h"
#include "NVS_Helper_Funcs.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"

//------------------------------------------------------------------------------
// Structures
typedef enum
{
    SOCK_OK,
    SOCK_errno,
    SOCK_BIND
} socket_err;

//------------------------------------------------------------------------------
// Function Definitions 
void networking_init(const Osc_Settings_t *cfg,const WirelessCallbacks *callbacks_);
esp_err_t wifi_configure_softap(const NVS_Global *ctx, wifi_config_t *cfg);
esp_err_t wifi_configure_station(const NVS_Global *ctx, wifi_config_t *cfg);
int udp_send_osc(const char *contents, const size_t size);

#endif 