
// Autonomo
#define RTC_INTERRUPT_PIN 10
#define BUTTON_PIN 15
#define SDCARD_PIN CS_SD
#define LED_PIN LED_BUILTIN
#define ERROR_LED_PIN LED_BUILTIN
#define SDI12_PIN 3         // The pin of the SDI-12 data bus


//These constants are used for reading the battery voltage
#define ADC_AREF 3.3
#define BATVOLTPIN BAT_VOLT
#define BATVOLT_R1 4.7
#define BATVOLT_R2 10

float GetBatteryVoltageMV() {
  uint16_t batteryVoltage = analogRead(BATVOLTPIN);
  return (ADC_AREF / 1.023) * (BATVOLT_R1 + BATVOLT_R2) / BATVOLT_R2 * batteryVoltage;
}
