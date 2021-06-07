#ifndef RGB_BUTTONS_H
#define RGB_BUTTONS_H

// This class manages all RGB buttons on the controller.
// Commands are directed from the Button LED Manager.

// Currently, this class is written exclusively for Roxy v2.0
// Assignments are as such:
// LED1 = TIM1 CH1
// LED2 = TIM1 CH2
// LED3 = TIM1 CH3
// LED4 = TIM8 CH2N
// LED5 = TIM8 CH3N
// LED6 = TIM1 CH2N
// LED7 = TIM1 CH3N
// LED8 = TIM8 CH1
// LED9 = TIM8 CH2
// LED10 = TIM8 CH3
// LED11 = TIM8 CH4

#include "../board_define.h"
#include "hsv2rgb.h"

class Rgb_Buttons {
    private:
        uint8_t global_brightness = 255;
        CHSV led_color[MAX_BUTTONS];
        uint8_t dmabuf[26];
        CRGB temp_color;
        volatile uint32_t cnt;
        volatile bool need_update;
        volatile bool busy;

        void schedule_dma() {
            cnt--;

            // TEMP hardcode LED 3 (TIM1 - DMA1 CH5)
            DMA1.reg.C[4].NDTR = 26;
			DMA1.reg.C[4].MAR = (uint32_t)&dmabuf;
			DMA1.reg.C[4].PAR = (uint32_t)&TIM1.CCR3;
			DMA1.reg.C[4].CR = 	(0 << 10) |	// MSIZE = 8-bits 
								(1 << 8) | 	// PSIZE = 16-bits
								(1 << 7) | 	// Memory increment mode enabled
								(0 << 6) | 	// Peipheral increment mode disabled
								(1 << 4) | 	// Direction: read from memory
								(1 << 1) | 	// Transfer complete interrupt enable
								(1 << 0);	// Channel enable
        }

        void set_color(uint8_t r, uint8_t g, uint8_t b) {
			uint32_t n = 0;

			dmabuf[0] = 0;
			n++;
			
			for(uint32_t i = 8; i-- > 0; n++) {
				dmabuf[n] = g & (1 << i) ? 58 : 29;
			}
			
			for(uint32_t i = 8; i-- > 0; n++) {
				dmabuf[n] = r & (1 << i) ? 58 : 29;
			}
			
			for(uint32_t i = 8; i-- > 0; n++) {
				dmabuf[n] = b & (1 << i) ? 58 : 29;
			}
			
			dmabuf[n] = 0;
		}

    public:
        void init() {
            RCC.enable(RCC.TIM1);
            RCC.enable(RCC.DMA1);

            Interrupt::enable(Interrupt::DMA1_Channel5);
			
			TIM1.ARR = (72000000 / 800000) - 1; // period = 90, 0 = 29, 1 = 58
			TIM1.CCR3 = 0;
			
			TIM1.CCMR2 = (6 << 4) | (1 << 3);   // Channel 3 PWM Mode, Preload Enabled
            TIM1.CCER = (1 << 9) | (1 << 8);    // CC3 output enable, invert polarity
            //TIM1.CCER = (1 << 8);    // CC3 output enable
			TIM1.DIER = 1 << 8;		// Update DMA request enable
			TIM1.BDTR = 1 << 15;	// Main output enable

            // TEMP hardcode Button 3
            current_pins->get_button_led(2)->set_af(6);
            current_pins->get_button_led(2)->set_mode(Pin::AF);
            current_pins->get_button_led(2)->set_pull(Pin::PullNone);

            TIM1.CR1 = 1 << 0;      // Enable counter
        }

        void update(uint8_t index) {
            if(index >= MAX_BUTTONS || busy || !need_update) {
                return;
            }
            hsv2rgb_rainbow(led_color[index], temp_color);
			set_color(temp_color.r, temp_color.g, temp_color.b);
			
			cnt = 1;
			busy = true;
            need_update = false;
			
			schedule_dma();
		}

        void set_brightness(uint8_t brightness) {
            global_brightness = brightness;
        }

        void set_brightness(uint8_t index, uint8_t brightness) {
            if(index >= MAX_BUTTONS) {
                return;
            }
            uint8_t temp = (uint16_t)brightness * (uint16_t)global_brightness / 255;

            if(led_color[index].v != temp) {
                led_color[index].v = temp;
                // TEMP CODE FOR EFFECT
                if(temp == 255) {
                    add_color(index, 36);
                }
                need_update = true;
            }
        }

        void set_color(uint8_t index, uint8_t hue) {
            if(index >= MAX_BUTTONS) {
                return;
            }

            if(led_color[index].h != hue) {
                led_color[index].h = hue;
                if(hue == 255) {
                    led_color[index].s = 0;
                } else {
                    led_color[index].s = 255;
                }
                need_update = true;
            }
        }

        void add_color(uint8_t index, uint8_t add) {
            if(index >= MAX_BUTTONS) {
                return;
            }

            led_color[index].h = (led_color[index].h + add) % 256;
            if(led_color[index].h  == 255) {
                led_color[index].s = 0;
            } else {
                led_color[index].s = 255;
            }
            need_update = true;
        }

        void irq() {
            DMA1.reg.C[4].CR = 0;
            DMA1.reg.IFCR = 1 << 16;

            if(cnt) {
                schedule_dma();
            } else {
                busy = false;
            }
        }

};

Rgb_Buttons rgb_buttons;

template<>
void interrupt<Interrupt::DMA1_Channel5>() {
	rgb_buttons.irq();
}

#endif