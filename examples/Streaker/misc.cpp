#include <Arduino.h>

extern "C" {
/*
  Nullify some support functions for digitalWrite
*/
  // No-op calls to override the PWM implementation
  bool IRAM_ATTR _stopPWM_weak(int pin) { (void) pin; return false; }
  int IRAM_ATTR stopWaveform_weak(uint8_t pin) { (void) pin; return false; }
};
