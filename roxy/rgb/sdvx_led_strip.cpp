#include <os/time.h>
#include <cmath>
#include <cstring>
#include "sdvx_led_strip.h"

void Sdvx_Leds::update_left() {
	if(burst_pos_left >= 0 && burst_pos_left < SDVX_NUM_LEDS) {
		CRGB temp;
		for(int8_t i = (burst_pos_left - burst_width); i <= (burst_pos_left + burst_width); i++) {
			float sat = (float)abs(burst_pos_left - i) / (float)(burst_width + 1);
			if(i >= 0 && i < SDVX_NUM_LEDS) {
				if(mode == RGB) {
					hsv2rgb_rainbow(CHSV(hue_left, 255, 255 * (1.0f - sat)), temp);
					leds[i] += temp;
				} else if(mode == TwoColor) {
					left_brightness[i] = (uint16_t)(65536.0 * (1.0f - sat));
				}	
			}
		}
	}
}

void Sdvx_Leds::update_right() {
	if(burst_pos_right >= 0 && burst_pos_right < SDVX_NUM_LEDS) {
		CRGB temp;
		for(int8_t i = (burst_pos_right - burst_width); i <= (burst_pos_right+ burst_width); i++) {
			float sat = (float)abs(burst_pos_right - i) / (float)(burst_width + 1);
			if(i >= 0 && i < SDVX_NUM_LEDS) {
				if(mode == RGB) {
					hsv2rgb_rainbow(CHSV(hue_right, 255, 255 * (1.0f - sat)), temp);
					leds[i] += temp;
				} else if(mode == TwoColor) {
					right_brightness[i] = (uint16_t)(65536.0 * (1.0f - sat));
				}	
			}
		}
	}
}

void Sdvx_Leds::init(Mode mode) {
	this->mode = mode;
}

// Return TRUE if there is an update that should be pushed to the LED driver
bool Sdvx_Leds::update() {
	// Don't update if there's no scrolling, or we didn't hit the update time yet
	if((!scroll_left && !scroll_right) || (Time::time() - last_update) < scroll_speed) {
		return false;
	}

	// Update scroll values
	if(scroll_left || scroll_right) {
		if(scroll_left) {
			burst_pos_left += dir_left ? 1 : -1;

			if(burst_pos_left < 0 || burst_pos_left >= SDVX_NUM_LEDS)
				scroll_left = false;
		}
		if(scroll_right) {
			burst_pos_right += dir_right ? 1 : -1;

			if(burst_pos_right < 0 || burst_pos_right >= SDVX_NUM_LEDS)
				scroll_right = false;
		}
	}

	// Clear and recalculate
	if(mode == RGB) {
		memset(&leds, 0, sizeof(CRGB) * SDVX_NUM_LEDS);
	} else if(mode == TwoColor) {
		memset(&left_brightness, 0, sizeof(uint16_t) * SDVX_NUM_LEDS);
		memset(&right_brightness, 0, sizeof(uint16_t) * SDVX_NUM_LEDS);
	}

	update_left();
	update_right();

	last_update = Time::time();

	return true;
}

void Sdvx_Leds::set_left_active(bool dir) {
	dir_left = dir;
	scroll_left = true;
	if(burst_pos_left >= SDVX_NUM_LEDS)
		burst_pos_left = 0;
	else if(burst_pos_left < 0)
		burst_pos_left = SDVX_NUM_LEDS - 1;
}

void Sdvx_Leds::set_right_active(bool dir) {
	dir_right = dir;
	scroll_right = true;
	if(burst_pos_right >= SDVX_NUM_LEDS)
		burst_pos_right = 0;
	else if(burst_pos_right < 0)
		burst_pos_right = SDVX_NUM_LEDS - 1;
}

void Sdvx_Leds::set_active(uint8_t index, bool dir) {
	if(index == 0) {
		set_left_active(dir);
	} else {
		set_right_active(dir);
	}
}