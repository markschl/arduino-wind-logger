#ifndef SYS_METHODS
#define SYS_METHODS

#include "Arduino.h"
#include "ArduinoLowPower.h"

// ARM-specific function for obtaining free RAM
// see https://github.com/mpflaga/Arduino-MemoryFree/blob/master/MemoryFree.cpp
extern "C" char* sbrk(int incr);

int freeMemory() {
  char top;
  return &top - reinterpret_cast<char*>(sbrk(0));
}

// Puts a SAMD21 chip to sleep.
// detachSerial ensures that all print output is shown, but will also
// block the device if the serial is closed and sometimes causes unexpected
// hangs
inline void standby(bool detachSerial = false, uint32_t serialBaudRate = 9600) {
  if (detachSerial) {
    SerialUSB.flush();
    SerialUSB.end();
    USBDevice.detach();
  }

  // LowPower.sleep();
  LowPower.deepSleep();

  if (detachSerial) {
    USBDevice.attach();
    SerialUSB.begin(serialBaudRate);
    while (!SerialUSB);
  }
}

#endif
