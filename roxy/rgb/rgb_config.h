#ifndef RGB_CONFIG_H
#define RGB_CONFIG_H

#include <stdint.h>

struct rgb_config_t {
	uint8_t rgb_mode;   // Mode 0: HID Only
                        // Mode 1: RGB1/2 pulse with QE1/2
                        // Mode 2: TLC59711-based Turbocharger strips
                        // Mode 3: Turntable mode
    uint8_t led1_hue;
    uint8_t led2_hue;
    uint8_t tt_mode;
    uint8_t tt_num_leds;
    uint8_t tt_axis;
    uint8_t tt_hue;
    uint8_t tt_sat;
    uint8_t tt_val;
    uint8_t tt_spin;
};

#endif
