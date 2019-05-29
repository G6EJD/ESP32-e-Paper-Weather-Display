# ESP32-e-Paper-Weather-Display
An ESP32 and an ePaper Display reads [Open Weather Map](https://openweathermap.org/) and displays the weather

Go to Sketch > Include Library... > Manage Libraries.... Then, for each library, put its name into the text field to have its metadata pulled from the internet and displayed below. Select the latest version and install it. Make sure to come back to this dialog from time to time to keep each library up to date. Also make sure that you only have one version of each of the libraries installed.

- [Mini Grafx](https://github.com/ThingPulse/minigrafx) by Daniel Eichhorn
- [Arduino JSON](https://github.com/bblanchon/ArduinoJson) (v6 or above) by Benoît Blanchon

Download the software to your Arduino's library directory.

1. From the examples, choose depending on your module either
   - ESP32_OWM_Current_Forecast_29_epaper_vX always choose the latest version
   - ESP32_OWM_Current_Forecast_42_epaper_vX always choose the latest version
   - ESP32_OWM_Current_Forecast_75_epaper_vX always choose the latest version
   - ESP32_OWM_Current_Forecast_75_epaper_v10 (instead of Mini Grafx requires [GxEPD2 library](https://github.com/ZinggJM/GxEPD2), which needs [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library))

2. Obtain your [OWM API key](https://openweathermap.org/appid) - it's free

3. Edit the owm_credentials.h file in the IDE (TAB at top of IDE) and change your Language, Country, choose your units Metric or Imperial and be sure to find a valid weather station location on OpenWeatherMap, if your display has all blank values your location does not exist!

NOTE: See schematic for the wiring diagram, all displays are wired the same, so wire a 7.5" the same as a 4.2", 2.9" or 1.54" display!

Compile and upload the code - Enjoy!

![alt text](/IMG_2096b.jpg)

