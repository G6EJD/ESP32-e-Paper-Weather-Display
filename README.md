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
   - Waveshare_1_54
   - Waveshare_2_13
   - Waveshare_2_7
   - Waveshare_2_9
   - Waveshare_4_2
   - Waveshare_7_5 
(instead of Mini Grafx requires [GxEPD2 library](https://github.com/ZinggJM/GxEPD2), which needs [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library), additionally requires U8g2_for_Adafruit_GFX)
   - Waveshare_7_5_T7 (the newer 800x480 version; above is 640x384)

2. Obtain your [OWM API key](https://openweathermap.org/appid) - it's free

3. Edit the owm_credentials.h file in the IDE (TAB at top of IDE) and change your Language, Country, choose your units Metric or Imperial and be sure to find a valid weather station location on OpenWeatherMap, if your display has all blank values your location does not exist!

4. If your are using the older style Waveshare HAT then you need to use:
  ** display.init(); **//for older Waveshare HAT's 
   In the InitialiseDisplay() function, comment out as required 

5. Save your files.

NOTE: See schematic for the wiring diagram, all displays are wired the same, so wire a 7.5" the same as a 4.2", 2.9" or 1.54" display! Both 2.13" TTGO T5 and 2.7" T5S boards come pre-wired.

The Battery monitor assumes the use of a Lolin D32 board which uses GPIO-35 as an ADC input, also it has an on-board 100K+100K voltage divider directly connected to the Battery terminals. On other boards, you will need to change the analogRead(35) statement to your board e.g. (39) and attach a voltage divider to the battery terminals. The TTGO T5 and T5S boards already contain the resistor divider on the correct pin.

Compile and upload the code - Enjoy!

7.5" 800x480 E-Paper Layout

![alt text width="600"](/Waveshare_7_5_new.jpg)

7.5" 640x384 E-Paper Layout

![alt text width="600"](/Waveshare_7_5.jpg)

4.2" E-Paper Layout

![alt_text, width="400"](/Waveshare_4_2.jpg)

2.7" 264x176 E-Paper Layout

![alt_text, width="400"](/Waveshare_2_7.jpg)

2.13" 250x122 E-Paper Layout

![alt_text, width="200"](/Waveshare_2_13.jpg)

1.54" E-Paper Layout

![alt_text, width="200"](/Waveshare_1_54.jpg)

**** NOTE change needed for latest Waveshare HAT versions ****

Ensure you have the latest GxEPD2 library

See here: https://github.com/ZinggJM/GxEPD2/releases/tag/1.2.10

Modify this line in the code:

display.init(115200, true, 2); // init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode)
