#include <rcc/rcc.h>
#include <gpio/gpio.h>
#include <interrupt/interrupt.h>
#include <timer/timer.h>
#include <dma/dma.h>
#include <adc/adc_f3.h>
#include <os/time.h>
#include <usb/usb.h>
#include <usb/descriptor.h>
#include <spi/spi.h>

#include "report_desc.h"
#include "usb_strings.h"
#include "configloader.h"
#include "config.h"

static uint32_t& reset_reason = *(uint32_t*)0x10000000;

static bool do_reset_bootloader;
static bool do_reset;

void reset() {
	SCB.AIRCR = (0x5fa << 16) | (1 << 2); // SYSRESETREQ
}

void reset_bootloader() {
	reset_reason = 0xb007;
	reset();
}

Configloader configloader(0x801f800);

config_t config;

auto dev_desc = device_desc(0x200, 0, 0, 0, 64, 0x1d50, 0x6080, 0x110, 1, 2, 3, 1);
auto conf_desc = configuration_desc(1, 1, 0, 0xc0, 0,
	// HID interface.
	interface_desc(0, 0, 1, 0x03, 0x00, 0x00, 0,
		hid_desc(0x111, 0, 1, 0x22, sizeof(report_desc)),
		endpoint_desc(0x81, 0x03, 16, 1)
	)
);

desc_t dev_desc_p = {sizeof(dev_desc), (void*)&dev_desc};
desc_t conf_desc_p = {sizeof(conf_desc), (void*)&conf_desc};
desc_t report_desc_p = {sizeof(report_desc), (void*)&report_desc};

static Pin usb_dm = GPIOA[11];
static Pin usb_dp = GPIOA[12];

static Pin button_inputs[] = {
  GPIOB[4], GPIOB[5], GPIOB[6], GPIOB[9], GPIOC[14], GPIOC[5],
  GPIOB[1], GPIOB[10], GPIOC[7], GPIOC[9], GPIOA[9], GPIOB[7]};
static Pin button_leds[] = {
  GPIOA[15], GPIOC[11], GPIOB[3], GPIOC[13], GPIOC[15], GPIOB[0],
  GPIOB[2], GPIOC[6], GPIOC[8], GPIOA[8], GPIOA[10], GPIOB[8]};

static Pin qe1a = GPIOA[0];
static Pin qe1b = GPIOA[1];
static Pin qe2a = GPIOA[6];
static Pin qe2b = GPIOA[7];

static Pin rgb_sck = GPIOC[10];
static Pin rgb_mosi = GPIOC[12];

static Pin led1 = GPIOC[4];

USB_f1 usb(USB, dev_desc_p, conf_desc_p);

class WS2812B {
	private:
		uint8_t dmabuf[25];
		volatile uint32_t cnt;
		volatile bool busy;
		
		void schedule_dma() {
			cnt--;
			
			DMA2.reg.C[0].NDTR = 25;
			DMA2.reg.C[0].MAR = (uint32_t)&dmabuf;
			DMA2.reg.C[0].PAR = (uint32_t)&TIM8.CCR3;
			DMA2.reg.C[0].CR = (0 << 10) | (1 << 8) | (1 << 7) | (0 << 6) | (1 << 4) | (1 << 1) | (1 << 0);
		}
		
		void set_color(uint8_t r, uint8_t g, uint8_t b) {
			uint32_t n = 0;
			
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
			RCC.enable(RCC.TIM8);
			RCC.enable(RCC.DMA2);
			
			Interrupt::enable(Interrupt::DMA2_Channel1);
			
			TIM8.ARR = (72000000 / 800000) - 1; // period = 90, 0 = 29, 1 = 58
			TIM8.CCR3 = 0;
			
			TIM8.CCMR2 = (6 << 4) | (1 << 3);
			TIM8.CCER = 1 << 10;	// CC3 complementary output enable
			TIM8.DIER = 1 << 8;		// Update DMA request enable
			TIM8.BDTR = 1 << 15;	// Main output enable
			
			rgb_mosi.set_af(4);
			rgb_mosi.set_mode(Pin::AF);
			rgb_mosi.set_pull(Pin::PullNone);
			
			TIM8.CR1 = 1 << 0;
			
			Time::sleep(1);
			
			update(0, 0, 0);
		}
		
		void update(uint8_t r, uint8_t g, uint8_t b) {
			set_color(r, g, b);
			
			cnt = 15;
			busy = true;
			
			schedule_dma();
		}
		
		void irq() {
			DMA2.reg.C[0].CR = 0;
			DMA2.reg.IFCR = 1 << 0;
			
			if(cnt) {
				schedule_dma();
			} else {
				busy = false;
			}
		}
};

WS2812B ws2812b;

template <>
void interrupt<Interrupt::DMA2_Channel1>() {
	ws2812b.irq();
}

// This class largely based on the Adafruit_TLC59711 library
class TLC59711 {
	private:
		uint8_t numdrivers;
		uint8_t bcr, bcg, bcb;	// Brightness
		uint16_t pwmbuffer[12 * 1];	// Array for all channels

		uint8_t count = 0;

	public:
		void init(uint8_t n) {
			RCC.enable(RCC.SPI3);

			// Initialize variables
			numdrivers = 1;	// TODO: fix hardcode
			bcr = bcg = bcb = 0x7F;	// Max brightness

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

			SPI3.reg.CR2 = (7 < 8);	//  DS = 8bit
			// CR1: LSBFIRST = 0 (default, MSBFIRST),  CPOL = 0 (default), CPHA = 0 (default)
			SPI3.reg.CR1 = (1 << 9) | (1 << 8 ) | (5 << 3) | (1 << 2);	
			// SSM = 1, SSI = 1, BR = 5 (FpCLK/64), MSTR = 1

			// CRC = default (not used anyway)
			SPI3.reg.CRCPR = 7;

			// Enable (SPE = 1)
			SPI3.reg.CR1 |= (1 << 6);
		}

		void write() {

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

			for (uint8_t n = 0; n < numdrivers; n++) {

				SPI3.transfer16(command >> 16);
				SPI3.transfer16(command);

				// 12 channels per TLC59711
				for (int8_t c = 11; c >= 0; c--) {
					SPI3.transfer16(pwmbuffer[n * 12 + c]);
				}
			}
		}

		void set_pwm(uint8_t chan, uint16_t pwm) {
			if (chan > 12 * numdrivers)
				return;
			pwmbuffer[chan] = pwm;
		}

		void set_led(uint8_t lednum, uint16_t r, uint16_t g, uint16_t b) {
			set_pwm(lednum * 3, r);
			set_pwm(lednum * 3 + 1, g);
			set_pwm(lednum * 3 + 2, b);
		}

		void set_led_8bit(uint8_t lednum, uint8_t r, uint8_t g, uint8_t b) {
			set_pwm(lednum * 3, (uint16_t)r * 256);
			set_pwm(lednum * 3 + 1, (uint16_t)g * 256);
			set_pwm(lednum * 3 + 2, (uint16_t)b * 256);
		}

		void set_brightness(uint8_t r, uint8_t g, uint8_t b) {
			// BC valid range 0-127
			bcr = r > 127 ? 127 : (r < 0 ? 0 : r);
			bcg = g > 127 ? 127 : (g < 0 ? 0 : g);
			bcb = b > 127 ? 127 : (b < 0 ? 0 : b);
		}

		void set_brightness(uint8_t brightness) {
			set_brightness(brightness, brightness, brightness);
		}
};

TLC59711 tlc59711;

uint32_t last_led_time;
bool push_rgb = false;

class HID_arcin : public USB_HID {
	private:
		bool set_feature_bootloader(bootloader_report_t* report) {
			switch(report->func) {
				case 0:
					return true;
				
				case 0x10: // Reset to bootloader
					do_reset_bootloader = true;
					return true;
				
				case 0x20: // Reset to runtime
					do_reset = true;
					return true;
				
				default:
					return false;
			}
		}
		
		bool set_feature_config(config_report_t* report) {
			if(report->segment != 0) {
				return false;
			}
			
			configloader.write(report->size, report->data);
			
			return true;
		}
		
		bool get_feature_config() {
			config_report_t report = {0xc0, 0, sizeof(config)};
			
			memcpy(report.data, &config, sizeof(config));
			
			usb.write(0, (uint32_t*)&report, sizeof(report));
			
			return true;
		}
	
	public:
		HID_arcin(USB_generic& usbd, desc_t rdesc) : USB_HID(usbd, rdesc, 0, 1, 64) {}
	
	protected:
		virtual bool set_output_report(uint32_t* buf, uint32_t len) {
			if(len != sizeof(output_report_t)) {
				return false;
			}
			
			output_report_t* report = (output_report_t*)buf;
			
			last_led_time = Time::time();
			for (int i = 0; i < 12; i++) {
				button_leds[i].set((report->leds) >> i & 0x1);
			}
			
			switch (config.rgb_mode) {
				case 1:
					tlc59711.set_led_8bit(0, report->r1, report->g1, report->b1);
					tlc59711.set_led_8bit(1, report->r2, report->g2, report->b2);
					push_rgb = true;
					break;
				case 2:
					ws2812b.update(report->r1, report->g1, report->b1);
					break;
			}
		
			return true;
		}
		
		virtual bool set_feature_report(uint32_t* buf, uint32_t len) {
			switch(*buf & 0xff) {
				case 0xb0:
					if(len != sizeof(bootloader_report_t)) {
						return false;
					}
					
					return set_feature_bootloader((bootloader_report_t*)buf);
				
				case 0xc0:
					if(len != sizeof(config_report_t)) {
						return false;
					}
					
					return set_feature_config((config_report_t*)buf);
				
				default:
					return false;
			}
		}
		
		virtual bool get_feature_report(uint8_t report_id) {
			switch(report_id) {
				case 0xc0:
					return get_feature_config();
				
				default:
					return false;
			}
		}
};

HID_arcin usb_hid(usb, report_desc_p);

USB_strings usb_strings(usb, config.label);

class Axis {
	public:
		virtual uint32_t get() = 0;
};

class QEAxis : public Axis {
	private:
		TIM_t& tim;
	
	public:
		QEAxis(TIM_t& t) : tim(t) {}
		
		void enable(bool invert, int8_t sens) {
			if(!invert) {
				tim.CCER = 1 << 1;
			}
			
			tim.CCMR1 = (1 << 8) | (1 << 0);
			tim.SMCR = 3;
			tim.CR1 = 1;
			
			if(sens < 0) {
				tim.ARR = 256 * -sens - 1;
			} else {
				tim.ARR = 256 - 1;
			}
		}
		
		virtual uint32_t get() final {
			return tim.CNT;
		}
};

QEAxis axis_qe1(TIM2);
QEAxis axis_qe2(TIM3);

class AnalogAxis : public Axis {
	private:
		ADC_t& adc;
		uint32_t ch;
	
	public:
		AnalogAxis(ADC_t& a, uint32_t c) : adc(a), ch(c) {}
		
		void enable() {
			// Turn on ADC regulator.
			adc.CR = 0 << 28;
			adc.CR = 1 << 28;
			Time::sleep(2);
			
			// Calibrate ADC.
			// TODO
			
			// Configure continous capture on one channel.
			adc.CFGR = (1 << 13) | (1 << 12) | (1 << 5); // CONT, OVRMOD, ALIGN
			adc.SQR1 = (ch << 6);
			adc.SMPR1 = (7 << (ch * 3)); // 72 MHz / 64 / 614 = apx. 1.8 kHz
			
			// Enable ADC.
			adc.CR |= 1 << 0; // ADEN
			while(!(adc.ISR & (1 << 0))); // ADRDY
			adc.ISR = (1 << 0); // ADRDY
			
			// Start conversion.
			adc.CR |= 1 << 2; // ADSTART
		}
		
		virtual uint32_t get() final {
			return adc.DR >> 8;
		}
};

AnalogAxis axis_ana1(ADC1, 2);
AnalogAxis axis_ana2(ADC2, 4);

int main() {
	rcc_init();
	
	// Set ADC12PRES to /64.
	RCC.CFGR2 |= (0x19 << 4);
	
	// Initialize system timer.
	STK.LOAD = 72000000 / 8 / 1000; // 1000 Hz.
	STK.CTRL = 0x03;
	
	// Load config.
	configloader.read(sizeof(config), &config);
	
	RCC.enable(RCC.GPIOA);
	RCC.enable(RCC.GPIOB);
	RCC.enable(RCC.GPIOC);
	
	usb_dm.set_mode(Pin::AF);
	usb_dm.set_af(14);
	usb_dp.set_mode(Pin::AF);
	usb_dp.set_af(14);
	
	RCC.enable(RCC.USB);
	
	usb.init();
	
	for (int i = 0; i < 12; i++) {
		button_inputs[i].set_mode(Pin::Input);
		button_inputs[i].set_pull(Pin::PullUp);
		
		button_leds[i].set_mode(Pin::Output);
	}
	
	led1.set_mode(Pin::Output);
	led1.on();
	
	Axis* axis_1;
	
	if(0) {
		RCC.enable(RCC.ADC12);
		
		axis_ana1.enable();
		
		axis_1 = &axis_ana1;
		
	} else {
		RCC.enable(RCC.TIM2);
		
		axis_qe1.enable(config.flags & (1 << 1), config.qe1_sens);
		
		qe1a.set_af(1);
		qe1b.set_af(1);
		qe1a.set_mode(Pin::AF);
		qe1b.set_mode(Pin::AF);
		qe1a.set_pull(Pin::PullUp);
		qe1b.set_pull(Pin::PullUp);
		
		axis_1 = &axis_qe1;
	}
	
	Axis* axis_2;
	
	if(0) {
		RCC.enable(RCC.ADC12);
		
		axis_ana2.enable();
		
		axis_2 = &axis_ana2;
		
	} else {
		RCC.enable(RCC.TIM3);
		
		axis_qe2.enable(config.flags & (1 << 2), config.qe2_sens);
		
		qe2a.set_af(2);
		qe2b.set_af(2);
		qe2a.set_mode(Pin::AF);
		qe2b.set_mode(Pin::AF);
		qe2a.set_pull(Pin::PullUp);
		qe2b.set_pull(Pin::PullUp);
		
		axis_2 = &axis_qe2;
	}
	
	switch(config.rgb_mode) {
		case 1:
			tlc59711.init(1);
			tlc59711.set_brightness(config.rgb_brightness / 2);
			break;
		case 2:
			ws2812b.init();
			break;
	}
	
	uint8_t last_x = 0;
	uint8_t last_y = 0;
	
	int8_t state_x = 0;
	int8_t state_y = 0;
	
	while(1) {
		usb.process();
		
		uint16_t buttons = 0xfff;
		for (int i = 0; i < 12; i++) {
			buttons ^= button_inputs[i].get() << i;
		}
		
		if(do_reset_bootloader) {
			Time::sleep(10);
			reset_bootloader();
		}
		
		if(do_reset) {
			Time::sleep(10);
			reset();
		}
		
		if(Time::time() - last_led_time > 1000) {
			for (int i = 0; i < 12; i++) {
				button_leds[i].set(buttons >> i & 0x1);
			}
		}

		if (push_rgb) {
			if(config.rgb_mode == 1)
				tlc59711.write();
			push_rgb = false;
		}
		
		if(usb.ep_ready(1)) {
			uint32_t qe1_count = axis_1->get();
			uint32_t qe2_count = axis_2->get();
			
			int8_t rx = qe1_count - last_x;
			int8_t ry = qe2_count - last_y;
			
			if(rx > 1) {
				state_x = 100;
				last_x = qe1_count;
			} else if(rx < -1) {
				state_x = -100;
				last_x = qe1_count;
			} else if(state_x > 0) {
				state_x--;
				last_x = qe1_count;
			} else if(state_x < 0) {
				state_x++;
				last_x = qe1_count;
			}
			
			if(ry > 1) {
				state_y = 100;
				last_y = qe2_count;
			} else if(ry < -1) {
				state_y = -100;
				last_y = qe2_count;
			} else if(state_y > 0) {
				state_y--;
				last_y = qe2_count;
			} else if(state_y < 0) {
				state_y++;
				last_y = qe2_count;
			}
			
			if(state_x > 0) {
				buttons |= 1 << 12;
			} else if(state_x < 0) {
				buttons |= 1 << 13;
			}
			
			if(state_y > 0) {
				buttons |= 1 << 14;
			} else if(state_y < 0) {
				buttons |= 1 << 15;
			}
			
			if(config.qe1_sens < 0) {
				qe1_count /= -config.qe1_sens;
			} else if(config.qe1_sens > 0) {
				qe1_count *= config.qe1_sens;
			}
			
			if(config.qe2_sens < 0) {
				qe2_count /= -config.qe2_sens;
			} else if(config.qe2_sens > 0) {
				qe2_count *= config.qe2_sens;
			}
			
			input_report_t report = {1, buttons, uint8_t(qe1_count), uint8_t(qe2_count)};
			
			usb.write(1, (uint32_t*)&report, sizeof(report));
		}
	}
}
