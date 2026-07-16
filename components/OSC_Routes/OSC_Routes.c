#include <stdio.h>
#include "OSC_Routes.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "NVS_Helper_Funcs.h"

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
    vTaskDelay(5);
    led_strip_clear(led_strip);
}

/**************  GENERAL / PING  **************/
void handlePing(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Ping received");
    sendPingMessage();
    ESP_LOGI("OSC", "Ping response Sent");
}

/**************  OSC NETWORK CONFIG  **************/
void handleRemoteIp(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set remote IP");
    char new_ip[64];
    OscError err = OscMessageGetArgumentAsString(msg, new_ip, sizeof(new_ip));
    if (err != OscErrorNone)
    {
        ESP_LOGE("OSC", "Failed to get osc contents");
        return;
    }

    ESP_ERROR_CHECK(nvs_save_str("OSC", "Remote_IP", new_ip));

    ESP_LOGI("OSC", "Set remote IP Successfull");
}

void handleRemotePort(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set remote port");
    int32_t new_port;
    OscError err = OscMessageGetArgumentAsInt32(msg, &new_port);
    if (err != OscErrorNone)
    {
        ESP_LOGE("OSC", "Failed to get osc contents");
        return;
    }

    ESP_ERROR_CHECK(nvs_save_int("OSC", "Remote_Port", new_port));

    ESP_LOGI("OSC", "Set remote port Successfull");
}

void handleLocalPort(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set local port");

    int32_t new_port;
    OscError err = OscMessageGetArgumentAsInt32(msg, &new_port);
    if (err != OscErrorNone)
    {
        ESP_LOGE("OSC", "Failed to get osc contents");
        return;
    }

    ESP_ERROR_CHECK(nvs_save_int("OSC", "Local_Port", new_port));

    ESP_LOGI("OSC", "Set remote port Successfull");
    // To push port change
    esp_restart();
}

void handleBundles(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set bundle mode");
}

void handlePrefixEnabled(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set prefix enabled");
}

void handlePrefixAddress(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set prefix address");
}

/**************  INPUT MODES  **************/
void handleInputModeAnalogue(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set analogue input mode ch=%d", channel);
}

void handleInputModeDigital(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set digital input mode ch=%d", channel);
}

void handleInputModeSerial(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set serial input mode ch=%d", channel);
}

/**************  ANALOGUE INPUTS  **************/
void handleAnalogueRead(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Analogue read request");

    send_analogue_inputs();
}

void handleAnalogueRate(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set analogue rate");

    int32_t New_Rate;
    OscError err = OscMessageGetArgumentAsInt32(msg, &New_Rate);

    if (err != OscErrorNone)
    {
        ESP_LOGE("OSC", "Failed to get osc contents");
        return;
    }

    ESP_ERROR_CHECK(nvs_save_int("GPIO", "Rate", New_Rate));
}

void handleAnalogueComparatorRead(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Analogue comparator read");
}

void handleAnalogueComparatorThreshold(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set analogue comparator threshold ch=%d", channel);
}

/**************  DIGITAL INPUTS  **************/
void handleDigitalRead(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Digital read request");
}

void handleDigitalUp(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Enable pull-up ch=%d", channel);

    const int gpio = channel;
    // Extract integer argument
    int32_t level;
    OscError err = OscMessageGetArgumentAsInt32(msg, &level);
    if (err != OscErrorNone)
    {
        return;
    }

    if (level != 0)
    {
        // Configure pin
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << gpio,
            .mode = GPIO_MODE_INPUT_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
        };
        gpio_config(&cfg);
        return;
    }

    // Configure pin
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&cfg);
}

void handleDigitalDown(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Enable pull-down ch=%d", channel);

    const int gpio = channel;
    // Extract integer argument
    int32_t level;
    OscError err = OscMessageGetArgumentAsInt32(msg, &level);
    if (err != OscErrorNone)
    {
        return;
    }

    // Configure pin
    if (level != 0)
    {
        // Configure pin
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << gpio,
            .mode = GPIO_MODE_INPUT_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
        };
        gpio_config(&cfg);
        return;
    }

    // Configure pin
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&cfg);
}

void handlePrintPinConfig(OscMessage *msg, const int channel)
{
    gpio_dump_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK);
}

/**************  OUTPUT MODES  **************/
void handleOutputModeDigital(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set output digital mode ch=%d", channel);
}

void handleOutputModePulse(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set output pulse mode ch=%d", channel);
}

void handleOutputModePwm(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set output PWM mode ch=%d", channel);
}

void handleOutputModeSerial(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Set output serial mode ch=%d", channel);
}

/**************  DIGITAL OUTPUTS  **************/
void handleDigitalOutput(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Digital output ch=%d", channel);
    const int gpio = channel;
    // Extract integer argument
    int32_t level;
    OscError err = OscMessageGetArgumentAsInt32(msg, &level);
    if (err != OscErrorNone)
    {
        return;
    }

    // Configure pin
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);

    // Set pin
    gpio_set_level(gpio, level ? 1 : 0);
}

void handleDigitalPattern(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Digital pattern");
}

/**************  PULSE OUTPUTS  **************/
void handlePulse(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Pulse ch=%d", channel);
}

void handlePulseWidth(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Pulse width ch=%d", channel);
}

void handlePulseInvert(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Pulse invert ch=%d", channel);
}

/**************  PWM OUTPUTS  **************/
void handlePwmFrequency(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "PWM frequency ch=%d", channel);
}

void handlePwmDuty(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "PWM duty ch=%d", channel);
}

/**************  RGB OUTPUTS  **************/
void handleRgbOutput(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "RGB output ch=%d", channel);
}

/**************  SERIAL OUTPUTS  **************/
void handleSerialOutput(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Serial output ch=%d", channel);
}

/**************  LED CONTROL  **************/
void handleLedRgb(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "LED RGB");
}

void handleLedDefault(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "LED default");
}

/**************  SERIAL CONFIG  **************/
void handleSerialBaud(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Serial baud ch=%d", channel);
}

void handleSerialBuffer(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Serial buffer ch=%d", channel);
}

void handleSerialTimeout(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Serial timeout ch=%d", channel);
}

void handleSerialFraming(OscMessage *msg, const int channel)
{
    ESP_LOGI("OSC", "Serial framing ch=%d", channel);
}
