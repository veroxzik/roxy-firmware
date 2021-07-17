#ifndef SPI_PS_H
#define SPI_PS_H

#include <gpio/gpio.h>
#include <spi/spi.h>
#include <interrupt/interrupt.h>
#include "board_define.h"
#include "config.h"

extern config_t config;

#define SELECT     0
#define L3         1
#define R3         2
#define START      3
#define DPAD_UP    4
#define DPAD_RIGHT 5
#define DPAD_DOWN  6
#define DPAD_LEFT  7
#define L2         8
#define R2         9
#define L1         10
#define R1         11
#define TRIANGLE   12
#define CIRCLE     13
#define CROSS      14
#define SQUARE     15

class SPI_PS {
    private:
        bool enabled;
        uint8_t state;
        uint16_t button_state;

        bool process_data(uint8_t data) {
            bool ack = false;
            if(data == 0x01) {
                // Reset state upon receiving 0x01 (first byte)
                state = 0;
            }

            switch(state) {
                case 0:
                    // 0x40 = digital mode
                    // 0x01 = one 16-bit words after header
                    SPI2.reg.DR8 = 0x41;
                    ack = true;
                    break;
                case 1:
                    // 0x42 = Poll controller
                    // 0x5A = Always sent
                    if(data == 0x42) {
                        SPI2.reg.DR8 = 0x5A;
                        ack = true;
                    }
                    break;
                case 2:
                    // Lower byte button state
                    SPI2.reg.DR8 = (uint8_t)(button_state & 0xFF);
                    ack = true;
                    break;
                case 3:
                    // Upper byte button state
                    SPI2.reg.DR8 = (uint8_t)((button_state >> 8) & 0xFF);
                    ack = true;
                    break;
                case 4:
                    // Send ack with dummy data
                    SPI2.reg.DR8 = 0xFF;
                    ack = true;
                    break;
                default:
                    // Dummy data, no ack
                    SPI2.reg.DR8 = 0xFF;
                    break;
            }
            state++;
            return ack;
        }

    public:
        void init() {
            enabled = true;

            RCC.enable(RCC.SPI2);

            ps_ack.on();
            ps_ack.set_type(Pin::OpenDrain);
            ps_ack.set_mode(Pin::Output);

            ps_ss.set_af(5);
            ps_ss.set_type(Pin::OpenDrain);
            ps_ss.set_mode(Pin::AF);

            ps_sck.set_af(5);
            ps_sck.set_type(Pin::OpenDrain);
            ps_sck.set_mode(Pin::AF);

            ps_miso.set_af(5);
            ps_miso.set_type(Pin::OpenDrain);
            ps_miso.set_mode(Pin::AF);

            ps_mosi.set_af(5);
            ps_mosi.set_type(Pin::OpenDrain);
            ps_mosi.set_mode(Pin::AF);

            SPI2.reg.CR2 =  (1 << 12) | // FRXTH
                            (7 << 8) |  // DS = 8bit
                            (1 << 6);   // RXNEIE
            SPI2.reg.CR1 =  (1 << 7) |  // LSBFIRST
                            (1 << 6) |  // SPI Enable
                            (1 << 1) |  // CPOL = 1
                            (1 << 0);   // CPHA = 1

            Interrupt::enable(Interrupt::SPI2);
        }

        void set_buttons(uint16_t buttons) {
            // Clear all buttons first
            button_state = 0xFFFF;
            
            // Iterate through button array and map accordingly
            switch(config.ps2_mode) {
                case 1: // Pop'n
                    // Pop'n requires Left, Right, and Down on the DPAD to be held
                    button_state ^= (1 << DPAD_LEFT);
                    button_state ^= (1 << DPAD_RIGHT);
                    button_state ^= (1 << DPAD_DOWN);

                    for(uint8_t i = 0; i < 11; i++) {
                        if(buttons & (1 << i)) {
                            switch(i) {
                                case 0:
                                    button_state ^= (1 << TRIANGLE);
                                    break;
                                case 1:
                                    button_state ^= (1 << CIRCLE);
                                    break;
                                case 2:
                                    button_state ^= (1 << R1);
                                    break;
                                case 3:
                                    button_state ^= (1 << CROSS);
                                    break;
                                case 4:
                                    button_state ^= (1 << L1);
                                    break;
                                case 5:
                                    button_state ^= (1 << SQUARE);
                                    break;
                                case 6:
                                    button_state ^= (1 << R2);
                                    break;
                                case 7:
                                    button_state ^= (1 << DPAD_UP);
                                    break;
                                case 8:
                                    button_state ^= (1 << L2);
                                    break;
                                case 9:
                                    button_state ^= (1 << START);
                                    break;
                                case 10:
                                    button_state ^= (1 << SELECT);
                                    break;                              
                            }
                        }
                    }
                    break;
                case 2: // IIDX (QE1)
                    for(uint8_t i = 0; i < 14; i++) {
                        if(buttons & (1 << i)) {
                            switch(i) {
                                case 0:
                                    button_state ^= (1 << SQUARE);
                                    break;
                                case 1:
                                    button_state ^= (1 << L1);
                                    break;
                                case 2:
                                    button_state ^= (1 << CROSS);
                                    break;
                                case 3:
                                    button_state ^= (1 << R1);
                                    break;
                                case 4:
                                    button_state ^= (1 << CIRCLE);
                                    break;
                                case 5:
                                    button_state ^= (1 << L2);
                                    break;
                                case 6:
                                    button_state ^= (1 << DPAD_LEFT);
                                    break;
                                case 7:
                                    button_state ^= (1 << START);
                                    break;
                                case 8:
                                    button_state ^= (1 << SELECT);
                                    break;
                                case 12:
                                    button_state ^= (1 << DPAD_UP);
                                    break;
                                case 13:
                                    button_state ^= (1 << DPAD_DOWN);
                                    break;                              
                            }
                        }
                    }
                    break;
                case 3: // IIDX (QE2)
                    for(uint8_t i = 0; i < 16; i++) {
                        if(buttons & (1 << i)) {
                            switch(i) {
                                case 0:
                                    button_state ^= (1 << SQUARE);
                                    break;
                                case 1:
                                    button_state ^= (1 << L1);
                                    break;
                                case 2:
                                    button_state ^= (1 << CROSS);
                                    break;
                                case 3:
                                    button_state ^= (1 << R1);
                                    break;
                                case 4:
                                    button_state ^= (1 << CIRCLE);
                                    break;
                                case 5:
                                    button_state ^= (1 << L2);
                                    break;
                                case 6:
                                    button_state ^= (1 << DPAD_LEFT);
                                    break;
                                case 7:
                                    button_state ^= (1 << START);
                                    break;
                                case 8:
                                    button_state ^= (1 << SELECT);
                                    break;
                                case 14:
                                    button_state ^= (1 << DPAD_UP);
                                    break;
                                case 15:
                                    button_state ^= (1 << DPAD_DOWN);
                                    break;                              
                            }
                        }
                    }
                    break;
            }
        }

        void irq() {
            if(!enabled) {
                return;
            }

            if(SPI2.reg.SR & (1 << 0)) {
                // Data was received
                uint8_t data = SPI2.reg.DR8;

                bool ack = process_data(data);

                if(ack) {
                    ps_ack.off();
                    for(uint8_t i = 0; i < 15; i++) {
                        asm volatile("nop");
                    }
                    ps_ack.on();
                }
            }

        }
};

SPI_PS spi_ps;

template <>
void interrupt<Interrupt::SPI2>() {
    spi_ps.irq();
}

#undef SELECT
#undef L3
#undef R3
#undef START
#undef DPAD_UP
#undef DPAD_RIGHT
#undef DPAD_DOWN
#undef DPAD_LEFT
#undef L2
#undef R2
#undef L1
#undef R1
#undef TRIANGLE
#undef CIRCLE
#undef CROSS
#undef SQUARE

#endif  // SPI_PS_H