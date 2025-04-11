#ifndef _HELPER_H
#define _HELPER_H

#define MM_SS_TO_HH_MM(a)           ((a) * 60)
#define SECONDS_TO_MILISECONDS(a)   ((a) * 1000)
#define MINUTES_TO_SECONDS(a)       ((a) * 60)
#define HOURS_TO_SECONDS(a)         (MINUTES_TO_SECONDS((a) * 60))

typedef enum heater_state {
    STATE_IDLE = 0,
    STATE_START_REQUESTED,
    STATE_HEATING,
    STATE_NEXTSTEP_REQUESTED,
    STATE_PAUSE_REQUESTED,
    STATE_HEATING_PAUSE,
    STATE_STOP_REQUESTED,
    STATE_MAX
} heater_state;

enum tabs {
    TAB_MAIN = 0,
    TAB_LIST,
    TAB_OPTIONS,
    TAB_COUNT
};

enum options {
    BUZZER_MENU = 0,
    BUZZER_HEATING,
    OTA_ACTIVE,
    OPTIONS_COUNT
};
  
#endif  // _HELPER_H
