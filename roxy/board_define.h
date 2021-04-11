#ifndef BOARD_DEFINE_H
#define BOARD_DEFINE_H

#if defined(ROXY)

Pin usb_dm = GPIOA[11];
Pin usb_dp = GPIOA[12];

Pin button_inputs[] = {
  GPIOB[4], GPIOB[5], GPIOB[6], GPIOB[9], GPIOC[14], GPIOC[5],
  GPIOB[1], GPIOB[10], GPIOC[7], GPIOC[9], GPIOA[9], GPIOB[7]};
Pin button_leds[] = {
  GPIOA[15], GPIOC[11], GPIOB[3], GPIOC[13], GPIOC[15], GPIOB[0],
  GPIOB[2], GPIOC[6], GPIOC[8], GPIOA[8], GPIOA[10], GPIOB[8]};

Pin qe1a = GPIOA[0];
Pin qe1b = GPIOA[1];
Pin qe2a = GPIOA[6];
Pin qe2b = GPIOA[7];

Pin rgb_sck = GPIOC[10];
Pin rgb_mosi = GPIOC[12];

Pin ws_data = GPIOC[12];

Pin led1 = GPIOC[4];

Pin ver_check = GPIOC[0];

const char16_t mfg_name[] = u"\u0312VeroxZik";
const char product_name[] = "Roxy";

#define NUM_BUTTONS 12

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

#elif defined(ARCIN)

Pin usb_dm = GPIOA[11];
Pin usb_dp = GPIOA[12];
Pin usb_pu = GPIOA[15];

Pin button_inputs[] = {
  GPIOB[0], GPIOB[1], GPIOB[2], GPIOB[3], GPIOB[4], GPIOB[5],
  GPIOB[6], GPIOB[7], GPIOB[8], GPIOB[9], GPIOB[10]};
Pin button_leds[] = {
  GPIOC[0], GPIOC[1], GPIOC[2], GPIOC[3], GPIOC[4], GPIOC[5],
  GPIOC[6], GPIOC[7], GPIOC[8], GPIOC[9], GPIOC[10]};

Pin qe1a = GPIOA[0];
Pin qe1b = GPIOA[1];
Pin qe2a = GPIOA[6];
Pin qe2b = GPIOA[7];

// TODO CHANGE TO SHARE PS2 SPI PINS
Pin rgb_sck = GPIOC[10];
Pin rgb_mosi = GPIOC[12];

Pin ws_data = GPIOB[8];

Pin led1 = GPIOA[8];
Pin led2 = GPIOA[9];

const char16_t mfg_name[] = u"\u0308zyp";
const char product_name[] = "arcin (Roxy fw)";

#define NUM_BUTTONS 11

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

#else
#error "Board not defined."

#endif

#endif