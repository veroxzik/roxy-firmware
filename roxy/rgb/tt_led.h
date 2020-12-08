#ifndef TTLED_H
#define TTLED_H

#include <os/time.h>

#include "rgb_config.h"
#include "pixeltypes.h"

#define TT_MAX_LEDS 60

extern rgb_config_t rgb_config;

class Turntable_Leds {
	public:
		enum Mode {
			Solid,			// TT is a single color
			Marquee,		// TT has a marquee-style lighting pattern
			Trigger,		// TT flashes brighter when TT or button is hit
			Rainbow			// TT goes through a rainbow pattern
		};

		enum Direction {
			CW,
			CCW
		};

		enum SpinType {
			Sync,			// Sync rotation with turntable
			SingleSpeed,	// Single speed of rotation
			TwoSpeed		// Two speeds of rotation
		};

		enum SpinDirection {
			None,			// Lights are stationary when TT is not spinning
			CWIdle,			// Lights move CW when TT is not spinning
			CCWIdle,		// Lights move CCW when TT is not spinning
			LastDirection	// Lights move in the last direction when TT is not spinning
		};

	private:
		enum MarqueeState {
			NeutralCW,
			NeutralCCW,
			FastCW,
			FastCCW
		};

		uint8_t num_leds;
		CRGB leds[TT_MAX_LEDS];
		uint8_t num_groups = 6;
		uint8_t mod_val = 0;
		float gamma[TT_MAX_LEDS];
		float gamma_inv[TT_MAX_LEDS];
		volatile uint8_t start_index = 0;
		volatile bool direction = false;

		Mode mode;
		SpinType spin_type;
		SpinDirection spin_dir;
		uint8_t brightness = 255;

		uint8_t cycle_count = 0;
		uint8_t num_slow_cycles = 10;
		uint8_t num_fast_cycles = 4;
		uint8_t timeout_count = 0;
		uint8_t fast_timeout = 10;
		MarqueeState marquee_state;

		uint8_t update_speed = 20;	// ms per frame

		uint32_t last_update = 0;
		bool update_required = false;

		void step_left() {
			if(start_index == 0) {
				start_index = mod_val - 1;
			} else {
				start_index--;
			}
		}

		void step_right() {
			if(start_index == (mod_val - 1)) {
				start_index = 0;
			} else {
				start_index++;
			}
		}

	public:
		void init(uint8_t num, Mode m, SpinType st, SpinDirection sd) {
			if(num > TT_MAX_LEDS) {
				num = TT_MAX_LEDS;
			}
			num_leds = num;
			mode = m;
			spin_type = st;
			spin_dir = sd;
			mod_val = num_leds / num_groups;

			switch(mode) {
				case Marquee:
					switch(spin_dir) {
						case CWIdle:
						case LastDirection:
							marquee_state = NeutralCW;
							break;
						case CCWIdle:
							marquee_state = NeutralCCW;
							break;
					}
					gamma[0] = 1.0f;
					gamma_inv[0] = 1.0f;
					float sub = 0.5f / (mod_val - 2);
					for(int i = 0; i < mod_val; i++) {
						gamma[i + 1] = 0.6 - sub * i;
						gamma_inv[i + 1] = 0.6 - sub * (mod_val - 2 - i);
					}
					break;
			}
		}

		void set_mode(Mode m) {
			mode = m;
		}

		void set_brightness(uint8_t b) {
			brightness = b;
		}

		void set_direction(Direction dir) {
			if(mode == Marquee || mode == Rainbow) {
				if(dir == CW) {
					if(spin_type == TwoSpeed) {
						marquee_state = FastCW;
					} else {
						marquee_state = NeutralCW;
					}
				} else {
					if(spin_type == TwoSpeed) {
						marquee_state = FastCCW;
					} else {
						marquee_state = NeutralCCW;
					}
				}
			}
		}

		void set_solid(uint8_t hue, uint8_t sat = 255, uint8_t val = 255) {
			for(uint8_t i = 0; i < num_leds; i++) {
				leds[i] = CHSV(hue, sat, val);
			}
			update_required = true;
		}

		bool update() {
			if(mode == Solid) {
				if(update_required) {
					update_required = false;
					return true;
				} else {
					return false;
				}
			}

			if((Time::time() - last_update) < update_speed) {
				return false;
			}
			last_update = Time::time();

			switch(mode) {
				case Marquee:
				case Rainbow:
					if(marquee_state == NeutralCW || marquee_state == NeutralCCW) {
						cycle_count++;

						if(cycle_count >= num_slow_cycles) {
							cycle_count = 0;
						} else {
							return false;
						}
					} else {
						cycle_count++;
						if(cycle_count >= num_fast_cycles) {
							cycle_count = 0;
							timeout_count++;
						} else {
							return false;
						}
					}

					if(mode == Marquee) {
						for(uint8_t i = 0; i < num_leds; i++) {
							float sat = 0.0f;
							if(marquee_state == NeutralCW || marquee_state == FastCW) {
								sat = gamma_inv[((i + start_index) % mod_val)];
							} else {
								sat = gamma[((i + start_index) % mod_val)];
							}

							//leds[i] = CHSV(rgb_config.tt_hue, rgb_config.tt_sat, gamma_table[(uint8_t)((float)brightness * sat)]);
							leds[i] = CHSV(rgb_config.tt_hue, rgb_config.tt_sat, (uint8_t)((float)brightness * sat));
						}
					} else if(mode == Rainbow) {
						float sub = 255.0f / mod_val;
						for(uint8_t i = 0; i < num_leds; i++) {
							uint8_t col = sub * ((i + start_index) % mod_val);

							leds[i] = CHSV(col, 255, brightness);
						}
					}
					if(marquee_state == NeutralCW || marquee_state == FastCW) {
						step_left();
					} else {
						step_right();
					}

					if((marquee_state == FastCW || marquee_state == FastCCW) && timeout_count >= fast_timeout) {
						switch (spin_dir) {
							case CWIdle:
								marquee_state = NeutralCW;
								break;
							case CCWIdle:
								marquee_state = NeutralCCW;
								break;
							case LastDirection:
								if(marquee_state == FastCW) {
									marquee_state = NeutralCW;
								} else {
									marquee_state = NeutralCCW;
								}
								break;
						}
						timeout_count = 0;
						cycle_count = num_slow_cycles;	// Force an update on the next cycle
					}
					break;
			}			

			return true;
		}

		CRGB* get_leds() {
			return leds;
		}

		uint8_t get_num_leds() {
			return num_leds;
		}
};

#endif