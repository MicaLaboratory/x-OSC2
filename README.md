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



# Observations 

Max throughput ~3300 msg/s [104] byte length ~330KB/s data transfer

## FreeRTOS tick rate 
``` c
vTaskDelay(pdMS_TO_TICKS(nvs_global.gpio_settings.gpio_rate)); 
```
When 
``` c
nvs_global.gpio_settings.gpio_rate <= 10 
```
Throughput spikes to max regardless of values 0->9 and stops at 10; 

- 1000 allows for rough rates up to 1000 hz 
- Further testing to allow for best granularity


## Fails to compile with -O2
OSC99 Fails to compile with -O2 optimisations due to warnings being treated as errors - Is possible to surpress but need more knowledge to know if its safe to do so

