#pragma once
#include <Arduino.h>
#include "config.h"

inline void ensureSerialReady() {
#if DEBUG_MODE
  unsigned long start = millis();
  while (!Serial && (millis() - start) < 800) { delay(10); }
#endif
}
//

#if DEBUG_MODE
  #define DBG_PRINT(x)     do { ensureSerialReady(); Serial.print(x); } while (0)
  #define DBG_PRINTLN(x)   do { ensureSerialReady(); Serial.println(x); } while (0)
  #define DBG_FLUSH()      do { ensureSerialReady(); Serial.flush(); delay(10); } while (0)
#else
  #define DBG_PRINT(x)     do {} while (0)
  #define DBG_PRINTLN(x)   do {} while (0)
  #define DBG_FLUSH()      do {} while (0)
#endif

inline void waitForOutputLow(int pin, uint32_t ms) {
  unsigned long start = millis();
  while (digitalRead(pin) != LOW && (millis() - start) < ms) {
    delay(1);
  }
}