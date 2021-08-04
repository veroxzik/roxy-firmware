#ifndef HIDARCIN_H
#define HIDARCIN_H

#include <usb/usb.h>

#include "config.h"
#include "configloader.h"
#include "report_desc.h"

#include "button_manager.h"

#include "rgb/rgb_config.h"
#include "rgb/ws2812b_spi.h"
#include "rgb/ws2812b_timer.h"
#include "rgb/tlc59711.h"
#include "rgb/tlc5973.h"

#include "device/device_config.h"
#include "device/svre9led.h"

extern bool do_reset_bootloader;
extern bool do_reset;
extern Configloader configloader;
extern Configloader rgb_configloader;
extern Configloader mapping_configloader;
extern Configloader device_configloader;

extern config_t config;
extern rgb_config_t rgb_config;
extern mapping_config_t mapping_config;
extern device_config_t device_config;

extern Button_Manager button_manager; 	// In button_manager.h

#if defined(ROXY)
extern WS2812B_Spi ws2812b;	// In rgb/ws2812b_spi.h
#elif defined(ARCIN)
extern WS2812B_Timer ws2812b;	// In rgb/ws2812b_timer.h
#endif
extern TLC59711 tlc59711;	// In rgb/tlc59711.h
extern TLC5973 tlc5973;	// In rgb/tlc5973.h

extern SVRE9LED svre9leds;		// In devices/svre9led.h

extern uint32_t last_led_time;

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
			} else if(report->segment == 3) {
				device_configloader.write(report->size, report->data);
			}
			else {
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
			} else if(config_id == 2) {
				config_report_t mapping_report = {0xc0, 2, sizeof(mapping_config)};
				memcpy(mapping_report.data, &mapping_config, sizeof(mapping_config));
				usb.write(0, (uint32_t*)&mapping_report, sizeof(mapping_report));
				config_id = 3;
			} else {
				config_report_t device_report = {0xc0, 3, sizeof(device_config)};
				memcpy(device_report.data, &device_config, sizeof(device_config));
				usb.write(0, (uint32_t*)&device_report, sizeof(device_report));
				config_id = 0;
			}
			
			return true;
		}

		bool set_feature_command(device_report_t* report) {
			switch (report->command_id)
			{
			case 1:	// Toggle SVRE9 leds
				if(report->data == 0) {
					svre9leds.toggle_left();
				} else if (report->data == 1) {
					svre9leds.toggle_right();
				}
				break;
			// case 2:	// Set TT LED color
			// 	uint8_t h = report->data & 0xFF;
			// 	uint8_t s = (report->data >> 8) & 0xFF;
			// 	uint8_t v = (report->data >> 16) & 0xFF;
			// 	tt_leds.set_solid(h, s, v);
			// 	break;
			default:
				return false;
			}

			return true;
		}

		bool get_fw_version_report() {
#ifndef VERSION
#warning "NO VERSION DEFINED, CHANGE BEFORE RELEASE"
#define VERSION "vTEST"
#endif
			uint8_t len = (uint8_t)strlen(VERSION);
			if(len > 60) {
				len = 60;
			}
			config_report_t version_report = {0xa0, 0, len};
			memcpy(version_report.data, VERSION, len);
			usb.write(0, (uint32_t*)&version_report, sizeof(version_report));
			return true;
		}

		bool get_board_version_report() {
			config_report_t version_report = {0xa4, 0, 0};
			switch(board_version.board) {
				case Board_Version::UNDEF:
					version_report.size = 9;
					memcpy(version_report.data, "Undefined", version_report.size);
					break;
				case Board_Version::V1_1:
					version_report.size = 9;
					memcpy(version_report.data, "Roxy v1.1", version_report.size);
					break;
				case Board_Version::V2_0:
					version_report.size = 9;
					memcpy(version_report.data, "Roxy v2.0", version_report.size);
					break;
			}
			usb.write(0, (uint32_t*)&version_report, sizeof(version_report));
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
			for (int i = 0; i < current_pins->get_num_buttons(); i++) {
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
				case 3:
					tlc5973.set_led_8bit(0, report->r1, report->g1, report->b1);
					tlc5973.set_led_8bit(1, report->r2, report->g2, report->b2);
					tlc5973.schedule_dma();
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
				
				case 0xd0:
					if(len != sizeof(device_report_t)) {
						return false;
					}

					return set_feature_command((device_report_t*)buf);

				default:
					return false;
			}
		}
		
		virtual bool get_feature_report(uint8_t report_id) {
			switch(report_id) {
				case 0xc0:
					return get_feature_config();

				case 0xa0:
					return get_fw_version_report();

				case 0xa4:
					return get_board_version_report();

				default:
					return false;
			}
		}
};

#endif