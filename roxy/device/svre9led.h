#ifndef SVRE9LED_H
#define SVRE9LED_H

#include <os/time.h>
#include <gpio/gpio.h>

// This class cycles through the stock SVRE9 LEDs
// For now, it is hardcoded to use the Buttons 11 and 12
// 11 = left
// 12 = right

extern Pin button_inputs[];

class SVRE9LED {
    private:
        const uint8_t switch_time = 2; // Number of ms that the signal goes low to switch
        uint32_t last_toggle_time[2];
        uint8_t pin_number[2];

    public:
        void init(uint8_t left, uint8_t right) {
            pin_number[0] = left;
            pin_number[1] = right;
            button_inputs[pin_number[0]].set_mode(Pin::Output);
            button_inputs[pin_number[0]].on();
            button_inputs[pin_number[1]].set_mode(Pin::Output);
            button_inputs[pin_number[1]].on();
        }

        void set_toggle_left() {
            button_inputs[pin_number[0]].off();
            last_toggle_time[0] = Time::time();
        }

        void toggle_left() {
            button_inputs[pin_number[0]].off();
            Time::sleep(50);
            button_inputs[pin_number[0]].on();
        }

        void set_toggle_right() {
            button_inputs[pin_number[1]].off();
            last_toggle_time[1] = Time::time();
        }

        void toggle_right() {
            button_inputs[pin_number[1]].off();
            Time::sleep(50);
            button_inputs[pin_number[1]].on();
        }

        void update() {
            uint32_t currentTime = Time::time();

            if(!button_inputs[pin_number[0]].get() && (currentTime - last_toggle_time[0]) > switch_time) {
                button_inputs[pin_number[0]].on();
            }

            if(!button_inputs[pin_number[1]].get() && (currentTime - last_toggle_time[1]) > switch_time) {
                button_inputs[pin_number[1]].on();
            }
        }

};

#endif