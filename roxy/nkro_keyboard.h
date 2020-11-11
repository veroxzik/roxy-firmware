#ifndef NKRO_H
#define NKRO_H

#include <stdint.h>

class NKRO_Keyboard {
	private:
		uint8_t nkro_report[32];

	public:
		void reset_keys() {
			for(uint8_t i = 0; i < 32; i++) {
				nkro_report[i] = 0;
			}
		}

		void reset_key(uint8_t keycode) {
			uint8_t bit = keycode % 8;
			uint8_t byte = (keycode / 8) + 1;

			if(keycode >= 240 && keycode <= 247) {
				nkro_report[0] &= ~(1 << bit);
			} else if(byte > 0 && byte <= 31) {
				nkro_report[byte] &= ~(1 << bit);
			}
		}

		void set_key(uint8_t keycode) {
			uint8_t bit = keycode % 8;
			uint8_t byte = (keycode / 8) + 1;

			if(keycode >= 240 && keycode <= 247) {
				nkro_report[0] |= (1 << bit);
			} else if(byte > 0 && byte <= 31) {
				nkro_report[byte] |= (1 << bit);
			}
		}

		uint8_t* get_data() {
			return nkro_report;
		}
};

NKRO_Keyboard nkro;

#endif