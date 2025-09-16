#include "Arduino.h"
#include "RTClib.h"
#include "SDI12.h"
#include "SdFat.h"
#include "SdLogger.h"
#include "Sensor.h"
#include "system.h"

// --------------------------------------------
// 1. Include header for board
// --------------------------------------------
// Every of the loggers has a different header file,
// which can be included by uncommenting when flashing.

#include "s1_Autonomo.h"  // Tools -> Board -> Sodaq Autonomo
//#include "s2_Feather.h" // Tools -> Board -> Adafruit Feather M0

// The headers define the following variables:
// LOGGER_ID: ID (name) of the logger, will be prepended to output file names
// RTC_INTERRUPT_PIN: Pin for RTC interrupt
// BUTTON_PIN: Pin for the control button
// SD_CARD_DETECT_PIN (optional): Pin signalling if the SD card was inserted/removed
// SDCARD_PIN: Pin for communicating with the SD reader (chip select / CS pin)
// LED_PIN: Pin of a diagnostic LED
// ERROR_LED_PIN: Pin of LED used for emitting error codes
// SDI12_PIN: The pin of the SDI-12 data bus
//
// In addition, the following function needs to be defined for getting
// battery voltage in mV (if available).
// float GetBatteryVoltageMV();
//
// Note that you need to install board support:
// - https://support.sodaq.com/getting_started/
// - https://learn.adafruit.com/adafruit-feather-m0-adalogger/setup
// ... and switch the board in Tools -> Board before flashing

#ifndef LOGGER_ID
#error "LOGGER_ID not defined in board header"
#endif

// --------------------------------------------
// 2. User settings
// --------------------------------------------

// uncomment if messages should be printed and/or date-time should be set
//#define DEBUG

// Name of the output directory
#define BASE_DIR "atmos22"

// Defines how many bytes should be logged to RAM before
// writing to the SD card. Higher numbers will lead to lower
// energy consumption, but in case of power loss, more data
// will be lost. 
// Also make sure that there is enough RAM. Memory is reported
// in debug mode.
#define BUFFER_SIZE 16384

// Measurement interval, at least 10s if using the R4! command
// according to ATMOS22 integrator guide
#define MEASUREMENT_INTERVAL_SECONDS 10

// header added on top of every file
#define HEADER \
"date_time\tnorth_speed\teast_speed\tgust_speed\ttemperature\t\
x_orientation\ty_orientation\tbattery_voltage\tboard_temperature\n"

// Error codes (error LED will blink this many times)
#define SD_ERROR 3
#define SENSOR_ERROR 4
#define CLOCK_ERROR 5
// interval, at which erros are indicated
#define ERROR_INTERVAL_SECS 10

// SD settings
#define ENABLE_DEDICATED_SPI 1
#define SDFAT_FILE_TYPE 1

// The baud rate for the output serial port
#define SERIAL_BAUD 9600

// --------------------------------------------
// 3. Serial and debugging
// --------------------------------------------

// Different boards name the USB serial differently
#define Serial SERIAL_PORT_MONITOR

// debugging macros
#ifdef DEBUG

bool debugMode = true;
#define PRINT(...) Serial.print(__VA_ARGS__)
#define PRINTLN(...) Serial.println(__VA_ARGS__)
#else

bool debugMode = false;
#define PRINT(...)
#define PRINTLN(...)

#endif

// --------------------------------------------
// 4. Global variables (occupy RAM from the beginning)
// --------------------------------------------

// RTC, SD card, logger, sensor
RTC_DS3231 rtc;
SdFat sd;
SdLogger logger = SdLogger(sd, rtc, LOGGER_ID, BUFFER_SIZE);
Sensor sensor = Sensor(SDI12_PIN);

// Next wake-up time, always incremented by wakeInterval
uint32_t wakeTime = 0;

// Measurement or error reporting interval
uint32_t wakeInterval = MEASUREMENT_INTERVAL_SECONDS;

// The current state of the logger
enum state {
  starting,
  logging,
  detaching,
  detached_logging,
  attaching,
  error,
};

volatile state runState = starting;

// must be true for logging to happen (usually set in ISR)
volatile bool regularWakeup = false;

// SD state
enum sdState { present, inserted, missing, unknown };

#ifdef SD_CARD_DETECT_PIN
// SD card is always present at the start, otherwise
// there will be an error
volatile sdState sdPresence = inserted;
#else
volatile sdState sdPresence = unknown;
#endif

// Buffers for time stamp and logger output
char TimeStamp[] = "0000-00-00 00:00:00";
String logLine = "";

// Error code
uint8_t errorNo = 0;

// --------------------------------------------
// 5. Utilities
// --------------------------------------------

void updateTimeStamp(DateTime t) {
  sprintf(TimeStamp, "%d-%02d-%02d %02d:%02d:%02d", t.year(), t.month(),
          t.day(), t.hour(), t.minute(), t.second());
}

void setError(uint8_t no) {
  errorNo = no;
  runState = error;
  wakeInterval = ERROR_INTERVAL_SECS;
}

// blink internal LED(s)
void blink(uint8_t pin, uint8_t pin2, uint32_t n, uint32_t wait_ms) {
  for (uint32_t i = 0; i < n; i++) {
    digitalWrite(pin, HIGH);
    if (pin2 != NULL) {
      digitalWrite(pin2, HIGH);
    }
    delay(wait_ms);
    digitalWrite(pin, LOW);
    if (pin2 != NULL) {
      digitalWrite(pin2, LOW);
    }
    delay(wait_ms);
  }
}

void sdDateTimeCallback(uint16_t* date, uint16_t* time) {
  DateTime now = rtc.now();
  *date = FAT_DATE(now.year(), now.month(), now.day());
  *time = FAT_TIME(now.hour(), now.minute(), now.second());
}

// prompt for yes / no answer
bool getAnswer(bool defaultYes) {
  Serial.setTimeout(100);
  while (true) {
    while (!Serial.available())
      ;
    String answer = Serial.readString();
    if (answer.length() >= 2 && answer[1] == '\r' || answer[1] == '\n') {
      switch (answer[0]) {
        case 'y':
        case 'Y':
          return true;
        case '\n':
        case '\r':
          return defaultYes;
        case 'n':
        case 'N':
          return false;
      }
    }
    Serial.println(F("Unknown answer."));
  }
}

// --------------------------------------------
// 6. Setup code
// --------------------------------------------

void setup() {
#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial)
    ;
#endif

  // LEDs (light on during setup)
  pinMode(LED_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  PRINTLN(F("Setting up the external RTC clock..."));
  if (!setupClock()) {
    // loop() will not work, since it depends on the RTC
    // waking up the device periodically. Therefore, we just
    // return, leaving the LED on.
    return;
  }
  // Set this when resetting after error
  wakeInterval = MEASUREMENT_INTERVAL_SECONDS;
  // Set the initial wake time as soon as possible,
  // (just after the reset button was pressed).
  // The first loop call will assume it was woken up at this time.
  wakeTime = rtc.now().unixtime();
  pinMode(RTC_INTERRUPT_PIN, INPUT_PULLUP);
  // Configure clock interrupt on the falling edge from the SQW pin
  // Some additional internal settings are needed for SAMD21 to wake up on
  // external RTC interrupts, which is done by LowPower.
  LowPower.attachInterruptWakeup(digitalPinToInterrupt(RTC_INTERRUPT_PIN),
                                 rtcISR, FALLING);

  PRINTLN(F("Setting up the sensor..."));
  sensor.init();
  if (!sensor.isConnected()) {
    PRINTLN(F("Sensor not found at default address"));
    setError(SENSOR_ERROR);
  }

  PRINTLN(F("Setting up the SD logger..."));
  if (!setupSD()) {
    setError(SD_ERROR);
  }
  logger.setBaseDir(BASE_DIR);
  logger.setHeader(HEADER);
  logLine.reserve(100);

  // // Make sure the internal LED is off
  digitalWrite(LED_BUILTIN, LOW);
  PRINTLN(F("... setup done."));
}

bool setupClock() {
  if (!rtc.begin()) {
    PRINTLN(F("Couldn't find RTC"));
    return false;
  }

  // Initial actions according to
  // https://github.com/adafruit/RTClib/blob/master/examples/DS3231_alarm/DS3231_alarm.ino
  rtc.disable32K();
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.writeSqwPinMode(DS3231_OFF);
  rtc.disableAlarm(2);

#ifdef DEBUG
  while (Serial.read() != -1);
  updateTimeStamp(rtc.now());
  Serial.print(F("Time is: "));
  Serial.print(TimeStamp);
  Serial.println(F(". Do you want to set the clock (y/n)? [default: n]"));
  if (getAnswer(false)) {
    syncClock();
  }
#endif
  return true;
}

void syncClock() {
  while (true) {
    Serial.println(
        F("Please close the serial monitor and type the following:\r\ndate +%s "
          "> /dev/ttyACM0"));
    Serial.println(
        F("in the terminal (Unix). Then re-open the serial monitor."));
    // based on
    // https://forum.arduino.cc/index.php?topic=367987.msg2536880#msg2536880
    while (!Serial.available());
    // read timestamp and set time immediately
    unsigned long pctime = Serial.parseInt();
    if (pctime != 0) {
      // Sync Arduino clock to the time received on the serial port
      rtc.adjust(DateTime(pctime));
    }
    // Disable Serial in order to block until the window is re-opened
    Serial.end();
    while (!Serial);
    Serial.begin(SERIAL_BAUD);
    Serial.println(pctime);
    if (pctime == 0) {
      Serial.println(F("No valid number was received, repeating."));
    } else {
      Serial.print(F("It worked, time is now: "));
      updateTimeStamp(rtc.now());
      Serial.println(TimeStamp);
      break;
    }
  }
}

bool setupSD() {
  while (true) {
    uint8_t res = initSD();
    if (res != 0) {
#ifdef DEBUG
      if (res == 1) {
        Serial.println(F("Should the card be reformatted? (y/n) [default: n]"));
        if (getAnswer(false)) {
          Serial.println(
              F("All data will be erased, are you sure? (y/n) [default: n]"));
          if (getAnswer(false)) {
            if (!formatCard()) {
              return false;
            }
            // again initialize card and show
            continue;
          }
        }
      }
#endif
      return false;
    }
    break;
  }

// enable card detection (if feature present)
#ifdef SD_CARD_DETECT_PIN
  pinMode(SD_CARD_DETECT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SD_CARD_DETECT_PIN), sdChange, CHANGE);
#endif

  // Button: attach interrupts
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_change, FALLING);

  // make sure the files have the correct creation date / time
  // according to https://forum.arduino.cc/index.php?topic=348562.0
  SdFile::dateTimeCallback(sdDateTimeCallback);
  return true;
}

uint8_t initSD() {
  // Initialize properly, see:
  // https://github.com/greiman/SdFat/blob/master/examples/QuickStart/QuickStart.ino
  if (!sd.begin(SDCARD_PIN)) {
    PRINTLN(F("Card initialization failed!"));
    if (sd.card()->errorCode()) {
      PRINT(F("Card not inserted? Error code: "));
      PRINT(sd.card()->errorCode());
      PRINT(F("Data: "));
      PRINTLN(sd.card()->errorData());
      return 2;
    } else if (sd.vol()->fatType() == 0) {
      PRINTLN(F("Can't find a valid FAT16/FAT32 partition."));
      return 1;
    } else {
      PRINTLN(F("Reason unknown."));
      return 1;
    }
  }

  uint32_t size = sd.card()->sectorCount();
  if (size == 0) {
    PRINTLN(F("Can't determine the card size."));
    PRINTLN(F("Try another SD card or reduce the SPI bus speed"));
    return 2;
  }

  uint32_t sizeMB = 0.000512 * size + 0.5;
  PRINT(F("  Card size: "));
  PRINT(sizeMB);
  PRINTLN(F(" MB"));
  PRINT(F("  Volume type: FAT"));
  PRINTLN(sd.vol()->fatType());
  PRINT(F("  Cluster size: "));
  PRINT(sd.vol()->bytesPerCluster() / 1024.);
  PRINTLN(F(" KiB"));

  if ((sizeMB > 1100 && sd.vol()->sectorsPerCluster() < 64) ||
      (sizeMB < 2200 && sd.vol()->fatType() == 32)) {
    PRINTLN(F("Reformat card using cluster size of 32 KB for cards > 1 GB."));
    PRINTLN(F("Only cards larger than 2 GB should be formatted FAT32."));
    return 1;
  }
  return 0;
}

bool formatCard() {
  FatFormatter fatFormatter;
  uint8_t sectorBuffer[512];
  bool res = fatFormatter.format(sd.card(), sectorBuffer, &Serial);
  return res;
}

// --------------------------------------------
// 7. Interrupt handlers (SD change, button)
// --------------------------------------------

// ISR fired by RTC
static void rtcISR() {
  regularWakeup = true;
}

#ifdef SD_CARD_DETECT_PIN
void sdChange() {
  if (digitalRead(SD_CARD_DETECT_PIN)) {
    sdPresence = present;
  } else {
    sdPresence = missing;
  }
}
#endif

void button_change() {
  switch (runState) {
    case logging:
      runState = detaching;
      break;
    case detached_logging:
      runState = attaching;
      break;
    // if there was an error and the button is pressed: restart
    case error:
      runState = starting;
      break;
  }
}

// --------------------------------------------
// 7. Event loop
// --------------------------------------------

void loop() {
  PRINTLN("State " + String(runState) + " at " + String(DateTime(wakeTime).timestamp(DateTime::TIMESTAMP_FULL) + "; free memory: " + String(freeMemory())));

  // indicate action
  digitalWrite(LED_PIN, HIGH);

  // Disable interrupts. A button press could change the run state and
  // thus interfere with the changes in this loop. The button will
  // thus not work during this time.
  noInterrupts();
 
  switch (runState) {
    case starting:
      PRINTLN(F("Starting..."));
      // running the loop for the first time or after the reset button
      // was pressed
      // indicate start by blinking 5x
      interrupts();  // blinking needs interrupts
      blink(LED_PIN, NULL, 10, 100);
      noInterrupts();
      digitalWrite(LED_PIN, HIGH);
      runState = logging;
      break;

    case logging:
    case detached_logging:
      // only wake
      if (regularWakeup) {
        doLogging(wakeTime);
        regularWakeup = false;
      } else {
        PRINTLN(F("Bug: Should not happen!"));
      }
      break;

    case detaching:
      // The control button has been pressed, indicating that the SD card will
      // be removed. Switch to filling up the log buffer only.
      if (sdPresence == inserted || sdPresence == unknown) {
        PRINTLN(F("Detaching SD card..."));
        if (!logger.isAttached()) {
          PRINTLN(F("Bug: logger already detached"));
        }
        unsigned long _t = millis();
        if (logger.detach()) {
          PRINTLN("Closing existing file and creating new file took " +
                  String(millis() - _t) + " ms");
          // Wait 3s to be absolutely sure that everything was written.
          // FIXME: is this necessary?
          delay(3000);
          runState = detached_logging;
        } else if (sdPresence == unknown) {
          PRINTLN(F("Could not close file, reinsert SD if not present!"));
          runState = detached_logging;
        } else {  // sdPresence == present
          PRINTLN(
              F("Could not close file, data may have been lost and the file "
                "system may be corrupt!"));
          setError(SD_ERROR);
        }
      } else {
        PRINTLN(F("SD card aready removed. Please re-insert!"));
      }
      break;

    case attaching:
      // The control button has been pressed, indicating that the SD card is
      // present again.
      if (sdPresence == inserted || sdPresence == unknown) {
        PRINTLN(F("Attaching SD card..."));
        if (initSD() == 0) {
          if (logger.isAttached()) {
            PRINTLN(F("Bug: logger already attached"));
          }
          logger.attach();
          runState = logging;
        } else if (sdPresence == unknown) {
          PRINTLN(
              F("Could not create a new file, please insert SD card if "
                "missing!"));
          runState = detached_logging;
        } else {
          setError(SD_ERROR);
        }
      } else {
        PRINTLN(F("Could not create a new file, please insert SD card!"));
      }
      break;

    case state::error:
      PRINT(F("Error code "));
      PRINTLN(errorNo);
      interrupts();  // blinking needs interrupts
      if (LED_PIN == ERROR_LED_PIN) {
        blink(LED_PIN, NULL, errorNo, 200);
      } else {
        // if there is a separate error LED, blink it
        // synchronously
        blink(LED_PIN, ERROR_LED_PIN, errorNo, 300);
      }
      break;
  }

  // re-enable interrupts
  interrupts();

  // switch LED off
  digitalWrite(LED_PIN, LOW);

  // indicate detached logging
  if (runState == detached_logging) {
    delay(300);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
  }

  // Increment next wake time until it lies in the future.
  // We require the wakeup time to be at least 3s in the future,
  // to be sure that the sensor will really wake up. If the
  // loop took too long, this will lead to one or several
  // measurements being skipped.
  const uint32_t minTimeDiff = 3;
  uint32_t now = rtc.now().unixtime();
  while (wakeTime <= now + minTimeDiff) {
    wakeTime += wakeInterval;
  }

  // PRINTLN("Next time: " +
  // DateTime(wakeTime).timestamp(DateTime::TIMESTAMP_FULL));

  // Set alarm, then power down
  rtc.clearAlarm(1);
  rtc.setAlarm1(DateTime(wakeTime), DS3231_A1_Second);
  standby(debugMode, SERIAL_BAUD);
}

// Obtains all the measurements (including RTC temperature and battery voltage)
// and assembles them to a tab delimited line
void doLogging(DateTime t) {
  updateTimeStamp(t);
  logLine = TimeStamp;

  logLine += '\t';
  if (!sensor.measureR4(logLine)) {
    PRINTLN(F("SDI-12 response too slow (>5s) or malformed."));
    return;
  }
  logLine += '\t';
  logLine += (String)GetBatteryVoltageMV();

  logLine += '\t';
  logLine += rtc.getTemperature();

  // add line end
  logLine += '\n';
  PRINT(logLine);

  if (!logger.append(logLine)) {
    PRINTLN(F("Logging resulted in an error! Still continuing..."));
  }
}
