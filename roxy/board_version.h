#ifndef VERSION_DET_H
#define VERSION_DET_H

#include <rcc/rcc.h>
#include <gpio/gpio.h>
#include <adc/adc_f3.h>
#include <os/time.h>

#include "board_define.h"

#define NOM_V2_0 2048
#define THRESH 100

extern Pin ver_check;

class Board_Version {
    private:
        uint16_t raw_voltage;

    public:
        enum Version {
            UNDEF,
            V1_1,
            V2_0
        };

        Version board = Board_Version::UNDEF;

        void get_version() {
#ifdef ROXY
            // Setup pins
            ver_check.set_mode(Pin::Analog);
            // Setup ADC
            RCC.enable(RCC.ADC12);
            // Disable ADC
            ADC1.CR &= ~(1 << 0);
            // Turn on ADC regulator
            ADC1.CR = 0 << 28;
            ADC1.CR = 1 << 28;
            Time::sleep(2);
            // Ignore calibration
            // Enable ADC
            ADC1.CR |= (1 << 0);
            // Select ADC Channel 6
            ADC1.SQR1 = (6 << 6);
            // Wait 5ms
            Time::sleep(5);
            // Read five times and average the reading
            raw_voltage = 0;
            for(uint8_t i = 0; i < 5; i++) {
                ADC1.CR |= (1 << 2);
                while(ADC1.CR & (1 << 2));
                raw_voltage += ADC1.DR;
            }
            raw_voltage /= 5;

            // Determine version based on result
            if(raw_voltage >= (NOM_V2_0 - THRESH) && raw_voltage <= (NOM_V2_0 + THRESH)) {
                board = Board_Version::V2_0;
            } else {
                board = Board_Version::V1_1;
            }
#endif
        }

};



#endif