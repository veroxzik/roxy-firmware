#ifndef WS2812B_SPI_H
#define WS2812B_SPI_H

#include <rcc/rcc.h>
#include <gpio/gpio.h>
#include <dma/dma.h>
#include <interrupt/interrupt.h>
#include <os/time.h>
#include "../board_define.h"

#ifndef MAX_LEDS
#define MAX_LEDS	60
#endif

extern Pin rgb_mosi;

class WS2812B_Spi {
	private:
		uint32_t rgb_data[MAX_LEDS];
		uint8_t dmabuf[10];
		uint8_t num_leds = MAX_LEDS;
		volatile uint32_t cnt;
		volatile bool use_array;
		volatile bool busy;
		bool enabled;
		
		void schedule_dma() {
			cnt--;
			
			DMA2.reg.C[1].NDTR = 10;
			DMA2.reg.C[1].MAR = (uint32_t)&dmabuf;
			DMA2.reg.C[1].PAR = (uint32_t)&SPI3.reg.DR8;
			DMA2.reg.C[1].CR = 	(0 << 10) |	// MSIZE = 8-bits
								(0 << 8) | 	// PSIZE = 8-bits
								(1 << 7) |	// Memory increment mode enabled
								(0 << 6) | 	// Peripheral increment mode disabled
								(1 << 4) |	// Direction: read from memory
								(1 << 1) |	// Transfer complete interrupt enable
								(1 << 0);
		}
		
		void set_color(uint8_t r, uint8_t g, uint8_t b) {
			// This section is referenced from:
			// https://ioprog.com/2016/04/09/stm32f042-driving-a-ws2812b-using-spi/
			// https://github.com/fduignan/NucleoF042_SingleWS2812B
			uint32_t encoding = 0;

			for(uint32_t i = 8; i-- > 0;) {
				encoding <<= 3;
				encoding |= g & (1 << i) ? 0b110 : 0b100;
			}
			dmabuf[0] = (encoding >> 20) & 0x0F;
			dmabuf[1] = (encoding >> 12) & 0xFF;
			dmabuf[2] = (encoding >> 4) & 0xFF;
			dmabuf[3] = (encoding << 4) & 0xF0;

			encoding = 0;
			for(uint32_t i = 8; i-- > 0;) {
				encoding <<= 3;
				encoding |= r & (1 << i) ? 0b110 : 0b100;
			}
			dmabuf[3] |= (encoding >> 20) & 0x0F;
			dmabuf[4] = (encoding >> 12) & 0xFF;
			dmabuf[5] = (encoding >> 4) & 0xFF;
			dmabuf[6] = (encoding << 4) & 0xF0;

			encoding = 0;
			for(uint32_t i = 8; i-- > 0;) {
				encoding <<= 3;
				encoding |= b & (1 << i) ? 0b110 : 0b100;
			}
			dmabuf[6] |= (encoding >> 20) & 0x0F;
			dmabuf[7] = (encoding >> 12) & 0xFF;
			dmabuf[8] = (encoding >> 4) & 0xFF;
			dmabuf[9] =  (encoding << 4) & 0xF0;
		}

		void set_color(uint32_t col) {
			set_color((col >> 8) & 0xFF, (col >> 16) & 0xFF, col & 0xFF);
		}
		
	public:
		void init() {
			enabled = true;
			RCC.enable(RCC.SPI3);
			RCC.enable(RCC.DMA2);

			rgb_mosi.set_mode(Pin::AF);
			rgb_mosi.set_af(6);
			rgb_mosi.set_type(Pin::PushPull);
			rgb_mosi.set_pull(Pin::PullNone);
			rgb_mosi.set_speed(Pin::High);

			// Setup DMA
			Interrupt::enable(Interrupt::DMA2_Channel2);

			SPI3.reg.CR2 = 	(7 << 8) |	// DS = 8bit
							(1 << 1);	// TX DMA Enabled
			// CR1: LSBFIRST = 0 (default, MSBFIRST),  CPOL = 0 (default), CPHA = 0 (default)
			SPI3.reg.CR1 = (1 << 9) | (1 << 8 ) | (3 << 3) | (1 << 2);
			// SSM = 1, SSI = 1, BR = 3 (FpCLK/16), MSTR = 1

			// CRC = default (not used anyway)
			SPI3.reg.CRCPR = 7;

			// Enable (SPE = 1)
			SPI3.reg.CR1 |= (1 << 6);

			Time::sleep(1);	

			update(0, 0, 0);
		}
		
		void update(uint8_t r, uint8_t g, uint8_t b) {
			set_color(r, g, b);
			
			cnt = num_leds;
			busy = true;
			use_array = false;
			
			schedule_dma();
		}

		void set_led(uint8_t index, uint8_t r, uint8_t g, uint8_t b, bool update = true) {
			rgb_data[index] = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
			
			if(update) {
				cnt = num_leds;
				busy = true;
				use_array = true;

				set_color(rgb_data[num_leds - cnt]);
				schedule_dma();
			}
		}

		void set_num_leds(uint8_t num) {
			num_leds = num;
		}
	
		void irq() {
			if(!enabled) {
				return;
			}

			DMA2.reg.C[1].CR = 0;		// Disable channel
			DMA2.reg.IFCR = (1 << 4);	// Clears all interrupt flags for Channel 2
			
			if(cnt) {
				if(use_array) {
					set_color(rgb_data[num_leds - cnt]);
				}
				schedule_dma();
			} else {
				busy = false;
			}
		}
};

#endif