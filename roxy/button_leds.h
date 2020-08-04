#ifndef BUTTON_LEDS_H
#define BUTTON_LEDS_H

#include <rcc/rcc.h>
#include <os/time.h>
#include <timer/timer.h>
#include <stdint.h>

#include "board_define.h"

#define NUM_STEPS	20

class Button_Leds {
	private:
		enum Mode {
			FadeOut,
			Pwm
		};
		uint32_t ramp_down_cycles;	// Converted to timer cycles
		float ramp_down_slope;
		bool led_on_req[NUM_BUTTONS];
		uint32_t led_release_time[NUM_BUTTONS];
		uint32_t led_set_period[NUM_BUTTONS];
		uint32_t led_current_period[NUM_BUTTONS];
		uint32_t led_percentage[NUM_BUTTONS];
		uint32_t cycle_count[NUM_BUTTONS];
		Mode led_mode[NUM_BUTTONS];
		

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
		}

		void set_led(uint8_t index, bool state) {
			if(index >= NUM_BUTTONS) {
				return;
			}
			led_mode[index] = Mode::FadeOut;

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
			led_mode[index] = Mode::Pwm;

			led_percentage[index] = percentage;
			led_current_period[index] = 0;
			led_set_period[index] = 100;
		}

		void irq() {
			// Clear flag
			TIM6.SR &= ~(1 << 0);	// Clear UIF

			uint32_t cur_time = Time::time();
			for(int i=0; i<NUM_BUTTONS; i++) {
				switch(led_mode[i]) {
					case Mode::FadeOut:
						if(led_on_req[i]) {
							button_leds[i].on();
						} else if((cur_time - led_release_time[i]) >= (ramp_down_cycles * 20)) {	// Convert cycles to ms
							button_leds[i].off();
						} else {
							uint32_t elapsed = 20 * (cur_time - led_release_time[i]);	// Get elapsed cycles
							// Set percentage based on elapsed cycles
							led_percentage[i] = ramp_down_slope * (float)elapsed + 100;
							if(led_current_period[i] < led_percentage[i]) {
								button_leds[i].on();
							} else {
								button_leds[i].off();
							}
							led_current_period[i]++;
							if(led_current_period[i] >= led_set_period[i]) {
								led_current_period[i] = 0;
							}
						}
						break;

					case Mode::Pwm:
						if(led_current_period[i] < led_percentage[i]) {
							button_leds[i].on();
						} else {
							button_leds[i].off();
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
}

#endif