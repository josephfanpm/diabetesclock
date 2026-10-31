#ifndef WEATHERMANAGER_H
#define WEATHERMANAGER_H

#include <Arduino.h>

// Current weather in the configured city, for the Weather face.
struct WeatherReading {
    bool valid = false;
    int temperature = 0;
    int code = 0;  // WMO weather interpretation code
    bool isDay = true;
};

// Looks up the configured city and fetches its current weather from Open-Meteo, which needs no
// account or key. Only runs while the Weather face is active and a city is set.
class WeatherManager_ {
public:
    static WeatherManager_& getInstance();
    void tick();
    const WeatherReading& getReading() const { return reading; }
    // Changes whenever the reading changes, so the face knows when to redraw.
    unsigned long getUpdateCount() const { return updateCount; }

private:
    WeatherManager_() = default;
    WeatherManager_(const WeatherManager_&) = delete;
    WeatherManager_& operator=(const WeatherManager_&) = delete;

    bool locate();
    bool fetch();

    bool located = false;
    float latitude = 0;
    float longitude = 0;
    bool attempted = false;
    bool lastAttemptSucceeded = false;
    unsigned long lastAttemptMillis = 0;
    unsigned long lastSuccessMillis = 0;
    WeatherReading reading;
    unsigned long updateCount = 0;
};

extern WeatherManager_& WeatherManager;

#endif  // WEATHERMANAGER_H
