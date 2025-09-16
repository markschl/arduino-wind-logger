# Data loggers for the ATMOS22 wind sensors (METER)

This repository describes the assembly and operation of low-cost data loggers for the [ATMOS 22 ultrasonic anemometer](https://www.metergroup.com/en/meter-environment/products/atmos-22-ultrasonic-anemometer) (METER group). Two different assemblies of Arduino-compatible components are presented with the programming code needed to operate the loggers in low-power mode. The output files with wind and temperature measurements are saved on a microSD card.

## Logger assembly

Two kinds of logger boards with a microSD card holder were used: [SODAQ Autonomo](https://support.sodaq.com/Boards/Autonomo) (discontinued) and [Adafruit Feather M0 Adalogger](https://learn.adafruit.com/adafruit-feather-m0-adalogger). An [external DS3231 RTC](https://learn.adafruit.com/adafruit-ds3231-precision-rtc-breakout) was connected for timekeeping and waking up the device from sleep mode. A button (switch) was connected to one of the pins and ground to interact with the logger. The pin numbers, to which RTC, button and sensor were connected are defined in `SodaqAutonomo.h` and `FeatherM0.h`.

The boards were powered by a 2700mAh Li-Ion battery, which was charged by a small 0.5 W / 100mA solar panel. The SODAQ Autonomo has an integrated solar power management circuit, while the Feather M0 Adalogger was powered by a separate [solar power manager](https://wiki.dfrobot.com/Solar_Power_Manager_5V_SKU__DFR0559) (DFROBOT DFR0559) providing a 5V power source and connected by USB, simultaneously providing power to the ATMOS22 sensor, which requires of minimum 3.6 V. For the SODAQ Autonomo wit an operating voltage of 3.3V, an additional [DC/DC boost converter](https://learn.adafruit.com/adafruit-powerboost) was needed to provide a 5V source. The solar chargers seem not to adapt to cold (< 0 °C) temperatures and thus may not be ideal for continued operation in winter.

Simple diagrams of the wiring created using the software [Fritzing](https://fritzing.org/download/) are [provided here](circuits).

## How to program the loggers

### Setup

First, install the [Arduino IDE](https://www.arduino.cc/en/software) and start it.

Next, board support has to be installed by clicking on File -> Preferences and pasting the necessary JSON file URLs (separated by comma) into the text field at "Additional Boards Manager URLs". Then go to Tools -> Boards Manager, and intall the following:

- [SODAQ Autonomo](https://support.sodaq.com/getting_started/): `http://downloads.sodaq.net/package_sodaq_samd_index.json`, provides *SODAQ SAMD Boards*
- [Adafruit Adalogger](https://learn.adafruit.com/adafruit-feather-m0-adalogger/setup): `https://adafruit.github.io/arduino-board-index/package_adafruit_index.json`, provides *Adafruit SAMD Boards*

#### Ubuntu Linux

For Ubuntu, access to the serial port has to be granted by typing this in the terminal: `sudo adduser $USER dialout`. Then log out and in again.

### Arduino libraries

A few libraries are needed (Tools -> Library Manager):

- *SdFat* by Bill Greimann (tested with versions 2.3.0)
- *RTClib* by Adafruit (2.1.4)
- *Arduino Low Power* by Arduino LLC (1.2.2)
- [*SDI-12*](https://github.com/EnviroDIY/Arduino-SDI-12) by Kevin Smith et al. (2.3.0)

### Preparation

- Open `datalogger/datalogger.ino` in the Arduino IDE. Select the correct board at *Tools -> Boards -> \<board name\>*. There are separate header files named after the boards, which include all the pin settings. If you have a different board or pin wiring, you need to adjust the code and add your own header file with the correct settings.
- **Configuration:** Adjust the variables in "User settings" section if necessary
- At a minimum, give your sensor/logger combo an unique name by setting the **LOGGER_ID**. This name is prepended to all output files and helps to avoid confusion when copying them all to your computer
- Make sure that `DEBUG` is `false` if uploading the final program for autonomous operation (the default is `DEBUG true`, which is useful for setting things up)

#### Configuration

The logger is configured to do *instantaneous measurements every 10 seconds* using the `R4!` command (see [Integrator Guide](http://publications.metergroup.com/Integrator%20Guide/18195%20ATMOS%2022%20Integrator%20Guide.pdf)). The measurement interval can easily be changed in the *User Settings* section. Switching to averaged / maximum values (`R3!` command) requires replacing `sensor.measureR4(logLine)` with `sensor.measureR3(logLine)` in `datalogger.ino`. Other commands will have to be implemented first (in `Sensor.h` / `Sensor.cpp`).

### Uploading the program to the logger

Connect the logger board to the computer using a mini-USB cable. For the the boards to be ready under Ubuntu 20.04, it was usually reqired to double tap the reset button. Make sure that the device is present under *Tools -> Port*, otherwise double tap the reset button another time. `Ctrl + U` will upload the program.

If `DEBUG` is set to `true` (the default) before the upload, the progress of the logger operation can be monitored in the serial console (Tools -> Serial Monitor) while still connected by the USB cable. This mode also allows setting the time of the RTC and formatting the SD card if necessary. For regular operation, the program should again be uploaded with DEBUG disabled.

## Preparing the SD cards

We used SD cards with 2 GB capacity (which is more than enough for long logging). Smaller cards tend to have less powerful chips that [also draw less power](https://thecavepearlproject.org/2015/11/05/a-diy-arduino-data-logger-build-instructions-part-4-power-optimization). Cards smaller than 2GB should be formatted to FAT16. In Linux, this can be done as follows (given the device of the SD card):

```sh
sudo mkdosfs -F 16 /dev/sdb1
```

Alternatively, the logger is able to detect problematic formatting and format the SD card when in debug mode (see above).

## Logger operation

Make sure to upload the program with `DEBUG` set to `false`.

- Startup is indicated by the LED blinking 10x. The SD card must be present. The LED will also light up during wind measurement (which takes a bit less than a second).

- Errors are indicated by the LED blinking 3-5 times every 10 seconds. 3x indicates an error with the SD card (not inserted, wrong format or any writing error). 4x indicates an error with the sensor (not connected, ...). 5x indicates a problem with the clock (RTC). To restart the logger, press the control button or board reset button.

- Before removing the SD card, press the control button (not the reset button) once and wait until the LED light has disappeared. The logger will continue measuring, but blinking twice (long-short) to indicate "detached" logging. After re-inserting the SD card (or a new one), again press the control button. Further regular measurements will be indicated by the "normal" single blinking event of the LED. If the long-short blinking pattern persists, this means that the logger is still in detached mode because there was a problem with the SD card (not inserted / wrong format).

- Every time the SD card is (re-)inserted, the logger creates a new file in the `atmos22` directory named according to the insertion date. The `txt` files can later be combined in Excel or R.
