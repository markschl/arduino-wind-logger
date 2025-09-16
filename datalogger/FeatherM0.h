
// --------------------------------------------
// 2. Pins
// --------------------------------------------

//// Adalogger M0
#define RTC_INTERRUPT_PIN 11  // Pin for RTC interrupt
#define BUTTON_PIN 1          // Pin for the control button
#define SDCARD_PIN 4          // Pin for communicating with the SD reader (chip select / CS pin)
#define SD_CARD_DETECT_PIN 7  // (optional): Pin signalling if the SD card was inserted/removed
#define LED_PIN 8             // Pin of a diagnostic LED
#define ERROR_LED_PIN 13      // Pin of LED used for emitting error codes
#define SDI12_PIN 12          // Pin of the SDI-12 data bus

// Function for getting battery voltage in mV
float GetBatteryVoltageMV() {
  // not known on this board as the power management is done separately
  return 0.;
}
