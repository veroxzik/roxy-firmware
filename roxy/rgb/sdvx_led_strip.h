#ifndef SDVX_LED_STRIP_H
#define SDVX_LED_STRIP_H

#include "pixeltypes.h"

// This class keeps track of what color each led should be
// It does not handle any of the hardware interfaces

#define SDVX_RGB    // Change this later to be settable via config
#define SDVX_NUM_LEDS   24  // Not sure how to approach this

class Sdvx_Leds {
	private:
		uint8_t burst_width = 3;    // Number of LEDs on each side of main
		uint8_t scroll_speed = 20;  // ms, Time per "frame"

		int8_t burst_pos_left = -1, burst_pos_right = -1;
		bool dir_left = true, dir_right = false;
		bool scroll_left = false, scroll_right = false;

		uint8_t hue_left = 0, hue_right = 160;

		uint32_t last_update = 0;

		// RGB
		CRGB leds[SDVX_NUM_LEDS];

		// Pin mapping
		uint8_t pinMap[SDVX_NUM_LEDS] = {
			11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
			12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
		};

		void update_left();
		void update_right();

	public:
		bool update(); 
		CRGB get_led(uint8_t index) {
			return leds[pinMap[index]];
		};

		void set_left_active(bool dir);
		void set_right_active(bool dir);
		void set_active(uint8_t index, bool dir);

		void set_left_hue(uint8_t col) { hue_left = col;};
		void set_right_hue(uint8_t col) { hue_right = col;};
		void set_hue(uint8_t index, uint8_t col) { 
			if(index == 0) {
				set_left_hue(col);
			} else {
				set_right_hue(col);
			}
		};
};

#endif