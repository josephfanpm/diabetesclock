#include <BGAlarmManager.h>
#include <BGDisplayManager.h>
#include <PeripheryManager.h>
#include <SettingsManager.h>

#include "ServerManager.h"
#include "globals.h"

#define ALARM_REPEAT_INTERVAL_INTENSIVE_SECONDS 2

// The getter for the instantiated singleton instance
BGAlarmManager_& BGAlarmManager_::getInstance() {
    static BGAlarmManager_ instance;
    return instance;
}

// Initialize the global shared instance
BGAlarmManager_& bgAlarmManager = bgAlarmManager.getInstance();

void BGAlarmManager_::setup() {
    // activeAlarm points into enabledAlarms, so it must not outlive a rebuild of the list.
    activeAlarm = NULL;
    enabledAlarms.clear();

    if (SettingsManager.settings.alarm_urgent_low_enabled) {
        AlarmData alarmData;
        alarmData.bottom = 1;
        alarmData.top = SettingsManager.settings.alarm_urgent_low_mgdl;
        alarmData.snoozeTimeMinutes = SettingsManager.settings.alarm_urgent_low_snooze_minutes;
        alarmData.alertWindows = SettingsManager.settings.alarm_urgent_low_alert_windows;
        alarmData.lastAlarmTime = 0;
        alarmData.alarmSound = SettingsManager.settings.alarm_urgent_low_melody;
        alarmData.isSnoozed = false;
        alarmData.isUrgentLow = true;
        enabledAlarms.push_back(alarmData);
    }
    if (SettingsManager.settings.alarm_low_enabled) {
        AlarmData alarmData;
        alarmData.bottom = SettingsManager.settings.alarm_urgent_low_mgdl + 1;
        alarmData.top = SettingsManager.settings.alarm_low_mgdl - 1;
        alarmData.snoozeTimeMinutes = SettingsManager.settings.alarm_low_snooze_minutes;
        alarmData.alertWindows = SettingsManager.settings.alarm_low_alert_windows;
        alarmData.lastAlarmTime = 0;
        alarmData.alarmSound = SettingsManager.settings.alarm_low_melody;
        alarmData.isSnoozed = false;
        enabledAlarms.push_back(alarmData);
    }
    if (SettingsManager.settings.alarm_high_enabled) {
        AlarmData alarmData;
        alarmData.bottom = SettingsManager.settings.alarm_high_mgdl;
        alarmData.top = 401;
        alarmData.snoozeTimeMinutes = SettingsManager.settings.alarm_high_snooze_minutes;
        alarmData.alertWindows = SettingsManager.settings.alarm_high_alert_windows;
        alarmData.lastAlarmTime = 0;
        alarmData.alarmSound = SettingsManager.settings.alarm_high_melody;
        alarmData.isSnoozed = false;
        enabledAlarms.push_back(alarmData);
    }

    if (SettingsManager.settings.alarm_intensive_mode) {
        alarmIntervalSeconds = ALARM_REPEAT_INTERVAL_INTENSIVE_SECONDS;  // repeat every 2 seconds
    } else {
        alarmIntervalSeconds = SettingsManager.settings.alarm_repeat_interval_seconds;
    }
}

// True when this alarm is allowed to sound now: no windows means at any time,
// otherwise only while one of them is open.
static bool isInsideAlertWindow(const std::vector<AlertWindow>& alertWindows) {
    if (alertWindows.empty()) {
        return true;
    }

    tm now;
    if (!ServerManager.tryGetTimezonedTime(now)) {
        // Unknown time (no NTP yet) must never silence an alarm.
        DEBUG_PRINTLN("Alarms: time is not known, ignoring alert windows and alerting anyway");
        return true;
    }

    const int nowMinutes = now.tm_hour * 60 + now.tm_min;
    const int today = now.tm_wday;  // 0 is Sunday, matching AlertWindow::days
    const int yesterday = (today + 6) % 7;

    for (const AlertWindow& window : alertWindows) {
        if (window.startMinutes < window.endMinutes) {
            // Contained in one day, for example Monday to Friday 09:00 - 17:00.
            if (window.days[today] && nowMinutes >= window.startMinutes &&
                nowMinutes < window.endMinutes) {
                return true;
            }
        } else {
            // Runs past midnight, for example every day 18:00 - 08:00. The evening half belongs
            // to today's window and the morning half to the one that opened yesterday.
            if (window.days[today] && nowMinutes >= window.startMinutes) {
                return true;
            }
            if (window.days[yesterday] && nowMinutes < window.endMinutes) {
                return true;
            }
        }
    }

    return false;
}

void BGAlarmManager_::startAlarm(AlarmData& alarmData) {
    activeAlarm = &alarmData;
    alarmData.isSnoozed = false;
    alarmData.lastAlarmTime = ServerManager.getUtcEpoch();
    PeripheryManager.playRTTTLString(alarmData.alarmSound);
    DEBUG_PRINTLN("Alarms: playing alarm sound (new alarm)");
}

void BGAlarmManager_::clearActiveAlarm() {
    if (activeAlarm != NULL) {
        activeAlarm->isSnoozed = false;
        activeAlarm->lastAlarmTime = 0;
    }
    activeAlarm = NULL;
}

// Plays the active alarm again when it is due. An urgent low that has not been silenced with a
// button plays back to back, so it never goes quiet on its own.
void BGAlarmManager_::soundActiveAlarmIfDue() {
    const unsigned long now = ServerManager.getUtcEpoch();

    if (activeAlarm->isSnoozed) {
        if (activeAlarm->snoozeTimeMinutes != 0 &&
            now - activeAlarm->lastAlarmTime > 60UL * activeAlarm->snoozeTimeMinutes) {
            activeAlarm->isSnoozed = false;
            activeAlarm->lastAlarmTime = now;
            PeripheryManager.playRTTTLString(activeAlarm->alarmSound);
            DEBUG_PRINTLN("Alarms: playing alarm sound after snooze");
        }
        return;
    }

    if (activeAlarm->isUrgentLow) {
        // The one-second floor keeps a melody that fails to load from being re-parsed every loop.
        if (!PeripheryManager.isSoundPlaying() && now - activeAlarm->lastAlarmTime >= 1) {
            activeAlarm->lastAlarmTime = now;
            PeripheryManager.playRTTTLString(activeAlarm->alarmSound);
        }
        return;
    }

    if (now - activeAlarm->lastAlarmTime > (unsigned long)alarmIntervalSeconds) {
        activeAlarm->lastAlarmTime = now;
        PeripheryManager.playRTTTLString(activeAlarm->alarmSound);
        DEBUG_PRINTLN("Alarms: playing alarm sound (alarm already active, not snoozed)");
    }
}

void BGAlarmManager_::tick() {
    auto glucoseReading = bgDisplayManager.getLastDisplayedGlucoseReading();
    if (glucoseReading == nullptr ||
        glucoseReading->getSecondsAgo() >
            SettingsManager.settings.bg_data_too_old_threshold_minutes * 60) {
        // Losing data does not mean the low is over: an urgent low keeps sounding until a
        // button is pressed or a fresh reading shows it has ended.
        if (activeAlarm != NULL && activeAlarm->isUrgentLow) {
            soundActiveAlarmIfDue();
            return;
        }
        clearActiveAlarm();
        return;
    }

    for (AlarmData& alarmData : enabledAlarms) {
        if (glucoseReading->sgv < alarmData.bottom || glucoseReading->sgv > alarmData.top) {
            continue;
        }

        if (!isInsideAlertWindow(alarmData.alertWindows)) {
            clearActiveAlarm();
            return;
        }

        if (activeAlarm != &alarmData) {
            // A different alarm is sounding (for example low dropping to urgent low). The new
            // one starts fresh and does not inherit the old one's snooze.
            clearActiveAlarm();

            // The Time only face holds back new alarms, but never an urgent low.
            if (!alarmData.isUrgentLow && bgDisplayManager.suppressesNewAlarms()) {
                return;
            }
            startAlarm(alarmData);
            return;
        }

        soundActiveAlarmIfDue();
        return;
    }

    clearActiveAlarm();
}

bool BGAlarmManager_::snoozeAlarm() {
    if (activeAlarm == NULL || activeAlarm->isSnoozed) {
        return false;
    }

    DEBUG_PRINTLN("Snoozing alarm");
    activeAlarm->isSnoozed = true;
    // The snooze runs from the press, not from the last time the sound started.
    activeAlarm->lastAlarmTime = ServerManager.getUtcEpoch();
    PeripheryManager.stopSound();

    DisplayManager.clearMatrix();
    DisplayManager.setTextColor(COLOR_CYAN);
    DisplayManager.printText(0, 6, "Snoozed", TEXT_ALIGNMENT::CENTER, 0);
    DisplayManager.update();
    delay(2000);
    bgDisplayManager.maybeRrefreshScreen(true);
    return true;
}
