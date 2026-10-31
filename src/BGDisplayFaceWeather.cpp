#include "BGDisplayFaceWeather.h"

#include <algorithm>

#include "WeatherManager.h"
#include "globals.h"

#pragma region Weather icons

// 8x8 icons; each pixel is a palette index, 0 for transparent.
static const uint16_t WEATHER_PALETTE[] PROGMEM = {
    0xFFE0,  // 1 yellow
    0xFD20,  // 2 orange
    0xFFFF,  // 3 white
    0x8410,  // 4 gray
    0x04BF,  // 5 blue
    0xC618,  // 6 light gray
};

static const uint8_t ICON_SUN[] PROGMEM = {
    2, 0, 0, 2, 0, 0, 2, 0,
    0, 2, 0, 2, 0, 2, 0, 0,
    0, 0, 1, 1, 1, 0, 0, 0,
    2, 2, 1, 1, 1, 2, 2, 0,
    0, 0, 1, 1, 1, 0, 0, 0,
    0, 2, 0, 2, 0, 2, 0, 0,
    2, 0, 0, 2, 0, 0, 2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint8_t ICON_MOON[] PROGMEM = {
    0, 0, 1, 1, 1, 0, 0, 0,
    0, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 3,
    1, 1, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 0, 0, 0, 1, 0,
    0, 0, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint8_t ICON_PARTLY_CLOUDY[] PROGMEM = {
    2, 0, 2, 0, 0, 0, 0, 0,
    0, 1, 1, 0, 0, 0, 0, 0,
    2, 1, 1, 3, 3, 0, 0, 0,
    0, 0, 3, 3, 3, 3, 0, 0,
    0, 3, 3, 3, 3, 3, 3, 0,
    3, 3, 3, 3, 3, 3, 3, 3,
    0, 4, 4, 4, 4, 4, 4, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint8_t ICON_CLOUD[] PROGMEM = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 3, 3, 0, 0, 0,
    0, 0, 3, 3, 3, 3, 0, 0,
    0, 3, 3, 3, 3, 3, 3, 0,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3,
    0, 4, 4, 4, 4, 4, 4, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint8_t ICON_FOG[] PROGMEM = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 6, 6, 6, 6, 6, 6, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    6, 6, 6, 6, 6, 6, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 6, 6, 6, 6, 6, 6,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 6, 6, 6, 6, 6, 0, 0,
};

static const uint8_t ICON_RAIN[] PROGMEM = {
    0, 0, 0, 3, 3, 0, 0, 0,
    0, 0, 3, 3, 3, 3, 0, 0,
    0, 3, 3, 3, 3, 3, 3, 0,
    3, 3, 3, 3, 3, 3, 3, 3,
    0, 4, 4, 4, 4, 4, 4, 0,
    0, 5, 0, 0, 5, 0, 5, 0,
    5, 0, 0, 5, 0, 5, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const uint8_t ICON_SNOW[] PROGMEM = {
    0, 0, 0, 6, 6, 0, 0, 0,
    0, 0, 6, 6, 6, 6, 0, 0,
    0, 6, 6, 6, 6, 6, 6, 0,
    6, 6, 6, 6, 6, 6, 6, 6,
    0, 4, 4, 4, 4, 4, 4, 0,
    0, 3, 0, 0, 3, 0, 0, 3,
    0, 0, 0, 0, 0, 0, 0, 0,
    3, 0, 0, 3, 0, 0, 3, 0,
};

static const uint8_t ICON_STORM[] PROGMEM = {
    0, 0, 0, 4, 4, 0, 0, 0,
    0, 0, 4, 4, 4, 4, 0, 0,
    0, 4, 4, 4, 4, 4, 4, 0,
    4, 4, 4, 4, 4, 4, 4, 4,
    0, 4, 4, 4, 1, 4, 4, 0,
    0, 0, 0, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 0,
};

// WMO weather interpretation codes, as Open-Meteo reports them.
static const uint8_t* getWeatherIcon(int code, bool isDay) {
    if (code == 0) {
        return isDay ? ICON_SUN : ICON_MOON;
    }
    if (code <= 2) {
        return ICON_PARTLY_CLOUDY;
    }
    if (code == 45 || code == 48) {
        return ICON_FOG;
    }
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return ICON_RAIN;
    }
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
        return ICON_SNOW;
    }
    if (code >= 95) {
        return ICON_STORM;
    }
    return ICON_CLOUD;
}

#pragma endregion Weather icons

void BGDisplayFaceWeather::showReadings(
    const std::list<GlucoseReading>& readings, bool dataIsOld) const {
    showWeather();
}

void BGDisplayFaceWeather::showNoData() const {
    DisplayManager.clearMatrix();
    showWeather();
}

// The weather changes every 15 minutes at most; redraw only when it has.
RenderDecision BGDisplayFaceWeather::getRenderDecision(const RenderContext& ctx) const {
    if (ctx.reason == RenderReason::TIME_TICK &&
        WeatherManager.getUpdateCount() == shownUpdateCount) {
        return RenderDecision::NONE;
    }
    return RenderDecision::FULL;
}

// The configured code, or one made from the city: the initials of a name of three or more words
// ("Salt Lake City" is SLC), otherwise its first three letters.
String BGDisplayFaceWeather::getCityCode() const {
    String code = SettingsManager.settings.weather_code;
    code.trim();
    if (code.isEmpty()) {
        String city = SettingsManager.settings.weather_city;
        int comma = city.indexOf(',');
        if (comma >= 0) {
            city = city.substring(0, comma);
        }
        city.trim();

        String initials;
        String letters;
        bool wordStart = true;
        for (size_t i = 0; i < city.length(); i++) {
            if (city[i] == ' ' || city[i] == '-') {
                wordStart = true;
                continue;
            }
            if (wordStart) {
                initials += city[i];
                wordStart = false;
            }
            if (isalpha(city[i])) {
                letters += city[i];
            }
        }
        code = initials.length() >= 3 ? initials : letters;
    }
    code = code.substring(0, 3);
    code.toUpperCase();
    return code;
}

// Code at the left, icon after it, temperature at the right edge with a degree sign when it fits.
void BGDisplayFaceWeather::showWeather() const {
    const WeatherReading& weather = WeatherManager.getReading();
    shownUpdateCount = WeatherManager.getUpdateCount();

    String code = getCityCode();
    DisplayManager.setFont(FONT_TYPE::MEDIUM);
    DisplayManager.setTextColor(COLOR_WHITE);
    DisplayManager.printText(0, 6, code.c_str(), TEXT_ALIGNMENT::LEFT, 1);

    int iconX = std::max(12, static_cast<int>(DisplayManager.getTextWidth(code.c_str(), 1)));

    if (!weather.valid) {
        DisplayManager.setTextColor(getDataOldColor());
        DisplayManager.printText(33, 6, "--", TEXT_ALIGNMENT::RIGHT, 2);
        return;
    }

    DisplayManager.drawIndexedSprite(
        iconX, 0, getWeatherIcon(weather.code, weather.isDay), 8, 8, WEATHER_PALETTE);

    String temperature = String(weather.temperature);
    DisplayManager.setTextColor(COLOR_WHITE);
    if (temperature.length() >= 3) {
        DisplayManager.printText(33, 6, temperature.c_str(), TEXT_ALIGNMENT::RIGHT, 2);
        return;
    }

    DisplayManager.printText(30, 6, temperature.c_str(), TEXT_ALIGNMENT::RIGHT, 2);
    // The font's degree glyph is not reachable through the GFX font table, so draw it.
    for (int x = 30; x <= 31; x++) {
        for (int y = 1; y <= 2; y++) {
            DisplayManager.drawPixel(x, y, COLOR_WHITE);
        }
    }
}
