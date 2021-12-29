
// --------------------------------------------
// 2. Pins
// --------------------------------------------

//// Adalogger M0
#define RTC_INTERRUPT_PIN 11
#define SD_CARD_DETECT_PIN 7
#define BUTTON_PIN 1
#define SDCARD_PIN 4
#define LED_PIN 8
#define ERROR_LED_PIN 13
#define SDI12_PIN 12

float GetBatteryVoltageMV() {
  // we don't know battery voltage
  return 0.;
}
