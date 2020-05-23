#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

struct config_t {
	uint8_t label[12];
	uint32_t flags;
	int8_t qe1_sens;
	int8_t qe2_sens;
	uint8_t ps2_mode;
	uint8_t rgb_mode;
	uint8_t rgb_brightness;
	uint8_t debounce_time;
};

#endif
