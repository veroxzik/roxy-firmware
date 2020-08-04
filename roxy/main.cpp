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
#include <syscfg/syscfg.h>
#include <interrupt/exti.h>

#include "board_define.h"
#include "report_desc.h"
#include "usb_strings.h"
#include "configloader.h"
#include "config.h"
#include "button_leds.h"
#include "rgb/rgb_config.h"
#include "rgb/ws2812b.h"
#include "rgb/tlc59711.h"
#include "rgb/pixeltypes.h"
#include "rgb/hsv2rgb.h"
#include "rgb/led_breathing.h"
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
Configloader mapping_configloader(0x8020800);

config_t config;
rgb_config_t rgb_config;
mapping_config_t mapping_config;

#if defined(ROXY)
auto dev_desc = device_desc(0x200, 0, 0, 0, 64, 0x16d0, 0x0f8b, 0x0002, 1, 2, 3, 1);
#elif defined(ARCIN)
auto dev_desc = device_desc(0x200, 0, 0, 0, 64, 0x1d50, 0x6080, 0x110, 1, 2, 3, 1);
#endif
auto conf_desc = configuration_desc(2, 1, 0, 0xc0, 0,
	// HID interface.
	interface_desc(0, 0, 1, 0x03, 0x00, 0x00, 0,
		hid_desc(0x111, 0, 1, 0x22, sizeof(report_desc)),
		endpoint_desc(0x81, 0x03, 16, 1)
	),
	// Keyboard interface
	interface_desc(1, 0, 1, 0x03, 0x00, 0x00, 0,
		hid_desc(0x111, 0, 1, 0x22, sizeof(keyboard_report_desc)),
		endpoint_desc(0x82, 0x03, 32, 1)
	)
);

desc_t dev_desc_p = {sizeof(dev_desc), (void*)&dev_desc};
desc_t conf_desc_p = {sizeof(conf_desc), (void*)&conf_desc};
desc_t report_desc_p = {sizeof(report_desc), (void*)&report_desc};
desc_t keyboard_desc_p = {sizeof(keyboard_report_desc), (void*)&keyboard_report_desc};

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

// Defined in "pin_define.h"
extern Pin usb_dm;
extern Pin usb_dp;
#ifdef ARCIN
extern Pin usb_pu;
#endif

extern Pin button_inputs[];
extern Pin button_leds[];

extern Button_Leds button_led_manager;	// In button_leds.h

extern Pin qe1a;
extern Pin qe1b;
extern Pin qe2a;
extern Pin qe2b;

extern Pin led1;
#ifdef ARCIN
extern Pin led2;
#endif

USB_f1 roxy_usb(USB, dev_desc_p, conf_desc_p);
USB_f1 iidx_usb(USB, iidx_dev_desc_p, konami_conf_desc_p);
USB_f1 sdvx_usb(USB, sdvx_dev_desc_p, konami_conf_desc_p);

WS2812B ws2812b;	// In rgb/ws2812b.h

#if defined(ROXY)
template <>
void interrupt<Interrupt::DMA2_Channel1>() {
	ws2812b.irq();
}
#elif defined(ARCIN)
template <>
void interrupt<Interrupt::DMA1_Channel7>() {
	ws2812b.irq();
}
#endif

TLC59711 tlc59711;	// In rgb/tlc59711.h

template <>
void interrupt<Interrupt::DMA2_Channel2>() {
	tlc59711.irq();
}

Sdvx_Leds sdvx_leds;	// In rgb/sdvx_led_strip.h
Led_Breathing breathing_leds;	// In rgb/led_breathing.h

uint32_t last_led_time;

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
			} else if(report->segment == 2) {
				mapping_configloader.write(report->size, report->data);
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
			} else if(config_id == 1) {
				config_report_t rgb_report = {0xc0, 1, sizeof(rgb_config)};

				memcpy(rgb_report.data, &rgb_config, sizeof(rgb_config));
				
				usb.write(0, (uint32_t*)&rgb_report, sizeof(rgb_report));

				config_id = 2;
			} else {
				config_report_t mapping_report = {0xc0, 2, sizeof(mapping_config)};

				memcpy(mapping_report.data, &mapping_config, sizeof(mapping_config));

				usb.write(0, (uint32_t*)&mapping_report, sizeof(mapping_report));

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
			for (int i = 0; i < NUM_BUTTONS; i++) {
				// button_leds[i].set(((report->leds) >> i & 0x1) ^ ((config.flags >> 7) & 0x1));
				button_led_manager.set_led(i, ((report->leds) >> i & 0x1) ^ ((config.flags >> 7) & 0x1));
			}
			
			switch (config.rgb_mode) {
				case 1:
					ws2812b.update(report->r1, report->g1, report->b1);
					break;
				case 2:
					switch (rgb_config.rgb_mode) {
						case 0:
						case 1:
							tlc59711.set_led_8bit(3, report->r1, report->g1, report->b1);
							tlc59711.set_led_8bit(2, report->r2, report->g2, report->b2);
							break;
						case 2:
							tlc59711.set_led_8bit(27, report->r1, report->g1, report->b1);
							tlc59711.set_led_8bit(26, report->r2, report->g2, report->b2);
						break;
					}
					tlc59711.schedule_dma();
					break;
			}
			// button_led_manager.set_pwm(0, report->r1 * 100 / 255);
		
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

class HID_keyboard : public USB_HID {
	public:
		HID_keyboard(USB_generic& usbd, desc_t rdesc) : USB_HID(usbd, rdesc, 1, 2, 64) {}
};

HID_arcin usb_roxy_hid(roxy_usb, report_desc_p);
HID_arcin usb_iidx_hid(iidx_usb, report_desc_p);
HID_arcin usb_sdvx_hid(sdvx_usb, report_desc_p);

USB_strings usb_roxy_strings(roxy_usb, config.label, 0);
USB_strings usb_iidx_strings(iidx_usb, config.label, 1);
USB_strings usb_sdvx_strings(sdvx_usb, config.label, 2);

HID_keyboard usb_keyboard(roxy_usb, keyboard_desc_p);

class Axis {
	public:
		virtual uint32_t get() = 0;
};

class NullAxis : public Axis {
	public:
		virtual uint32_t get() final {
			return 0;
		}
};

NullAxis null_axis;

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
				// Special cases
				if(sens == -127) {
					tim.ARR = (600 * 4) - 1;
				} else if(sens == -126) {
					tim.ARR = (400 * 4) - 1;
				} else if(sens == -125) {
					tim.ARR = (360 * 4) - 1;
				} else {
					tim.ARR = 256 * -sens - 1;
				}
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

// This class inspired heavily by mon's Pocket Voltex, which was adapted from Encoder.h by PRJC
// https://github.com/mon/PocketVoltex
class IntAxis : public Axis {
	private:
		Pin& a, b;
		uint32_t count = 0;
		uint32_t max = 255;
		uint8_t state;

		bool invert = false;

	public:
		IntAxis(Pin &_a, Pin &_b) : a(_a), b(_b) {}

		void enable(bool _invert, int8_t sens) {
			invert = _invert;

			if(sens < 0) {
				if(sens == -127) {
					max = (600 * 4) - 1;
				} else if(sens == -126) {
					max = (400 * 4) - 1;
				} else if(sens == -125) {
					max = (360 * 4) - 1;
				} else {
					max = 256 * -sens - 1;
				}
			} else {
				sens = 256 - 1;
			}

			state = a.get() | (b.get() << 1);
		}

		void updateEncoder() {
			uint8_t newState = a.get() | (b.get() << 1);
			int8_t delta;
			uint8_t tempState = state | (newState << 2);
			state = newState;
			switch (tempState) {
				case 1:
				case 7:
				case 8:
				case 14:
					delta = 1;
					break;
				case 2:
				case 4:
				case 11:
				case 13:
					delta = -1;
					break;
				case 3:
				case 12:
					delta = 2;
					break;
				case 6:
				case 9:
					delta = -2;
					break;
			}
			if(((int16_t)count + delta) < 0) {
				count += max;
			}
			count += delta;
			if(count >= max) {
				count -= max;
			}

		}

		virtual uint32_t get() final {
			return count;
		}
};

IntAxis axis_int(qe1b, qe2b);

template<>
void interrupt<Interrupt::EXTI1>() {
	if(EXTI.PR1 & (1 << 1)) {
		EXTI.PR1 |= (1 << 1);	// Clear flag
		axis_int.updateEncoder();
	}
}

template<>
void interrupt<Interrupt::EXTI9_5>() {
	if(EXTI.PR1 & (1 <<7 )) {
		EXTI.PR1 |= (1 << 7);	// Clear flag
		axis_int.updateEncoder();
	}
}

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

class NKRO_Keyboard {
	private:
		uint8_t nkro_report[32];

	public:
		void reset_keys() {
			for(uint8_t i = 0; i < 32; i++) {
				nkro_report[i] = 0;
			}
		}

		void reset_key(uint8_t keycode) {
			uint8_t bit = keycode % 8;
			uint8_t byte = (keycode / 8) + 1;

			if(keycode >= 240 && keycode <= 247) {
				nkro_report[0] &= ~(1 << bit);
			} else if(byte > 0 && byte <= 31) {
				nkro_report[byte] &= ~(1 << bit);
			}
		}

		void set_key(uint8_t keycode) {
			uint8_t bit = keycode % 8;
			uint8_t byte = (keycode / 8) + 1;

			if(keycode >= 240 && keycode <= 247) {
				nkro_report[0] |= (1 << bit);
			} else if(byte > 0 && byte <= 31) {
				nkro_report[byte] |= (1 << bit);
			}
		}

		uint8_t* get_data() {
			return nkro_report;
		}
};

NKRO_Keyboard nkro;

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
	mapping_configloader.read(sizeof(mapping_config), &mapping_config);
	
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

#ifdef ARCIN
	usb_pu.set_mode(Pin::Output);
	usb_pu.on();
#endif

	uint32_t button_time[12];
	bool last_state[12];
	
	for (int i = 0; i < NUM_BUTTONS; i++) {
		button_inputs[i].set_mode(Pin::Input);
		button_inputs[i].set_pull(Pin::PullUp);
		
		button_leds[i].set_mode(Pin::Output);

		button_time[i] = Time::time();
		last_state[i] = button_inputs[i].get();
	}

	button_led_manager.init(500);
	
	led1.set_mode(Pin::Output);
	if(config.flags & (1 << 3)) {
		led1.on();
	}
#ifdef ARCIN
	led2.set_mode(Pin::Output);
	if(config.flags & (1 << 4)) {
		led2.on();
	}
#endif
	
	Axis* axis[2];
	
	if(config.flags & (1 << 8)) {
		// Setup interrupts on pins PA1 and PA7
		RCC.enable(RCC.SYSCFG);	// Enable SYSCFG
		// No need to set SYSCFG_EXTICR since we're using PA
		EXTI.IMR1 |= (1 << 1) | (1 << 7); 		// Enable interrupts EXT1 and EXT7
		EXTI.RTSR1 |= (1 << 1) | (1 << 7);		// Enable rising edge trigger on EXT1 and EXT7
		EXTI.FTSR1 |= (1 << 1) | (1 << 7);		// Enable falling edge trigger on EXT1 and EXT7
		Interrupt::enable(Interrupt::EXTI1);		// Enable EXTI1 interrupt
		Interrupt::enable(Interrupt::EXTI9_5);	// Enable EXTI5-9 interrupt
		qe1b.set_mode(Pin::Input);
		qe2b.set_mode(Pin::Input);
		qe1b.set_pull(Pin::PullUp);
		qe2b.set_pull(Pin::PullUp);

		axis_int.enable(config.flags & (1 << 1), config.qe_sens[0]);
		
		axis[0] = &axis_int;
		axis[1] = &null_axis;
	} else {
		if(config.flags & (1 << 5)) {
			RCC.enable(RCC.ADC12);
			
			axis_ana1.enable();
			
			axis[0] = &axis_ana1;
			
		} else {
			RCC.enable(RCC.TIM2);
			
			axis_qe1.enable(config.flags & (1 << 1), config.qe_sens[0]);
			
			qe1a.set_af(1);
			qe1b.set_af(1);
			qe1a.set_mode(Pin::AF);
			qe1b.set_mode(Pin::AF);
			qe1a.set_pull(Pin::PullUp);
			qe1b.set_pull(Pin::PullUp);
			
			axis[0] = &axis_qe1;
		}
		
		if(config.flags & (1 << 5)) {
			RCC.enable(RCC.ADC12);
			
			axis_ana2.enable();
			
			axis[1] = &axis_ana2;
			
		} else {
			RCC.enable(RCC.TIM3);
			
			axis_qe2.enable(config.flags & (1 << 2), config.qe_sens[1]);
			
			qe2a.set_af(2);
			qe2b.set_af(2);
			qe2a.set_mode(Pin::AF);
			qe2b.set_mode(Pin::AF);
			qe2a.set_pull(Pin::PullUp);
			qe2b.set_pull(Pin::PullUp);
			
			axis[1] = &axis_qe2;
		}
	}

	uint32_t axis_time[2] = {0, 0};
	
	switch(config.rgb_mode) {
		case 1:
			ws2812b.init();
			break;
		case 2:
			switch(rgb_config.rgb_mode) {
				case 1:
					tlc59711.init(1);
					break;
				case 2:
					tlc59711.init(7);
					break;
			}
			tlc59711.set_brightness(config.rgb_brightness / 2);
			if(rgb_config.rgb_mode == 1) {
				breathing_leds.set_hue(0, rgb_config.led1_hue);
				breathing_leds.set_hue(1, rgb_config.led2_hue);
			}
			if(rgb_config.rgb_mode == 2) {
				sdvx_leds.set_left_hue(rgb_config.led1_hue);
				sdvx_leds.set_right_hue(rgb_config.led2_hue);
			}
			break;
	}
	
	// Number of cycles TT buttons will turn off once no motion is detected
	const int8_t tt_cycles = 50;

	// Number of cycles remaining before the TT buttons can swap polarities
	const int8_t tt_switch_threshold = 20;

	uint8_t last_axis[2] = {0, 0};
	int8_t state_axis[2] = {0, 0};
	int8_t last_axis_state[2] = {0, 0};
	uint32_t qe_count[2] = {0, 0};
	uint32_t axis_buttons[4] = {(1 << 12), (1 << 13), (1 << 14), (1 << 15)};

	while(1) {
		usb->process();
		
#ifdef ARCIN
		uint16_t buttons = 0x7ff;
#else
		uint16_t buttons = 0xfff;
#endif
		for (int i = 0; i < NUM_BUTTONS; i++) {
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
		
		for(int i = 0; i < 2; i++) {
			qe_count[i] = axis[i]->get();
			int8_t delta = qe_count[i] - last_axis[i];

			if(state_axis[i] > -tt_switch_threshold && delta > 1) {
				state_axis[i] = tt_cycles;
				last_axis[i] = qe_count[i];
				axis_time[i] = Time::time();
			} else if(state_axis[i] < tt_switch_threshold && delta < -1) {
				state_axis[i] = -tt_cycles;
				last_axis[i] = qe_count[i];
				axis_time[i] = Time::time();
			} else if(state_axis[i] > 0) {
				state_axis[i]--;
				last_axis[i] = qe_count[i];
			} else if(state_axis[i] < 0) {
				state_axis[i]++;
				last_axis[i] = qe_count[i];
			}

			if (state_axis[i] > 0 && delta > 0) {
				state_axis[i] = tt_cycles;
			}
			if (state_axis[i] < 0 && delta < 0) {
				state_axis[i] = -tt_cycles;
			}

			int8_t state_req = 0;

			if(state_axis[i] > 0) {
				state_req = 1;
				sdvx_leds.set_active(i, true);
			} else if(state_axis[i] < 0) {
				state_req = -1;
				sdvx_leds.set_active(i, false);
			}

			if(last_axis_state[i] != 0) {
				if((Time::time() - axis_time[i]) >= config.axis_debounce_time) {
					if(last_axis_state[i] != state_req) {
						axis_time[i] = Time::time();
					}
					last_axis_state[i] = 0;	// Clear, let below take care of it
				} else if(last_axis_state[i] == -state_req) {
					// Allow the ability to change direction instantly
					axis_time[i] = Time::time();
					last_axis_state[i] = 0;	// Clear, let below take care of it
				}
			}

			if(last_axis_state[i] == 0 && state_req > 0) {
				axis_time[i] = Time::time();
				last_axis_state[i] = 1;
			} else if(last_axis_state[i] == 0 && state_req < 0) {
				axis_time[i] = Time::time();
				last_axis_state[i] = -1;
			}

			if(last_axis_state[i] == 1 && config.flags & (1 << 6)) {
				buttons |= axis_buttons[2 * i];
			} else if(last_axis_state[i] == -1 && config.flags & (1 << 6)) {
				buttons |= axis_buttons[2 * i + 1];
			}

			if(config.qe_sens[i] == -127) {
				qe_count[i] *= (256.0f / (600.0f * 4.0f));
			} else if(config.qe_sens[i] == -126) {
				qe_count[i] *= (256.0f / (400.0f * 4.0f));
			} else if(config.qe_sens[i] == -125) {
				qe_count[i] *= (256.0f / (360.0f * 4.0f));
			} else if(config.qe_sens[i] < 0) {
				qe_count[i] /= -config.qe_sens[i];
			} else if(config.qe_sens[i] > 0) {
				qe_count[i] *= config.qe_sens[i];
			}
			qe_count[i] -= 128;
		}
		
		input_report_t report = {1, buttons, uint8_t(qe_count[0]), uint8_t(qe_count[1])};
			
		// Joystick
		if(usb->ep_ready(1) && (config.output_mode == 0 || config.output_mode == 2)) {
			usb->write(1, (uint32_t*)&report, sizeof(report));
		}

		// Keyboard
		if(usb->ep_ready(2) && (config.output_mode == 1 || config.output_mode == 2)) {
			for (int i = 0; i < NUM_BUTTONS; i++) {
				if (buttons & (1 << i) && mapping_config.button_kb_map[i] > 0) {
					nkro.set_key(mapping_config.button_kb_map[i]);
				} else {
					nkro.reset_key(mapping_config.button_kb_map[i]);
				}
			}
			for (int i = 0; i < 2; i++) {
				if (last_axis_state[i] == 1 && mapping_config.axes_kb_map[2 * i] > 0) {
					nkro.set_key(mapping_config.axes_kb_map[2 * i]);
				} else {
					nkro.reset_key(mapping_config.axes_kb_map[2 * i]);
				}
				if (last_axis_state[i] == -1 && mapping_config.axes_kb_map[2 * i + 1] > 0) {
					nkro.set_key(mapping_config.axes_kb_map[2 * i + 1]);
				} else {
					nkro.reset_key(mapping_config.axes_kb_map[2 * i + 1]);
				}
			}
			usb->write(2, (uint32_t*)nkro.get_data(), 32);
		}

		if(Time::time() - last_led_time > 1000) {
			for (int i = 0; i < NUM_BUTTONS; i++) {
				//button_leds[i].set((buttons >> i & 0x1) ^ ((config.flags >> 7) & 0x1));
				button_led_manager.set_led(i, (buttons >> i & 0x1) ^ ((config.flags >> 7) & 0x1));
			}

			// Breathing LEDs, only TLC59711 supported
			if(config.rgb_mode == 2 && rgb_config.rgb_mode == 1) {
				if(breathing_leds.update(state_axis[0], state_axis[1])) {
					CRGB temp = breathing_leds.get_led(0);
					tlc59711.set_led_8bit(3, temp.r, temp.g, temp.b);
					temp = breathing_leds.get_led(1);
					tlc59711.set_led_8bit(2, temp.r, temp.g, temp.b);
					tlc59711.schedule_dma();
				}
			}
		}	

		// SDVX LED strips, only TLC59711 supported
		if(config.rgb_mode == 2 && rgb_config.rgb_mode == 2) {
			if(sdvx_leds.update()) {
				for( int i = 0; i < 24; i++) {
					CRGB temp = sdvx_leds.get_led(i);
					tlc59711.set_led_8bit(i, temp.b, temp.r, temp.g);
				}
				tlc59711.schedule_dma();
			}
		}	
	}
}
