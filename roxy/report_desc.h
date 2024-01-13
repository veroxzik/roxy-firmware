#ifndef REPORT_DESC_H
#define REPORT_DESC_H

#include <usb/hid.h>

auto report_desc = joystick(
	// Inputs.
	report_id(1),
	
	buttons(16),
	
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
	report_id(2),
	logical_minimum(0),
	logical_maximum(1),
	
	usage_page(UsagePage::Ordinal),
	usage(1),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x10),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(2),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x11),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(3),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x12),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(4),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x13),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(5),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x14),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(6),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x15),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(7),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x16),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(8),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x17),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(9),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x18),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(10),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x19),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(11),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x1a),
		report_size(1),
		report_count(1),
		output(0x02)
	),

	usage_page(UsagePage::Ordinal),
	usage(12),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x1b),
		report_size(1),
		report_count(1),
		output(0x02)
	),
	
	padding_out(4),
	
	logical_maximum(255),
	
	usage_page(UsagePage::Ordinal),
	usage(13),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x1c),
		report_size(8),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(14),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x1d),
		report_size(8),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(15),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x1e),
		report_size(8),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(16),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x1f),
		report_size(8),
		report_count(1),
		output(0x02)
	),
	
	usage_page(UsagePage::Ordinal),
	usage(17),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x20),
		report_size(8),
		report_count(1),
		output(0x02)
	),

	usage_page(UsagePage::Ordinal),
	usage(18),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x21),
		report_size(8),
		report_count(1),
		output(0x02)
	),

	usage_page(UsagePage::Ordinal),
	usage(19),
	collection(Collection::Logical, 
		usage_page(UsagePage::LED),
		usage(0x4b),
		string_index(0x22),
		report_size(8),
		report_count(1),
		output(0x02)
	),
	
	// Bootloader
	report_id(0xb0),
	
	usage_page(0xff55),
	usage(0xb007),
	logical_minimum(0),
	logical_maximum(255),
	report_size(8),
	report_count(1),
	
	feature(0x02), // HID bootloader function
	
	// Configuration
	report_id(0xc0),
	
	usage(0xc000),
	feature(0x02), // Config segment
	
	usage(0xc001),
	feature(0x02), // Config segment size
	
	feature(0x01), // Padding
	
	usage(0xc0ff),
	report_count(60),
	feature(0x02), // Config data

	// Devices
	report_id(0xd0),

	usage(0xd000),
	report_count(2),
	feature(0x02),	// Command ID

	usage(0xd001),
	report_count(4),
	feature(0x02),	// Data

	// Firmware version
	report_id(0xa0),

	usage(0xd000),
	report_count(2),
	feature(0x02),	// Command ID

	usage(0xd001),
	report_count(4),
	feature(0x02),	// Data

	// Firmware version
	report_id(0xa4),

	usage(0xd000),
	report_count(2),
	feature(0x02),	// Command ID

	usage(0xd001),
	report_count(4),
	feature(0x02)	// Data
);

auto keyboard_report_desc = keyboard(
	// Modifiers
	report_size(1),
	report_count(8),
	usage_page(DesktopUsage::KeyCodes),
	usage_minimum(224),
	usage_maximum(231),
	logical_minimum(0),
	logical_maximum(1),
	input(0x02),

	// Keys
	report_size(1),
	report_count(31 * 8),
	logical_minimum(0),
	logical_maximum(1),
	usage_page(DesktopUsage::KeyCodes),
	usage_minimum(0),
	usage_maximum(31 * 8 - 1),
	input(0x02)
);

struct input_report_t {
	uint8_t report_id;
	uint16_t buttons;
	uint8_t axis_x;
	uint8_t axis_y;
} __attribute__((packed));

struct output_report_t {
	uint8_t report_id;
	uint16_t leds;
	uint8_t led1;
	uint8_t r1;
	uint8_t g1;
	uint8_t b1;
	uint8_t r2;
	uint8_t g2;
	uint8_t b2;
} __attribute__((packed));

struct bootloader_report_t {
	uint8_t report_id;
	uint8_t func;
} __attribute__((packed));

struct config_report_t {
	uint8_t report_id;
	uint8_t segment;
	uint8_t size;
	uint8_t pad;
	uint8_t data[60];
} __attribute__((packed));

struct device_report_t {
	uint8_t report_id;
	uint16_t command_id;
	uint32_t data;
} __attribute__((packed));

#endif
