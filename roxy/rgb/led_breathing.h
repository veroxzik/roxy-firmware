#ifndef LED_BREATHING_H
#define LED_BREATHING_H

#include "pixeltypes.h"

#define BREATHING_NUM_LEDS	2	// TBD if this needs to change

class Led_Breathing {
	private:
		uint8_t update_period = 20;			// ms, fastest update speed
		uint16_t slow_period = 2000;		// ms, period of slow breathing
		uint8_t slow_brightness_low = 80;	// Max 255
		uint8_t slow_brightness_high = 180;	// Max 255
		uint8_t slow_steps;					// Brightness steps changed every update
		uint16_t fast_period = 200;			// ms, period of fast flashing
		uint8_t fast_brightness_low = 160;	// Max 255
		uint8_t fast_brightness_high = 255;	// Max 255
		uint8_t fast_steps;					// Brightness steps changed every update

		// RGB
		CRGB leds[BREATHING_NUM_LEDS];

		uint8_t led_hue[BREATHING_NUM_LEDS] = {0, 160};
		uint8_t led_brightness[BREATHING_NUM_LEDS] = {slow_brightness_low, slow_brightness_low};
		uint8_t led_mode[BREATHING_NUM_LEDS] = {0, 0};
		int8_t input_state[BREATHING_NUM_LEDS] = {0, 0};

		uint32_t last_update = 0;

	public:
		Led_Breathing() {
			slow_steps = (uint8_t)((float)(slow_brightness_high - slow_brightness_low) / ((float)slow_period / (float)update_period));
			fast_steps = (uint8_t)((float)(fast_brightness_high - fast_brightness_low) / ((float)fast_period / (float)update_period));
		};

		bool update(int8_t state0, int8_t state1);
		CRGB get_led(uint8_t index) {
			return leds[index];
		};

		void set_hue(uint8_t index, uint8_t col) {
			if(index >= 0 && index < BREATHING_NUM_LEDS)
				led_hue[index] = col;
		}
};

#endif