#include <rcc/rcc.h>
#include <gpio/gpio.h>
#include <interrupt/interrupt.h>
#include <timer/timer.h>
#include <os/time.h>
#include <usb/usb.h>
#include <usb/descriptor.h>
#include <usb/hid.h>

auto report_desc = gamepad(
	// Inputs.
	buttons(64),
	
	// Feature.
	usage_page(0xff55),
	usage(0xb007),
	logical_minimum(0),
	logical_maximum(255),
	report_size(8),
	report_count(1),
	
	feature(0x02) // HID bootloader function
);

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

static PinArray button_inputs = GPIOB.array(0, 10);
static PinArray button_leds = GPIOC.array(0, 10);

USB_f1 usb(USB, dev_desc_p, conf_desc_p);

class HID_arcin : public USB_HID {
	public:
		HID_arcin(USB_generic& usbd, desc_t rdesc) : USB_HID(usbd, rdesc, 0, 1, 64) {}
	
	protected:
		virtual bool set_output_report(uint32_t* buf, uint32_t len) {
			return true;
		}
		
		virtual bool set_feature_report(uint32_t* buf, uint32_t len) {
			if(len != 1) {
				return false;
			}
			
			switch(*buf & 0xff) {
				case 0:
					return true;
				
				case 0x10: // Reset to bootloader
					//do_reset_bootloader = true;
					return true;
				
				default:
					return false;
			}
		}
};

HID_arcin usb_hid(usb, report_desc_p);

uint32_t serial_num() {
	uint32_t* uid = (uint32_t*)0x1ffff7ac;
	
	return uid[0] * uid[1] * uid[2];
}

class USB_strings : public USB_class_driver {
	private:
		USB_generic& usb;
		
	public:
		USB_strings(USB_generic& usbd) : usb(usbd) {
			usb.register_driver(this);
		}
	
	protected:
		virtual SetupStatus handle_setup(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength) {
			// Get string descriptor.
			if(bmRequestType == 0x80 && bRequest == 0x06 && (wValue & 0xff00) == 0x0300) {
				const void* desc = nullptr;
				uint16_t buf[9];
				
				switch(wValue & 0xff) {
					case 0:
						desc = u"\u0304\u0409";
						break;
					
					case 1:
						desc = u"\u0308zyp";
						break;
					
					case 2:
						desc = u"\u030carcin";
						break;
					
					case 3:
						{
							buf[0] = 0x0312;
							uint32_t id = serial_num();
							for(int i = 8; i > 0; i--) {
								buf[i] = (id & 0xf) > 9 ? 'A' + (id & 0xf) - 0xa : '0' + (id & 0xf);
								id >>= 4;
							}
							desc = buf;
						}
						break;
				}
				
				if(!desc) {
					return SetupStatus::Unhandled;
				}
				
				uint8_t len = *(uint8_t*)desc;
				
				if(len > wLength) {
					len = wLength;
				}
				
				usb.write(0, (uint32_t*)desc, len);
				
				return SetupStatus::Ok;
			}
			
			return SetupStatus::Unhandled;
		}
};

USB_strings usb_strings(usb);

struct report_t {
	uint64_t buttons;
} __attribute__((packed));

enum io_t : uint8_t {A, B, C, D, E};

struct mapping_t {
	io_t io;
	uint8_t n;
};

const mapping_t map[][4] {
	{
		{E, 2},
		{E, 4},
		{E, 3},
		{E, 5},
	}, {
		{C, 3},
		{C, 1},
		{C, 2},
		{C, 0},
	}, {
		{A, 0},
		{A, 2},
		{A, 1},
		{A, 3},
	}, {
		{A, 7},
		{A, 5},
		{A, 6},
		{A, 4},
	}, {
		{B, 0},
		{C, 5},
		{B, 1},
		{C, 4},
	}, {
		{E, 9},
		{E, 7},
		{E, 8},
		{B, 2},
	}, {
		{E, 13},
		{E, 11},
		{E, 12},
		{E, 10},
	}, {
		{B, 11},
		{E, 15},
		{B, 10},
		{E, 14},
	}, {
		{B, 15},
		{B, 13},
		{B, 14},
		{B, 12},
	}, {
		{D, 8},
		{D, 10},
		{D, 9},
		{D, 11},
	}, {
		{D, 15},
		{D, 13},
		{D, 14},
		{D, 12},
	}, {
		{C, 9},
		{C, 7},
		{C, 8},
		{C, 6},
	}, {
		{D, 0},
		{D, 2},
		{D, 1},
		{D, 3},
	}, {
		{D, 4},
		{D, 6},
		{D, 5},
		{D, 7},
	}, {
		{B, 8},
		{E, 0},
		{B, 9},
		{E, 1},
	}, {
		{A, 8},
		{C, 10},
		{A, 9},
		{C, 11},
	}
};

uint32_t bit(uint32_t w, uint32_t n) {
	return w & (1 << n) ? 0 : 1;
}

GPIO_t* gpios[] = {
	&GPIOA,
	&GPIOB,
	&GPIOC,
	&GPIOD,
	&GPIOE,
};

int main() {
	rcc_init();
	
	// Initialize system timer.
	STK.LOAD = 72000000 / 8 / 1000; // 1000 Hz.
	STK.CTRL = 0x03;
	
	RCC.enable(RCC.GPIOA);
	RCC.enable(RCC.GPIOB);
	RCC.enable(RCC.GPIOC);
	RCC.enable(RCC.GPIOD);
	RCC.enable(RCC.GPIOE);
	
	/*
	usb_dm.set_mode(Pin::AF);
	usb_dm.set_af(14);
	usb_dp.set_mode(Pin::AF);
	usb_dp.set_af(14);
	*/
	
	RCC.enable(RCC.USB);
	
	usb.init();
	
	for(uint32_t i = 0; i < 16; i++) {
		for(uint32_t j = 0; j < 4; j++) {
			auto& m = map[i][j];
			
			(*gpios[m.io])[m.n].set_mode(Pin::InputPull);
			(*gpios[m.io])[m.n].on();
		}
	}
	
	while(1) {
		usb.process();
		
		uint32_t inputs[] = {
			GPIOA.reg.IDR,
			GPIOB.reg.IDR,
			GPIOC.reg.IDR,
			GPIOD.reg.IDR,
			GPIOE.reg.IDR,
		};
		
		uint64_t buttons = 0;
		
		for(uint32_t i = 0; i < 16; i++) {
			for(uint32_t j = 0; j < 4; j++) {
				auto& m = map[i][j];
			
				buttons |= bit(inputs[m.io], m.n) << i;
			}
		}
		
		if(usb.ep_ready(1)) {
			report_t report = {buttons};
			
			usb.write(1, (uint32_t*)&report, sizeof(report));
		}
	}
}
