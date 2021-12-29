#include "SdLogger.h"

// 4 GiB
#define MAX_FILESIZE 4294967295  // (1 << 32) - 1;

SdLogger::SdLogger(SdFat &sd, RTC_DS3231 &rtc, String loggerId,
                   uint32_t bufsize)
    : sd(sd),
      rtc(rtc),
      loggerId(loggerId),
      bufsize(bufsize),
      header(""),
      baseDir(""),
      fileName(""),
      attached(true),
      logfile(File()),
      LogBuf("") {
  LogBuf.reserve(bufsize);
}

void SdLogger::setHeader(String h) {
  header = h;
  LogBuf = header;
}

void SdLogger::setBaseDir(String bd) {
  baseDir = bd;
}

bool SdLogger::detach() {
  if (!closeFile()) {
    return false;
  };
  attached = false;
  return true;
}

void SdLogger::attach() {
  attached = true;
}

bool SdLogger::createFile() {
  // create base directory
  if (baseDir.length() > 0 && !sd.exists(baseDir.c_str()) &&
      !sd.mkdir(baseDir.c_str())) {
    // SerialUSB.print("Could not create " + baseDir);
    return false;
  }
  // obtain an unique file name
  DateTime dt = rtc.now();
  uint32_t max_i = pow(10, 4) - 1;
  char suffix[20];
  for (uint32_t i = 1; i < max_i; i++) {
    snprintf(suffix, 20, "%04d-%02d-%02d_%04d.txt", dt.year(), dt.month(),
             dt.day(), i);
    fileName = '/' + baseDir + '/' + loggerId + '-' + suffix;
    if (!sd.exists(fileName.c_str())) {
      break;
    }
    if (i == max_i) {
      // SerialUSB.print("Maximum number of " + String(max_i) + " files per day
      // reached.");
      return false;
    }
  }
  // open the file
  if (!logfile.open(fileName.c_str(), O_WRONLY | O_CREAT)) {
    // SerialUSB.print("Could not create " + fileName);
    return false;
  }
  return true;
}

// Closes the logfile and initializes the buffer with a new header
bool SdLogger::closeFile() {
  if (!writeToSd() || !logfile.close()) {
    return false;
  }
  LogBuf = header;
  return true;
}

bool SdLogger::append(String &data) {
  if (attached) {
    bool res = writeOrAppend(data);
    return res;
  }

  // detached logging
  
  // discard buffer if full
  if (LogBuf.length() + data.length() > bufsize) {
    // SerialUSB.println(F("Buffer full, discarding..."));
    LogBuf = header;
  }

  // append
  LogBuf += data;

  return true;
}

// Check state of logfile and logging buffer,
// make sure there is enough room.
bool SdLogger::writeOrAppend(String &data) {
  // check if buffer full
  uint32_t filePos = logfile.position();
  uint32_t n = LogBuf.length();
  // SerialUSB.println("File pos.: " + String(filePos) + ", LogBuf length: " +
  // String(n));

  // write buffer to SD if necessary (which also empties it)
  if (n + data.length() > bufsize) {
    if (!writeToSd()) {
      return false;
    }
    LogBuf = "";
    n = 0;
  }

  // Check size of file and create a new file if the maximum
  // possible size for FAT16 / FAT32 (4 GiB) would be exceeded.
  if (filePos + n + data.length() > MAX_FILESIZE) {
    if (!closeFile()) {
      return false;
    }
  }

  // now finally append the data
  LogBuf += data;

  return true;
}

// Writes the whole buffer to SD and flushes the file.
bool SdLogger::writeToSd() {
  if (LogBuf.length() > 0) {
    // file already created and open?
    if (!logfile.isOpen() && !createFile()) {
      return false;
    }
    // write contents of buffer
    unsigned long _t = millis();
    int n = logfile.write(LogBuf.c_str(), LogBuf.length());
    if (n != LogBuf.length()) {
      return false;
    }
    logfile.flush();
    //SerialUSB.println("Write + flush took " + String(millis() - _t) + " ms");
  }
  return true;
}
