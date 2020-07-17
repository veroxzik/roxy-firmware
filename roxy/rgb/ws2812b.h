#ifndef WS2812B_H
#define WS2812B_H

#include <rcc/rcc.h>
#include <gpio/gpio.h>
#include <dma/dma.h>
#include <timer/timer.h>
#include <interrupt/interrupt.h>
#include <os/time.h>
#include "../board_define.h"

extern Pin ws_data;

class WS2812B {
	private:
		uint8_t dmabuf[26];
		volatile uint32_t cnt;
		volatile bool busy;
		
		void schedule_dma() {
			cnt--;
			
#if defined(ROXY)
			DMA2.reg.C[0].NDTR = 26;
			DMA2.reg.C[0].MAR = (uint32_t)&dmabuf;
			DMA2.reg.C[0].PAR = (uint32_t)&TIM8.CCR3;
			DMA2.reg.C[0].CR = 	(0 << 10) |	// MSIZE = 8-bits 
								(1 << 8) | 	// PSIZE = 16-bits
								(1 << 7) | 	// Memory increment mode enabled
								(0 << 6) | 	// Peipheral increment mode disabled
								(1 << 4) | 	// Direction: read from memory
								(1 << 1) | 	// Transfer complete interrupt enable
								(1 << 0);	// Channel enable
#elif defined(ARCIN)
			DMA1.reg.C[6].NDTR = 26;
			DMA1.reg.C[6].MAR = (uint32_t)&dmabuf;
			DMA1.reg.C[6].PAR = (uint32_t)&TIM4.CCR3;
			DMA1.reg.C[6].CR = (0 << 10) | (1 << 8) | (1 << 7) | (0 << 6) | (1 << 4) | (1 << 1) | (1 << 0);
#endif
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
#if defined(ROXY)
			RCC.enable(RCC.TIM8);
			RCC.enable(RCC.DMA2);
			
			Interrupt::enable(Interrupt::DMA2_Channel1);
			
			TIM8.ARR = (72000000 / 800000) - 1; // period = 90, 0 = 29, 1 = 58
			TIM8.CCR3 = 0;
			
			TIM8.CCMR2 = (6 << 4) | (1 << 3);
			TIM8.CCER = 1 << 10;	// CC3 complementary output enable
			TIM8.DIER = 1 << 8;		// Update DMA request enable
			TIM8.BDTR = 1 << 15;	// Main output enable
			
			ws_data.set_af(4);
			ws_data.set_mode(Pin::AF);
			ws_data.set_pull(Pin::PullNone);
			
			TIM8.CR1 = 1 << 0;
#elif defined(ARCIN)
			RCC.enable(RCC.TIM4);
			RCC.enable(RCC.DMA1);
			
			Interrupt::enable(Interrupt::DMA1_Channel7);
			
			TIM4.ARR = (72000000 / 800000) - 1; // period = 90, 0 = 29, 1 = 58
			TIM4.CCR3 = 0;
			
			TIM4.CCMR2 = (6 << 4) | (1 << 3);
			TIM4.CCER = 1 << 8;
			TIM4.DIER = 1 << 8;
			
			ws_data.set_af(2);
			ws_data.set_mode(Pin::AF);
			ws_data.set_pull(Pin::PullNone);
			
			TIM4.CR1 = 1 << 0;
#endif

			Time::sleep(1);
			
			update(0, 0, 0);
		}
		
		void update(uint8_t r, uint8_t g, uint8_t b) {
			set_color(r, g, b);
			
			cnt = 24;
			busy = true;
			
			schedule_dma();
		}
		
		void irq() {
#if defined(ROXY)
			DMA2.reg.C[0].CR = 0;
			DMA2.reg.IFCR = 1 << 0;
#elif defined(ARCIN)
			DMA1.reg.C[6].CR = 0;
			DMA1.reg.IFCR = 1 << 24;
#endif
			
			if(cnt) {
				schedule_dma();
			} else {
				busy = false;
			}
		}
};

#endif