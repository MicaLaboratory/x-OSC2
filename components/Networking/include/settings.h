/**
 * @file settings.h
 * @author Ben Marples
 *
 * @brief Network and wireless configuration settings, including access point,
 *        station, and general network settings, as well as wireless event
 *        callback definitions.
 */

//------------------------------------------------------------------------------
#ifndef SETTINGS_H
#define SETTINGS_H

/**
 * @struct AP_cfg
 * @brief Configuration settings for the device operating in Access Point (AP) mode.
 *
 * @details Intended to hold parameters such as SSID, password, channel,
 *          maximum connections, and other AP-specific configuration options.
 */
typedef struct
{

} AP_cfg;

/**
 * @struct Station
 * @brief Configuration settings for the device operating in Station (client) mode.
 *
 * @details Intended to hold parameters such as the target SSID, password,
 *          and connection/reconnection behavior when joining an existing network.
 */
typedef struct
{

} Station;

/**
 * @struct Network_Settings_t
 * @brief General network settings shared across the device's networking stack.
 *
 * @details Intended to hold parameters such as IP configuration (static/DHCP),
 *          hostname, and other settings not specific to AP or Station mode alone.
 */
typedef struct
{

} Network_Settings_t;

/**
 * @struct WirelessCallbacks
 * @brief Set of function pointer callbacks used to notify the application of
 *        wireless subsystem events.
 *
 * @details Each member represents an optional callback that, when assigned,
 *          is invoked by the wireless subsystem in response to a specific
 *          event (e.g. client connection, IP acquisition, data reception,
 *          or errors). Callbacks left unassigned (NULL) are simply not invoked.
 *          Several callbacks are currently disabled/commented out and reserved
 *          for future use.
 */
typedef struct
{
    // /**
    //  * @brief Called when a wireless client connects to the device (AP mode).
    //  * @param ssid The SSID of the client that connected.
    //  * @param channel The wireless channel the client connected on.
    //  */
    // void (*client_connected)(const char *const ssid, const WirelessClientChannel channel);

    // /**
    //  * @brief Called when a wireless client disconnects from the device (AP mode).
    //  * @param ssid The SSID of the client that disconnected.
    //  */
    // void (*client_disconnected)(const char *const ssid);

    // /**
    //  * @brief Called when the device obtains an IP address (Station mode).
    //  * @param ip_address The acquired IP address, in network byte order.
    //  */
    // void (*got_ip)(const uint32_t ip_address);

    // /**
    //  * @brief Called when a wireless operation fails or an error occurs.
    //  * @param result The result/error code describing the failure.
    //  */
    // void (*send_error)(const WirelessResult result);

    /**
     * @brief Called when data is received over the wireless connection.
     * @param data Pointer to the received data buffer.
     * @param number_of_bytes Number of bytes contained in @p data.
     */
    void (*received)(const void *const data, const size_t number_of_bytes);

    // /**
    //  * @brief Called when an unrecognized/unhandled Wi-Fi stack event occurs.
    //  * @param event Name or description of the unhandled event.
    //  */
    // void (*unhandled_wifi_event)(const char *const event);

    // /**
    //  * @brief Called when an unrecognized/unhandled IP stack event occurs.
    //  * @param event Name or description of the unhandled event.
    //  */
    // void (*unhandled_ip_event)(const char *const event);
} WirelessCallbacks;

#endif