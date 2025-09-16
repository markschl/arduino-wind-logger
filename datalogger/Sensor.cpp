#include "Sensor.h"

Sensor::Sensor(uint8_t dataPin, char sensorAddress)
    : sensorAddress(sensorAddress), sdi12(dataPin) {}

void Sensor::init(uint32_t wait) {
  sdi12.begin();
  delay(wait);  // allow things to settle
}

char Sensor::findAddress() {
  sensorAddress = _findAddress();
  return sensorAddress;
}

// this checks for activity at a particular address
// expects a char, '0'-'9', 'a'-'z', or 'A'-'Z'
bool Sensor::_checkActive(char i) {
  char cmd[3];
  snprintf(cmd, 3, "%c!", i);
  // go through three rapid contact attempts
  for (int j = 0; j < 3; j++) {
    sdi12.sendCommand(cmd);
    delay(30);
    // If we hear anything, assume we have an active sensor
    if (sdi12.available()) {
      sdi12.clearBuffer();
      return true;
    }
  }
  sdi12.clearBuffer();
  return false;
}

// Searches the address space to find the sensor
// Returns NULL if none found
char Sensor::_findAddress() {
  // scan address space 0-9
  // 0 is the default, so it should be found immediately
  for (byte i = '0'; i <= '9'; i++)
    if (_checkActive(i)) {
      return i;
    }
  // scan a-z
  for (byte i = 'a'; i <= 'z'; i++)
    if (_checkActive(i)) {
      return i;
    }
  // scan A-Z
  for (byte i = 'A'; i <= 'Z'; i++)
    if (_checkActive(i)) {
      return i;
    }
  return 0;
}

bool Sensor::measure(String &out, char *command, uint32_t expectedLen,
                     uint32_t timeout, uint32_t skip, char stopByte) {
  if (sensorAddress == NULL) {
    return false;
  }
  sdi12.clearBuffer();
  out.reserve(out.length() + expectedLen);

  // Construct the command: it should be at most 4 characters long
  char cmd[5];
  uint32_t cmdlen = strlen(command);
  if (cmdlen > 4) {
    return false;
  }
  strncpy(cmd, command, cmdlen + 1);
  cmd[0] = sensorAddress;

  // send the command
  sdi12.sendCommand(cmd);

  // Calculate approximate initial wait time
  // (transmission: 8.3ms per char at 1200 baud rate)
  uint32_t waitTime = 12 + 8 * strlen(command) + 8 * expectedLen;
  delay(waitTime);

  // check in 9 ms intervals (a bit more than 8.3ms)
  const uint8_t interval = 9;
  bool isFirst = true;
  bool doAppend = true;
  while (true) {
    // wait for characters to arrive
    if (!sdi12.available()) {
      delay(interval);
      waitTime += interval;
      if (waitTime > timeout) return false;
      continue;
    }
    // character has arrived
    char res = sdi12.read();
    // SerialUSB.println(String(res) + "  " + String((uint8_t)res));
    // first character: check sensor address
    if (isFirst) {
      if (res != sensorAddress)  // malformed
        return false;
      isFirst = false;
      continue;
    }
    // check for end (we only check for LF, not for CR before)
    if (res == '\n') return true;
    // skip if necessary
    if (skip > 0) {
      skip--;
      continue;
    }
    if (res == stopByte) doAppend = false;
    // check for stop byte
    if (doAppend) {
      out += res;
    }
  }
}

bool Sensor::_measureR3_4(String &out, char *command, uint32_t timeout) {
  uint32_t offset = out.length();

  if (!measure(out, command, 35, timeout, 1, '\r'))
    return false;

  if (out.length() < offset + 2 || out[out.length() - 1] != '0')
    return false;

  out = out.substring(0, out.length() - 2);

  // Convert spaces to tabs
  for (uint32_t i = offset; i < out.length(); i++) {
    if (out[i] == ' ') out[i] = '\t';
  }

  return true;
}

bool Sensor::measureR3(String &out, uint32_t timeout) {
  bool res = _measureR3_4(out, "aR3!", timeout);
  return res;
}

bool Sensor::measureR4(String &out, uint32_t timeout) {
  bool res = _measureR3_4(out, "aR4!", timeout);
  return res;
}
