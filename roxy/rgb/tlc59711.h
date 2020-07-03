#ifndef TLC59711_H
#define TLC59711_H

#include <rcc/rcc.h>
#include <dma/dma.h>
#include <gpio/gpio.h>
#include <spi/spi.h>

extern Pin rgb_sck;
extern Pin rgb_mosi;

// This class largely based on the Adafruit_TLC59711 library
class TLC59711 {
	private:
		uint8_t numdrivers;
		uint8_t bcr, bcg, bcb;	// Brightness
		uint16_t pwmbuffer[(2 + 12) * 7];	// Command (4 byte) + array for all channels, max of 7 TLC59711 supported
		volatile bool busy;

		void set_command_buffer() {
			// Setup buffer
			uint32_t command = 0x25;	// Magic word
			command <<= 5;
			// OUTTMG = 1, EXTGCK = 0, TMGRST = 1, DSPRPT = 1, BLANK = 0 -> 0x16
			command |= 0x16;
			command <<= 7;
			command |= bcr;
			command <<= 7;
			command |= bcg;
			command <<= 7;
			command |= bcb;

			pwmbuffer[0] = ((command >> 16) << 8 & 0xFF00) | ((command >> 16) >> 8 & 0xFF);
			pwmbuffer[1] = (command << 8 & 0xFF00) | (command >> 8 & 0xFF);

			for (uint8_t i = 1; i < numdrivers; i++) {
				pwmbuffer[i * 14] = pwmbuffer[0];
				pwmbuffer[(i * 14) + 1] = pwmbuffer[1];
			}
		}

	public:
		void init(uint8_t n) {
			RCC.enable(RCC.SPI3);
			RCC.enable(RCC.DMA2);

			// Initialize variables
			numdrivers = n;
			bcr = bcg = bcb = 0x7F;	// Max brightness

			set_command_buffer();

			rgb_sck.set_mode(Pin::AF);
			rgb_sck.set_af(6);
			rgb_sck.set_type(Pin::PushPull);
			rgb_sck.set_pull(Pin::PullNone);
			rgb_sck.set_speed(Pin::High);

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

		void schedule_dma() {

			busy = true;

			DMA2.reg.C[1].NDTR = (2 + 12) * numdrivers;
			DMA2.reg.C[1].MAR = (uint32_t)&pwmbuffer;
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

		void set_pwm(uint8_t lednum, uint8_t chan, uint16_t pwm) {
			chan = lednum * 3 + chan;
			if (chan > 12 * numdrivers)
				return;
			uint8_t driverindex = (uint8_t)(lednum / 4.0f);
			pwmbuffer[(driverindex * 14) + 2 + (chan % 12)] = (pwm << 8 & 0xFF00) | (pwm >> 8 & 0xFF);;
		}

		void set_led(uint8_t lednum, uint16_t r, uint16_t g, uint16_t b) {
			set_pwm(lednum, 0, b);
			set_pwm(lednum, 1, g);
			set_pwm(lednum, 2, r);
		}

		void set_led_8bit(uint8_t lednum, uint8_t r, uint8_t g, uint8_t b) {
			set_pwm(lednum, 0, (uint16_t)b * 256);
			set_pwm(lednum, 1, (uint16_t)g * 256);
			set_pwm(lednum, 2, (uint16_t)r * 256);
		}

		void set_brightness(uint8_t r, uint8_t g, uint8_t b) {
			// BC valid range 0-127
			bcr = r > 127 ? 127 : (r < 0 ? 0 : r);
			bcg = g > 127 ? 127 : (g < 0 ? 0 : g);
			bcb = b > 127 ? 127 : (b < 0 ? 0 : b);

			set_command_buffer();
		}

		void set_brightness(uint8_t brightness) {
			set_brightness(brightness, brightness, brightness);
		}

		void irq() {
			DMA2.reg.C[1].CR = 0;	// Disable channel
			DMA2.reg.IFCR = (1 << 4);	// Clears all interrupt flags for Channel 2

			busy = false;
		}
};

#endif