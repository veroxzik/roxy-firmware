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
#include "rgb/rgb_config.h"
#include "rgb/ws2812b.h"
#include "rgb/tlc59711.h"
#include "rgb/pixeltypes.h"
#include "rgb/hsv2rgb.h"
#include "rgb/sdvx_led_strip.h"

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
Configloader rgb_configloader(0x8020000);

config_t config;
rgb_config_t rgb_config;

auto dev_desc = device_desc(0x200, 0, 0, 0, 64, 0x16d0, 0x0f8b, 0x0002, 1, 2, 3, 1);
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

auto iidx_dev_desc = device_desc(0x200, 0, 0, 0, 64, 0x1ccf, 0x8048, 0x100, 1, 2, 3, 1);
auto konami_conf_desc = configuration_desc(1, 1, 0, 0xc0, 0,
	// HID interface.
	interface_desc(0, 0, 1, 0x03, 0x00, 0x00, 0,
		hid_desc(0x111, 0, 1, 0x22, sizeof(report_desc)),
		endpoint_desc(0x81, 0x03, 16, 4)
	)
);

desc_t iidx_dev_desc_p = {sizeof(iidx_dev_desc), (void*)&iidx_dev_desc};
desc_t konami_conf_desc_p = {sizeof(konami_conf_desc), (void*)&konami_conf_desc};

auto sdvx_dev_desc = device_desc(0x200, 0, 0, 0, 64, 0x1ccf, 0x101c, 0x100, 1, 2, 3, 1);
desc_t sdvx_dev_desc_p = {sizeof(sdvx_dev_desc), (void*)&sdvx_dev_desc};

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

Pin rgb_sck = GPIOC[10];
Pin rgb_mosi = GPIOC[12];

static Pin led1 = GPIOC[4];

USB_f1 roxy_usb(USB, dev_desc_p, conf_desc_p);
USB_f1 iidx_usb(USB, iidx_dev_desc_p, konami_conf_desc_p);
USB_f1 sdvx_usb(USB, sdvx_dev_desc_p, konami_conf_desc_p);

WS2812B ws2812b;	// In rgb/ws2812b.h

template <>
void interrupt<Interrupt::DMA2_Channel1>() {
	ws2812b.irq();
}

TLC59711 tlc59711;	// In rgb/tlc59711.h

template <>
void interrupt<Interrupt::DMA2_Channel2>() {
	tlc59711.irq();
}

Sdvx_Leds sdvx_leds;	// In rgb/sdvx_led_strip.h

CHSV rgb_led1, rgb_led2;
uint32_t last_led_time;
uint32_t last_rgb_time;

class HID_arcin : public USB_HID {
	private:
		uint8_t config_id = 0;

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
			if(report->segment == 0) {
				configloader.write(report->size, report->data);
			} else if(report->segment == 1) {
				rgb_configloader.write(report->size, report->data);
			} else {
				return false;
			}
			
			return true;
		}
		
		bool get_feature_config() {
			if(config_id == 0) {
				config_report_t report = {0xc0, 0, sizeof(config)};
			
				memcpy(report.data, &config, sizeof(config));
				
				usb.write(0, (uint32_t*)&report, sizeof(report));

				config_id = 1;
			} else {
				config_report_t rgb_report = {0xc0, 1, sizeof(rgb_config)};

				memcpy(rgb_report.data, &rgb_config, sizeof(rgb_config));
				
				usb.write(0, (uint32_t*)&rgb_report, sizeof(rgb_report));

				config_id = 0;
			}
			
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
					ws2812b.update(report->r1, report->g1, report->b1);
					break;
				case 2:
					tlc59711.set_led_8bit(0, report->r1, report->g1, report->b1);
					tlc59711.set_led_8bit(1, report->r2, report->g2, report->b2);
					tlc59711.schedule_dma();
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

HID_arcin usb_roxy_hid(roxy_usb, report_desc_p);
HID_arcin usb_iidx_hid(iidx_usb, report_desc_p);
HID_arcin usb_sdvx_hid(sdvx_usb, report_desc_p);

USB_strings usb_roxy_strings(roxy_usb, config.label, 0);
USB_strings usb_iidx_strings(iidx_usb, config.label, 1);
USB_strings usb_sdvx_strings(sdvx_usb, config.label, 2);

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
	rgb_configloader.read(sizeof(rgb_config), &rgb_config);
	
	RCC.enable(RCC.GPIOA);
	RCC.enable(RCC.GPIOB);
	RCC.enable(RCC.GPIOC);
	
	usb_dm.set_mode(Pin::AF);
	usb_dm.set_af(14);
	usb_dp.set_mode(Pin::AF);
	usb_dp.set_af(14);

	RCC.enable(RCC.USB);
	
	USB_f1* usb;

	switch (config.controller_emulation)
	{
		case 1:
			usb = &iidx_usb;
			break;
		case 2:
			usb = &sdvx_usb;
			break;
		default:
			usb = &roxy_usb;
			break;
	}

	usb->init();

	uint32_t button_time[12];
	bool last_state[12];
	
	for (int i = 0; i < 12; i++) {
		button_inputs[i].set_mode(Pin::Input);
		button_inputs[i].set_pull(Pin::PullUp);
		
		button_leds[i].set_mode(Pin::Output);

		button_time[i] = Time::time();
		last_state[i] = button_inputs[i].get();
	}
	
	led1.set_mode(Pin::Output);
	led1.on();
	
	Axis* axis_1;
	
	if(config.flags & (1 << 5)) {
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
	
	if(config.flags & (1 << 5)) {
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
			ws2812b.init();
			break;
		case 2:
			tlc59711.init(7);
			tlc59711.set_brightness(config.rgb_brightness / 2);
			if(rgb_config.rgb_mode == 2) {
				sdvx_leds.set_left_hue(rgb_config.led1_hue);
				sdvx_leds.set_right_hue(rgb_config.led2_hue);
			}
			break;
	}
	
	uint8_t last_x = 0;
	uint8_t last_y = 0;
	
	int8_t state_x = 0;
	int8_t state_y = 0;

	// Number of cycles TT buttons will turn off once no motion is detected
	const int8_t tt_cycles = 50;

	// Number of cycles remaining before the TT buttons can swap polarities
	const int8_t tt_switch_threshold = 20;

	// RGB variables
	uint8_t rgb_update_period = 20;		// ms, Period of updating RGB
	uint16_t breathing_period = 2000;	// ms, Period of low breathing
	uint8_t breathing_brightness_low = 80;		// Max 255
	uint8_t breathing_brightness_high = 180;	// Max 255
	uint8_t led_steps_breathing = (uint8_t)((float)(breathing_brightness_high - breathing_brightness_low) / ((float)breathing_period / (float)rgb_update_period));
	uint8_t brightness1 = breathing_brightness_low, brightness2 = breathing_brightness_low;
	uint8_t led1_mode = 0, led2_mode = 0;

	uint16_t flashing_period = 200;		// ms, Period of flashing when a knob is turned
	uint8_t	flashing_brightness_low = 160;
	uint8_t flashing_brightness_high = 255;
	uint8_t led_steps_flashing = (uint8_t)((float)(flashing_brightness_high - flashing_brightness_low) / ((float)flashing_period / (float)rgb_update_period));
	
	while(1) {
		usb->process();
		
		uint16_t buttons = 0xfff;
		for (int i = 0; i < 12; i++) {
			bool read = button_inputs[i].get();
			if((Time::time() - button_time[i]) >= config.debounce_time)
			{
				if(last_state[i] != read)
					button_time[i] = Time::time();

				buttons ^= read << i;
				last_state[i] = read;
			}
		}
		
		if(do_reset_bootloader) {
			Time::sleep(10);
			reset_bootloader();
		}
		
		if(do_reset) {
			Time::sleep(10);
			reset();
		}	
		
		uint32_t qe1_count = axis_1->get();
		uint32_t qe2_count = axis_2->get();
		
		int8_t rx = qe1_count - last_x;
		int8_t ry = qe2_count - last_y;
		
		if(state_x > -tt_switch_threshold && rx > 1) {
			state_x = tt_cycles;
			last_x = qe1_count;
		} else if(state_x < tt_switch_threshold && rx < -1) {
			state_x = -tt_cycles;
			last_x = qe1_count;
		} else if(state_x > 0) {
			state_x--;
			last_x = qe1_count;
		} else if(state_x < 0) {
			state_x++;
			last_x = qe1_count;
		}

		if (state_x > 0 && rx > 0) {
			state_x = tt_cycles;
		}
		if (state_x < 0 && rx < 0) {
			state_x = -tt_cycles;
		}
		
		if(state_y > -tt_switch_threshold && ry > 1) {
			state_y = tt_cycles;
			last_y = qe2_count;
		} else if(state_y < tt_switch_threshold && ry < -1) {
			state_y = -tt_cycles;
			last_y = qe2_count;
		} else if(state_y > 0) {
			state_y--;
			last_y = qe2_count;
		} else if(state_y < 0) {
			state_y++;
			last_y = qe2_count;
		}

		if (state_y > 0 && ry > 0) {
			state_y = tt_cycles;
		}
		if (state_y < 0 && ry < 0) {
			state_y = -tt_cycles;
		}
		
		if(state_x > 0) {
			if(config.flags & (1 << 6)) {
				buttons |= 1 << 12;
			}
			sdvx_leds.set_left_active(true);
		} else if(state_x < 0) {
			if(config.flags & (1 << 6)) {
				buttons |= 1 << 13;
			}
			sdvx_leds.set_left_active(false);
		}
		
		if(state_y > 0) {
			if(config.flags & (1 << 6)) {
				buttons |= 1 << 14;
			}
			sdvx_leds.set_right_active(true);
		} else if(state_y < 0) {
			if(config.flags & (1 << 6)) {
				buttons |= 1 << 15;
			}
			sdvx_leds.set_right_active(false);
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
			
		if(usb->ep_ready(1)) {
			usb->write(1, (uint32_t*)&report, sizeof(report));
		}

		if(Time::time() - last_led_time > 1000) {
			for (int i = 0; i < 12; i++) {
				button_leds[i].set(buttons >> i & 0x1);
			}

			if(Time::time() - last_rgb_time > rgb_update_period) {
				// If TLC59711
				if(config.rgb_mode == 2) {
					// If knobs react to encoders
					if(rgb_config.rgb_mode == 1) {
						if((state_x > 1 || state_x < -1) && (led1_mode == 0 || led1_mode == 1)) {
							led1_mode = 2;
							brightness1 = flashing_brightness_high - led_steps_flashing;
						} else if(state_x == 0 && (led1_mode == 2 || led1_mode == 3)) {
							led1_mode = 1;
						}
						if((state_y > 1 || state_y < -1) && (led2_mode == 0 || led2_mode == 1)) {
							led2_mode = 2;
							brightness2 = flashing_brightness_high - led_steps_flashing;
						} else if(state_y == 0 && (led2_mode == 2 || led2_mode == 3)) {
							led2_mode = 1;
						}
						switch(led1_mode){
							case 0:
								brightness1 += led_steps_breathing;
								if(brightness1 >= breathing_brightness_high) {
									led1_mode = 1;
								}
								break;
							case 1:
								brightness1 -= led_steps_breathing;
								if(brightness1 <= breathing_brightness_low) {
									led1_mode = 0;
								}
								break;
							case 2:
								brightness1 += led_steps_flashing;
								if(brightness1 >= flashing_brightness_high) {
									led1_mode = 3;
								}
								break;
							case 3:
								brightness1 -= led_steps_flashing;
								if(brightness1 <= flashing_brightness_low) {
									led1_mode = 0;
								}
								break;
						}
						switch(led2_mode){
							case 0:
								brightness2 += led_steps_breathing;
								if(brightness2 >= breathing_brightness_high) {
									led2_mode = 1;
								}
								break;
							case 1:
								brightness2 -= led_steps_breathing;
								if(brightness2 <= breathing_brightness_low) {
									led2_mode = 0;
								}
								break;
							case 2:
								brightness2 += led_steps_flashing;
								if(brightness2 >= flashing_brightness_high) {
									led2_mode = 3;
								}
								break;
							case 3:
								brightness2 -= led_steps_flashing;
								if(brightness2 <= flashing_brightness_low) {
									led2_mode = 0;
								}
								break;
						}
						rgb_led1 = CHSV(rgb_config.led1_hue, 255, brightness1);
						rgb_led2 = CHSV(rgb_config.led2_hue, 255, brightness2);
						CRGB rgb_temp1, rgb_temp2;
						hsv2rgb_rainbow(rgb_led1, rgb_temp1);
						tlc59711.set_led_8bit(0, rgb_temp1.r, rgb_temp1.g, rgb_temp1.b);
						hsv2rgb_rainbow(rgb_led2, rgb_temp2);
						tlc59711.set_led_8bit(1, rgb_temp2.r, rgb_temp2.g, rgb_temp2.b);
						tlc59711.schedule_dma();
						last_rgb_time = Time::time();
					}
				}
			}

			// SDVX LED strips, only TLC59711 supported
			if(config.rgb_mode == 2 && rgb_config.rgb_mode == 2) {
				if(sdvx_leds.update()) {
					for( int i = 0; i < 24; i++) {
						CRGB temp = sdvx_leds.get_led(i);
						tlc59711.set_led_8bit(i, temp.r, temp.b, temp.g);
					}
					tlc59711.schedule_dma();
				}
			}
		}		
	}
}
