# ESP32-e-Paper-Weather-Display
An ESP32 and an ePaper Display reads [Open Weather Map](https://openweathermap.org/) and displays the weather

For standalone use, download the ZIP file to your desktop.

Go to Sketch > Include Library... > Add .ZIP Library... Then, choose the ZIP file.

After inclusion, Go to File, Examples and scroll down to 'ESP32-e-paperWeather-display' and choose your version/screen size. Make sure to come back to this dialog from time to time to keep each library up to date. Also make sure that you only have one version of each of the libraries installed.

Also see: https://www.arduino.cc/en/Guide/Libraries#toc4

- [Mini Grafx](https://github.com/ThingPulse/minigrafx) by Daniel Eichhorn
- [Arduino JSON](https://github.com/bblanchon/ArduinoJson) (v6 or above) by Benoît Blanchon

Download the software to your Arduino's library directory.

1. From the examples, choose depending on your module either
   - Waveshare_2_9
   - Waveshare_4_2
   - Waveshare_7_5 (instead of Mini Grafx requires [GxEPD2 library](https://github.com/ZinggJM/GxEPD2), which needs [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library), additionally requires U8g2_for_Adafruit_GFX)

2. Obtain your [OWM API key](https://openweathermap.org/appid) - it's free

3. Edit the owm_credentials.h file in the IDE (TAB at top of IDE) and change your Language, Country, choose your units Metric or Imperial and be sure to find a valid weather station location on OpenWeatherMap, if your display has all blank values your location does not exist!

4. Save your files.

NOTE: See schematic for the wiring diagram, all displays are wired the same, so wire a 7.5" the same as a 4.2", 2.9" or 1.54" display!

The Battery monitor assumes the use of a Lolin D32 board which uses GPIO-35 as an ADC input, also it has an on-board 100K+100K voltage divider directly connected to the Battery terminals. On other boards, you will need to change the analogRead(35) statement to your board e.g. (39) and attach a voltage divider to the battery terminals.

Compile and upload the code - Enjoy!

![alt text](/IMG_2096b.jpg width="600")

![alt_text](/Waveshare_4_2.jpg width="400")

