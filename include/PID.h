#ifndef _PID_H
#define _PID_H

#include <Arduino.h>

#define PID_PIN_INPUT           0
#define PID_PIN_RELAY           25
#define PID_WINDOW_SIZE         8000
#define PID_DEADTIME_SIZE       2000
#define PID_INTERVAL_COMPUTE    10  // the period (in ms) at which the calculation is performed

void PID_Init();
void PID_Compute();
void PID_SetPoint( uint16_t targetPoint );
void PID_On();
void PID_Off();
void PID_updateTemp( double temp );

#endif  // _PID_H
