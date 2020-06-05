#ifndef RGB_CONFIG_H
#define RGB_CONFIG_H

#include <stdint.h>

struct rgb_config_t {
	uint8_t rgb_mode;
    //uint32_t flags;   // For some reason this isn't working?
    uint8_t led1_hue;
    uint8_t led2_hue;
};

#endif
