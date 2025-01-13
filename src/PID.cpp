#include <Arduino.h>
#include <PID.h>
#include <PID_v1.h>

#define TOTAL_WINDOW_SIZE   ( PID_WINDOW_SIZE + PID_DEADTIME_SIZE )
#define START_NEW_PROCESS   ( -1.0f )
#define SAMPLES_PER_WINDOW  ( TOTAL_WINDOW_SIZE / PID_INTERVAL_COMPUTE )

static unsigned long windowStartTime, currentTime;
static double setPoint, input, output;
static double avgOutput;
static bool isOn;
static double Kp=70, Ki=0.1, Kd=1000;   //CDHW methode from [https://newton.ex.ac.uk/teaching/CDHW/Feedback/Setup-PID.html]
// static double Kp=2, Ki=5, Kd=1;

PID myPID( &input, &output, &setPoint, Kp, Ki, Kd, DIRECT );

void PID_Init() {
  pinMode( PID_PIN_RELAY, OUTPUT );
  digitalWrite( PID_PIN_RELAY, LOW );
  isOn = false;
  windowStartTime = millis();
  setPoint = 20;
  output = 0.0;
  myPID.SetOutputLimits( 0, PID_WINDOW_SIZE );
  myPID.SetSampleTime( PID_INTERVAL_COMPUTE / portTICK_PERIOD_MS );
  myPID.SetMode( AUTOMATIC );
}

void PID_Compute() {
  static double sumOutput = 0.0f;

  if( !isOn ) {
    digitalWrite( PID_PIN_RELAY, LOW );
    return;
  }

  myPID.Compute();
  currentTime = millis();

  if( START_NEW_PROCESS == avgOutput ) {
    avgOutput = output;   // use first value at the beginning of the first cycle (total windows time)
  }
  sumOutput += output;

  if( avgOutput > currentTime - windowStartTime ) {
    digitalWrite( PID_PIN_RELAY, HIGH );
  }
  else {
    digitalWrite( PID_PIN_RELAY, LOW );
  }

  if( currentTime - windowStartTime > TOTAL_WINDOW_SIZE )
  {
    // it's time to shift the Relay Window
    windowStartTime += TOTAL_WINDOW_SIZE;
    // calculate output once for one time window
    avgOutput = sumOutput / SAMPLES_PER_WINDOW;
    sumOutput = 0.0f;
  }
}

void PID_SetPoint( uint16_t targetPoint ) {
  setPoint = (double)targetPoint;
}

void PID_On() {
  windowStartTime = millis();
  avgOutput = START_NEW_PROCESS;
  isOn = true;
}

void PID_Off() {
  digitalWrite( PID_PIN_RELAY, LOW );
  isOn = false;
}

void PID_updateTemp( double temp ) {
  input = temp;
}
