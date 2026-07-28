#ifndef OSC_ROUTES_H
#define OSC_ROUTES_H

#include <stdlib.h>
#include <string.h>
#include "OscError.h"
#include "OscPacket.h"
#include "OscSlip.h"
#include "Osc99.h"
#include "NVS_Helper_Funcs.h"

typedef struct
{
    const char *prefix;
    bool has_channel;
    void (*handler)(OscMessage *msg, const int channel,  NVS_Global *ctx);
} OscRoute;

// typedef enum {
//     GPIO_OFF,
//     GPIO_ANALOGUE,
//     GPIO_DIGITAL
// }GPIO_CONV;
static inline int parseChannel(const char *addr, const char *prefix)
{
    return atoi(addr + strlen(prefix));
}

extern void sendPingMessage();
extern void send_analogue_inputs();

void handlePrintPinConfig(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handlePing(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleRemoteIp(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleRemotePort(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleLocalPort(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleBundles(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handlePrefixEnabled(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handlePrefixAddress(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handleInputModeAnalogue(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleInputModeDigital(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleInputModeSerial(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handleAnalogueRead(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleAnalogueRate(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleAnalogueComparatorRead(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleAnalogueComparatorThreshold(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handleDigitalRead(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleDigitalUp(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleDigitalDown(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handleOutputModeDigital(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleOutputModePulse(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleOutputModePwm(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleOutputModeSerial(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handleDigitalOutput(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleDigitalPattern(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handlePulse(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handlePulseWidth(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handlePulseInvert(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handlePwmFrequency(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handlePwmDuty(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handleRgbOutput(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleSerialOutput(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handleLedRgb(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleLedDefault(OscMessage *msg, const int channel,  NVS_Global *ctx);

void handleSerialBaud(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleSerialBuffer(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleSerialTimeout(OscMessage *msg, const int channel,  NVS_Global *ctx);
void handleSerialFraming(OscMessage *msg, const int channel,  NVS_Global *ctx);

void flashLedRed(void);

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

#endif
