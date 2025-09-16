
// Autonomo
#define RTC_INTERRUPT_PIN 10       // Pin for RTC interrupt
#define BUTTON_PIN 15              // Pin for the control button
#define SDCARD_PIN CS_SD           // Pin for communicating with the SD reader (chip select / CS pin)
#define LED_PIN LED_BUILTIN        // Pin of a diagnostic LED
#define ERROR_LED_PIN LED_BUILTIN  // Pin of LED used for emitting error codes
#define SDI12_PIN 3                // Pin of the SDI-12 data bus


//These constants are used for reading the battery voltage
#define ADC_AREF 3.3
#define BATVOLTPIN BAT_VOLT
#define BATVOLT_R1 4.7
#define BATVOLT_R2 10

// Function for getting battery voltage in mV
float GetBatteryVoltageMV() {
  uint16_t batteryVoltage = analogRead(BATVOLTPIN);
  return (ADC_AREF / 1.023) * (BATVOLT_R1 + BATVOLT_R2) / BATVOLT_R2 * batteryVoltage;
}
