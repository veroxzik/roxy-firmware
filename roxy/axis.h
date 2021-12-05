#ifndef AXIS_H
#define AXIS_H

#include <gpio/gpio.h>
#include <timer/timer.h>
#include <adc/adc_f3.h>
#include <os/time.h>

class Axis {
	private:
		uint32_t axis_sustain_start;
		uint32_t axis_debounce_start;

		uint32_t axis_time = 0;
		uint32_t last_axis = 0;
		int8_t last_axis_stae = 0;

		uint8_t axis_debounce_time = 0;
		uint8_t axis_sustain_time = 0;

		uint8_t reduction_ratio = 0;

		float deadzone_angle = 0.0f;	// Deadzone before movement from idle is registered
		uint32_t idle_count = 0;		// Count after movement has stopped for some time

	protected:
		uint16_t max_count = 255;
		int8_t sensitivity = 0;

	public:
		uint32_t count;
		int8_t dir_state = 0;

		virtual uint32_t get() = 0;

		void set_config(uint8_t _debounce, uint8_t _sustain, uint8_t _reduction, uint8_t _deadzone) {
			axis_debounce_time = _debounce;
			axis_sustain_time = _sustain;
			reduction_ratio = _reduction;
			deadzone_angle = (float)_deadzone / 2.0f;
		}

		void process() {
			uint32_t current_time = Time::time();
			count = get();

			// Perform reduction
			uint32_t qe_temp = count;
			if(reduction_ratio > 0) {
				qe_temp = uint32_t((float)count / (4.0f * float(reduction_ratio) * 0.5f + 1.0f));
				qe_temp = uint32_t((float)qe_temp * (4.0f * float(reduction_ratio) * 0.5f + 1.0f));
			}

			// Get delta
			int8_t delta = qe_temp - last_axis;
			// Detect rollover
			if ((last_axis > (max_count / 2)) && qe_temp < (max_count / 4)) {
				delta += max_count;
			}
			float delta_angle = (float)delta / (float)max_count * 360.0f;

			// Logic:
			// If QE was stationary and is now moving, change must exceed deadzone
			// If QE was moving and is now stationary, it sustains for a set time / resets deadzone
			// If QE was moving and is now moving in the opposite direction, change must exceed deadzone to trigger

			switch(dir_state) {
				case 0:   // Not moving
					// Deadzone goes here
					if(delta > 0 && delta_angle >= deadzone_angle) {
						last_axis = qe_temp;
						dir_state = 1;
					} else if(delta < 0 && delta_angle <= -deadzone_angle) {
						last_axis = qe_temp;
						dir_state = -1;
					}
					break;
				case 1:   // Started moving CW
					if(delta == 0) {
						axis_sustain_start = current_time;
						last_axis = qe_temp;
						dir_state = 2;
					} else if(delta < 0 && delta_angle <= -deadzone_angle) {
						last_axis = qe_temp;
						dir_state = -1;
					} else if(delta > 0) {
						last_axis = qe_temp;
					}
					break;
				case 2:   // Sustaining CW
					if(delta <= 0 && ((current_time - axis_sustain_start) > axis_sustain_time)) {
						last_axis = qe_temp;
						dir_state = 0;
					} else if(delta > 0) {
						axis_sustain_start = current_time;
						last_axis = qe_temp;
					} else if(delta < 0 && delta_angle <= -deadzone_angle) {
						last_axis = qe_temp;
						dir_state = -1;
					}
					break;
				case -1:  // Started moving CCW
					if(delta == 0) {
						axis_sustain_start = current_time;
						last_axis = qe_temp;
						dir_state = -2;
					} else if(delta > 0 && delta_angle >= deadzone_angle) {
						last_axis = qe_temp;
						dir_state = 1;
					} else if(delta < 0) {
						last_axis = qe_temp;
					}
					break;
				case -2:  // Sustaining CCW
					if(delta >= 0 && ((current_time - axis_sustain_start) > axis_sustain_time)) {
						last_axis = qe_temp;
						dir_state = 0;
					} else if(delta < 0) {
						axis_sustain_start = current_time;
						last_axis = qe_temp;
					} else if(delta > 0 && delta_angle >= deadzone_angle) {
						last_axis = qe_temp;
						dir_state = 1;
					}
					break;
			}

			if(sensitivity == -127) {
				count = last_axis * (256.0f / (600.0f * 4.0f));
			} else if(sensitivity == -126) {
				count = last_axis * (256.0f / (400.0f * 4.0f));
			} else if(sensitivity == -125) {
				count = last_axis * (256.0f / (360.0f * 4.0f));
			} else if(sensitivity < 0) {
				count = last_axis / -sensitivity;
			} else if(sensitivity > 0) {
				count = last_axis * sensitivity;
			} else {
				count = last_axis;
			}

			count -= 128;
		}
};

class NullAxis : public Axis {
	public:
		virtual uint32_t get() final {
			return 0;
		}
};

class QEAxis : public Axis {
	private:
		TIM_t& tim;
	
	public:
		QEAxis(TIM_t& t) : tim(t) {}
		
		void enable(bool invert, int8_t sens) {
			if(!invert) {
				tim.CCER = 1 << 1;
			}
			
			tim.CCMR1 = (1 << 8) | (1 << 0);
			tim.SMCR = 3;
			tim.CR1 = 1;
			
			if(sens < 0) {
				// Special cases
				if(sens == -127) {
					tim.ARR = (600 * 4) - 1;
				} else if(sens == -126) {
					tim.ARR = (400 * 4) - 1;
				} else if(sens == -125) {
					tim.ARR = (360 * 4) - 1;
				} else {
					tim.ARR = 256 * -sens - 1;
				}
			} else {
				tim.ARR = 256 - 1;
			}
			sensitivity = sens;
			max_count = tim.ARR;
		}
		
		virtual uint32_t get() final {
			return tim.CNT;
		}
};

// This class inspired heavily by mon's Pocket Voltex, which was adapted from Encoder.h by PRJC
// https://github.com/mon/PocketVoltex
class IntAxis : public Axis {
	private:
		Pin *a;
		Pin *b;
		uint32_t count = 0;
		uint8_t state;

		bool invert = false;

	public:
		void set_pins(Pin _a, Pin _b) {
			a = &(_a);
			b = &(_b);
		}

		void enable(bool _invert, int8_t sens) {
			invert = _invert;

			if(sens < 0) {
				if(sens == -127) {
					max_count = (600 * 4) - 1;
				} else if(sens == -126) {
					max_count = (400 * 4) - 1;
				} else if(sens == -125) {
					max_count = (360 * 4) - 1;
				} else {
					max_count = 256 * -sens - 1;
				}
			} else {
				max_count = 256 - 1;
			}
			sensitivity = sens;

			state = a->get() | (b->get() << 1);
		}

		void updateEncoder() {
			uint8_t newState = a->get() | (b->get() << 1);
			int8_t delta;
			uint8_t tempState = state | (newState << 2);
			state = newState;
			switch (tempState) {
				case 1:
				case 7:
				case 8:
				case 14:
					delta = 1;
					break;
				case 2:
				case 4:
				case 11:
				case 13:
					delta = -1;
					break;
				case 3:
				case 12:
					delta = 2;
					break;
				case 6:
				case 9:
					delta = -2;
					break;
			}
			if(((int16_t)count + delta) < 0) {
				count += max_count;
			}
			count += delta;
			if(count >= max_count) {
				count -= max_count;
			}

		}

		virtual uint32_t get() final {
			return count;
		}
};

class AnalogAxis : public Axis {
	private:
		ADC_t& adc;
		uint32_t ch;
	
	public:
		AnalogAxis(ADC_t& a, uint32_t c) : adc(a), ch(c) {}
		
		void enable() {
			// Turn on ADC regulator.
			adc.CR &= ~((1 << 28) | (1 << 29));	// Reset ADVREGEN
			adc.CR |= 1 << 28;	// Turn on ADVREGEN
			Time::sleep(2);		// Wait for regulator to turn on
			
			// Calibrate ADC.
			adc.CR &= ~(1 << 30);	// ADCALDIF = 0 (single ended)
			adc.CR |= 1 << 31;	// Enable ADCAL
			while(!(adc.CR & (1 << 31)));	// Wait for ADCAL to finish
			
			// Configure continous capture on one channel.
			adc.CFGR = (1 << 13) | (1 << 12) | (1 << 5); // CONT, OVRMOD, ALIGN
			adc.SQR1 = (ch << 6);
			// adc.SMPR1 = (7 << (ch * 3)); // 72 MHz / 64 / 614 = apx. 1.8 kHz
			
			// Enable ADC.
			adc.CR |= 1 << 0; // ADEN
			while(!(adc.ISR & (1 << 0))); // ADRDY
			adc.ISR = (1 << 0); // ADRDY
			
			// Start conversion.
			adc.CR |= 1 << 2; // ADSTART
		}
		
		virtual uint32_t get() final {
			return adc.DR >> 8;
		}
};

#endif