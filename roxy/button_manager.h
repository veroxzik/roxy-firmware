#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <gpio/gpio.h>
#include <os/time.h>

#include "board_define.h"
#include "config.h"
#include "device/device_config.h"

extern Pin button_inputs[];

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
                
                // Continue setting up the button if it has not been disabled
                if (enabled[i]) {
                    button_inputs[i].set_mode(Pin::Input);
                    button_inputs[i].set_pull(Pin::PullUp);

                    button_time[i] = Time::time();
                    last_state[i] = button_inputs[i].get();
                }
            }
        }

        uint16_t read_buttons() {
#ifdef ARCIN
		    uint16_t buttons = 0x7ff;
#else
		    uint16_t buttons = 0xfff;
#endif
            for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
                if (enabled[i]) {
                    bool read = button_inputs[i].get();
                    if ((Time::time() - button_time[i]) >= config.debounce_time)
                    {
                        if (last_state[i] != read)
                            button_time[i] = Time::time();

                        buttons ^= read << i;
                        last_state[i] = read;
                    }
                } else {
                    // Clear bit if button is disabled
                    buttons &= ~(1 << i);
                }
            }

            return buttons;
        }
};

#endif