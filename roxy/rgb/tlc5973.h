#ifndef TLC5973_H
#define TLC5973_H

#include <rcc/rcc.h>
#include <dma/dma.h>
#include <gpio/gpio.h>
#include <spi/spi.h>

#include "../board_version.h"

extern Pin rgb_mosi;
extern Pin spi1_mosi;

class TLC5973 {
	private:
		uint16_t buf[52];
		uint8_t brightness;
		SPI_t *spi;
		DMA_t *dma;
		uint8_t dma_chan;
		Interrupt::IRQ interrupt;
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
			if(board_version.board == Board_Version::V2_0) {
				enabled = true;
				RCC.enable(RCC.SPI1);
				RCC.enable(RCC.DMA1);

				spi = &SPI1;
				dma = &DMA1;
				dma_chan = 2;

				// Setup DMA
				interrupt = Interrupt::DMA1_Channel3;
				Interrupt::enable(interrupt);

				// Set pins
				spi1_mosi.set_mode(Pin::AF);
				spi1_mosi.set_af(5);
				spi1_mosi.set_type(Pin::PushPull);
				spi1_mosi.set_pull(Pin::PullNone);
				spi1_mosi.set_speed(Pin::High);

			} else if(board_version.board == Board_Version::V1_1) {
				enabled = true;
				RCC.enable(RCC.SPI3);
				RCC.enable(RCC.DMA2);

				spi = &SPI3;
				dma = &DMA2;
				dma_chan = 1;

				// Setup DMA
				interrupt = Interrupt::DMA2_Channel2;
				Interrupt::enable(interrupt);

				// Set pins
				rgb_mosi.set_mode(Pin::AF);
				rgb_mosi.set_af(6);
				rgb_mosi.set_type(Pin::PushPull);
				rgb_mosi.set_pull(Pin::PullNone);
				rgb_mosi.set_speed(Pin::High);

			} else {
				return;
			}
			set_command_buffer();

			spi->reg.CR2 = 	(7 < 8) |	// DS = 8bit
							(1 << 1);	// TX DMA Enabled
			// CR1: LSBFIRST = 0 (default, MSBFIRST),  CPOL = 0 (default), CPHA = 0 (default)
			spi->reg.CR1 = (1 << 9) | (1 << 8 ) | (4 << 3) | (1 << 2);
			// SSM = 1, SSI = 1, BR = 4 (FpCLK/32), MSTR = 1

			// CRC = default (not used anyway)
			spi->reg.CRCPR = 7;

			// Enable (SPE = 1)
			spi->reg.CR1 |= (1 << 6);
		}

		void schedule_dma()  {
			busy = true;

			dma->reg.C[dma_chan].NDTR = 52;
			dma->reg.C[dma_chan].MAR = (uint32_t)&buf;
			dma->reg.C[dma_chan].PAR = (uint32_t)&(spi->reg.DR);
			dma->reg.C[dma_chan].CR = 	(1 << 10) |	// MSIZE = 16-bits
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

		void irq(Interrupt::IRQ _interrupt) {
			if(!enabled || _interrupt != interrupt) {
				return;
			}

			dma->reg.C[dma_chan].CR = 0;	// Disable channel
			dma->reg.IFCR = (1 << (4 * dma_chan));	// Clears all interrupt flags for this selected channel

			busy = false;
		}
};

#endif