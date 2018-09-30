# ESP32-e-Paper-Weather-Display
An ESP32 and a 2.9" ePaper Display reads Weather Underground and displays the weather

THERE ARE TWO SETS OF INSTRUCTIONS HERE: WU and OWM

Download the software and install all 3 files in the same sketch folder.

**WEATHERUNDERGROUND VERSION**

1. ESP32_WU_Current_Forecast_XX_epaper_vX
2. credentials.h
3. ArialRounded.h 

Install the libraries required via the Arduino IDE Library Manager, by:

Go to Sketch > Include
Library... > Manage
Libraries.... Then, for each library, put its name into the text field
to have its metadata pulled from the internet and displayed below. Select the
latest version and install it. Make sure to come back to this dialog from time to
time to keep each library up to date. Also make sure that you only have one
version of each of the libraries installed.

1. 'Mini Grafx' by Daniel Eichhorn
2. 'ESP8266 WeatherStation' by Daniel Eichhorn (required for additional fonts)

Edit the Credentials tab and enter your network SSID and password in the variable assignment places provided.

Adjust your Country and Town location. You need to select the correct country and city in the Credentials tab.

1. Find your country here: https://www.wunderground.com/weather-by-country.asp

2. Click on your Country and then it shows you all available City's that WU has data for, there are a lot...

3. Or you can browser the Weather Underground map and choose a station from on the map. There are many PWS too that increase the volume of data available.

Go to the Main tab and choose your Units display format, set to either 'M' for Metric or 'I' for Imperial

Go to the time Setup function near the end of the programme and adjust your time zone to suit your location, there is a link to the list of avialable time zones.

UK is typically 'GMT0BST,M3.5.0/01,M10.5.0/02'  the 5 denotes the last Sunday in the month of 3 which is March and 10 October

**OPEN WEATHER MAP VERSION**

This version requires *** Arduino JSON v5.13.2 ***

Go to Sketch > Include Library... > Manage Libraries.... Then, for each library, put its name into the text field to have its metadata pulled from the internet and displayed below. Select the latest version and install it. Make sure to come back to this dialog from time to time to keep each library up to date. Also make sure that you only have one version of each of the libraries installed.

'Mini Grafx' by Daniel Eichhorn
'ESP8266 WeatherStation' by Daniel Eichhorn (required for additional fonts)
Download the software and install all 3 files in the same sketch folder.

1. ESP32_OWM_Current_Forecast_29_epaper_vX always choose the latest version
   ESP32_OWM_Current_Forecast_42_epaper_vX always choose the latest version
   ESP32_OWM_Current_Forecast_75_epaper_vX always choose the latest version

2. owm_credentials2.h

3. ArialRounded.h 

Obtain your OWM API key - it's free

4. Edit the owm_credentials2.h file in the IDE (TAB at top of IDE) and change your Language, Country, choose your units Metric or Imperial and be sure to find a valid weather station location on OpenWeatherMap, if your display has all blank values your location does not exist!.

**OPEN WEATHER MAP VERSION - STREAMING JSON**

This version requires *** Arduino JSON v6 or above *** it streams the Openweather and processes the data as received, rather than downloading all the data first, then decoding it. It uses a lot less memory this way.

Go to Sketch > Include Library... > Manage Libraries.... Then, for each library, put its name into the text field to have its metadata 

NOTE: See scematic for the wiring diagram, all displays are wired the same, so wire a 7.5" the same as a 1.54" or 2.9" display!

Compile and upload the code - Enjoy!

![alt text](https://github.com/G6EJD/ESP32-e-Paper-Weather-Display/blob/master/IMG_2096b.jpg)

