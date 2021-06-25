#ifndef RGB_BUTTONS_H
#define RGB_BUTTONS_H

// This class manages all RGB buttons on the controller.
// Commands are directed from the Button LED Manager.

// Currently, this class is written exclusively for Roxy v2.0
// Assignments are as such:
// LED1 = TIM1 CH1  - PA8  - AF6
// LED2 = TIM1 CH2  - PA9  - AF6
// LED3 = TIM1 CH3  - PA10 - AF6
// LED4 = TIM8 CH2N - PC11 - AF4
// LED5 = TIM1 CH1N - PC13 - AF4
// LED6 = TIM1 CH2N - PB0  - AF6
// LED7 = TIM1 CH3N - PB1  - AF6
// LED8 = TIM8 CH1  - PC6  - AF4
// LED9 = TIM8 CH2  - PC7  - AF4
// LED10 = TIM8 CH3 - PC8  - AF4
// LED11 = TIM8 CH4 - PC9  - AF4

#include "../board_define.h"
#include "hsv2rgb.h"

class Rgb_Buttons {
    private:
        enum Timer {
            Timer1,
            Timer8
        };
        uint8_t global_brightness = 255;
        CHSV led_color[MAX_BUTTONS];
        bool enabled[MAX_BUTTONS];
        uint8_t dmabuf[26];
        CRGB temp_color;
        bool init_pin_set;
        bool init_run;
        volatile uint32_t* dma_src;
        volatile uint8_t current_led = 0;
        volatile Timer current_timer;
        volatile uint32_t cnt;
        volatile bool need_update[MAX_BUTTONS];
        volatile bool busy;

        void schedule_dma() {
            cnt--;

            // TODO: Only TIM1 supported
            if(current_timer == Timer1) {
                DMA1.reg.C[4].NDTR = 26;
                DMA1.reg.C[4].MAR = (uint32_t)&dmabuf;
                DMA1.reg.C[4].PAR = (uint32_t)dma_src;
                DMA1.reg.C[4].CR = 	(0 << 10) |	// MSIZE = 8-bits
                                    (1 << 8) | 	// PSIZE = 16-bits
                                    (1 << 7) | 	// Memory increment mode enabled
                                    (0 << 6) | 	// Peipheral increment mode disabled
                                    (1 << 4) | 	// Direction: read from memory
                                    (1 << 1) | 	// Transfer complete interrupt enable
                                    (1 << 0);	// Channel enable
            } else {
                DMA2.reg.C[0].NDTR = 26;
                DMA2.reg.C[0].MAR = (uint32_t)&dmabuf;
                DMA2.reg.C[0].PAR = (uint32_t)dma_src;
                DMA2.reg.C[0].CR = 	(0 << 10) |	// MSIZE = 8-bits
                                    (1 << 8) | 	// PSIZE = 16-bits
                                    (1 << 7) | 	// Memory increment mode enabled
                                    (0 << 6) | 	// Peipheral increment mode disabled
                                    (1 << 4) | 	// Direction: read from memory
                                    (1 << 1) | 	// Transfer complete interrupt enable
                                    (1 << 0);	// Channel enable
            }
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

        // Advance current_led to the next enabled pin
        void advance_pin() {
            for(uint8_t i = 0; i < MAX_BUTTONS; i++) {
                current_led++;
                if(current_led >= MAX_BUTTONS) {
                    current_led = 0;
                }
                if(enabled[current_led]) {
                    break;
                }
            }
        }

        // Set pin to Timer AF if active, or high otherwise
        void set_pin(uint8_t index, bool state) {
            if(index >= MAX_BUTTONS) {
                return;
            }

            if(state) {
                uint8_t af = 4;
                current_timer = Timer8;
                switch(index) {
                    case 0:
                    case 1:
                    case 2:
                    case 5:
                    case 6:
                        af = 6;
                        current_timer = Timer1;
                        break;
                    default:
                        break;
                }
                current_pins->get_button_led(index)->set_af(af);
                current_pins->get_button_led(index)->set_mode(Pin::AF);
                current_pins->get_button_led(index)->set_pull(Pin::PullNone);
            } else {
                // When not actively writing to this pin, pull it high
                current_pins->get_button_led(index)->set_mode(Pin::Output);
                current_pins->get_button_led(index)->on();
            }
        }

        // Enable relevant timer pins
        void set_output(uint8_t index) {
            if(index >= MAX_BUTTONS) {
                return;
            }
            TIM1.CCER = 0;
            TIM8.CCER = 0;
            switch(index) {
                case 0:
                    TIM1.CCER = (1 << 1) | (1 << 0);    // CC1 output enable, invert polarity
                    dma_src = &TIM1.CCR1;
                    break;
                case 1:
                    TIM1.CCER = (1 << 5) | (1 << 4);    // CC2 output enable, invert polarity
                    dma_src = &TIM1.CCR2;
                    break;
                case 2:
                    TIM1.CCER = (1 << 9) | (1 << 8);    // CC3 output enable, invert polarity
                    dma_src = &TIM1.CCR3;
                    break;
                case 3:
                    TIM8.CCER = (1 << 7) | (1 << 6);    // CC2N output enable, invert polarity
                    dma_src = &TIM8.CCR2;
                    break;
                case 4:
                    TIM1.CCER = (1 << 3) | (1 << 2);    // CC1N output enable, invert polarity
                    dma_src = &TIM1.CCR1;
                    break;
                case 5:
                    TIM1.CCER = (1 << 7) | (1 << 6);    // CC2N output enable, invert polarity
                    dma_src = &TIM1.CCR2;
                    break;
                case 6:
                    TIM1.CCER = (1 << 11) | (1 << 10);  // CC3N output enable, invert polarity
                    dma_src = &TIM1.CCR3;
                    break;
                case 7:
                    TIM8.CCER = (1 << 1) | (1 << 0);    // CC1 output enable, invert polarity
                    dma_src = &TIM8.CCR1;
                    break;
                case 8:
                    TIM8.CCER = (1 << 5) | (1 << 4);    // CC2 output enable, invert polarity
                    dma_src = &TIM8.CCR2;
                    break;
                case 9:
                    TIM8.CCER = (1 << 9) | (1 << 8);    // CC3 output enable, invert polarity
                    dma_src = &TIM8.CCR3;
                    break;
                case 10:
                    TIM8.CCER = (1 << 13) | (1 << 12);    // CC4 output enable, invert polarity
                    dma_src = &TIM8.CCR4;
                    break;
                case 11:
                    break;
            }

        }

    public:
        void init() {
            if(init_run) {
                return;
            }
            init_run = true;

            RCC.enable(RCC.TIM1);
            RCC.enable(RCC.TIM8);
            RCC.enable(RCC.DMA1);

            Interrupt::enable(Interrupt::DMA1_Channel5);
			
			TIM1.ARR = (72000000 / 800000) - 1; // period = 90, 0 = 29, 1 = 58
			TIM1.CCR1 = 0;
            TIM1.CCR2 = 0;
            TIM1.CCR3 = 0;

            TIM1.CCMR1 = (6 << 4 ) | (1 << 3);  // Channel 1 PWM mode, Preload Enabled
            TIM1.CCMR1 |= (6 << 12) | (1 << 11);// Channel 2 PWM mode, Preload Enabled
			TIM1.CCMR2 = (6 << 4) | (1 << 3);   // Channel 3 PWM Mode, Preload Enabled
			TIM1.DIER = 1 << 8;		// Update DMA request enable
			TIM1.BDTR = 1 << 15;	// Main output enable

            TIM1.CR1 = 1 << 0;      // Enable counter

            Interrupt::enable(Interrupt::DMA2_Channel1);

            TIM8.ARR = (72000000 / 800000) - 1; // period = 90, 0 = 29, 1 = 58
			TIM8.CCR1 = 0;
            TIM8.CCR2 = 0;
            TIM8.CCR3 = 0;
            TIM8.CCR4 = 0;

            TIM8.CCMR1 = (6 << 4 ) | (1 << 3);  // Channel 1 PWM mode, Preload Enabled
            TIM8.CCMR1 |= (6 << 12) | (1 << 11);// Channel 2 PWM mode, Preload Enabled
			TIM8.CCMR2 = (6 << 4) | (1 << 3);   // Channel 3 PWM Mode, Preload Enabled
            TIM8.CCMR2 |= (6 << 12) | (1 << 11);// Channel 4 PWM Mode, Preload Enabled
			TIM8.DIER = 1 << 8;		// Update DMA request enable
			TIM8.BDTR = 1 << 15;	// Main output enable

            TIM8.CR1 = 1 << 0;      // Enable counter

            set_brightness(255);
        }

        void enable(uint8_t index) {
            if(index >= MAX_BUTTONS) {
                return;
            }

            enabled[index] = true;
            set_pin(index, false);

            if(!init_pin_set) {
                set_pin(index, true);
                set_output(index);
                current_led = index;
                init_pin_set = true;
            }
        }

        void update(uint8_t index) {
            if(index >= MAX_BUTTONS || busy || !need_update[index]) {
                return;
            }
            hsv2rgb_rainbow(led_color[index], temp_color);
			set_color(temp_color.r, temp_color.g, temp_color.b);
			
			cnt = 1;
			busy = true;
            need_update[index] = false;
			
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
                need_update[index] = true;
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
                need_update[index] = true;
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
            need_update[index] = true;
        }

        // This interrupt routine fires from buttons_leds_manager (TIM6) every 0.05ms.
        // It schedules single LED bursts if an update is required.
        void irq() {
            // Leave if no pin has ever been enabled
            if(!init_pin_set) {
                return;
            }

            if(need_update[current_led]) {
                update(current_led);
            } else {
                set_pin(current_led, false);
                advance_pin();
                set_pin(current_led, true);
                set_output(current_led);
            }
        }

        void irq_dma(Interrupt::IRQ _interrupt) {
            if(_interrupt == Interrupt::DMA1_Channel5) {
                DMA1.reg.C[4].CR = 0;
                DMA1.reg.IFCR = 1 << 16;
            } else if(_interrupt == Interrupt::DMA2_Channel1) {
                DMA2.reg.C[0].CR = 0;
                DMA2.reg.IFCR = 1 << 0;
            }

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
	rgb_buttons.irq_dma(Interrupt::DMA1_Channel5);
}

template<>
void interrupt<Interrupt::DMA2_Channel1>() {
    rgb_buttons.irq_dma(Interrupt::DMA2_Channel1);
}

#endif