#ifndef TLC5973_H
#define TLC5973_H

#include <rcc/rcc.h>
#include <dma/dma.h>
#include <gpio/gpio.h>
#include <spi/spi.h>

extern Pin rgb_mosi;

class TLC5973 {
	private:
		uint16_t buf[52];
		uint8_t brightness;
		volatile bool busy;
		bool enabled = false;

		void set_command_buffer() {
			write_word(buf, 0x3AA);
			write_word(buf + 26, 0x3AA);
		}

		void write_word(uint16_t *buf, uint16_t word) {
			uint8_t lb, ub;
			for(uint8_t i = 0; i < 12; i+=2) {
				if(word & 0x800) {
					lb = 0xA0;
				} else {
					lb = 0x80;
				}
				if(word & 0x400) {
					ub = 0xA0;
				} else {
					ub = 0x80;
				}
				buf[i / 2] = ub << 8 | lb;
				word <<= 2;
			}
		}

	public:
		void init() {
			enabled = true;

			RCC.enable(RCC.SPI3);
			RCC.enable(RCC.DMA2);

			set_command_buffer();

			rgb_mosi.set_mode(Pin::AF);
			rgb_mosi.set_af(6);
			rgb_mosi.set_type(Pin::PushPull);
			rgb_mosi.set_pull(Pin::PullNone);
			rgb_mosi.set_speed(Pin::High);

			// Setup DMA
			Interrupt::enable(Interrupt::DMA2_Channel2);

			SPI3.reg.CR2 = 	(7 < 8) |	// DS = 8bit
							(1 << 1);	// TX DMA Enabled
			// CR1: LSBFIRST = 0 (default, MSBFIRST),  CPOL = 0 (default), CPHA = 0 (default)
			SPI3.reg.CR1 = (1 << 9) | (1 << 8 ) | (5 << 3) | (1 << 2);	
			// SSM = 1, SSI = 1, BR = 5 (FpCLK/64), MSTR = 1

			// CRC = default (not used anyway)
			SPI3.reg.CRCPR = 7;

			// Enable (SPE = 1)
			SPI3.reg.CR1 |= (1 << 6);
		}

		void schedule_dma()  {
			busy = true;

			DMA2.reg.C[1].NDTR = 52;
			DMA2.reg.C[1].MAR = (uint32_t)&buf;
			DMA2.reg.C[1].PAR = (uint32_t)&SPI3.reg.DR;
			DMA2.reg.C[1].CR = 	(1 << 10) |	// MSIZE = 16-bits
								(1 << 8) | 	// PSIZE = 16-bits
								(1 << 7) |	// Memory increment mode enabled
								(0 << 6) | 	// Peripheral increment mode disabled
								//(1 << 5) | 	// Circular mode enabled
								(1 << 4) |	// Direction: read from memory
								(1 << 1) |	// Transfer complete interrupt enable
								(1 << 0);
		}

		void set_led_8bit(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
			set_led(index, (uint16_t)r << 4, (uint16_t)g << 4, (uint16_t)b << 4);
		}

		void set_led(uint8_t index, uint16_t r, uint16_t g, uint16_t b) {
			if(index < 2) {
				uint8_t start = index * 26 + 6;
				write_word(buf + start, r * ((float)brightness / 255.0f));
				write_word(buf + start + 6, g * ((float)brightness / 255.0f));
				write_word(buf + start + 12, b * ((float)brightness / 255.0f));
			}
		}

		void set_brightness(uint8_t b) {
			brightness = b;
		}

		void irq() {
			if(!enabled) {
				return;
			}

			DMA2.reg.C[1].CR = 0;	// Disable channel
			DMA2.reg.IFCR = (1 << 4);	// Clears all interrupt flags for Channel 2

			busy = false;
		}
};

#endif