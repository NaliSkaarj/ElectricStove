#ifndef _HELPER_H
#define _HELPER_H

#define MM_SS_TO_HH_MM(a)       ((a) * 60)

typedef enum heater_state {
    STATE_IDLE = 0,
    STATE_START_REQUESTED,
    STATE_HEATING,
    STATE_PAUSE_REQUESTED,
    STATE_HEATING_PAUSE,
    STATE_STOP_REQUESTED,
    STATE_HEATING_STOP,
    STATE_MAX
} heater_state;

#endif  // _HELPER_H
