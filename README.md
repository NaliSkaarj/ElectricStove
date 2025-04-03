# ElectricStove
Computerized furnace control.

# Hardware
* LCD_TFT_ILI9488 (480x320) + TOUCH XPT2046 + SDCard
* Digital thermocouple IC MAX6675
* Buzzer 5V

# Pins usage
### All used pins
Numbers on ESP32_Devkitc_V4: 5, 9, 12, 14, 18, 19, 21, 22, 23, 25, 33

### LCD
TFT_MISO    19<br/>
TFT_MOSI    23<br/>
TFT_SCLK    18<br/>
TFT_CS      22<br/>
TFT_DC      12<br/>
TFT_RST     21<br/>

### Touchscreen
TOUCH_CS    5<br/>

### Temperature chip (MAX6675)
HEATER_MAX6675_MISO     19<br/>
HEATER_MAX6675_CLK      18<br/>
HEATER_MAX6675_CS       9<br/>

### SD_Card
SD_CS 14<br/>

### Other pins
PID_PIN_RELAY           25<br/>
BUZZ_OUTPUT_PIN         33<br/>