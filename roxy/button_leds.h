#ifndef BUTTON_LEDS_H
#define BUTTON_LEDS_H

#include <rcc/rcc.h>
#include <os/time.h>
#include <timer/timer.h>
#include <stdint.h>

#include "board_define.h"
#include "rgb/rgb_buttons.h"

extern Pin_Definition *current_pins;
extern Rgb_Buttons rgb_buttons;

#define NUM_STEPS	20

enum LedMode {
	Standard = 0,
	StandardInvert,
	FadeOut,
	FadeOutInvert,
	Pwm
};

enum LedType {
	TypeStandard = 0,
	TypeRGB
};

class Button_Leds {
	private:
		uint32_t ramp_down_cycles;	// Converted to timer cycles
		float ramp_down_slope;
		bool led_on_req[MAX_BUTTONS];
		uint32_t led_release_time[MAX_BUTTONS];
		uint32_t led_set_period[MAX_BUTTONS];
		uint32_t led_current_period[MAX_BUTTONS];
		uint32_t led_percentage[MAX_BUTTONS];
		uint32_t cycle_count[MAX_BUTTONS];
		LedMode led_mode[MAX_BUTTONS];
		LedType led_type[MAX_BUTTONS];
		

		const uint8_t gamma_input[NUM_STEPS] = {100, 30, 20, 10, 0};
		uint8_t timing_positions[NUM_STEPS];
		uint8_t value_positions[NUM_STEPS];

	public:
		void init(uint32_t ramp_down) {
			RCC.enable(RCC.TIM6);

			Interrupt::enable(Interrupt::TIM6);

			TIM6.ARR = (72000000 / 20000);	// 3,600 counts per period (20kHz = 0.05ms update)
			TIM6.DIER = (1 << 0);			// UIE = 1 (Update interrupt enable)
			TIM6.CR1 = 	(1 << 7) |			// ARPE = 1 (Auto-reload preload enabled)
						(1 << 0);			// CEN = 1 (Counter enabled)

			ramp_down_cycles = ramp_down * 20;	// Input (ms) converted to timer cycles (20 per ms)
			ramp_down_slope = -100.0f / (float)ramp_down_cycles;

			// TEMP
			led_type[0] = LedType::TypeRGB;
			led_type[1] = LedType::TypeRGB;
			led_type[2] = LedType::TypeRGB;
			rgb_buttons.init();
			rgb_buttons.set_brightness(255);
			rgb_buttons.set_color(0, 14);
			rgb_buttons.set_color(1, 65);
			rgb_buttons.set_color(2, 200);
			rgb_buttons.enable(0);
			rgb_buttons.enable(1);
			rgb_buttons.enable(2);
		}

		void set_mode(uint8_t index, LedMode mode) {
			if(index >= current_pins->get_num_buttons()) {
				return;
			}
			led_mode[index] = mode;
		}

		void set_led(uint8_t index, bool state) {
			if(index >= current_pins->get_num_buttons()) {
				return;
			}

			if(state) {
				led_on_req[index] = true;
			} else {
				if(led_on_req[index]) {
					led_release_time[index] = Time::time();
					led_current_period[index] = 0;
					led_set_period[index] = 100;
					cycle_count[index] = 0;
				}
				led_on_req[index] = false;
			}
		}

		void set_pwm(uint8_t index, uint8_t percentage) {
			if(percentage == led_percentage[index]) {
				return;
			}
			led_mode[index] = LedMode::Pwm;

			led_percentage[index] = percentage;
			led_current_period[index] = 0;
			led_set_period[index] = 100;
		}

		void set_led_pin(uint8_t index, bool state) {
			if(state) {
				set_led_on(index);
			} else {
				set_led_off(index);
			}
		}

		void set_led_on(uint8_t index) {
			if(index >= MAX_BUTTONS) {
				return;
			}

			switch(led_type[index]) {
				case LedType::TypeStandard:
					current_pins->get_button_led(index)->on();
					break;
				
				case LedType::TypeRGB:
					rgb_buttons.set_brightness(index, 255);
					break;
			}
		}

		void set_led_off(uint8_t index) {
			if(index >= MAX_BUTTONS) {
				return;
			}

			switch(led_type[index]) {
				case LedType::TypeStandard:
					current_pins->get_button_led(index)->off();
					break;
				
				case LedType::TypeRGB:
					rgb_buttons.set_brightness(index, 0);
					break;
			}
		}

		void irq() {
			// Clear flag
			TIM6.SR &= ~(1 << 0);	// Clear UIF

			uint32_t cur_time = Time::time();
			for(int i=0; i<current_pins->get_num_buttons(); i++) {
				switch(led_mode[i]) {
					case LedMode::Standard:
						set_led_pin(i, led_on_req[i]);
						break;

					case LedMode::StandardInvert:
						set_led_pin(i, !led_on_req[i]);
						break;

					case LedMode::FadeOut:
						if(led_on_req[i]) {
							set_led_on(i);
						} else if((cur_time - led_release_time[i]) >= (ramp_down_cycles * 20)) {	// Convert cycles to ms
							set_led_off(i);
						} else {
							uint32_t elapsed = 20 * (cur_time - led_release_time[i]);	// Get elapsed cycles
							// Set percentage based on elapsed cycles
							led_percentage[i] = ramp_down_slope * (float)elapsed + 100;
							if(led_type[i] == LedType::TypeStandard) {
								if(led_current_period[i] < led_percentage[i]) {
									current_pins->get_button_led(i)->on();
								} else {
									current_pins->get_button_led(i)->off();
								}
							} else if(led_type[i] == LedType::TypeRGB) {
								rgb_buttons.set_brightness(i, (float)led_percentage[i] / (float)led_set_period[i] * 255.0);
							}
							led_current_period[i]++;
							if(led_current_period[i] >= led_set_period[i]) {
								led_current_period[i] = 0;
							}
						}
						break;

					case LedMode::FadeOutInvert:
						if(led_on_req[i]) {
							set_led_off(i);
						} else if((cur_time - led_release_time[i]) >= (ramp_down_cycles * 20)) {	// Convert cycles to ms
							set_led_on(i);
						} else {
							uint32_t elapsed = 20 * (cur_time - led_release_time[i]);	// Get elapsed cycles
							// Set percentage based on elapsed cycles
							led_percentage[i] = ramp_down_slope * (float)elapsed + 100;
							if(led_type[i] == LedType::TypeStandard) {
								if(led_current_period[i] < led_percentage[i]) {
									current_pins->get_button_led(i)->off();
								} else {
									current_pins->get_button_led(i)->on();
								}
							} else if(led_type[i] == LedType::TypeRGB) {
								rgb_buttons.set_brightness(i, 255.0 - ((float)led_percentage[i] / (float)led_set_period[i] * 255.0));
							}
							led_current_period[i]++;
							if(led_current_period[i] >= led_set_period[i]) {
								led_current_period[i] = 0;
							}
						}
						break;

					case LedMode::Pwm:
						if(led_current_period[i] < led_percentage[i]) {
							current_pins->get_button_led(i)->on();
						} else {
							current_pins->get_button_led(i)->off();
						}
						led_current_period[i]++;
						if(led_current_period[i] >= led_set_period[i]) {
							led_current_period[i] = 0;
						}
						break;
				}
				
			}
		}
};

Button_Leds button_led_manager;

template<>
void interrupt<Interrupt::TIM6>() {
	button_led_manager.irq();
	rgb_buttons.irq();
}

#endif