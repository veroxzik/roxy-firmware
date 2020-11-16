#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>

struct device_config_t {
    uint32_t device_enable;     // Bit 0:   Enable SVRE9 lights
                                // Bit 1:   Enable Turbocharger support on SPI3
    uint8_t svre_led_mapping;   //  1 nibble per light, button input mapping
};

#endif