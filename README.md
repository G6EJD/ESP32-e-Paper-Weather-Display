# ESP32-e-Paper-Weather-Display
[![CI](https://github.com/matthias-bs/ESP32-e-Paper-Weather-Display/actions/workflows/CI.yml/badge.svg)](https://github.com/matthias-bs/ESP32-e-Paper-Weather-Display/actions/workflows/CI.yml)

<img src="https://github.com/matthias-bs/ESP32-e-Paper-Weather-Display/blob/main/weather_station_architecture.png" alt="Weather Station Architecture Diagram" width="1080">

## Features
* [Open Weather Map](https://openweathermap.org/) Weather Report / Forecast Display (from [G6EJD/ESP32-e-Paper-Weather-Display](https://github.com/G6EJD/ESP32-e-Paper-Weather-Display))
* Local Sensor Data Display
    * [Theengs Decoder](https://github.com/theengs/decoder) Bluetooth Low Energy Sensors Integration
    * [BME280 Temperature/Humidity/Barometric Pressure Sensor](https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/) Integration
* Remote Sensor Data Display
    * MQTT Client Integration (e.g. for [BresserWeatherSensorReceiver](https://github.com/matthias-bs/BresserWeatherSensorReceiver) or [BresserWeatherSensorTTN](https://github.com/matthias-bs/BresserWeatherSensorTTN))
* Switching between virtual Screens via TTP223 Touch Sensors
* Currently only 7.5" e-Paper Displays supported

## Screens
**Note:** Display quality is much better in reality than in the images below! 
### Weather Report / Forecast
![2-weather_report_forecast](https://user-images.githubusercontent.com/83612361/219954116-dd68a860-7884-4ef7-af2b-0ddd452a2d07.jpg)
### Local Sensor Data
![3-weather_local](https://user-images.githubusercontent.com/83612361/219953502-6f0e3b16-58f8-4845-b5d6-c796484c778f.jpg)
### Remote Sensor Data
![4-weather_remote](https://user-images.githubusercontent.com/83612361/219953834-cd48c8b0-d533-40d9-b4aa-15b58e0bcb52.png)


## Setup

For standalone use, download the ZIP file to your desktop.

Go to Sketch > Include Library... > Add .ZIP Library... Then, choose the ZIP file.

After inclusion, Go to File, Examples and scroll down to 'ESP32-e-paperWeather-display' and choose your version/screen size. Make sure to come back to this dialog from time to time to keep each library up to date. Also make sure that you only have one version of each of the libraries installed.

Also see: https://www.arduino.cc/en/Guide/Libraries#toc4

- [Arduino JSON](https://github.com/bblanchon/ArduinoJson) (v6 or above) by Benoît Blanchon

Download the software to your Arduino's library directory.

1. From the examples, choose depending on your module either
   - Waveshare_7_5
   - Waveshare_7_5_T7 (newer 800x480 version of the older 640x384)
   
   Code requires [GxEPD2 library](https://github.com/ZinggJM/GxEPD2)
   - which needs [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library)
   - an also requires [U8g2_for_Adafruit_GFX](https://github.com/olikraus/U8g2_for_Adafruit_GFX)

2. Obtain your [OWM API key](https://openweathermap.org/appid) - it's free

3. Edit the owm_credentials.h file in the IDE (TAB at top of IDE) and change your Language, Country, choose your units Metric or Imperial and be sure to find a valid weather station location on OpenWeatherMap, if your display has all blank values your location does not exist!

4. If your are using the older style Waveshare HAT then you need to use:
  
  **display.init(); //for older Waveshare HAT's 
  
  In the InitialiseDisplay() function, comment out the variant as required 

5. Save your files.

NOTE: See schematic for the wiring diagram, all displays are wired the same, so wire a 7.5" the same as a 4.2", 2.9" or 1.54" display! Both 2.13" TTGO T5 and 2.7" T5S boards come pre-wired. The 3.7" FireBeetle example contains wiring details.

The Battery monitor assumes the use of a Lolin D32 board which uses GPIO-35 as an ADC input, also it has an on-board 100K+100K voltage divider directly connected to the Battery terminals. On other boards, you will need to change the analogRead(35) statement to your board e.g. (39) and attach a voltage divider to the battery terminals. The TTGO T5 and T5S boards already contain the resistor divider on the correct pin. The FireBeetle has a battery monitor on GPIO-36.

Compile and upload the code - Enjoy!

7.5" 800x480 E-Paper Layout

![alt text width="600"](/Waveshare_7_5_new.jpg)

7.5" 640x384 E-Paper Layout

![alt text width="600"](/Waveshare_7_5.jpg)


**** NOTE change needed for latest Waveshare HAT versions ****

Ensure you have the latest GxEPD2 library

See here: https://github.com/ZinggJM/GxEPD2/releases/

Modify this line in the code:

display.init(115200, true, 2); // init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode)

Wiring Schematic for ALL Waveshare E-Paper Displays
![alt_text, width="300"](/Schematic.JPG)
