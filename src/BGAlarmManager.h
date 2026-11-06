#ifndef BGAlarmManager_h
#define BGAlarmManager_h

#include <Arduino.h>
#include <SettingsAlarm.h>

#include <vector>

struct AlarmData {
    int bottom;
    int top;
    unsigned long lastAlarmTime;
    int snoozeTimeMinutes;
    std::vector<AlertWindow> alertWindows;
    String alarmSound;
    bool isSnoozed;
    bool isUrgentLow = false;
};

class BGAlarmManager_ {
private:
    BGAlarmManager_() = default;
    std::vector<AlarmData> enabledAlarms;
    AlarmData* activeAlarm = NULL;
    int alarmIntervalSeconds;

    void startAlarm(AlarmData& alarmData);
    void clearActiveAlarm();
    void soundActiveAlarmIfDue();

public:
    static BGAlarmManager_& getInstance();
    void setup();
    void tick();
    // Returns true when a sounding alarm was silenced, so the press is not also used to navigate.
    bool snoozeAlarm();
};

extern BGAlarmManager_& bgAlarmManager;
#endif
