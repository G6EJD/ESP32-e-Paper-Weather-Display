# ESP32-e-Paper-Weather-Display
An ESP32 and a 2.9" ePaper Display reads Weather Underground and displays the weather

Download the software and install all 3 files in the same sketch folder.

Install the libraries required via the Arduino IDE Library Manager, 

Go to Sketch > Include
Library... > Manage
Libraries.... Then, for each library, put its name into the text field
to have its metadata pulled from the internet and displayed below. Select the
latest version and install it. Make sure to come back to this dialog from time to
time to keep each library up to date. Also make sure that you only have one
version of each of the libraries installed.

- Mini Grafx by Daniel Eichhorn

- ESP8266 WeatherStation by Daniel Eichhorn (required for additional fonts)

Edit the Credentials tab and place your netwrok SSID and passowrd in the variables provided.

Adjust your Country and Town location.

Go to the Main tab and choose your Units display format, set to either M for Metric or I for Imperial

Go to the time Setup function near the end of the programme and adjust your time zone to suit your location, there is a link to the list of avialable time zones.

UK is typically 'GMT0BST,M3.5.0/01,M10.5.0/02'  the 5 denotes the last Sunday in the month of 3 which is March and 10 October

Compile and upload the code - Enjoy!



