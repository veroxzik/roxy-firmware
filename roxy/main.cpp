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
#include "board_version.h"
#include "report_desc.h"
#include "usb_strings.h"
#include "configloader.h"
#include "config.h"
#include "button_leds.h"
#include "button_manager.h"
#include "axis.h"
#include "hid_arcin.h"
#include "nkro_keyboard.h"
#include "spi_ps.h"

#include "rgb/rgb_config.h"
#include "rgb/ws2812b_timer.h"
#include "rgb/ws2812b_spi.h"
#include "rgb/tlc59711.h"
#include "rgb/tlc5973.h"
#include "rgb/pixeltypes.h"
#include "rgb/hsv2rgb.h"
#include "rgb/led_breathing.h"
#include "rgb/sdvx_led_strip.h"
#include "rgb/tt_led.h"

#include "device/device_config.h"
#include "device/svre9led.h"
#include "device/turbocharger.h"

static uint32_t& reset_reason = *(uint32_t*)0x10000000;

bool do_reset_bootloader;
bool do_reset;

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
Configloader device_configloader(0x8021000);

config_t config;
rgb_config_t rgb_config;
mapping_config_t mapping_config;
device_config_t device_config;

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
		endpoint_desc(0x81, 0x03, 16, 1)
	)
);

desc_t iidx_dev_desc_p = {sizeof(iidx_dev_desc), (void*)&iidx_dev_desc};
desc_t konami_conf_desc_p = {sizeof(konami_conf_desc), (void*)&konami_conf_desc};

auto sdvx_dev_desc = device_desc(0x200, 0, 0, 0, 64, 0x1ccf, 0x101c, 0x100, 1, 2, 3, 1);
desc_t sdvx_dev_desc_p = {sizeof(sdvx_dev_desc), (void*)&sdvx_dev_desc};

// Defined in "board_define.h"
extern Pin_Definition *current_pins;
#ifdef ROXY
extern Roxy_v11_Pins roxy_v11_pins;
extern Roxy_v20_Pins roxy_v20_pins;
#endif

extern Button_Leds button_led_manager;	// In button_leds.h
Button_Manager button_manager; 	// In button_manager.h

extern Board_Version board_version;	// In board_version.h

USB_f1 roxy_usb(USB, dev_desc_p, conf_desc_p);
USB_f1 iidx_usb(USB, iidx_dev_desc_p, konami_conf_desc_p);
USB_f1 sdvx_usb(USB, sdvx_dev_desc_p, konami_conf_desc_p);

#if defined(ROXY)
WS2812B_Spi ws2812b;	// In rgb/ws2812b_spi.h
#elif defined(ARCIN)
WS2812B_Timer ws2812b;	// In rgb/ws2812b_timer.h
template <>
void interrupt<Interrupt::DMA1_Channel7>() {
	ws2812b.irq();
}
#endif

TLC59711 tlc59711;	// In rgb/tlc59711.h

TLC5973 tlc5973;	// In rgb/tlc5973.h

Sdvx_Leds sdvx_leds;	// In rgb/sdvx_led_strip.h
Led_Breathing breathing_leds;	// In rgb/led_breathing.h
Turntable_Leds tt_leds;	// In rgb/tt_led.h

uint32_t last_led_time;

// Other vendor devices
SVRE9LED svre9leds;		// In devices/svre9led.h
Turbocharger tcleds;	// In devices/turbocharger.h

#if defined(ROXY)
template <>
void interrupt<Interrupt::DMA2_Channel2>() {
	ws2812b.irq();
	tcleds.irq();
	tlc59711.irq();
	tlc5973.irq(Interrupt::DMA2_Channel2);
}
#elif defined(ARCIN)
template <>
void interrupt<Interrupt::DMA2_Channel2>() {
	tcleds.irq();
	tlc59711.irq();
	tlc5973.irq(Interrupt::DMA2_Channel2);
}
#endif

template<>
void interrupt<Interrupt::DMA1_Channel3>() {
	tlc5973.irq(Interrupt::DMA1_Channel3);
}

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

NullAxis null_axis;

QEAxis axis_qe1(TIM2);
QEAxis axis_qe2(TIM3);

IntAxis axis_int;

template<>
void interrupt<Interrupt::EXTI1>() {
	if(EXTI.PR1 & (1 << 1)) {
		EXTI.PR1 |= (1 << 1);	// Clear flag
		axis_int.updateEncoder();
	}
}

template<>
void interrupt<Interrupt::EXTI9_5>() {
	if(EXTI.PR1 & (1 << 7)) {
		EXTI.PR1 |= (1 << 7);	// Clear flag
		axis_int.updateEncoder();
	}
}

AnalogAxis axis_ana1(ADC1, 2);
AnalogAxis axis_ana2(ADC2, 4);

extern NKRO_Keyboard nkro;	// In "nkro_keyboard.h"

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
	device_configloader.read(sizeof(device_config), &device_config);
	
	RCC.enable(RCC.GPIOA);
	RCC.enable(RCC.GPIOB);
	RCC.enable(RCC.GPIOC);
	RCC.enable(RCC.GPIOD);
	
	current_pins->usb_dm.set_mode(Pin::AF);
	current_pins->usb_dm.set_af(14);
	current_pins->usb_dp.set_mode(Pin::AF);
	current_pins->usb_dp.set_af(14);

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

#if defined(ARCIN)
	usb_pu.set_mode(Pin::Output);
	usb_pu.on();
#endif

#if defined(ROXY)
	// Get board version
	board_version.get_version();
	// Set pins based on version
	if(board_version.board == Board_Version::V1_1) {
		current_pins = &roxy_v11_pins;
	} else if(board_version.board == Board_Version::V2_0) {
		current_pins = &roxy_v20_pins;
	}
#endif

	button_manager.init();	
	
	for(int i = 0; i < current_pins->get_num_leds(); i++) {
		Pin* led;
		led = current_pins->get_led(i);
		led->set_mode(Pin::Output);
		if(config.flags & (1 << (3 + i))) {
			led->on();
		}
	}

	// Configure QE / Analog
	Axis* axis[2];
	if(config.flags & (1 << 8)) {
		// Setup interrupts on pins PA1 and PA7
		RCC.enable(RCC.SYSCFG);	// Enable SYSCFG
		// No need to set SYSCFG_EXTICR since we're using PA
		EXTI.IMR1 |= (1 << 1) | (1 << 7); 		// Enable interrupts EXT1 and EXT7
		EXTI.RTSR1 |= (1 << 1) | (1 << 7);		// Enable rising edge trigger on EXT1 and EXT7
		EXTI.FTSR1 |= (1 << 1) | (1 << 7);		// Enable falling edge trigger on EXT1 and EXT7
		Interrupt::enable(Interrupt::EXTI1);	// Enable EXTI1 interrupt
		Interrupt::enable(Interrupt::EXTI9_5);	// Enable EXTI5-9 interrupt
		current_pins->qe1b.set_mode(Pin::Input);
		current_pins->qe2b.set_mode(Pin::Input);
		current_pins->qe1b.set_pull(Pin::PullUp);
		current_pins->qe2b.set_pull(Pin::PullUp);
		axis_int.set_pins(current_pins->qe1b, current_pins->qe1b);

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
			
			current_pins->qe1a.set_af(1);
			current_pins->qe1b.set_af(1);
			current_pins->qe1a.set_mode(Pin::AF);
			current_pins->qe1b.set_mode(Pin::AF);
			current_pins->qe1a.set_pull(Pin::PullUp);
			current_pins->qe1b.set_pull(Pin::PullUp);
			
			axis[0] = &axis_qe1;
		}
		
		if(config.flags & (1 << 5)) {
			RCC.enable(RCC.ADC12);
			
			axis_ana2.enable();
			
			axis[1] = &axis_ana2;
			
		} else {
			RCC.enable(RCC.TIM3);
			
			axis_qe2.enable(config.flags & (1 << 2), config.qe_sens[1]);
			
			current_pins->qe2a.set_af(2);
			current_pins->qe2b.set_af(2);
			current_pins->qe2a.set_mode(Pin::AF);
			current_pins->qe2b.set_mode(Pin::AF);
			current_pins->qe2a.set_pull(Pin::PullUp);
			current_pins->qe2b.set_pull(Pin::PullUp);
			
			axis[1] = &axis_qe2;
		}
	}
	axis[0]->set_config(config.axis_debounce_time, config.axis_sustain_time, config.reduction_ratio[0], config.deadzone_angle[0]);
	axis[1]->set_config(config.axis_debounce_time, config.axis_sustain_time, config.reduction_ratio[1], config.deadzone_angle[1]);

	// Initialize Playstation Mode
	if(config.ps2_mode > 0) {
		spi_ps.init();
	}

	// Initialize RGB modes
	switch(rgb_config.rgb_mode) {
		case 1:
			breathing_leds.set_hue(0, rgb_config.led1_hue);
			breathing_leds.set_hue(1, rgb_config.led2_hue);
			break;
		case 3:
			tt_leds.init(
				rgb_config.tt_num_leds, 
				(Turntable_Leds::Mode)rgb_config.tt_mode, 
				(Turntable_Leds::SpinType)(rgb_config.tt_spin & 0xF), 
				(Turntable_Leds::SpinDirection)((rgb_config.tt_spin >> 4) & 0xF));
			if(rgb_config.tt_mode == Turntable_Leds::Solid) {
				tt_leds.set_solid(rgb_config.tt_hue, rgb_config.tt_sat, rgb_config.tt_val);
			}
			tt_leds.set_brightness(config.rgb_brightness);
			if(config.rgb_mode == 1) {
				ws2812b.set_num_leds(rgb_config.tt_num_leds);
			}
			break;
	}
	
	// Set RGB Interfaces
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
			if(rgb_config.rgb_mode == 2) {
				sdvx_leds.init(Sdvx_Leds::RGB);
				sdvx_leds.set_left_hue(rgb_config.led1_hue);
				sdvx_leds.set_right_hue(rgb_config.led2_hue);
			}
			break;
		case 3:
			tlc5973.init();
			tlc5973.set_brightness(config.rgb_brightness);
			break;
	}

	// Set up other vendor devices
	if(device_config.device_enable & (1 << 0)) {
		svre9leds.init(device_config.svre_led_mapping & 0xF, (device_config.svre_led_mapping >> 4) & 0xF);
	}
	if(device_config.device_enable & (1 << 1)) {
		sdvx_leds.init(Sdvx_Leds::TwoColor);
		tcleds.init();
	}

	// int8_t axis_state[2] = {0, 0};				// Current axis state
	// uint32_t axis_sustain_start[2] = {0, 0};	// Clock time a sustain is started
	// uint32_t axis_debounce_start[2] = {0, 0};

	// uint32_t axis_time[2] = {0, 0};
	// uint8_t last_axis[2] = {0, 0};
	
	// int8_t last_axis_state[2] = {0, 0};
	// uint32_t qe_count[2] = {0, 0};
	uint32_t axis_buttons[4] = {(1 << 12), (1 << 13), (1 << 14), (1 << 15)};

	while(1) {
		usb->process();
		uint32_t current_time = Time::time();
		
		uint16_t buttons = button_manager.read_buttons();
		
		if(do_reset_bootloader) {
			Time::sleep(10);
			reset_bootloader();
		}
		
		if(do_reset) {
			Time::sleep(10);
			reset();
		}	
		
		for(int i = 0; i < 2; i++) {
			// Process axis
			axis[i]->process();

			// Set lights and/or buttons
			if(axis[i]->dir_state > 0) {
				sdvx_leds.set_active(i, true);
				if(i == rgb_config.tt_axis) {
					tt_leds.set_direction(Turntable_Leds::CW);
				}
				if(config.flags & (1 << 6)) {
					buttons |= axis_buttons[2 * i];
				}
			} else if(axis[i]->dir_state < 0) {
				sdvx_leds.set_active(i, false);
				if(i == rgb_config.tt_axis) {
					tt_leds.set_direction(Turntable_Leds::CCW);
				}
				if(config.flags & (1 << 6)) {
					buttons |= axis_buttons[2 * i + 1];
				}
			}

			// Translate axis to buttons if enabled, or if IIDX PS2 mode is on
			if(axis[i]->dir_state == 1 && (config.flags & (1 << 6) || config.ps2_mode == 2)) {
				buttons |= axis_buttons[2 * i];
			} else if(axis[i]->dir_state == -1 && (config.flags & (1 << 6) || config.ps2_mode == 3)) {
				buttons |= axis_buttons[2 * i + 1];
			}
		}
		
		input_report_t report = {1, buttons, uint8_t(axis[0]->count), uint8_t(axis[1]->count)};

		// PS2 (if enabled)
		if(config.ps2_mode > 0) {
			spi_ps.set_buttons(buttons);
		}
			
		// Joystick
		if(usb->ep_ready(1) && (config.output_mode == 0 || config.output_mode == 2)) {
			usb->write(1, (uint32_t*)&report, sizeof(report));
		}

		// Keyboard
		if(usb->ep_ready(2) && (config.output_mode == 1 || config.output_mode == 2)) {
			for (int i = 0; i < current_pins->get_num_buttons(); i++) {
				if (buttons & (1 << i) && mapping_config.button_kb_map[i] > 0) {
					nkro.set_key(mapping_config.button_kb_map[i]);
				} else {
					nkro.reset_key(mapping_config.button_kb_map[i]);
				}
			}
			for (int i = 0; i < 2; i++) {
				if (axis[i]->dir_state == 1 && mapping_config.axes_kb_map[2 * i] > 0) {
					nkro.set_key(mapping_config.axes_kb_map[2 * i]);
				} else {
					nkro.reset_key(mapping_config.axes_kb_map[2 * i]);
				}
				if (axis[i]->dir_state == -1 && mapping_config.axes_kb_map[2 * i + 1] > 0) {
					nkro.set_key(mapping_config.axes_kb_map[2 * i + 1]);
				} else {
					nkro.reset_key(mapping_config.axes_kb_map[2 * i + 1]);
				}
			}
			usb->write(2, (uint32_t*)nkro.get_data(), 32);
		}

		if(Time::time() - last_led_time > 1000) {
			button_manager.set_leds_reactive();

			// Breathing LEDs
			if(rgb_config.rgb_mode == 1) {
				if(breathing_leds.update(axis[0]->dir_state, axis[1]->dir_state)) { 
					CRGB temp1 = breathing_leds.get_led(0);
					CRGB temp2 = breathing_leds.get_led(1);
					if(config.rgb_mode == 2) {
						tlc59711.set_led_8bit(3, temp1.r, temp1.g, temp1.b);
						tlc59711.set_led_8bit(2, temp2.r, temp2.g, temp2.b);
						tlc59711.schedule_dma();
					} else if(config.rgb_mode == 3) {
						tlc5973.set_led_8bit(0, temp1.r, temp1.g, temp1.b);
						tlc5973.set_led_8bit(1, temp2.r, temp2.g, temp2.b);
						tlc5973.schedule_dma();
					}
				}
			}
		}	

		// TT LEDs
		if(rgb_config.rgb_mode == 3) {
			if(tt_leds.update()) {
				CRGB* leds = tt_leds.get_leds();
				if(config.rgb_mode == 1) {
					uint8_t num_leds = tt_leds.get_num_leds();
					for(uint8_t i = 0; i < num_leds; i++) {
						ws2812b.set_led(i, leds[i].r, leds[i].g, leds[i].b, i == (num_leds - 1));
					}
				}
			}
		}

		// SDVX LED strips
		if(device_config.device_enable & (1 << 1)) {
			// TC hardware takes precedent if it is enabled
			if(sdvx_leds.update()) {
				for(uint8_t i = 0; i < sdvx_leds.get_num_leds(); i++) {
					tcleds.set_left_led(i, sdvx_leds.get_left_brightness(i));
					tcleds.set_right_led(i, sdvx_leds.get_right_brightness(i));
				}
				tcleds.schedule_dma();
			}
		} else if(config.rgb_mode == 2 && rgb_config.rgb_mode == 2) {
			// TLC59711 mode
			if(sdvx_leds.update()) {
				for( int i = 0; i < sdvx_leds.get_num_leds(); i++) {
					CRGB temp = sdvx_leds.get_led(i);
					tlc59711.set_led_8bit(i, temp.b, temp.r, temp.g);
				}
				tlc59711.schedule_dma();
			}
		}
	}
}
