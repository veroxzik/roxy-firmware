#ifndef SVRE9LED_H
#define SVRE9LED_H

#include <os/time.h>
#include <gpio/gpio.h>
#include "../board_define.h"

// This class cycles through the stock SVRE9 LEDs

extern Pin_Definition *current_pins;

class SVRE9LED {
    private:
        const uint8_t switch_time = 2; // Number of ms that the signal goes low to switch
        uint32_t last_toggle_time[2];
        uint8_t pin_number[2];

    public:
        void init(uint8_t left, uint8_t right) {
            pin_number[0] = left;
            pin_number[1] = right;
            current_pins->get_button_input(pin_number[0])->set_mode(Pin::Output);
            current_pins->get_button_input(pin_number[0])->on();
            current_pins->get_button_input(pin_number[1])->set_mode(Pin::Output);
            current_pins->get_button_input(pin_number[1])->on();
        }

        void set_toggle_left() {
            current_pins->get_button_input(pin_number[0])->off();
            last_toggle_time[0] = Time::time();
        }

        void toggle_left() {
            current_pins->get_button_input(pin_number[0])->off();
            Time::sleep(50);
            current_pins->get_button_input(pin_number[0])->on();
        }

        void set_toggle_right() {
            current_pins->get_button_input(pin_number[1])->off();
            last_toggle_time[1] = Time::time();
        }

        void toggle_right() {
            current_pins->get_button_input(pin_number[1])->off();
            Time::sleep(50);
            current_pins->get_button_input(pin_number[1])->on();
        }

        void update() {
            uint32_t currentTime = Time::time();

            if(!current_pins->get_button_input(pin_number[0])->get() && (currentTime - last_toggle_time[0]) > switch_time) {
                current_pins->get_button_input(pin_number[0])->on();
            }

            if(!current_pins->get_button_input(pin_number[1])->get() && (currentTime - last_toggle_time[1]) > switch_time) {
                current_pins->get_button_input(pin_number[1])->on();
            }
        }

};

#endif