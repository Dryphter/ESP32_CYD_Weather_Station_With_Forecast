#include <Preferences.h>
#include <WiFiManager.h>
#include <vector>

Preferences preferences;
WiFiManager wm;

bool shouldSaveConfig = false;
int cfg_tzIndex = 3;  // matches "usET" in tzNames[]

// Buffers and parameter objects moved to global scope to avoid
// "WiFiManagerParameter is out of scope" issues
char param_lat[16];
char param_lon[16];
char param_units[10] = "imperial";
char param_tz[10] = "usET";
char param_apikey[40];

WiFiManagerParameter custom_lat("lat", "Latitude", param_lat, 15);
WiFiManagerParameter custom_lon("lon", "Longitude", param_lon, 15);
WiFiManagerParameter custom_units("units", "Units (imperial or metric)", param_units, 9);
WiFiManagerParameter custom_tz("tz", "Timezone (UK, euCET, ausET, usET, usCT, usMT, usAZ, usPT)", param_tz, 9);
WiFiManagerParameter custom_apikey("apikey", "OpenWeatherMap API Key", param_apikey, 39);

void saveParamsCallback() {
  shouldSaveConfig = true;
}

void loadConfig() {
  preferences.begin("weatherCfg", false);
  latitude  = preferences.getString("lat", latitude);
  longitude = preferences.getString("lon", longitude);
  units     = preferences.getString("units", units);
  api_key   = preferences.getString("apikey", api_key);
  cfg_tzIndex = preferences.getInt("tzIndex", cfg_tzIndex);
  preferences.end();

  if (cfg_tzIndex < 0 || cfg_tzIndex >= NUM_TIMEZONES) cfg_tzIndex = 3;
  activeTimezone = tzObjects[cfg_tzIndex];
}

void saveConfig() {
  preferences.begin("weatherCfg", false);
  preferences.putString("lat", latitude);
  preferences.putString("lon", longitude);
  preferences.putString("units", units);
  preferences.putString("apikey", api_key);
  preferences.putInt("tzIndex", cfg_tzIndex);
  preferences.end();
}

bool paramsInitialized = false;

void setupWiFiManagerParams() {
  if (paramsInitialized) return;  // guard against ever running twice

  std::vector<const char*> menu = { "wifi", "param", "info", "sep", "restart", "exit" };
  wm.setMenu(menu);

  wm.addParameter(&custom_lat);
  wm.addParameter(&custom_lon);
  wm.addParameter(&custom_units);
  wm.addParameter(&custom_tz);
wm.addParameter(&custom_apikey);

  wm.setSaveParamsCallback(saveParamsCallback);
  wm.setBreakAfterConfig(true);
  wm.setConfigPortalTimeout(180);

  paramsInitialized = true;
}

void runWiFiManager(bool forcePortal) {
strncpy(param_lat, latitude.c_str(), sizeof(param_lat));
strncpy(param_lon, longitude.c_str(), sizeof(param_lon));
strncpy(param_units, units.c_str(), sizeof(param_units));
strncpy(param_tz, tzNames[cfg_tzIndex], sizeof(param_tz));
strncpy(param_apikey, api_key.c_str(), sizeof(param_apikey));
custom_lat.setValue(param_lat, 15);
custom_lon.setValue(param_lon, 15);
custom_units.setValue(param_units, 9);
custom_tz.setValue(param_tz, 9);
custom_apikey.setValue(param_apikey, 39);

  bool connected;
  if (forcePortal) {
    Serial.println("Long-press detected - forcing config portal");
    connected = wm.startConfigPortal("WeatherStationSetup");
  } else {
    connected = wm.autoConnect("WeatherStationSetup");
  }

if (shouldSaveConfig) {
  latitude  = String(custom_lat.getValue());
  longitude = String(custom_lon.getValue());
  units     = String(custom_units.getValue());
  api_key = String(custom_apikey.getValue());

  String tzInput = String(custom_tz.getValue());
  int foundIndex = -1;
  for (int i = 0; i < NUM_TIMEZONES; i++) {
    if (tzInput.equalsIgnoreCase(tzNames[i])) { foundIndex = i; break; }
  }
  cfg_tzIndex = (foundIndex >= 0) ? foundIndex : 3;  // default to usET if typed wrong
  activeTimezone = tzObjects[cfg_tzIndex];

  saveConfig();
  Serial.println("New config saved!");
  shouldSaveConfig = false;
}

  if (!connected) {
    Serial.println("Failed to connect / portal timed out - restarting");
    ESP.restart();
  }

  Serial.println("WiFi connected!");
}
