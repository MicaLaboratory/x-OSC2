#include <stdio.h>
#include "OSC_Routes.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_strip.h"

static led_strip_handle_t led_strip;

static void configure_led(void)
{
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = 27,
        .max_leds = 1, // at least one LED on board
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

void flashLedRed(void)
{
    if (led_strip == NULL)
    {
        ESP_LOGE("OSC", "flashLedRed called but LED strip is not initialized");
        configure_led();
        return;
    }
    led_strip_set_pixel(led_strip, 0, 0, 255, 0);
    led_strip_refresh(led_strip);
    vTaskDelay(100);
    led_strip_clear(led_strip);
}

/**************  GENERAL / PING  **************/
void handlePing(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Ping received");
    sendPingMessage();
}

/**************  OSC NETWORK CONFIG  **************/
void handleRemoteIp(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set remote IP");
}

void handleRemotePort(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set remote port");
}

void handleLocalPort(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set local port");
}

void handleBundles(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set bundle mode");
}

void handlePrefixEnabled(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set prefix enabled");
}

void handlePrefixAddress(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set prefix address");
}

/**************  INPUT MODES  **************/
void handleInputModeAnalogue(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set analogue input mode ch=%d", channel);
}

void handleInputModeDigital(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set digital input mode ch=%d", channel);
}

void handleInputModeSerial(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set serial input mode ch=%d", channel);
}

/**************  ANALOGUE INPUTS  **************/
void handleAnalogueRead(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Analogue read request");
}

void handleAnalogueRate(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set analogue rate");
}

void handleAnalogueComparatorRead(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Analogue comparator read");
}

void handleAnalogueComparatorThreshold(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set analogue comparator threshold ch=%d", channel);
}

/**************  DIGITAL INPUTS  **************/
void handleDigitalRead(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Digital read request");
}

void handleDigitalUp(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Enable pull-up ch=%d", channel);
}

void handleDigitalDown(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Enable pull-down ch=%d", channel);
}

/**************  OUTPUT MODES  **************/
void handleOutputModeDigital(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set output digital mode ch=%d", channel);
}

void handleOutputModePulse(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set output pulse mode ch=%d", channel);
}

void handleOutputModePwm(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set output PWM mode ch=%d", channel);
}

void handleOutputModeSerial(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Set output serial mode ch=%d", channel);
}

/**************  DIGITAL OUTPUTS  **************/
void handleDigitalOutput(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Digital output ch=%d", channel);
}

void handleDigitalPattern(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Digital pattern");
}

/**************  PULSE OUTPUTS  **************/
void handlePulse(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Pulse ch=%d", channel);
}

void handlePulseWidth(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Pulse width ch=%d", channel);
}

void handlePulseInvert(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Pulse invert ch=%d", channel);
}

/**************  PWM OUTPUTS  **************/
void handlePwmFrequency(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "PWM frequency ch=%d", channel);
}

void handlePwmDuty(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "PWM duty ch=%d", channel);
}

/**************  RGB OUTPUTS  **************/
void handleRgbOutput(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "RGB output ch=%d", channel);
}

/**************  SERIAL OUTPUTS  **************/
void handleSerialOutput(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Serial output ch=%d", channel);
}

/**************  LED CONTROL  **************/
void handleLedRgb(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "LED RGB");
}

void handleLedDefault(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "LED default");
}

/**************  SERIAL CONFIG  **************/
void handleSerialBaud(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Serial baud ch=%d", channel);
}

void handleSerialBuffer(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Serial buffer ch=%d", channel);
}

void handleSerialTimeout(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Serial timeout ch=%d", channel);
}

void handleSerialFraming(OscMessage *msg, int channel)
{
    ESP_LOGI("OSC", "Serial framing ch=%d", channel);
}
