#include "BGSourceManager.h"

#include "ServerManager.h"

// Restart the clock when no new reading has arrived for this long.
#define NO_READINGS_RESTART_MILLIS (15UL * 60 * 1000)

// Define the static getInstance method
BGSourceManager_& BGSourceManager_::getInstance() {
    static BGSourceManager_ instance;
    return instance;
}

// Define the constructor
BGSourceManager_::BGSourceManager_() {}

// Define the destructor
BGSourceManager_::~BGSourceManager_() {}

// Define the extern variable
BGSourceManager_& bgSourceManager = BGSourceManager_::getInstance();

void BGSourceManager_::setup(BG_SOURCE bgSourceType) {
    switch (bgSourceType) {
        case BG_SOURCE::NIGHTSCOUT:
            bgSource = new BGSourceNightscout();
            break;
        case BG_SOURCE::DEXCOM:
            bgSource = new BGSourceDexcom();
            break;
        case BG_SOURCE::API:
            bgSource = new BGSourceApi();
            break;
        case BG_SOURCE::LIBRELINKUP:
            bgSource = new BGSourceLibreLinkUp();
            break;
        case BG_SOURCE::MEDTRONIC:
            bgSource = new BGSourceMedtronic();
            break;
        case BG_SOURCE::MEDTRUM:
            bgSource = new BGSourceMedtrum();
            break;
        default:
            DEBUG_PRINTLN(
                "BGSourceManager_::setup: Unknown BG_SOURCE: " + toString(bgSourceType) + " (" +
                String((int)bgSourceType) + ")");
            DisplayManager.showFatalError(
                "Unknown data source: " + toString(bgSourceType) + " (" + String((int)bgSourceType) +
                ")");
    }
    bgSource->setup();
    currentSourceType = bgSourceType;
}

void BGSourceManager_::tick() {
    bgSource->tick();
    restartIfNoNewReadings();
}

// A source can stop delivering readings (an expired session, a stuck connection) without failing
// outright; a restart signs in again from scratch.
void BGSourceManager_::restartIfNoNewReadings() {
    if (lastNewReadingMillis == 0 || bgSource->hasNewData(newestReadingEpoch)) {
        auto readings = bgSource->getGlucoseData();
        newestReadingEpoch = readings.empty() ? 0 : readings.back().epoch;
        lastNewReadingMillis = millis();
        return;
    }

    if (millis() - lastNewReadingMillis < NO_READINGS_RESTART_MILLIS) {
        return;
    }

    DEBUG_PRINTLN("No new glucose readings for 15 minutes, restarting");
    DisplayManager.setFont(FONT_TYPE::MEDIUM);
    DisplayManager.scrollColorfulText("No readings for 15 min, restarting");
    ESP.restart();
}

bool BGSourceManager_::hasNewData(unsigned long long epochToCompare) {
    return bgSource->hasNewData(epochToCompare);
}

std::list<GlucoseReading> BGSourceManager_::getGlucoseData() { return bgSource->getGlucoseData(); }
