#include <rcc/rcc.h>
#include <gpio/gpio.h>
#include <interrupt/interrupt.h>
#include <timer/timer.h>
#include <os/time.h>
#include <spi/spi.h>
#include <usb/usb.h>
#include <usb/descriptor.h>
#include <usb/hid.h>

static uint32_t& reset_reason = *(uint32_t*)0x10000000;

static bool do_reset_bootloader;

void reset_bootloader() {
	reset_reason = 0xb007;
	SCB.AIRCR = (0x5fa << 16) | (1 << 2); // SYSRESETREQ
}

auto report_desc = gamepad(
	// Inputs.
	buttons(15),
	padding_in(1),
	
	usage_page(UsagePage::Desktop),
	usage(DesktopUsage::X),
	logical_minimum(0),
	logical_maximum(255),
	report_count(1),
	report_size(8),
	input(0x02),

	usage_page(UsagePage::Desktop),
	usage(DesktopUsage::Y),
	logical_minimum(0),
	logical_maximum(255),
	report_count(1),
	report_size(8),
	input(0x02),
	
	// Outputs.
	usage_page(UsagePage::Ordinal),
	usage(1),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(2),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(3),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(4),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(5),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(6),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(7),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(8),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(9),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(10),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(11),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	padding_out(5),
	
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
static Pin usb_pu = GPIOA[15];

static PinArray button_inputs = GPIOB.array(0, 10);
static PinArray button_leds = GPIOC.array(0, 10);

static Pin qe1a = GPIOA[0];
static Pin qe1b = GPIOA[1];
static Pin qe2a = GPIOA[6];
static Pin qe2b = GPIOA[7];

static Pin led1 = GPIOA[8];
static Pin led2 = GPIOA[9];

static Pin spi_ack = GPIOB[11];
static Pin spi_nss = GPIOB[12];
static Pin spi_sck = GPIOB[13];
static Pin spi_miso = GPIOB[14];
static Pin spi_mosi = GPIOB[15];

RBLog<256, 2> ps_rblog; 

class SPI_PS {
	private:
		uint32_t last_byte_time;
		
		int32_t pos;
		
		uint32_t state;
		
		bool received_byte(uint8_t data) {
			if(Time::time() - last_byte_time > 2) {
				pos = 0; // Assume start of new command.
			}
			
			last_byte_time = Time::time();
			
			if(pos == -1) {
				return false;
			}
			
			ps_rblog.log("pos = %d", pos);
			
			if(pos == 0 && data != 0x01) {
				pos = -1;
				return false;
			}
			
			if(pos == 1 && data != 0x42) {
				pos = -1;
				return false;
			}
			
			if(pos > 4) {
				pos = -1;
				return false;
			}
			
			switch(pos) {
				case 0:
					SPI2.reg.DR8 = 0x41;
					break;
				
				case 1:
					SPI2.reg.DR8 = 0x5a;
					break;
				
				case 2:
					SPI2.reg.DR8 = state & 0xff;
					break;
				
				case 3:
					SPI2.reg.DR8 = (state >> 8) & 0xff;
					break;
				
				case 4:
					//SPI2.reg.DR8 = 0xff;
					break;
			}
			
			pos++;
			
			return true;
		}
	
	public:
		void init() {
			RCC.enable(RCC.SPI2);
			
			spi_ack.on();
			spi_ack.set_type(Pin::OpenDrain);
			spi_ack.set_mode(Pin::Output);
			
			spi_nss.set_af(5);
			spi_nss.set_type(Pin::OpenDrain);
			spi_nss.set_mode(Pin::AF);
			
			spi_sck.set_af(5);
			spi_sck.set_type(Pin::OpenDrain);
			spi_sck.set_mode(Pin::AF);
			
			spi_miso.set_af(5);
			spi_miso.set_type(Pin::OpenDrain);
			spi_miso.set_mode(Pin::AF);
			
			spi_mosi.set_af(5);
			spi_mosi.set_type(Pin::OpenDrain);
			spi_mosi.set_mode(Pin::AF);
			
			SPI2.reg.CR2 = (1 << 12) | (7 << 8) | (1 << 6); // FRXTH, DS = 8bit, RXNEIE
			SPI2.reg.CR1 = (1 << 7) | (1 << 6) | (1 << 1); // LSBFIRST, SPE, CPOL
			
			Interrupt::enable(Interrupt::SPI2);
		}
		
		void spi_irq() {
			ps_rblog.log("SPI2 interrupt, SR = %#06x", SPI2.reg.SR);
			
			// RXNE
			if(SPI2.reg.SR & (1 << 0)) {
				uint8_t data = SPI2.reg.DR8;
				
				ps_rblog.log("Received byte: %#04x", data);
				
				bool ack = received_byte(data);
				
				if(ack) {
					ps_rblog.log("ACK");
					spi_ack.off();
					
					for(int i = 0; i < 100; i++) {
						asm volatile("nop");
					}
					
					spi_ack.on();
				} else {
					ps_rblog.log("NAK");
				}
			}
		}
		
		void set_state(uint32_t s) {
			state = ~s;
		}
};

SPI_PS spi_ps;

template <>
void interrupt<Interrupt::SPI2>() {
	spi_ps.spi_irq();
}

USB_f1 usb(USB, dev_desc_p, conf_desc_p);

uint32_t last_led_time;

class HID_arcin : public USB_HID {
	public:
		HID_arcin(USB_generic& usbd, desc_t rdesc) : USB_HID(usbd, rdesc, 0, 1, 64) {}
	
	protected:
		virtual bool set_output_report(uint32_t* buf, uint32_t len) {
			last_led_time = Time::time();
			button_leds.set(*buf);
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
					do_reset_bootloader = true;
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
	uint16_t buttons;
	uint8_t axis_x;
	uint8_t axis_y;
} __attribute__((packed));

int main() {
	rcc_init();
	
	// Initialize system timer.
	STK.LOAD = 72000000 / 8 / 1000; // 1000 Hz.
	STK.CTRL = 0x03;
	
	RCC.enable(RCC.GPIOA);
	RCC.enable(RCC.GPIOB);
	RCC.enable(RCC.GPIOC);
	
	usb_dm.set_mode(Pin::AF);
	usb_dm.set_af(14);
	usb_dp.set_mode(Pin::AF);
	usb_dp.set_af(14);
	
	spi_ps.init();
	
	RCC.enable(RCC.USB);
	
	usb.init();
	
	usb_pu.set_mode(Pin::Output);
	usb_pu.on();
	
	button_inputs.set_mode(Pin::Input);
	button_inputs.set_pull(Pin::PullUp);
	
	button_leds.set_mode(Pin::Output);
	
	led1.set_mode(Pin::Output);
	led1.on();
	
	led2.set_mode(Pin::Output);
	led2.on();
	
	RCC.enable(RCC.TIM2);
	RCC.enable(RCC.TIM3);
	
	TIM2.CCMR1 = (1 << 8) | (1 << 0);
	TIM2.CCER = 1 << 1;
	TIM2.SMCR = 3;
	TIM2.CR1 = 1;
	
	TIM3.CCMR1 = (1 << 8) | (1 << 0);
	TIM3.CCER = 1 << 1;
	TIM3.SMCR = 3;
	TIM3.CR1 = 1;
	
	qe1a.set_af(1);
	qe1b.set_af(1);
	qe1a.set_mode(Pin::AF);
	qe1b.set_mode(Pin::AF);
	
	qe2a.set_af(2);
	qe2b.set_af(2);
	qe2a.set_mode(Pin::AF);
	qe2b.set_mode(Pin::AF);
	
	uint8_t last_x = 0;
	uint8_t last_y = 0;
	
	int8_t state_x = 0;
	int8_t state_y = 0;
	
	while(1) {
		usb.process();
		
		uint16_t buttons = button_inputs.get() ^ 0x7ff;
		
		if(do_reset_bootloader) {
			Time::sleep(10);
			reset_bootloader();
		}
		
		if(Time::time() - last_led_time > 1000) {
			button_leds.set(buttons);
		}
		
		if(usb.ep_ready(1)) {
			uint8_t x = TIM2.CNT;
			uint8_t y = TIM3.CNT;
			int8_t rx = x - last_x;
			int8_t ry = y - last_y;
			last_x = x;
			last_y = y;
			
			if(rx > 0) {
				state_x = 100;
			} else if(rx < 0) {
				state_x = -100;
			} else if(state_x > 0) {
				state_x--;
			} else if(state_x < 0) {
				state_x++;
			}
			
			if(ry > 0) {
				state_y = 100;
			} else if(ry < 0) {
				state_y = -100;
			} else if(state_y > 0) {
				state_y--;
			} else if(state_y < 0) {
				state_y++;
			}
			
			if(state_x > 0) {
				buttons |= 1 << 11;
			} else if(state_x < 0) {
				buttons |= 1 << 12;
			}
			
			if(state_y > 0) {
				buttons |= 1 << 13;
			} else if(state_y < 0) {
				buttons |= 1 << 14;
			}
			
			report_t report = {buttons, x, y};
			
			usb.write(1, (uint32_t*)&report, sizeof(report));
			
			/*
			uint32_t ps_state = (1 << 5) | (1 << 6) | (1 << 7);
			ps_state |= buttons & (1 <<  0) ? 1 << 12 : 0; // B1
			ps_state |= buttons & (1 <<  1) ? 1 << 13 : 0; // B2
			ps_state |= buttons & (1 <<  2) ? 1 << 11 : 0; // B3
			ps_state |= buttons & (1 <<  3) ? 1 << 14 : 0; // B4
			ps_state |= buttons & (1 <<  4) ? 1 << 10 : 0; // B5
			ps_state |= buttons & (1 <<  5) ? 1 << 15 : 0; // B6
			ps_state |= buttons & (1 <<  6) ? 1 <<  9 : 0; // B7
			ps_state |= buttons & (1 <<  6) ? 1 <<  4 : 0; // B8
			ps_state |= buttons & (1 <<  6) ? 1 <<  8 : 0; // B9
			ps_state |= buttons & (1 << 10) ? 1 <<  0 : 0; // select
			ps_state |= buttons & (1 <<  9) ? 1 <<  3 : 0; // start
			*/
			
			uint32_t ps_state = 0;
			ps_state |= buttons & (1 <<  0) ? 1 << 15 : 0; // B1
			ps_state |= buttons & (1 <<  1) ? 1 << 10 : 0; // B2
			ps_state |= buttons & (1 <<  2) ? 1 << 14 : 0; // B3
			ps_state |= buttons & (1 <<  3) ? 1 << 11 : 0; // B4
			ps_state |= buttons & (1 <<  4) ? 1 << 13 : 0; // B5
			ps_state |= buttons & (1 <<  5) ? 1 <<  8 : 0; // B6
			ps_state |= buttons & (1 <<  6) ? 1 <<  7 : 0; // B7
			ps_state |= buttons & (1 << 10) ? 1 <<  0 : 0; // select
			ps_state |= buttons & (1 <<  9) ? 1 <<  3 : 0; // start
			ps_state |= buttons & (1 << 11) ? 1 <<  4 : 0; // TT+
			ps_state |= buttons & (1 << 12) ? 1 <<  6 : 0; // TT-
			
			spi_ps.set_state(ps_state);
		}
	}
}
