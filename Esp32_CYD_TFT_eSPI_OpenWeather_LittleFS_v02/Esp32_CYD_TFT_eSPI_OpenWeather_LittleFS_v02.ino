/*
  The sketch is displaying a nice Internet Weather Station with 3 4 days forecast on an
  ESP32-Cheap Yellow Display (CYD) with 2.8-inch TFT display and 320x240 pixels resolution.

  The weather data is retrieved from OpenWeatherMap.org service and you need to get your own 
  (free) API key before running the sketch.

  Please add the key, your Wi-Fi credentials, the location coordinates for the weather information and 
  additional data about your Timezone in the settings file: 'All_Settings.h'.

  The code is not written by me, but from Daniel Eichhorn (https://blog.squix.ch) and was 
  adapted by Bodmer as an example for his OpenWeather library (https://github.com/Bodmer/OpenWeather/).

  I changed lines of code to crrect some issues, for more information see: https://github.com/Bodmer/OpenWeather/issues/26
  1):
  Search for: String date = "Updated: " + strDate(local_time); 
  Change to:  String date = "Updated: " + strDate(now());
  2)
  Search for: if ( today && id/100 == 8 && (forecast->dt[0] < forecast->sunrise || forecast->dt[0] > forecast->sunset)) id += 1000;
  Change to:  if ( today && id/100 == 8 && (now() < forecast->sunrise || now() > forecast->sunset)) id += 1000;

  The code is optimized to run on the Cheap Yellow Display ('CYD') with a screen resolution of 320 x 240 pixels in Landscape
  orientation, that is the 'ESP32-2432S028R' variant.

  The weather icons and fonts are stored in ESP32's LittleFS filesystem, so you need to upload the files in the 'data' subfolder
  first before uploading the code and starting the device.
  See 'Upload_To_LittleFS.md' for more details about this.

              >>>       IMPORTANT TO PREVENT CRASHES      <<<
  >>>>>>  Set LittleFS to at least 1.5Mbytes before uploading files  <<<<<<  

  The sketch is using the TFT_eSPI library by Bodmer (https://github.com/Bodmer/TFT_eSPI), so please select the correct
  'User_Setups' file in the library folder. I prepare two files (see my GitHub Repository) that should work:
  For older device with one USB connector (chip driver ILI9341) use: 
    Setup801_ESP32_CYD_ILI9341_240x320.h
  New devices with two USB connectors (chip driver ST7789) require:
    Setup805_ESP32_CYD_ST7789_240x320.h

  Original by Daniel Eichhorn, see license at end of file.

*/

/*
  This sketch uses font files created from the Noto family of fonts as bitmaps
  generated from these fonts may be freely distributed:
  https://www.google.com/get/noto/

  A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
  https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font
  New fonts can be generated to include language specific characters. The Noto family
  of fonts has an extensive character set coverage.

  Json streaming parser (do not use IDE library manager version) to use is here:
  https://github.com/Bodmer/JSON_Decoder
*/

#define SERIAL_MESSAGES  // For serial output weather reports
//#define SCREEN_SERVER   // For dumping screen shots from TFT
//#define RANDOM_LOCATION // Test only, selects random weather location every refresh
//#define FORMAT_LittleFS   // Wipe LittleFS and all files!

const char* PROGRAM_VERSION = "ESP32 CYD OpenWeatherMap LittleFS V02";

#include <FS.h>
#include <LittleFS.h>

#define AA_FONT_SMALL "fonts/NSBold15"  // 15 point Noto sans serif bold
#define AA_FONT_LARGE "fonts/NSBold36"  // 36 point Noto sans serif bold
/***************************************************************************************
**                          Load the libraries and settings
***************************************************************************************/
#include <Arduino.h>

#include <SPI.h>
#include <TFT_eSPI.h>  // https://github.com/Bodmer/TFT_eSPI

// Additional functions
#include "GfxUi.h"  // Attached to this sketch

//*****************************************************************************************
// Light Detecting Resistor - used to PWM control pin 21 (LED brightness)
//*****************************************************************************************
#define LDR_PIN 34
#define LDR_PWM_CHANNEL 0
#define LDR_PWM_FREQ 5000
#define LDR_PWM_RESOLUTION 8  // 0-255 brightness range

int minBrightness = 40;    // Dimmest allowed level (avoid going fully black)
int maxBrightness = 255;   // Brightest level

int currentBrightness = 255;
uint32_t lastBacklightUpdate = 0;
const uint32_t BACKLIGHT_UPDATE_INTERVAL = 500;  // ms between brightness checks
//******************************************************************************************


// Choose library to load
#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_ARCH_RP2040)
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#else
#include <WiFiNINA.h>
#endif
#else  // ESP32
#include <WiFi.h>
#endif


// check All_Settings.h for adapting to your needs
#include "All_Settings.h"

#include <JSON_Decoder.h>  // https://github.com/Bodmer/JSON_Decoder

#include <OpenWeather.h>  // Latest here: https://github.com/Bodmer/OpenWeather

#include "NTP_Time.h"  // Attached to this sketch, see that tab for library needs
// Time zone correction library: https://github.com/JChristensen/Timezone

/****** Runtime-selectable timezone (replaces the old compile-time TIMEZONE macro) **/
const char* tzNames[] = { "UK", "euCET", "ausET", "usET", "usCT", "usMT", "usAZ", "usPT" };
Timezone* tzObjects[] = { &UK, &euCET, &ausET, &usET, &usCT, &usMT, &usAZ, &usPT };
const int NUM_TIMEZONES = 8;

Timezone* activeTimezone = &usET;  // Default until loadConfig() runs
#define TIMEZONE (*activeTimezone)
/********************************************************************************/




/**************************************************************************************
**                          Define the globals and class instances
***************************************************************************************/

TFT_eSPI tft = TFT_eSPI();  // Invoke custom library

OW_Weather ow;  // Weather forecast library instance

OW_forecast* forecast;

boolean booted = true;

GfxUi ui = GfxUi(&tft);  // Jpeg and bmpDraw functions

long lastDownloadUpdate = millis();

uint32_t lastNTPSync = 0;
const uint32_t NTP_RESYNC_INTERVAL = 6UL * 60UL * 60UL * 1000UL;  // 6 hours

/***************************************************************************************
**                          Declare prototypes
***************************************************************************************/
void loadConfig();
void saveConfig();
void runWiFiManager(bool forcePortal);   // <- note: also needs the (bool) parameter now
void setupWiFiManagerParams();           // <- this one was missing entirely
void updateBacklight();
void updateData();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawCurrentWeather();
void drawForecast();
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
const char* getMeteoconIcon(uint16_t id, bool today);
void drawAstronomy();
void drawSeparator(uint16_t y);
void fillSegment(int x, int y, int start_angle, int sub_angle, int r, unsigned int colour);
String strDate(time_t unixTime);
String strTime(time_t unixTime);
void printWeather(void);
int leftOffset(String text, String sub);
int rightOffset(String text, String sub);
int splitIndex(String text);
int getNextDayIndex(void);

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Stop further decoding as image is running off bottom of screen
  if (y >= tft.height()) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

/***************************************************************************************
**                          Setup
***************************************************************************************/
void setup() {
  Serial.begin(115200);
  loadConfig(); //load config variables: timezone, wifi, etc
  delay(500);
  Serial.println(PROGRAM_VERSION);

setCpuFrequencyMhz(240);

  tft.begin();
  //***************** LDR *****************************************************
  analogSetAttenuation(ADC_0db);  // Improves LDR reading sensitivity
  ledcSetup(LDR_PWM_CHANNEL, LDR_PWM_FREQ, LDR_PWM_RESOLUTION);
  ledcAttachPin(TFT_BL, LDR_PWM_CHANNEL);
  ledcWrite(LDR_PWM_CHANNEL, maxBrightness);  // Start at full brightness
  //***************************************************************************
  tft.setRotation(0);  // For 320x480 screen
  tft.fillScreen(TFT_BLACK);

  if (!LittleFS.begin()) {
    Serial.println("Flash FS initialisation failed!");
    while (1) yield();  // Stay here twiddling thumbs waiting
  }
  Serial.println("\nFlash FS available!");

// Enable if you want to erase LittleFS, this takes some time!
// then disable and reload sketch to avoid reformatting on every boot!
#ifdef FORMAT_LittleFS
  tft.setTextDatum(BC_DATUM);  // Bottom Centre datum
  tft.drawString("Formatting LittleFS, so wait!", 120, 195);
  LittleFS.format();
#endif

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);
  TJpgDec.setSwapBytes(true);  // May need to swap the jpg colour bytes (endianess)

  // Draw splash screen
  if (LittleFS.exists("/splash/OpenWeather.jpg") == true) {
    TJpgDec.drawFsJpg(0, 40, "/splash/OpenWeather.jpg", LittleFS);
  }

  delay(2000);

  // Clear bottom section of screen
  tft.fillRect(0, 206, 240, 320 - 206, TFT_BLACK);

  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.setTextDatum(BC_DATUM);  // Bottom Centre datum
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

  tft.drawString("Original by: blog.squix.org", 120, 260);
  tft.drawString("Adapted by: Bodmer", 120, 280);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  delay(2000);

  tft.fillRect(0, 206, 240, 320 - 206, TFT_BLACK);

  tft.drawString("Connecting to WiFi", 120, 240);
  tft.setTextPadding(240);  // Pad next drawString() text to full width to over-write old text

    
    pinMode(0, INPUT_PULLUP);
    setupWiFiManagerParams();
    runWiFiManager(false);
    /**********************************************/

  Serial.println();
  tft.setTextDatum(BC_DATUM);
  tft.setTextPadding(240);        // Pad next drawString() text to full width to over-write old text
  tft.drawString(" ", 120, 220);  // Clear line above using set padding width
  tft.drawString("Fetching weather data...", 120, 240);
  

  // Fetch the time
  udp.begin(localPort);
  syncTime();

  tft.unloadFont();
}

/***************************************************************************************
**                          Loop
***************************************************************************************/
void loop() {

  // Check for long press on Boot button to start webserver & prompt for settings
  static uint32_t buttonPressStart = 0;
  if (digitalRead(0) == LOW) {
    if (buttonPressStart == 0) buttonPressStart = millis();
    if (millis() - buttonPressStart > 3000) {
      Serial.println("Button held 3s - entering config portal");
      tft.fillScreen(TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.loadFont(AA_FONT_SMALL, LittleFS);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("Entering WiFi Setup...", 120, 160);
      tft.unloadFont();
      runWiFiManager(true);
      buttonPressStart = 0;
      booted = true;  // force a full weather refresh after reconnecting
    }
  } else {
    buttonPressStart = 0;
  }

  // Check if we should update weather information
  if (booted || (millis() - lastDownloadUpdate > 1000UL * UPDATE_INTERVAL_SECS)) {
    updateData();
    lastDownloadUpdate = millis();
  }

  // Update displayed time every minute
  if (booted || minute() != lastMinute) {
    drawTime();
    lastMinute = minute();
#ifdef SCREEN_SERVER
    screenServer();
#endif
  }

  // Re-sync with NTP server periodically, not every minute
  if (booted || (millis() - lastNTPSync > NTP_RESYNC_INTERVAL)) {
    syncTime();
    lastNTPSync = millis();
  }

  // Check backlight brightness every loop iteration
  if (millis() - lastBacklightUpdate > BACKLIGHT_UPDATE_INTERVAL) {
    updateBacklight();
    lastBacklightUpdate = millis();
  }

  booted = false;
}

/***************************************************************************************
**                          Fetch the weather data  and update screen
***************************************************************************************/
// Update the Internet based information and update screen
void updateData() {
  // booted = true;  // Test only
  // booted = false; // Test only

  if (booted) drawProgress(20, "Updating time...");
  else fillSegment(22, 22, 0, (int)(20 * 3.6), 16, TFT_NAVY);

  if (booted) drawProgress(50, "Updating conditions...");
  else fillSegment(22, 22, 0, (int)(50 * 3.6), 16, TFT_NAVY);

  // Create the structure that holds the retrieved weather
  forecast = new OW_forecast;

#ifdef RANDOM_LOCATION  // Randomly choose a place on Earth to test icons etc
  String latitude = "";
  latitude = (random(180) - 90);
  String longitude = "";
  longitude = (random(360) - 180);
  Serial.print("Lat = ");
  Serial.print(latitude);
  Serial.print(", Lon = ");
  Serial.println(longitude);
#endif

  bool parsed = ow.getForecast(forecast, api_key, latitude, longitude, units, language);

  if (parsed) Serial.println("Data points received");
  else Serial.println("Failed to get data points");

  //Serial.print("Free heap = "); Serial.println(ESP.getFreeHeap(), DEC);

  printWeather();  // For debug, turn on output with #define SERIAL_MESSAGES

  if (booted) {
    drawProgress(100, "Done...");
    delay(2000);
    tft.fillScreen(TFT_BLACK);
  } else {
    fillSegment(22, 22, 0, 360, 16, TFT_NAVY);
    fillSegment(22, 22, 0, 360, 22, TFT_BLACK);
  }

  if (parsed) {
    tft.loadFont(AA_FONT_SMALL, LittleFS);
    drawCurrentWeather();
    drawForecast();
    drawAstronomy();
    tft.unloadFont();

    // Update the temperature here so we don't need to keep
    // loading and unloading font which takes time
    tft.loadFont(AA_FONT_LARGE, LittleFS);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);

    // Font ASCII code 0xB0 is a degree symbol, but o used instead in small font
    tft.setTextPadding(tft.textWidth(" -88"));  // Max width of values

    String weatherText = "";
    weatherText = String(forecast->temp[0], 0);  // Make it integer temperature
    tft.drawString(weatherText, 215, 95);        //  + "°" symbol is big... use o in small font
    tft.unloadFont();
  } else {
    Serial.println("Failed to get weather");
  }

  // Delete to free up space
  delete forecast;
}

/***************************************************************************************
**                          Update progress bar
***************************************************************************************/
void drawProgress(uint8_t percentage, String text) {
  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(240);
  tft.drawString(text, 120, 260);

  ui.drawProgressBar(10, 269, 240 - 20, 15, percentage, TFT_WHITE, TFT_BLUE);

  tft.setTextPadding(0);
  tft.unloadFont();
}

/***************************************************************************************
**                          Draw the clock digits
***************************************************************************************/
//Updated to show 12hr time not 24hr time
void drawTime() {
  tft.loadFont(AA_FONT_LARGE, LittleFS);

  // Convert UTC to local time, returns zone code in tz1_Code, e.g "GMT"
  time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);

  uint8_t h = hour(local_time);
  bool isPM = (h >= 12);
  uint8_t h12 = h % 12;
  if (h12 == 0) h12 = 12;

  String timeNow = "";
  timeNow += h12;
  timeNow += ":";
  if (minute(local_time) < 10) timeNow += "0";
  timeNow += minute(local_time);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" 44:44 "));  // String width + margin
  tft.drawString(timeNow, 120, 53);

  tft.unloadFont();

  // Draw AM/PM in the small font, since the large clock font has no letters
  tft.loadFont(AA_FONT_SMALL, LittleFS);
  tft.setTextDatum(BL_DATUM);  // Bottom-left datum
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" PM "));
  int timeWidth = tft.textWidth(timeNow);
  tft.drawString(isPM ? "PM" : "AM", 120 + (timeWidth / 2) + 25, 53);
  tft.unloadFont();

  tft.loadFont(AA_FONT_LARGE, LittleFS);  // reload so drawSeparator/rest of code isn't affected
  drawSeparator(51);
  tft.setTextPadding(0);
  tft.unloadFont();
}

// void drawTime() {
//   tft.loadFont(AA_FONT_LARGE, LittleFS);

//   // Convert UTC to local time, returns zone code in tz1_Code, e.g "GMT"
//   time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);

//   String timeNow = "";

//   if (hour(local_time) < 10) timeNow += "0";
//   timeNow += hour(local_time);
//   timeNow += ":";
//   if (minute(local_time) < 10) timeNow += "0";
//   timeNow += minute(local_time);

//   tft.setTextDatum(BC_DATUM);
//   tft.setTextColor(TFT_YELLOW, TFT_BLACK);
//   tft.setTextPadding(tft.textWidth(" 44:44 "));  // String width + margin
//   tft.drawString(timeNow, 120, 53);

//   drawSeparator(51);

//   tft.setTextPadding(0);

//   tft.unloadFont();
// }

/***************************************************************************************
**                          Draw the current weather
***************************************************************************************/
void drawCurrentWeather() {
  time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);
  //String date = "Updated: " + strDate(local_time);
  String date = "Updated: " + strDate(now());  // see isue https://github.com/Bodmer/OpenWeather/issues/26
  String weatherText = "None";

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" Updated: Mmm 44 44:44 "));  // String width + margin
  tft.drawString(date, 120, 16);

  String weatherIcon = "";

  String currentSummary = forecast->main[0];
  currentSummary.toLowerCase();

  weatherIcon = getMeteoconIcon(forecast->id[0], true);

  ui.drawBmp("/icon/" + weatherIcon + ".bmp", 0, 53);

  // Weather Text
  if (language == "en")
    weatherText = forecast->main[0];
  else
    weatherText = forecast->description[0];

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);

  int splitPoint = 0;
  int xpos = 235;
  splitPoint = splitIndex(weatherText);

  tft.setTextPadding(xpos - 100);  // xpos - icon width
  if (splitPoint) tft.drawString(weatherText.substring(0, splitPoint), xpos, 69);
  else tft.drawString(" ", xpos, 69);
  tft.drawString(weatherText.substring(splitPoint), xpos, 86);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(0);
  if (units == "metric") tft.drawString("oC", 237, 95);
  else tft.drawString("oF", 237, 95);

  //Temperature large digits added in updateData() to save swapping font here

//moved the wind speed number above the arrow icon
tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
weatherText = String(forecast->wind_speed[0], 0);

if (units == "metric") weatherText += " m/s";
else weatherText += " mph";

tft.setTextDatum(BC_DATUM);  // changed from TC_DATUM to bottom-center
tft.setTextPadding(tft.textWidth("888 m/s"));  // Max string length?
tft.drawString(weatherText, 124, 84);  // moved from y=136 to y=84, just above the icon


  // tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  // weatherText = String(forecast->wind_speed[0], 0);

  // if (units == "metric") weatherText += " m/s";
  // else weatherText += " mph";

  // tft.setTextDatum(TC_DATUM);
  // tft.setTextPadding(tft.textWidth("888 m/s"));  // Max string length?
  // tft.drawString(weatherText, 124, 136);

//****************Millibar units and color code the pressure 
tft.setTextDatum(TR_DATUM);
tft.setTextPadding(tft.textWidth(" 8888hPa"));  // Max string length?

weatherText = String(forecast->pressure[0], 0);
weatherText += " hPa";

float pressureVal = forecast->pressure[0];
if (pressureVal < 980) {
  tft.setTextColor(TFT_RED, TFT_BLACK);
} else if (pressureVal < 1000) {
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
} else {
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
}

tft.drawString(weatherText, 230, 136);

/*********************orginal - just change the label, but results in wrong info for imperial*/
  // if (units == "imperial") {
  //   weatherText = forecast->pressure[0];
  //   weatherText += " in";
  // } else {
  //   weatherText = String(forecast->pressure[0], 0);
  //   weatherText += " hPa";
  // }
  //tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  //tft.setTextDatum(TR_DATUM);
  //tft.setTextPadding(tft.textWidth(" 8888hPa"));  // Max string length?
  //tft.drawString(weatherText, 230, 136);

  int windAngle = (forecast->wind_deg[0] + 22.5) / 45;
  if (windAngle > 7) windAngle = 0;
  String wind[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
  ui.drawBmp("/wind/" + wind[windAngle] + ".bmp", 101, 86);

  drawSeparator(153);

  tft.setTextDatum(TL_DATUM);  // Reset datum to normal
  tft.setTextPadding(0);       // Reset padding width to none
}

/***************************************************************************************
**                          Draw the 4 forecast columns
***************************************************************************************/
// draws the three forecast columns
void drawForecast() {
  int8_t dayIndex = getNextDayIndex();

  drawForecastDetail(8, 171, dayIndex);
  dayIndex += 8;
  drawForecastDetail(66, 171, dayIndex);  // was 95
  dayIndex += 8;
  drawForecastDetail(124, 171, dayIndex);  // was 180
  dayIndex += 8;
  drawForecastDetail(182, 171, dayIndex);  // was 180
  drawSeparator(171 + 69);
}

/***************************************************************************************
**                          Draw 1 forecast column at x, y
***************************************************************************************/
// helper for the forecast columns
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {

  if (dayIndex >= MAX_DAYS * 8) return;

  String day = shortDOW[weekday(TIMEZONE.toLocal(forecast->dt[dayIndex + 4], &tz1_Code))];
  day.toUpperCase();

  tft.setTextDatum(BC_DATUM);

  tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("WWW"));
  tft.drawString(day, x + 25, y);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("-88   -88"));

  // Find the temperature min and max during the day
  float tmax = -9999;
  float tmin = 9999;
  for (int i = 0; i < 8; i++)
    if (forecast->temp_max[dayIndex + i] > tmax) tmax = forecast->temp_max[dayIndex + i];
  for (int i = 0; i < 8; i++)
    if (forecast->temp_min[dayIndex + i] < tmin) tmin = forecast->temp_min[dayIndex + i];

  String highTemp = String(tmax, 0);
  String lowTemp = String(tmin, 0);
  tft.drawString(highTemp + "/" + lowTemp, x + 25, y + 17);

  String weatherIcon = getMeteoconIcon(forecast->id[dayIndex + 4], false);

  ui.drawBmp("/icon50/" + weatherIcon + ".bmp", x, y + 18);

  tft.setTextPadding(0);  // Reset padding width to none
}

/***************************************************************************************
**                          Draw Sun rise/set, Moon, cloud cover and humidity
***************************************************************************************/
void drawAstronomy() {

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" Last qtr "));

  time_t local_time = TIMEZONE.toLocal(forecast->dt[0], &tz1_Code);
  uint16_t y = year(local_time);
  uint8_t m = month(local_time);
  uint8_t d = day(local_time);
  uint8_t h = hour(local_time);
  int ip;
  uint8_t icon = moon_phase(y, m, d, h, &ip);

  tft.drawString(moonPhase[ip], 120, 319);
  ui.drawBmp("/moon/moonphase_L" + String(icon) + ".bmp", 120 - 30, 318 - 16 - 60);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(0);  // Reset padding width to none
  tft.drawString(sunStr, 40, 270);

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" 88:88 "));

  String rising = strTime(forecast->sunrise) + " ";
  int dt = rightOffset(rising, ":");  // Draw relative to colon to them aligned
  tft.drawString(rising, 40 + dt, 290);

  String setting = strTime(forecast->sunset) + " ";
  dt = rightOffset(setting, ":");
  tft.drawString(setting, 40 + dt, 305);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(cloudStr, 195, 260);  // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ?

  String cloudCover = "";
  cloudCover += forecast->clouds_all[0];
  cloudCover += "%";

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" 100%"));
  tft.drawString(cloudCover, 210, 277);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(humidityStr, 195, 300 - 2);  // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ?

  String humidity = "";
  humidity += forecast->humidity[0];
  humidity += "%";

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("100%"));
  tft.drawString(humidity, 210, 315);

  tft.setTextPadding(0);  // Reset padding width to none
}

/***************************************************************************************
**                          Get the icon file name from the index number
***************************************************************************************/
const char* getMeteoconIcon(uint16_t id, bool today) {
  // if ( today && id/100 == 8 && (forecast->dt[0] < forecast->sunrise || forecast->dt[0] > forecast->sunset)) id += 1000;
  if (today && id / 100 == 8 && (now() < forecast->sunrise || now() > forecast->sunset)) id += 1000;
  // see issue https://github.com/Bodmer/OpenWeather/issues/26
  if (id / 100 == 2) return "thunderstorm";
  if (id / 100 == 3) return "drizzle";
  if (id / 100 == 4) return "unknown";
  if (id == 500) return "lightRain";
  else if (id == 511) return "sleet";
  else if (id / 100 == 5) return "rain";
  if (id >= 611 && id <= 616) return "sleet";
  else if (id / 100 == 6) return "snow";
  if (id / 100 == 7) return "fog";
  if (id == 800) return "clear-day";
  if (id == 801) return "partly-cloudy-day";
  if (id == 802) return "cloudy";
  if (id == 803) return "cloudy";
  if (id == 804) return "cloudy";
  if (id == 1800) return "clear-night";
  if (id == 1801) return "partly-cloudy-night";
  if (id == 1802) return "cloudy";
  if (id == 1803) return "cloudy";
  if (id == 1804) return "cloudy";

  return "unknown";
}

/***************************************************************************************
**                          Draw screen section separator line
***************************************************************************************/
// if you don't want separators, comment out the tft-line
void drawSeparator(uint16_t y) {
  tft.drawFastHLine(10, y, 240 - 2 * 10, 0x4228);
}

/***************************************************************************************
**                          Determine place to split a line line
***************************************************************************************/
// determine the "space" split point in a long string
int splitIndex(String text) {
  uint16_t index = 0;
  while ((text.indexOf(' ', index) >= 0) && (index <= text.length() / 2)) {
    index = text.indexOf(' ', index) + 1;
  }
  if (index) index--;
  return index;
}

/***************************************************************************************
**                          Right side offset to a character
***************************************************************************************/
// Calculate coord delta from end of text String to start of sub String contained within that text
// Can be used to vertically right align text so for example a colon ":" in the time value is always
// plotted at same point on the screen irrespective of different proportional character widths,
// could also be used to align decimal points for neat formatting
int rightOffset(String text, String sub) {
  int index = text.indexOf(sub);
  return tft.textWidth(text.substring(index));
}

/***************************************************************************************
**                          Left side offset to a character
***************************************************************************************/
// Calculate coord delta from start of text String to start of sub String contained within that text
// Can be used to vertically left align text so for example a colon ":" in the time value is always
// plotted at same point on the screen irrespective of different proportional character widths,
// could also be used to align decimal points for neat formatting
int leftOffset(String text, String sub) {
  int index = text.indexOf(sub);
  return tft.textWidth(text.substring(0, index));
}

/***************************************************************************************
**                          Draw circle segment
***************************************************************************************/
// Draw a segment of a circle, centred on x,y with defined start_angle and subtended sub_angle
// Angles are defined in a clockwise direction with 0 at top
// Segment has radius r and it is plotted in defined colour
// Can be used for pie charts etc, in this sketch it is used for wind direction
#define DEG2RAD 0.0174532925  // Degrees to Radians conversion factor
#define INC 2                 // Minimum segment subtended angle and plotting angle increment (in degrees)
void fillSegment(int x, int y, int start_angle, int sub_angle, int r, unsigned int colour) {
  // Calculate first pair of coordinates for segment start
  float sx = cos((start_angle - 90) * DEG2RAD);
  float sy = sin((start_angle - 90) * DEG2RAD);
  uint16_t x1 = sx * r + x;
  uint16_t y1 = sy * r + y;

  // Draw colour blocks every INC degrees
  for (int i = start_angle; i < start_angle + sub_angle; i += INC) {

    // Calculate pair of coordinates for segment end
    int x2 = cos((i + 1 - 90) * DEG2RAD) * r + x;
    int y2 = sin((i + 1 - 90) * DEG2RAD) * r + y;

    tft.fillTriangle(x1, y1, x2, y2, x, y, colour);

    // Copy segment end to segment start for next segment
    x1 = x2;
    y1 = y2;
  }
}

/***************************************************************************************
**                          Get 3 hourly index at start of next day
***************************************************************************************/
int getNextDayIndex(void) {
  int index = 0;
  String today = forecast->dt_txt[0].substring(8, 10);
  for (index = 0; index < 9; index++) {
    if (forecast->dt_txt[index].substring(8, 10) != today) break;
  }
  return index;
}

/***************************************************************************************
**                          Print the weather info to the Serial Monitor
***************************************************************************************/
void printWeather(void) {
#ifdef SERIAL_MESSAGES
  Serial.println("Weather from OpenWeather\n");

  Serial.print("city_name           : ");
  Serial.println(forecast->city_name);
  Serial.print("sunrise             : ");
  Serial.println(strTime(forecast->sunrise));
  Serial.print("sunset              : ");
  Serial.println(strTime(forecast->sunset));
  Serial.print("Latitude            : ");
  Serial.println(ow.lat);
  Serial.print("Longitude           : ");
  Serial.println(ow.lon);
  // We can use the timezone to set the offset eventually...
  Serial.print("Timezone            : ");
  Serial.println(forecast->timezone);
  Serial.println();

  if (forecast) {
    Serial.println("###############  Forecast weather  ###############\n");
    for (int i = 0; i < (MAX_DAYS * 8); i++) {
      Serial.print("3 hourly forecast   ");
      if (i < 10) Serial.print(" ");
      Serial.print(i);
      Serial.println();
      Serial.print("dt (time)        : ");
      Serial.println(strTime(forecast->dt[i]));

      Serial.print("temp             : ");
      Serial.println(forecast->temp[i]);
      Serial.print("temp.min         : ");
      Serial.println(forecast->temp_min[i]);
      Serial.print("temp.max         : ");
      Serial.println(forecast->temp_max[i]);

      Serial.print("pressure         : ");
      Serial.println(forecast->pressure[i]);
      Serial.print("sea_level        : ");
      Serial.println(forecast->sea_level[i]);
      Serial.print("grnd_level       : ");
      Serial.println(forecast->grnd_level[i]);
      Serial.print("humidity         : ");
      Serial.println(forecast->humidity[i]);

      Serial.print("clouds           : ");
      Serial.println(forecast->clouds_all[i]);
      Serial.print("wind_speed       : ");
      Serial.println(forecast->wind_speed[i]);
      Serial.print("wind_deg         : ");
      Serial.println(forecast->wind_deg[i]);
      Serial.print("wind_gust        : ");
      Serial.println(forecast->wind_gust[i]);

      Serial.print("visibility       : ");
      Serial.println(forecast->visibility[i]);
      Serial.print("pop              : ");
      Serial.println(forecast->pop[i]);
      Serial.println();

      Serial.print("dt_txt           : ");
      Serial.println(forecast->dt_txt[i]);
      Serial.print("id               : ");
      Serial.println(forecast->id[i]);
      Serial.print("main             : ");
      Serial.println(forecast->main[i]);
      Serial.print("description      : ");
      Serial.println(forecast->description[i]);
      Serial.print("icon             : ");
      Serial.println(forecast->icon[i]);

      Serial.println();
    }
  }
#endif
}
/***************************************************************************************
**             Convert Unix time to a "local time" time string "12:34"
***************************************************************************************/
//updated to have 12hr with am/pm labels
String strTime(time_t unixTime) {
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);

  uint8_t h = hour(local_time);
  String ampm = (h < 12) ? "AM" : "PM";
  uint8_t h12 = h % 12;
  if (h12 == 0) h12 = 12;

  String localTime = "";
  localTime += h12;
  localTime += ":";
  if (minute(local_time) < 10) localTime += "0";
  localTime += minute(local_time);
  localTime += " " + ampm;

  return localTime;
}

// String strTime(time_t unixTime) {
//   time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);

//   String localTime = "";

//   if (hour(local_time) < 10) localTime += "0";
//   localTime += hour(local_time);
//   localTime += ":";
//   if (minute(local_time) < 10) localTime += "0";
//   localTime += minute(local_time);

//   return localTime;
// }

/***************************************************************************************
**  Convert Unix time to a local date + time string "Oct 16 17:18", ends with newline
***************************************************************************************/
String strDate(time_t unixTime) {
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);

  String localDate = "";

  localDate += monthShortStr(month(local_time));
  localDate += " ";
  localDate += day(local_time);
  localDate += " " + strTime(unixTime);

  return localDate;
}
/*************************************************************************************
*Backlight 
**************************************************************************************/
void updateBacklight() {
  int raw = analogRead(LDR_PIN);  // 0-4095

  // NOTE: verify direction against your board - see calibration note below
  int target = map(raw, 0, 4095, maxBrightness, minBrightness);
  target = constrain(target, minBrightness, maxBrightness);

  // Smooth the change so brightness doesn't jump abruptly
  currentBrightness = (currentBrightness * 3 + target) / 4;

  ledcWrite(LDR_PWM_CHANNEL, currentBrightness);

#ifdef SERIAL_MESSAGES
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    Serial.print("LDR raw: ");
    Serial.print(raw);
    Serial.print("  Backlight: ");
    Serial.println(currentBrightness);
    lastPrint = millis();
  }
#endif
}


/**The MIT License (MIT)
  Copyright (c) 2015 by Daniel Eichhorn
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYBR_DATUM HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
  See more at http://blog.squix.ch
*/

//  Changes made by Bodmer:

//  Minor changes to text placement and auto-blanking out old text with background colour padding
//  Moon phase text added (not provided by OpenWeather)
//  Forecast text lines are automatically split onto two lines at a central space (some are long!)
//  Time is printed with colons aligned to tidy display
//  Min and max forecast temperatures spaced out
//  New smart splash startup screen and updated progress messages
//  Display does not need to be blanked between updates
//  Icons nudged about slightly to add wind direction + speed
//  Barometric pressure added

//  Adapted to use the OpenWeather library: https://github.com/Bodmer/OpenWeather
//  Moon phase/rise/set (not provided by OpenWeather) replace with  and cloud cover humidity
//  Created and added new 100x100 and 50x50 pixel weather icons, these are in the
//  sketch data folder, press Ctrl+K to view
//  Add moon icons, eliminate all downloads of icons (may lose server!)
//  Adapted to use anti-aliased fonts, tweaked coords
//  Added forecast for 4th day
//  Added cloud cover and humidity in lieu of Moon rise/set
//  Adapted to be compatible with ESP32
