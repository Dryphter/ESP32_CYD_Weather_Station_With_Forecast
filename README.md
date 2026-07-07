## Changes from base fork

### Bug fixes
- Fixed several incorrect library includes (`JsonListener.h`, `JsonStreamingParser.h`) that were leftover from an earlier version of the code and caused compile errors — `OpenWeather.h`/`JSON_Decoder.h` already provide everything needed.
- Fixed `TIMEZONE` macro in `All_Settings.h` — was pointing to a `TimeChangeRule` (`usEDT`) instead of the actual `Timezone` object (`usET`), causing a compile error on `.toLocal()`.
- Resolved a white-screen-after-WiFi-connect issue — traced to an incorrect ESP32 board selection in Arduino IDE (not a code issue).

### Display tweaks
- **Time format:** Main clock now displays in 12-hour format with AM/PM (previously 24-hour). AM/PM is drawn in the small font since the large clock font (`NSBold36.vlw`) only contains digits and a colon.
- **Forecast day labels:** Changed from orange to sky blue for better contrast/less yellow on screen.
- **Forecast temps:** Added a "/" separator between high and low temperature.
- **Wind speed label:** Moved above the wind direction arrow icon (previously below).
- **Barometric pressure:**
  - Removed inaccurate imperial (inHg) conversion — the OpenWeatherMap API always returns pressure in hPa regardless of the `units` setting, and the old code just relabeled the raw hPa value as "in" without converting it. Now always displays in hPa/millibars.
  - Added color coding based on pressure range: GREENYELLOW (≥1000 hPa, normal), ORANGE (980–1000 hPa, tropical storm range), RED (<980 hPa, severe low pressure).

### New feature
- **Auto-dimming backlight:** Added ambient light sensing via the onboard LDR (GPIO 34) to automatically adjust screen brightness via PWM (backlight pin GPIO 21), instead of a fixed always-on backlight.

### New Feature
- ** Added webconfig: webserver for user to enter wifi settings (SSID/Password), timezone, lattitude/longitude, OpenWeather API key, units (imperial/metric). Removed these parameters from All_settings.h so they no longer need to be defined upfront and compiled in.  To open the Webserver, hold the Boot button for 3-4 seconds once the weather station is running. Connect to WeatherStationSetup SSID and set parameters.



# ESP32 Cheap Yellow Display (CYD) Weather Station with 3 days Forecast

This is the accompanying repository for my article "Create an Internet Weather Station with 3 days Forecast on an ESP32 Cheap Yellow Display ("CYD")" available here: https://medium.com/@androidcrypto/create-an-internet-weather-station-with-3-days-forecast-on-an-esp32-cheap-yellow-display-cyd-15eb5c353b1d

For short - what is a "Cheap Yellow Display" ? This device was introduced some years ago and allowed for very fast development of projects where an ESP32, a TFT (optional Touch surface), an SD Card Reader and an RGB LED is required. The first version was equipped with a 2.8 inch large TFT display with **ILI9341** driver chip and **XPT2046** resistive Touch driver chip. Newer versions are sold with a **ST7789** display driver chip. Nowadays, the device is available with different display sizes (1.28 up to 7 inches) and driver chips, but I'm focusing on the 2.8 inch variants. The display has a size of **320 x 240** pixels in Landscape orientation. Most of the devices are driven by an ESP32 WROOM microcontroller, but I saw some others with an ESP32-S3 chip.

![Image 1](./images/esp32_cyd_weather_station_01_600h.png)

## Set up the TFT_eSPI library

Please don't forget to copy the files "*Setup801_ESP32_CYD_ILI9341_240x320.h*" and "*Setup805_ESP32_CYD_ST7789_240x320.h*" in the "User_Setups" folder of the TFT_eSPI library and edit the 
"*User_Setup_Select.h*" to include the set up, depending on your CYD board type (ILI9341 or ST7789).

## Required Libraries
````plaintext
TFT_eSPI Version: 2.4.3 *1) (https://github.com/Bodmer/TFT_eSPI)
OpenWeather Version: Feb 16, 2023 (https://github.com/Bodmer/OpenWeather)
JSON_Decoder Version: n.a. (https://github.com/Bodmer/JSON_Decoder)
TJpg_Decoder Version: 1.1.0 (https://github.com/Bodmer/TJpg_Decoder)
Timezone Version: 1.2.4 (https://github.com/JChristensen/Timezone)

*1) In case you encounter any problems with the TFT_eSPI library you should consider to use my forked TFT_eSPI library that solved some problems, see link below
````

Forked TFT_eSPI library by AndroidCrypto: https://github.com/AndroidCrypto/TFT_eSPI

## This sketch uses the LittleFS file system
The weather icons and font files are stored in the LittleFS filesystem. Before running the sketch you need to upload the files in the sketch subfolder 'data' to the ESP32, 
for e.g. with the arduino-littlefs-upload plugin (https://github.com/earlephilhower/arduino-littlefs-upload).

A Description can be found here: https://randomnerdtutorials.com/arduino-ide-2-install-esp32-littlefs/

## Development Environment
````plaintext
Arduino IDE Version 2.3.6 (Windows)
arduino-esp32 boards Version 3.2.0 (https://github.com/espressif/arduino-esp32)
````
