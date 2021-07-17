#ifndef BOARD_DEFINE_H
#define BOARD_DEFINE_H

class Pin_Definition {
	public:
		Pin usb_dm = GPIOA[11];
		Pin usb_dp = GPIOA[12];

		Pin qe1a = GPIOA[0];
		Pin qe1b = GPIOA[1];
		Pin qe2a = GPIOA[6];
		Pin qe2b = GPIOA[7];

		virtual bool has_usb_pullup();
		virtual Pin get_usb_pullup();

		virtual uint8_t get_num_buttons();

		virtual Pin* get_button_input(uint8_t index);
		virtual Pin* get_button_led(uint8_t index);

		virtual uint8_t get_num_leds();
		virtual Pin* get_led(uint8_t index);

		virtual bool has_version_check();
		virtual Pin* get_version_check();
};


class Roxy_v11_Pins : public Pin_Definition {
	private:
		uint8_t num_buttons = 12;

		Pin button_inputs[12] = {
			GPIOB[4], GPIOB[5], GPIOB[6], GPIOB[9], GPIOC[14], GPIOC[5],
			GPIOB[1], GPIOB[10], GPIOC[7], GPIOC[9], GPIOA[9], GPIOB[7]};
		Pin button_leds[12] = {
			GPIOA[15], GPIOC[11], GPIOB[3], GPIOC[13], GPIOC[15], GPIOB[0],
			GPIOB[2], GPIOC[6], GPIOC[8], GPIOA[8], GPIOA[10], GPIOB[8]};

		Pin leds[1] = { GPIOC[4] };

		Pin version_pin = GPIOC[0];

		Pin null_pin = GPIOF[4];	// Unused and unconnected

	public:
		bool has_usb_pullup() {
			return false;
		}

		Pin get_usb_pullup() {
			return null_pin;
		}

		uint8_t get_num_buttons() {
			return num_buttons;
		}

		Pin* get_button_input(uint8_t index) {
			if(index < num_buttons) {
				return &button_inputs[index];
			}
		}

		Pin* get_button_led(uint8_t index) {
			if (index < num_buttons) {
				return &button_leds[index];
			}
		}

		uint8_t get_num_leds() {
			return 1;
		}

		Pin* get_led(uint8_t index) {
			return &leds[0];	// Always return the first one
		}

		bool has_version_check() {
			return true;
		}

		Pin* get_version_check() {
			return &version_pin;
		}
};

class Roxy_v20_Pins : public Pin_Definition {
	private:
		uint8_t num_buttons = 11;

		Pin button_inputs[11] = {
			GPIOA[15], GPIOD[2], GPIOB[8], GPIOB[9], GPIOC[14], GPIOC[3],
			GPIOC[2], GPIOA[4], GPIOA[5], GPIOB[2], GPIOB[10]};
		Pin button_leds[11] = {
			GPIOA[8], GPIOA[9], GPIOA[10], GPIOC[11], GPIOC[13], GPIOB[0],
			GPIOB[1], GPIOC[6], GPIOC[7], GPIOC[8], GPIOC[9]};

		Pin version_pin = GPIOC[0];

		Pin null_pin = GPIOF[4];	// Unused and unconnected

	public:
		bool has_usb_pullup() {
			return false;
		}

		Pin get_usb_pullup() {
			return null_pin;
		}

		uint8_t get_num_buttons() {
			return num_buttons;
		}

		Pin* get_button_input(uint8_t index) {
			if(index < num_buttons) {
				return &button_inputs[index];
			}
		}

		Pin* get_button_led(uint8_t index) {
			if (index < num_buttons) {
				return &button_leds[index];
			}
		}

		uint8_t get_num_leds() {
			return 0;
		}

		Pin* get_led(uint8_t index) {
			return &null_pin;
		}

		bool has_version_check() {
			return true;
		}

		Pin* get_version_check() {
			return &version_pin;
		}
};

class Arcin_Pins : public Pin_Definition {
	private:
		Pin usb_pu = GPIOA[15];

		uint8_t num_buttons = 11;

		Pin button_inputs[11] = {
			GPIOB[0], GPIOB[1], GPIOB[2], GPIOB[3], GPIOB[4], GPIOB[5],
			GPIOB[6], GPIOB[7], GPIOB[8], GPIOB[9], GPIOB[10]};
		Pin button_leds[11] = {
			GPIOC[0], GPIOC[1], GPIOC[2], GPIOC[3], GPIOC[4], GPIOC[5],
			GPIOC[6], GPIOC[7], GPIOC[8], GPIOC[9], GPIOC[10]};

		Pin leds[2] = { GPIOA[8], GPIOA[9] };

		Pin null_pin = GPIOF[4];	// Unused and unconnected

	public:
		bool has_usb_pullup() {
			return true;
		}

		Pin get_usb_pullup() {
			return usb_pu;
		}

		uint8_t get_num_buttons() {
			return num_buttons;
		}

		Pin* get_button_input(uint8_t index) {
			if(index < num_buttons) {
				return &button_inputs[index];
			}
		}

		Pin* get_button_led(uint8_t index) {
			if (index < num_buttons) {
				return &button_leds[index];
			}
		}

		uint8_t get_num_leds() {
			return 2;
		}

		Pin* get_led(uint8_t index) {
			if(index < 2) {
				return &leds[index];
			}
			return &null_pin;
		}

		bool has_version_check() {
			return false;
		}

		Pin* get_version_check() {
			return &null_pin;
		}
};



#if defined(ROXY)

Pin rgb_sck = GPIOC[10];
Pin rgb_mosi = GPIOC[12];

Pin spi1_sck = GPIOB[3];
Pin spi1_mosi = GPIOB[5];

Pin ws_data = GPIOC[12];

Pin ps_ack = GPIOB[11];
Pin ps_ss = GPIOB[12];
Pin ps_sck = GPIOB[13];
Pin ps_miso = GPIOB[14];
Pin ps_mosi = GPIOB[15];

const char16_t mfg_name[] = u"\u0312VeroxZik";
const char product_name[] = "Roxy";

#define MAX_BUTTONS 12

const char led_names[][14] = {
	"Button 1 LED",
	"Button 2 LED",
	"Button 3 LED",
	"Button 4 LED",
	"Button 5 LED",
	"Button 6 LED",
	"Button 7 LED",
	"Button 8 LED",
	"Button 9 LED",
	"Button 10 LED",
	"Button 11 LED",
	"Button 12 LED",
	"LED 1",
	"RGB 1 (Red)",
	"RGB 1 (Green)",
	"RGB 1 (Blue)",
	"RGB 2 (Red)",
	"RGB 2 (Green)",
	"RGB 2 (Blue)"
};

Roxy_v11_Pins roxy_v11_pins;
Roxy_v20_Pins roxy_v20_pins;
Pin_Definition* current_pins = &roxy_v20_pins;

#elif defined(ARCIN)

// TODO CHANGE TO SHARE PS2 SPI PINS
Pin rgb_sck = GPIOC[10];
Pin rgb_mosi = GPIOC[12];

// UNUSED, just here so it builds
Pin spi1_sck = GPIOB[3];
Pin spi1_mosi = GPIOB[5];

Pin ws_data = GPIOB[8];

Pin ps_ack = GPIOB[11];
Pin ps_ss = GPIOB[12];
Pin ps_sck = GPIOB[13];
Pin ps_miso = GPIOB[14];
Pin ps_mosi = GPIOB[15];

const char16_t mfg_name[] = u"\u0308zyp";
const char product_name[] = "arcin (Roxy fw)";

#define MAX_BUTTONS 11

const char led_names[][14] = {
	"Button 1 LED",
	"Button 2 LED",
	"Button 3 LED",
	"Button 4 LED",
	"Button 5 LED",
	"Button 6 LED",
	"Button 7 LED",
	"Button 8 LED",
	"Button 9 LED",
	"Button 10 LED",
	"Button 11 LED",
	"Unused",
	"LED 1",
	"RGB (Red)",
	"RGB (Green)",
	"RGB (Blue)",
	"LED 2",
	"Unused",
  "Unused"
};

Arcin_Pins arcin_pins;
Pin_Definition* current_pins = &arcin_pins;

#else
#error "Board not defined."

#endif

#endif