#ifndef USB_STRINGS_H
#define USB_STRINGS_H

#include <usb/usb.h>
#include "board_define.h"

extern const char16_t mfg_name[];
extern const char product_name[];

extern const char led_names[][14];

uint32_t serial_num() {
	uint32_t* uid = (uint32_t*)0x1ffff7ac;
	
	return uid[0] * uid[1] * uid[2];
}

class USB_strings : public USB_class_driver {
	private:
		USB_generic& usb;
		const uint8_t* label;
		uint8_t emulation;
		
	public:
		USB_strings(USB_generic& usbd, const uint8_t* l, uint8_t e) : usb(usbd), label(l), emulation(e) {
			usb.register_driver(this);
		}
	
	protected:
		virtual SetupStatus handle_setup(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength) {
			// Get string descriptor.
			if(bmRequestType == 0x80 && bRequest == 0x06 && (wValue & 0xff00) == 0x0300) {
				const void* desc = nullptr;
				uint16_t buf[128] = {0x300};
				uint32_t i = 1;
				uint8_t identifier = wValue & 0xff;
				switch(identifier) {
					case 0:
						desc = u"\u0304\u0409";
						break;
					
					case 1:
						switch(emulation) {
							case 0:
								desc = mfg_name;
								break;
							case 1:
							case 2:
								desc = u"\u0322Konami Amusement";
								break;
						}
						break;
					
					case 2:
						switch(emulation) {
							case 0:
								for(const char* p = product_name; *p; p++) {
									buf[i++] = *p;
								}
								
								if(label[0]) {
									buf[i++] = ' ';
									buf[i++] = '(';
									
									for(const uint8_t* p = label; *p; p++) {
										buf[i++] = *p;
									}
									
									buf[i++] = ')';
								}
								
								buf[0] |= i * 2;
								
								desc = buf;
								break;
							case 1:
								desc = u"\u0350beatmania IIDX controller premium model";
								break;
							case 2:
								desc = u"\u0330SOUND VOLTEX controller";
								break;
						}
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

					default:
						if(identifier >= 0x10 && identifier <= 0x22) {
							// desc = u"\u0316LIGHTS!!";
							for(const char* p = led_names[identifier - 0x10]; *p; p++) {
									buf[i++] = *p;
								}
								
								buf[0] |= i * 2;
								
								desc = buf;
						} else {
							desc = u"\u0316Fallback";
						}
						break;
					// case 0x1c:
					// case 0x1d:
					// case 0x1e:
					// case 0x1f:
					// case 0x20:
					// case 0x21:
					// case 0x22:
					// 	for(const char* p = led_names[identifier - 0x10]; *p; p++) {
					// 		buf[i++] = *p;
					// 	}
					// 	buf[0] |= i * 2;
					// 	desc = buf;
					// 	break;
					// default:
					// 	if(identifier >= 0x10 && identifier <= 0x1b) {
					// 		for(const char* p = led_names[identifier - 0x10]; *p; p++) {
					// 			buf[i++] = *p;
					// 		}
					// 		buf[0] |= i * 2;
					// 		desc = buf;
					// 	}
					// 	break;
				}
				
				if(!desc) {
					return SetupStatus::Unhandled;
				}
				
				uint8_t len = *(uint8_t*)desc;
				uint32_t *dp = (uint32_t*)desc;
				
				while( len > 64) {
					usb.write(0, dp, 64);
					dp += 16;
					len -= 64;
					while(!usb.ep_ready(0)) ;
				}
				usb.write(0, dp, len);
				
				return SetupStatus::Ok;
			}
			
			return SetupStatus::Unhandled;
		}
};

#endif
