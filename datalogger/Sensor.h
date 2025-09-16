#ifndef Sensor_h
#define Sensor_h

#include "SDI12.h"

// Maximum length of SDI-12 commands (must be large enough)
#define SDI12_CMD_BUFSIZE 8

class Sensor {
 private:
  SDI12 sdi12;
  char sensorAddress;

  bool _checkActive(char i);
  char _findAddress();
  bool _measureR3_4(String &out, char *command, uint32_t timeout);

 public:
  // Initialization with a given sensor address
  Sensor(uint8_t dataPin, char sensorAddress = '0');
  // Start SDI-12 (given wait time in ms)
  void init(uint32_t wait = 500);
  // Returns `true` if the sensor is found under the current address
  bool isConnected() { return _checkActive(sensorAddress); }
  // Searches for an active sensor by checking all possible addresses for a
  // response to the "c!" command.
  char findAddress();
  // Does a measurement with a given SDI-12 command and appends the output to
  // `out`. Returns `false` if `init()` wasn't called, `command` is too long,
  // the sensor is not found or the response took too long / was malformed.
  // expectedLen: helps in determining the initial wait time and setting buffer
  // size skip: skip this many initial characters stopByte: stop appending when
  // byte is encountered. The function will still wait until all data has been
  // sent by the sensor (that is, LF has been returned) before returning.
  bool measure(String &out, char *command, uint32_t expectedLen,
               uint32_t timeout = 5000, uint32_t skip = 0,
               char stopByte = '\r');               
  // Does an R3! measurement reporting average or maximum values,
  // without appending sensortype / checksum / CRC.
  // *Do not issue more frequently than every 20s*
  bool measureR4(String &out, uint32_t timeout = 5000);
  // Does an instantaneous R4! measurement, without appending
  // sensortype / checksum / CRC.
  // *Do not issue more frequently than every 10s*
  bool measureR3(String &out, uint32_t timeout = 5000);
};

#endif
