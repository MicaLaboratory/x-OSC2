/**
 * @file OSC_Routes.h
 * @author Ben Marples
 * @brief OSC routing table and handler declarations for incoming OSC messages.
 *
 * This module defines the routing structure used to dispatch incoming OSC
 * messages to the correct handler based on their address pattern. Each handler
 * processes a specific OSC command and may update device configuration stored
 * in NVS or trigger hardware behaviour.
 *
 * The ROUTES table maps OSC address prefixes to handler functions, enabling
 * fast lookup and clean separation between parsing and execution.
 */

#ifndef OSC_ROUTES_H
#define OSC_ROUTES_H

//------------------------------------------------------------------------------
// Includes
#include <stdlib.h>
#include <string.h>
#include "OscError.h"
#include "OscPacket.h"
#include "OscSlip.h"
#include "Osc99.h"
#include "NVS_Helper_Funcs.h"

//------------------------------------------------------------------------------
// Structs

/**
 * @struct OscRoute
 * @brief Represents a single OSC route entry used for message dispatch.
 *
 * Each route contains:
 * - A prefix string that must match the start of the OSC address.
 * - A flag indicating whether the route expects a numeric channel suffix.
 * - A handler function that will be invoked when the route matches.
 *
 * The dispatcher compares incoming OSC addresses against this table and calls
 * the appropriate handler with the parsed channel (or -1 if unused).
 */
typedef struct
{
    const char *prefix; /**< OSC address prefix to match */
    bool has_channel;   /**< Whether the route expects a numeric channel suffix */
    void (*handler)(OscMessage *msg, const int channel, NVS_Global *ctx);
    /**< Handler invoked when the route matches */
} OscRoute;

//------------------------------------------------------------------------------
// Utility Functions

/**
 * @brief Extract the channel number from an OSC address.
 *
 * This helper reads the integer immediately following the route prefix.
 * It is used for routes where `has_channel == true`.
 *
 * @param addr Full OSC address string.
 * @param prefix The prefix portion to skip before reading the channel.
 *
 * @return Parsed integer channel number.
 */
static inline int parseChannel(const char *addr, const char *prefix)
{
    return atoi(addr + strlen(prefix));
}

//------------------------------------------------------------------------------
// Outbound Message Helpers

/**
 * @brief Send a standard OSC ping message to the configured remote endpoint.
 *
 * This function is typically used for connectivity testing or heartbeat
 * signalling.
 */
extern void sendPingMessage();

/**
 * @brief Send all analogue input values as an OSC message.
 *
 * Intended for bulk analogue reporting, typically triggered by a remote
 * request or periodic update.
 */
extern void send_analogue_inputs();

//------------------------------------------------------------------------------
// Handler Function Declarations
//
// Each handler processes a specific OSC command. Handlers receive:
// - The OSC message object
// - The parsed channel (or -1 if unused)
// - The global NVS context for configuration updates
//

/** @brief Print pin configuration for debugging. */
void handlePrintPinConfig(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Respond to /ping messages. */
void handlePing(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Update remote IP address used for outgoing OSC messages. */
void handleRemoteIp(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Update remote UDP port used for outgoing OSC messages. */
void handleRemotePort(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Update local UDP port used for incoming OSC messages. */
void handleLocalPort(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Enable or disable OSC bundle mode. */
void handleBundles(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Enable or disable OSC prefixing behaviour. */
void handlePrefixEnabled(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Update the OSC prefix address string. */
void handlePrefixAddress(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set analogue input mode for a specific channel. */
void handleInputModeAnalogue(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set digital input mode for a specific channel. */
void handleInputModeDigital(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set serial input mode for a specific channel. */
void handleInputModeSerial(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Read analogue input values. */
void handleAnalogueRead(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Configure analogue sampling rate. */
void handleAnalogueRate(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Read analogue comparator state. */
void handleAnalogueComparatorRead(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set analogue comparator threshold for a channel. */
void handleAnalogueComparatorThreshold(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Read digital input values. */
void handleDigitalRead(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Configure digital rising‑edge behaviour. */
void handleDigitalUp(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Configure digital falling‑edge behaviour. */
void handleDigitalDown(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set digital output mode for a channel. */
void handleOutputModeDigital(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set pulse output mode for a channel. */
void handleOutputModePulse(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set PWM output mode for a channel. */
void handleOutputModePwm(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set serial output mode for a channel. */
void handleOutputModeSerial(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Write a digital output value. */
void handleDigitalOutput(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Write a digital output pattern. */
void handleDigitalPattern(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Trigger a pulse output. */
void handlePulse(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set pulse width for a channel. */
void handlePulseWidth(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Invert pulse polarity for a channel. */
void handlePulseInvert(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set PWM frequency for a channel. */
void handlePwmFrequency(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set PWM duty cycle for a channel. */
void handlePwmDuty(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set RGB output values for a channel. */
void handleRgbOutput(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Send serial output data. */
void handleSerialOutput(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Set RGB LED colour. */
void handleLedRgb(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Restore LED to default behaviour. */
void handleLedDefault(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Configure serial baud rate. */
void handleSerialBaud(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Configure serial buffer size. */
void handleSerialBuffer(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Configure serial timeout behaviour. */
void handleSerialTimeout(OscMessage *msg, const int channel, NVS_Global *ctx);

/** @brief Configure serial framing parameters. */
void handleSerialFraming(OscMessage *msg, const int channel, NVS_Global *ctx);

/**
 * @brief Flash the LED red to indicate an unrecognised OSC command.
 *
 * Typically used by the dispatcher when no route matches the incoming address.
 */
void flashLedRed(void);

//------------------------------------------------------------------------------
// Route Table

/**
 * @brief Static table mapping OSC address prefixes to handler functions.
 *
 * The dispatcher iterates through this table and selects the first entry whose
 * prefix matches the incoming OSC address. If `has_channel` is true, the
 * dispatcher extracts the numeric channel suffix using `parseChannel()`.
 *
 * This table defines the entire OSC command set supported by the firmware.
 */
static const OscRoute ROUTES[] = {
    // Debug
    {"/pdgf", false, handlePrintPinConfig},

    // Ping
    {"/ping", false, handlePing},

    // OSC network configuration
    {"/osc/remote/ip", false, handleRemoteIp},
    {"/osc/remote/port", false, handleRemotePort},
    {"/osc/local/port", false, handleLocalPort},
    {"/osc/bundles", false, handleBundles},
    {"/osc/prefix/enabled", false, handlePrefixEnabled},
    {"/osc/prefix/address", false, handlePrefixAddress},

    // Input modes
    {"/inputs/mode/analogue/", true, handleInputModeAnalogue},
    {"/inputs/mode/digital/", true, handleInputModeDigital},
    {"/inputs/mode/serial/", true, handleInputModeSerial},

    // Analogue inputs
    {"/inputs/analogue/read", false, handleAnalogueRead},
    {"/inputs/analogue/rate", false, handleAnalogueRate},
    {"/inputs/analogue/comparator/read", false, handleAnalogueComparatorRead},
    {"/inputs/analogue/comparator/threshold/", true, handleAnalogueComparatorThreshold},

    // Digital inputs
    {"/inputs/digital/read", false, handleDigitalRead},
    {"/inputs/digital/up/", true, handleDigitalUp},
    {"/inputs/digital/down/", true, handleDigitalDown},

    // Output modes
    {"/outputs/mode/digital/", true, handleOutputModeDigital},
    {"/outputs/mode/pulse/", true, handleOutputModePulse},
    {"/outputs/mode/pwm/", true, handleOutputModePwm},
    {"/outputs/mode/serial/", true, handleOutputModeSerial},

    // Digital outputs
    {"/outputs/digital/", true, handleDigitalOutput},
    {"/outputs/digital/pattern", false, handleDigitalPattern},

    // Pulse outputs
    {"/outputs/pulse/", true, handlePulse},
    {"/outputs/pulse/width/", true, handlePulseWidth},
    {"/outputs/pulse/invert/", true, handlePulseInvert},

    // PWM outputs
    {"/outputs/pwm/frequency/", true, handlePwmFrequency},
    {"/outputs/pwm/duty/", true, handlePwmDuty},

    // RGB outputs
    {"/outputs/rgb/", true, handleRgbOutput},

    // Serial outputs
    {"/outputs/serial/", true, handleSerialOutput},

    // LED control
    {"/led/rgb", false, handleLedRgb},
    {"/led/default", false, handleLedDefault},

    // Serial configuration
    {"/serial/baud/", true, handleSerialBaud},
    {"/serial/buffer/", true, handleSerialBuffer},
    {"/serial/timeout/", true, handleSerialTimeout},
    {"/serial/framing/", true, handleSerialFraming},
};

#endif // OSC_ROUTES_H
