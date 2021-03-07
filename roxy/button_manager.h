#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <gpio/gpio.h>
#include <os/time.h>

#include "board_define.h"
#include "config.h"
#include "button_leds.h"
#include "device/device_config.h"

extern Pin button_inputs[];
extern Pin button_leds[];
extern Button_Leds button_led_manager;

extern config_t config;
extern mapping_config_t mapping_config;
extern device_config_t device_config;

class Button_Manager {
    private:
        bool enabled[NUM_BUTTONS];
        uint8_t mapping[NUM_BUTTONS];
        uint32_t button_time[NUM_BUTTONS];
	    bool last_state[NUM_BUTTONS];

    public:
        void init() {
            for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
                // Buttons are enabled by default
                enabled[i] = true;

                // Go through various checks for when buttons would not act as buttons
                if (device_config.device_enable & (1 << 0)) {
                    // SVRE9 lights are enabled
                    // Check mapping
                    if ((device_config.svre_led_mapping & 0xF) == i) {
                        enabled[i] = false;
                    } 
                    if (((device_config.svre_led_mapping >> 4) & 0xF) == i) {
                        enabled[i] = false;
                    }
                }
#ifdef ARCIN
                // On arcin, disable Button 9 if WS2812b is enabled
                if(config.rgb_mode == 1 && i == 8) {
                    enabled[i] = false;
                }
#endif
                
                // Continue setting up the button if it has not been disabled
                if (enabled[i]) {
                    button_inputs[i].set_mode(Pin::Input);
                    button_inputs[i].set_pull(Pin::PullUp);

                    button_time[i] = Time::time();
                    last_state[i] = button_inputs[i].get();

                    mapping[i] = (mapping_config.button_joy_map[i / 2] >> ((i % 2) * 4)) & 0xF;

                    // Setup LEDs
                    button_leds[i].set_mode(Pin::Output);
                    button_led_manager.set_mode(i, (LedMode)((mapping_config.button_led_mode[i / 2] >> ((i % 2) * 4)) & 0xF));
                }
            }

            button_led_manager.init(mapping_config.button_led_fade_time);
        }

        uint16_t read_buttons() {
		    uint16_t buttons = 0;

            for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
                if (enabled[i]) {
                    bool read = button_inputs[i].get();

                    // If state is now different
                    if (last_state[i] != read) {
                        // Check if debounce has passed
                        if ((Time::time() - button_time[i]) >= config.debounce_time)
                        {
                            // If yes, set new time and set new state
                            button_time[i] = Time::time();
                            last_state[i] = read;
                        }
                    }

                    // State reported is always the last state
                    if (!last_state[i]) {
                        uint8_t shift = mapping[i] > 0 ? mapping[i] - 1 : i;
                            buttons |= 1 << shift;
                    }
                }
            }

            return buttons;
        }

        void set_leds_reactive() {
            for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
                if (enabled[i]) {
                    button_led_manager.set_led(i, !last_state[i] ^ ((config.flags >> 7) & 0x1));
                }
            }
        }
};

#endif