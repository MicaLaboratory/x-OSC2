# x-OSC2


| Supported Targets | ESP32-C5 |
| ----------------- | -------- |



# Setup

Use the esp-idf tool to build and flash the esp, From here the board should start hosting its own network with:

Default:
- SSID: x-osc-2
- PASSWORD: mypassword

This is just the default and can be changed in the esp-idf menuconfig under Default AP SSID and Password.  

To Access the Configuration Page go to 

Address: 192.168.4.1

From here you can configure all options for it to join a network.


# Style Guide

## File Structure 
Every file opens with Doxygen-style header block:
``` c
/**
 * @file file.c / file.h 
 * @author Author 
 * @brief File contents
 */
```
File body divided into labelled sections with marked full-width divider comments:
``` c
//------------------------------------------------------------------------------
// Includes

//------------------------------------------------------------------------------
// Variables

//------------------------------------------------------------------------------
// Function declarations

//------------------------------------------------------------------------------
// Functions Implementations
```
Section order: Includes → Variables → Function declarations → Functions. 
Every static (private) function is forward-declared in the Function declarations section, in the order it will be defined.

File ends:
``` c 
//------------------------------------------------------------------------------
// End of file
```

## Naming conventions

### Functions
snake_case, Prefixed with the owning module.

### Variables 
snake_case

### Types/enums
PascalCase

## Doxygen Comments
Each function requires a brief and a description of what each @param does with @notes for current implementation flaws/important information for the future and @details for functions needing to be implemented. @return needs to show all possible return values and any possible unexpected returns e.g esp_restart()
e.g
``` c
/**
 * @brief GET /network — updates nvs_global.net_settings (WiFi mode + AP/STA credentials),
 * then redirects to "/" and restarts the device to apply the new settings.
 * @param req HTTP GET request; expects query params: mode=AP|STA, AP-SSID, AP-Password,
 * STA-SSID, STA-Password (all required).
 * @note Long SSID/password input is silently truncated to 15 characters
 * (value buffer size) rather than rejected or reported to the caller.
 * @return ESP_OK on success; sends a 400 response if any required param is missing or mode is invalid.
 */
static esp_err_t Network_Handler(httpd_req_t *req)
```

## Const Correctness 
Parameters that aren't mutated are const, including pointer targets: const char *const ssid.
Local values that don't change after initialization are declared const: const WirelessSettings settings = { ... };, const unsigned int id = 0;.
Struct literals use designated initializers:
``` c
const WirelessSettings settings = {
    .region = WirelessRegionGb,
    .client_ssid = "x-IMU3 Network",
    .client_password = "xiotechnologies",
    .client_channel = WirelessClientChannel44,
};
``` 

## Constants & Magic Numbers
Give magic numbers a named const and explain any non-obvious constraint inline with a trailing comment:
``` c
const int loop_hz = 100; // max is configTICK_RATE_HZ
```