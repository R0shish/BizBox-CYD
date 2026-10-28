// TFT_eSPI setup for the ESP32 Cheap Yellow Display (ESP32-2432S028, ILI9341).
// CI copies this over TFT_eSPI's own User_Setup.h. You can do the same
// locally (back up the original first: Library Manager updates overwrite it).
#define USER_SETUP_INFO "BizBox CYD"

#define ILI9341_2_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Display on the HSPI pins. Touch (XPT2046) is bit-banged by the sketch on
// GPIO 25/32/33/36/39, so TOUCH_CS is deliberately not defined here.
#define USE_HSPI_PORT
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
// Backlight (GPIO21) is PWM-dimmed by the sketch, so TFT_BL is not defined.

// The UI uses anti-aliased fonts from fonts.h; GLCD is TFT_eSPI's minimum.
#define LOAD_GLCD
#define SMOOTH_FONT

#define SPI_FREQUENCY      55000000
#define SPI_READ_FREQUENCY 20000000
