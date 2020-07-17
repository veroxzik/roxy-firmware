#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

struct config_t {
	uint8_t label[12];
	uint32_t flags;		// Bit 0:	Hide serial num
						// Bit 1:	Invert QE1
						// Bit 2:	Invert QE2
						// Bit 3:	LED1 always on
						// Bit 4:	LED2 always on	 (not used on Roxy)
						// Bit 5:	Enable analog knobs
	int8_t qe1_sens;
	int8_t qe2_sens;
	uint8_t ps2_mode;
	uint8_t rgb_mode;
	uint8_t rgb_brightness;
	uint8_t debounce_time;
	uint8_t controller_emulation;
	uint8_t axis_debounce_time;
};

#endif
