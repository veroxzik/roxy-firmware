#ifndef AXIS_H
#define AXIS_H

#include <gpio/gpio.h>
#include <timer/timer.h>
#include <adc/adc_f3.h>
#include <os/time.h>

class Axis {
	public:
		virtual uint32_t get() = 0;
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
		uint32_t max = 255;
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
					max = (600 * 4) - 1;
				} else if(sens == -126) {
					max = (400 * 4) - 1;
				} else if(sens == -125) {
					max = (360 * 4) - 1;
				} else {
					max = 256 * -sens - 1;
				}
			} else {
				sens = 256 - 1;
			}

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
				count += max;
			}
			count += delta;
			if(count >= max) {
				count -= max;
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
			adc.CR = 0 << 28;
			adc.CR = 1 << 28;
			Time::sleep(2);
			
			// Calibrate ADC.
			// TODO
			
			// Configure continous capture on one channel.
			adc.CFGR = (1 << 13) | (1 << 12) | (1 << 5); // CONT, OVRMOD, ALIGN
			adc.SQR1 = (ch << 6);
			adc.SMPR1 = (7 << (ch * 3)); // 72 MHz / 64 / 614 = apx. 1.8 kHz
			
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