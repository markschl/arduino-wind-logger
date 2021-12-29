#ifndef SdLogger_h
#define SdLogger_h

#include "Arduino.h"
#include "RTClib.h"
#include "SdFat.h"

class SdLogger {
 private:
  SdFat &sd;
  RTC_DS3231 &rtc;
  File logfile;
  String loggerId;
  String baseDir;
  String header;
  String fileName;
  bool attached;
  String LogBuf;
  uint32_t bufsize;

  bool closeFile();
  bool writeOrAppend(String &data);

 public:
  SdLogger(SdFat &sd, RTC_DS3231 &rtc, String loggerId, uint32_t bufsize);
  // Creates a new logfile
  bool createFile();
  // returns the current file name
  String &getFileName() { return fileName; }
  bool isAttached() { return attached; }
  // Sets a header, which will be added to the top of every file.
  // Needs to be called before any `append()` for the header to be correct.
  void setHeader(String h);
  // Sets the directory containing all files.
  void setBaseDir(String bd);
  // Tells the logger that the SD card will be removed. All data is
  // written to the card, and further data will only be added
  // to the buffer. If full, the buffer will be discarded and
  // the data is lost.
  // Returns `false` if the file could not be closed.
  bool detach();
  // Tells the logger that the SD card has been inserted and data
  // can again be written to it. Data added in the detached state
  // is not lost, unless the buffer was completely filled and discarded.
  // *Note*: the previous file is *not* re-opened, instead a new
  // file with another header will created during the next call to `append`.
  void attach();
  // Appends data to the internal buffer and automatically writes to
  // SD if the buffer is full (if the SD is "attached"). Automatically
  // issues `createFile` if necessary.
  bool append(String &data);
  // Writes the whole buffer to SD and flushes the file.
  bool writeToSd();
};

#endif
