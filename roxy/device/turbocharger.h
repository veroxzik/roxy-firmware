#ifndef TURBOCHARGER_H
#define TURBOCHARGER_H

#include <rcc/rcc.h>
#include <dma/dma.h>
#include <gpio/gpio.h>
#include <spi/spi.h>
#include <string.h>

extern Pin rgb_sck;
extern Pin rgb_mosi;

// This class handles the built-in strips on the Turbocharger SDVX models
// The on-board chip is the MBI6024
class Turbocharger {
    private:
        uint8_t numdrivers;
        uint16_t data_header[3];
        uint16_t data_array[51];
        bool config_sent = false;
        volatile bool busy;
        bool enabled;

        uint8_t pin_map_left[24] = {
            23, 21, 19, 17, 15, 13, 11, 9, 7, 5, 3, 1, 
            25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47
        };
        uint8_t pin_map_right[24] = {
            22, 20, 18, 16, 14, 12, 10, 8, 6, 4, 2, 0, 
            24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46
        };

    public:
        // TODO: Modify init to select the SPI port of the user's choosing
        void init() {
            enabled = true;

            RCC.enable(RCC.SPI3);
			RCC.enable(RCC.DMA2);

            numdrivers = 4;

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

            uint16_t parity = is_even(count_bits((uint16_t)numdrivers - 1)) ? 0 : 1;
            // Bit 2 is always 0
            // Bit 3 is always 1
            parity |= 0b100;
            parity |= is_even(count_bits(parity)) ? 0b0000 : 0b1000;
            parity <<= 12;

            uint16_t header_array[3];
            header_array[0] = 0b1000110000000000;
            header_array[1] = 0b1000110000000000 | (numdrivers - 1);
            header_array[2] = parity | (numdrivers - 1);

            uint16_t config_array[3];
            config_array[0] = 0b0000001011111110;
            config_array[1] = 0b0000001011111110;
            config_array[2] = 0b0000000000000111;

            // Clear array
            memset(data_array, 0, 51 * sizeof(uint16_t));

            for (uint8_t i = 0; i < 3; i++) {
                data_array[i] = (header_array[i] << 8 & 0xFF00) | (header_array[i] >> 8 & 0xFF);
                data_array[i + 3] = (config_array[i] << 8 & 0xFF00) | (config_array[i] >> 8 & 0xFF);
            }


            parity = is_even(count_bits((uint16_t)numdrivers - 1)) ? 0 : 1;
            // Bit 2 is always 0
            // Bit 3 is always 0
            parity |= is_even(count_bits(parity)) ? 0b0000 : 0b1000;
            parity <<= 12;

            data_header[0] = 0b1111110000000000;
            data_header[1] = 0b1111110000000000 | (numdrivers - 1);
            data_header[2] = parity | (numdrivers - 1);

            // Setup DMA
			Interrupt::enable(Interrupt::DMA2_Channel2);

			SPI3.reg.CR2 = 	(7 < 8) |	// DS = 8bit
							(1 << 1);	// TX DMA Enabled
			// CR1: LSBFIRST = 0 (default, MSBFIRST),  CPOL = 0 (default), CPHA = 1
			SPI3.reg.CR1 = (1 << 9) | (1 << 8 ) | (7 << 3) | (1 << 2) | (1 << 0);	
			// SSM = 1, SSI = 1, BR = 7 (FpCLK/256), MSTR = 1

			// CRC = default (not used anyway)
			SPI3.reg.CRCPR = 7;

			// Enable (SPE = 1)
			SPI3.reg.CR1 |= (1 << 6);

            schedule_dma(); // Send config bytes
        }

        void set_left_led(uint8_t index, uint16_t grayscale) {
            if(index >= 24 || config_sent == false) {
                return;
            }

            index = pin_map_left[index];

            data_array[index + 3] = (grayscale << 8 & 0xFF00) | (grayscale >> 8 & 0xFF);
        }

        void set_right_led(uint8_t index, uint16_t grayscale) {
            if(index >= 24 || config_sent == false) {
                return;
            }

            index = pin_map_right[index];

            data_array[index + 3] = (grayscale << 8 & 0xFF00) | (grayscale >> 8 & 0xFF);
        }

        void schedule_dma() {
            busy = true;

			DMA2.reg.C[1].NDTR = config_sent ? 51 : 6;
			DMA2.reg.C[1].MAR = (uint32_t)&data_array;
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

        void clear_all() {
            memset(data_array + 3, 0, 48 * sizeof(uint16_t));
        }

        void irq() {
            if(!enabled) {
                return;
            }

            DMA2.reg.C[1].CR = 0;	// Disable channel
			DMA2.reg.IFCR = (1 << 4);	// Clears all interrupt flags for Channel 2

            if(config_sent == false) {
                config_sent = true;
                data_array[0] = (data_header[0] >> 8 & 0xFF) | (data_header[0] << 8 & 0xFF00);
                data_array[1] = (data_header[1] >> 8 & 0xFF) | (data_header[1] << 8 & 0xFF00);
                data_array[2] = (data_header[2] >> 8 & 0xFF) | (data_header[2] << 8 & 0xFF00);
            }
			busy = false;
        }

        uint8_t count_bits(uint16_t input) {
            uint8_t count;
            for (uint8_t i = 0; i < 16; i++) {
                if((input >> i) & 0x1 == 1) {
                    count++;
                }
            }
            return count;
        }

        bool is_even(uint8_t input) {
            return input % 2 == 0 ? true : false;
        }
};

#endif