#include <os/time.h>
#include "led_breathing.h"

// Return TRUE if there is an update that should be pushed to the LED driver
bool Led_Breathing::update(int8_t state0, int8_t state1) {

	if((Time::time() - last_update) < update_period) {
		return false;
	}
	input_state[0] = state0;
	input_state[1] = state1;

	for(int i = 0; i < BREATHING_NUM_LEDS; i++) {
		if((input_state[i] > 1 || input_state[i] < -1) && (led_mode[i] == 0 || led_mode[i] == 1)) {
			led_mode[i] = 2;
			led_brightness[i] = fast_brightness_high - fast_steps;
		} else if(input_state[i] == 0 && (led_mode[i] == 2 || led_mode[i] == 3)) {
			led_mode[i] = 1;
		}
		switch(led_mode[i]){
			case 0:
				led_brightness[i] += slow_steps;
				if(led_brightness[i] >= slow_brightness_high) {
					led_mode[i] = 1;
				}
				break;
			case 1:
				led_brightness[i] -= slow_steps;
				if(led_brightness[i] <= slow_brightness_low) {
					led_mode[i] = 0;
				}
				break;
			case 2:
				led_brightness[i] += fast_steps;
				if(led_brightness[i] >= fast_brightness_high) {
					led_mode[i] = 3;
				}
				break;
			case 3:
				led_brightness[i] -= fast_steps;
				if(led_brightness[i] <= fast_brightness_low) {
					led_mode[i] = 0;
				}
				break;
		}
		leds[i] = CHSV(led_hue[i], 255, led_brightness[i]);
	}

	last_update = Time::time();

	return true;
}