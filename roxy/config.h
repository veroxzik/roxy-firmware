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
						// Bit 6:	Enable buttons for the axes
						// Bit 7:	Invert light signals (always on, press or HID turns them off)
						// Bit 8:	Enable QE pair mode (divert QE1B and QE2B to X Axis)
	int8_t qe_sens[2];
	uint8_t ps2_mode;	// 0: Disabled
						// 1: Pop'n Music
						// 2: IIDX (QE1 for TT)
						// 3: IIDX (QE2 for TT)
	uint8_t rgb_mode;
	uint8_t rgb_brightness;
	uint8_t debounce_time;
	uint8_t controller_emulation;
	uint8_t axis_debounce_time;
	uint8_t output_mode;	// 0: Joystick only
							// 1: Keyboard only
							// 2: Joystick + Keyboard
	uint8_t reduction_ratio[2];
	uint8_t axis_sustain_time;
};

struct mapping_config_t {
	uint8_t button_kb_map[12];		// Keycodes for each button
	uint8_t axes_kb_map[4];			// Keycodes for the axes (Overrides Flag bit 6)
	uint8_t button_joy_map[6];		// Remapped joystick buttons if they are not the default
									// 1 nibble per button
	uint8_t button_led_mode[6];		// 1 nibble per button
									// 0 = Standard
									// 1 = Standard (Invert)
									// 2 = Fade out
									// 3 = Fade out (Invert)
	uint16_t button_led_fade_time;	// Time in ms
	uint8_t button_led_type[6];		// 1 nibble per button
									// 0 = Standard LED
									// 1 = mintyLED (inverted ws2812b)
									// 2 = Standard ws2812b
};

#endif
