#include "WeatherManager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <algorithm>

#include "SettingsManager.h"
#include "globals.h"

#define WEATHER_REFRESH_MILLIS (15UL * 60 * 1000)
#define WEATHER_RETRY_MILLIS (2UL * 60 * 1000)
// Older weather is no longer shown.
#define WEATHER_MAX_AGE_MILLIS (90UL * 60 * 1000)

WeatherManager_& WeatherManager_::getInstance() {
    static WeatherManager_ instance;
    return instance;
}

WeatherManager_& WeatherManager = WeatherManager_::getInstance();

// US state abbreviations, so "Austin, TX" finds the Texas Austin.
static const char* const US_STATES[][2] = {
    {"al", "alabama"},        {"ak", "alaska"},         {"az", "arizona"},       {"ar", "arkansas"},
    {"ca", "california"},     {"co", "colorado"},       {"ct", "connecticut"},   {"de", "delaware"},
    {"fl", "florida"},        {"ga", "georgia"},        {"hi", "hawaii"},        {"id", "idaho"},
    {"il", "illinois"},       {"in", "indiana"},        {"ia", "iowa"},          {"ks", "kansas"},
    {"ky", "kentucky"},       {"la", "louisiana"},      {"me", "maine"},         {"md", "maryland"},
    {"ma", "massachusetts"},  {"mi", "michigan"},       {"mn", "minnesota"},     {"ms", "mississippi"},
    {"mo", "missouri"},       {"mt", "montana"},        {"ne", "nebraska"},      {"nv", "nevada"},
    {"nh", "new hampshire"},  {"nj", "new jersey"},     {"nm", "new mexico"},    {"ny", "new york"},
    {"nc", "north carolina"}, {"nd", "north dakota"},   {"oh", "ohio"},          {"ok", "oklahoma"},
    {"or", "oregon"},         {"pa", "pennsylvania"},   {"ri", "rhode island"},  {"sc", "south carolina"},
    {"sd", "south dakota"},   {"tn", "tennessee"},      {"tx", "texas"},         {"ut", "utah"},
    {"vt", "vermont"},        {"va", "virginia"},       {"wa", "washington"},    {"wv", "west virginia"},
    {"wi", "wisconsin"},      {"wy", "wyoming"},        {"dc", "district of columbia"},
};

static String urlEncode(const String& text) {
    static const char* HEX_DIGITS = "0123456789ABCDEF";
    String out;
    for (size_t i = 0; i < text.length(); i++) {
        const uint8_t c = text[i];
        if (isalnum(c) || c == '-' || c == '.' || c == '_') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += HEX_DIGITS[c >> 4];
            out += HEX_DIGITS[c & 0x0F];
        }
    }
    return out;
}

static bool getJson(const String& url, JsonDocument& doc, JsonDocument& filter) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(secureClient, url)) {
        DEBUG_PRINTLN("Weather: could not start request");
        return false;
    }

    int responseCode = http.GET();
    if (responseCode != HTTP_CODE_OK) {
        DEBUG_PRINTF("Weather: request failed with code %d\n", responseCode);
        http.end();
        return false;
    }

    String response = http.getString();
    http.end();
    DeserializationError error = deserializeJson(doc, response, DeserializationOption::Filter(filter));
    if (error) {
        DEBUG_PRINTF("Weather: invalid response: %s\n", error.c_str());
        return false;
    }
    return true;
}

// Whether a geocoding result is in the region typed after the city's comma: a country, its code, a
// state or province, or a US state abbreviation.
static bool isInRegion(JsonObject result, const String& region) {
    String stateName = region;
    for (const auto& state : US_STATES) {
        if (region == state[0]) {
            stateName = state[1];
            break;
        }
    }

    String names[] = {
        result["country_code"] | "", result["country"] | "", result["admin1"] | ""};
    for (String& name : names) {
        name.toLowerCase();
        if (name.length() > 0 && (name.startsWith(region) || name == stateName)) {
            return true;
        }
    }
    return false;
}

bool WeatherManager_::locate() {
    const auto& settings = SettingsManager.settings;
    if (!isnan(settings.weather_latitude) && !isnan(settings.weather_longitude)) {
        latitude = settings.weather_latitude;
        longitude = settings.weather_longitude;
        return true;
    }

    String city = settings.weatherCity();
    String region;
    int comma = city.indexOf(',');
    if (comma >= 0) {
        region = city.substring(comma + 1);
        city = city.substring(0, comma);
    }
    city.trim();
    region.trim();
    region.toLowerCase();

    JsonDocument filter;
    JsonObject resultFilter = filter["results"].add<JsonObject>();
    for (const char* key : {"latitude", "longitude", "country_code", "country", "admin1"}) {
        resultFilter[key] = true;
    }

    JsonDocument doc;
    String url = "https://geocoding-api.open-meteo.com/v1/search?count=10&language=en&format=json&name=" +
                 urlEncode(city);
    if (!getJson(url, doc, filter)) {
        return false;
    }

    JsonArray results = doc["results"].as<JsonArray>();
    if (results.size() == 0) {
        DEBUG_PRINTLN("Weather: city not found: " + city);
        return false;
    }

    JsonObject match = results[0];
    if (region.length() > 0) {
        for (JsonObject result : results) {
            if (isInRegion(result, region)) {
                match = result;
                break;
            }
        }
    }

    latitude = match["latitude"].as<float>();
    longitude = match["longitude"].as<float>();
    DEBUG_PRINTF("Weather: %s is at %.4f, %.4f\n", city.c_str(), latitude, longitude);
    return true;
}

bool WeatherManager_::fetch() {
    JsonDocument filter;
    filter["current"]["temperature_2m"] = true;
    filter["current"]["weather_code"] = true;
    filter["current"]["is_day"] = true;

    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 4) +
                 "&longitude=" + String(longitude, 4) +
                 "&current=temperature_2m,weather_code,is_day&temperature_unit=" +
                 (SettingsManager.settings.weather_fahrenheit ? "fahrenheit" : "celsius");

    JsonDocument doc;
    if (!getJson(url, doc, filter) || !doc["current"]["temperature_2m"].is<float>()) {
        return false;
    }

    reading.valid = true;
    reading.temperature = lroundf(doc["current"]["temperature_2m"].as<float>());
    reading.code = doc["current"]["weather_code"] | 0;
    reading.isDay = (doc["current"]["is_day"] | 1) == 1;
    return true;
}

void WeatherManager_::tick() {
    const auto& settings = SettingsManager.settings;
    const auto& inactive = settings.inactive_faces;
    if (std::find(inactive.begin(), inactive.end(), "weather") != inactive.end()) {
        return;
    }

    unsigned long wait = lastAttemptSucceeded ? WEATHER_REFRESH_MILLIS : WEATHER_RETRY_MILLIS;
    if (attempted && millis() - lastAttemptMillis < wait) {
        return;
    }
    attempted = true;
    lastAttemptMillis = millis();

    if (!located) {
        located = locate();
    }
    lastAttemptSucceeded = located && fetch();

    if (lastAttemptSucceeded) {
        lastSuccessMillis = millis();
        updateCount++;
    } else if (reading.valid && millis() - lastSuccessMillis > WEATHER_MAX_AGE_MILLIS) {
        reading.valid = false;
        updateCount++;
    }
}
