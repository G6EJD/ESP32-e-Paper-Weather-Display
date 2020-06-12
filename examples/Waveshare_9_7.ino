/* ESP32 Weather Display using an EPD 9.7" 1200x825 Display, obtains data from Open Weather Map, decodes and then displays it.
  ####################################################################################################################################
  This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.

  Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
  1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
  2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
  3. You may not, except with my express written permission, distribute or commercially exploit the content.
  4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

  The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
  software use is visible to an end-user.

  THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY
  OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  See more at http://www.dsbird.org.uk

  Caution:
  In the GxEPD2 library do this changes: 
     open the file 'src/it8951/GxEPD2_it60.h' and change 
        static const uint16_t WIDTH =  800  to 1200
  and
        static const uint16_t HEIGHT = 480  to 825.

  The e-Paper IT8951 Driver HAT needs 5V on VCC, but 3,3V are the maximum for the data connections.
*/

#include "owm_credentials.h"          // See 'owm_credentials' tab and enter your OWM API key and set the Wifi SSID and PASSWORD
#include <ArduinoJson.h>              // https://github.com/bblanchon/ArduinoJson needs version v6 or above
#include <WiFi.h>                     // Built-in
#include <ESPmDNS.h>
#include "time.h"                     // Built-in
#include <SPI.h>                      // Built-in 
#define  ENABLE_GxEPD2_GFX 1          // enable GxEPD2_GFX base class
#include <GxEPD2_BW.h>
//#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>

/* Private includes *
    to prevent not found issues make the files available in the library folder of the Arduino IDE:
    ln -s ~/Arduino/ESP32-e-Paper-Weather-Display-master/src ~/Arduino/libraries/G6EJD-Lib
*/
#include "epaper_fonts.h"
#include "forecast_record.h"
#include "lang.h"                     // Localisation (English)
//#include "lang_cz.h"                  // Localisation (Czech)
//#include "lang_fr.h"                  // Localisation (French)
//#include "lang_gr.h"                  // Localisation (German)
//#include "lang_it.h"                  // Localisation (Italian)
//#include "lang_nl.h"
//#include "lang_pl.h"                  // Localisation (Polish)

#undef BUILTIN_LED
#define BUILTIN_LED 5

#define SCREEN_WIDTH  1200             // Set for landscape mode
#define SCREEN_HEIGHT 825

enum alignment {LEFT, RIGHT, CENTER};

// Connections for e.g. LOLIN 32
static const uint8_t EPD_BUSY = 4;  // to EPD BUSY
static const uint8_t EPD_CS   = 16; // to EPD CS - Pin 5 should not be used because an issue with the HAT and deep dleep of the ESP32
static const uint8_t EPD_RST  = 15; // to EPD RST
static const uint8_t EPD_DC   = 17; // DC not used with the IT8951E-64 Chip !!!!
static const uint8_t EPD_SCK  = 18; // to EPD CLK
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23; // to EPD DIN

// Connections for e.g. Waveshare ESP32 e-Paper Driver Board
//static const uint8_t EPD_BUSY = 25;
//static const uint8_t EPD_CS   = 15;
//static const uint8_t EPD_RST  = 26;
//static const uint8_t EPD_DC   = 27;
//static const uint8_t EPD_SCK  = 13;
//static const uint8_t EPD_MISO = 12; // Master-In Slave-Out not used, as no data from display
//static const uint8_t EPD_MOSI = 14;

GxEPD2_BW<GxEPD2_it60, GxEPD2_it60::HEIGHT / 2> display(GxEPD2_it60(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY));   // B/W display
//  GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY));   // B/W display
//GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display(GxEPD2_750(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY)); // 3-colour displays
// use GxEPD_BLACK or GxEPD_WHITE or GxEPD_RED or GxEPD_YELLOW depending on display type

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;  // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
// Using fonts:
// u8g2_font_helvB08_tf
// u8g2_font_helvB10_tf
// u8g2_font_helvB12_tf
// u8g2_font_helvB14_tf
// u8g2_font_helvB18_tf
// u8g2_font_helvB24_tf
// u8g2_font_fub35_tf
// u8g2_font_fub42_tf


//################  VERSION  ##################################################
String version = "1.0 / 9.7in";     // Programme version, see change log at end
//################ VARIABLES ##################################################

boolean LargeIcon = true, SmallIcon = false;
#define Large  28           // For icon drawing, needs to be odd number for best effect
#define Small  8            // 6  For icon drawing, needs to be odd number for best effect
const byte MaxEvents = 10;  // For event reporting, the maximum that can be recorded
String     Time_str = "--:--:--";
String     Date_str = "-- -- ----";
String     EventMessage[MaxEvents]; // strings for Time, Date and Error reporting
const byte EventThreshold = 2; // Change to 1 to view all messages on e-paper screen
int        wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0, EventCnt = 0;

//################ PROGRAM VARIABLES and OBJECTS ################

#define max_readings 24

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];

#include "common.h"
#include <rom/rtc.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings]    = {0};
float rain_readings[max_readings]        = {0};
float snow_readings[max_readings]        = {0};

long SleepDuration = 30; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int  WakeupTime    = 7;  // Don't wakeup until after 07:00 to save battery power
int  SleepTime     = 23; // Sleep after (23+1) 00:00 to save battery power
long StartTime = 0, SleepTimer = 0;
long Delta         = 12; // correction factor to compensate the ESP32 speed
// Prevents display at xx:59:yy and then xx:00:yy (one minute later) to save power
String LocalIP;

//#########################################################################################
void setup() {
  StartTime = millis();
  DisableBrownOutDetector();
  VerboseRecordOfResetReason(rtc_get_reset_reason(0)); // 0 means CPU0 (Main core)
  delay(500); // Allow the PSU to stabilise

  Serial.begin(115200);

  // Early initilisation of the display to be able to display the EventLog in case of a WiFi problem
  InitialiseDisplay(); // Give screen time to initialise by getting weather data!

  if (StartWiFi() == WL_CONNECTED && SetupTime() == true) {
    if ((CurrentHour >= WakeupTime && CurrentHour <= SleepTime)) {
      //khl moved to above;     InitialiseDisplay(); // Give screen time to initialise by getting weather data!
      byte Attempts = 1;
      bool RxWeather = false, RxForecast = false;
      WiFiClient client;   // wifi client object
      while ((RxWeather == false || RxForecast == false) && Attempts <= 2) { // Try up-to 2 time for Weather and Forecast data
        if (RxWeather  == false) RxWeather  = obtain_wx_data(client, "weather");
        if (RxForecast == false) RxForecast = obtain_wx_data(client, "forecast");
        Attempts++;
      }
      if (RxWeather && RxForecast) { // Only if received both Weather or Forecast proceed
        StopWiFi(); // Reduces power consumption
        DisplayWeather();
      }
      else
      {
        if (!RxWeather)  AddToEventLog("*** Failed to Rx Weather data ***");
        if (!RxForecast) AddToEventLog("*** Failed to Rx Forecast data ***");
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("StopWiFi to reduce power consumption");
    StopWiFi(); // Reduces power consumption
  }

  if ((CurrentHour == SleepTime)) {
    Serial.println("Sleeping for 3600 * WakeupTime seconds....");
    SleepTimer = WakeupTime * 3600;
  }
  else {
    SleepTimer = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec - Delta)); //Some ESP32 are too fast to maintain accurate time
  }
  BeginSleep();
}
//#########################################################################################
void loop() { // this will never run!
}
//#########################################################################################
void BeginSleep() {

  // The window size is set accordingly to the number of events
  // the report event section:  display.setPartialWindow(wx, wy, tbw, tbh)
  AddToEventLog("*** Entering Sleep *** at " + Time_str + " on " + Date_str);
  if (EventCnt > EventThreshold) {
    display.firstPage();
    do {
    ReportEvent(EventMessage);
    } while (display.nextPage());
  }
  Serial.printf("Available heap : %d\n", ESP.getFreeHeap());
  Serial.printf("Min free heap:   %d\n", ESP.getMinFreeHeap());
  Serial.printf("Largest block of heap: %d\n", ESP.getMaxAllocHeap());
  delay(500);
  //  display.powerOff();
  display.hibernate();     // Hibernate instead of powerOff

  esp_sleep_enable_timer_wakeup(SleepTimer * 1000000LL);

#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Entering " + String(SleepTimer) + "-secs of sleep time");
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();      // Sleep for e.g. 30 minutes
}
//#########################################################################################
void DisableBrownOutDetector() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
}
//#########################################################################################
void DisplayWeather() {                          // 9.7" e-paper display is 1200x825 resolution
  display.firstPage();
  do
  {
    DisplayGeneralInfoSection();                 // Top line of the display
    DisplayDisplayWindSection(180, 250, WxConditions[0].Winddir, WxConditions[0].Windspeed, 130);
    DisplayAstronomySection(5, 410);             // Astronomy section Sun rise/set, Moon phase and Moon icon
    DisplayMainWeatherSection(499, 190);          // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
    DisplayStatusSection(990, 303, wifi_signal); // Wi-Fi signal strength and Battery voltage
    DisplayForecastSection(372, 390);             // 3hr forecast boxes
  }
  while (display.nextPage());
}
//#########################################################################################
void DisplayGeneralInfoSection() {
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(12, 20, "[Version: " + version + "]", LEFT); // Programme version
  drawString(SCREEN_WIDTH / 2, 20, City, CENTER);
  // Uncomment the next line if the display of IP- and MAC-Adddress is wanted
  //drawString(SCREEN_WIDTH - 150, 20, "IP=" + LocalIP + ",  MAC=" + WiFi.macAddress() ,RIGHT);
  display.drawLine(5, 40, SCREEN_WIDTH - 8, 40, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  drawString(720, 330, Date_str, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  drawString(730, 370, TXT_UPDATED + Time_str, CENTER);
}
//#########################################################################################
void DisplayMainWeatherSection(int x, int y) {  // (x=500, y=190)
  //  display.drawRect(x-67, y-65, 140, 182, GxEPD_BLACK);
  display.drawLine(5, 80, SCREEN_WIDTH - 8, 80,  GxEPD_BLACK);
  DisplayConditionsSection(x + 3, y + 50, WxConditions[0].Icon, LargeIcon);
  DisplayTemperatureSection(x + 222, y - 150, 180, 170);
  DisplayPressureSection(x + 387, y - 150, 180, 170,  WxConditions[0].Pressure, WxConditions[0].Trend);
  DisplayPrecipitationSection(x + 562, y - 150, 181, 170);
  DisplayForecastTextSection(x + 147, y + 22, 548, 90);
}
//#########################################################################################
void DisplayDisplayWindSection(int x, int y, float angle, float windspeed, int Cradius) {
  arrow(x, y, Cradius - 22, angle, 18, 33); // Show wind direction on outer circle of width and length
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x - 30, y - Cradius - 60, TXT_WIND_SPEED_DIRECTION, CENTER);
  int dxo, dyo, dxi, dyi;
  display.drawLine(5, 40, 5, y + Cradius + 50, GxEPD_BLACK);
  display.drawCircle(x, y, Cradius, GxEPD_BLACK);     // Draw compass circle
  display.drawCircle(x, y, Cradius + 1, GxEPD_BLACK); // Draw compass circle
  display.drawCircle(x, y, Cradius * 0.7, GxEPD_BLACK); // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45)  drawString(dxo + x + 27, dyo + y - 12, TXT_NE, CENTER);
    if (a == 135) drawString(dxo + x + 27, dyo + y + 6,  TXT_SE, CENTER);
    if (a == 225) drawString(dxo + x - 43, dyo + y + 6,  TXT_SW, CENTER);
    if (a == 315) drawString(dxo + x - 43, dyo + y - 12, TXT_NW, CENTER);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
  }
  drawString(x - 3, y - Cradius - 22, TXT_N, CENTER);
  drawString(x - 5, y + Cradius + 26, TXT_S, CENTER);
  drawString(x - Cradius - 27, y - 3, TXT_W, CENTER);
  drawString(x + Cradius + 15, y - 3, TXT_E, CENTER);
  drawString(x - 12, y - 53, WindDegToDirection(angle), CENTER);
  drawString(x + 3, y + 50, String(angle, 0) + "°", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  drawString(x - 12, y - 3, String(windspeed, 1), CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x - 5, y + 22, (Units == "M" ? "m/s" : "mph"), CENTER);
}
//#########################################################################################
String WindDegToDirection(float winddirection) {
  if (winddirection >= 348.75 || winddirection < 11.25)  return TXT_N;
  if (winddirection >=  11.25 && winddirection < 33.75)  return TXT_NNE;
  if (winddirection >=  33.75 && winddirection < 56.25)  return TXT_NE;
  if (winddirection >=  56.25 && winddirection < 78.75)  return TXT_ENE;
  if (winddirection >=  78.75 && winddirection < 101.25) return TXT_E;
  if (winddirection >= 101.25 && winddirection < 123.75) return TXT_ESE;
  if (winddirection >= 123.75 && winddirection < 146.25) return TXT_SE;
  if (winddirection >= 146.25 && winddirection < 168.75) return TXT_SSE;
  if (winddirection >= 168.75 && winddirection < 191.25) return TXT_S;
  if (winddirection >= 191.25 && winddirection < 213.75) return TXT_SSW;
  if (winddirection >= 213.75 && winddirection < 236.25) return TXT_SW;
  if (winddirection >= 236.25 && winddirection < 258.75) return TXT_WSW;
  if (winddirection >= 258.75 && winddirection < 281.25) return TXT_W;
  if (winddirection >= 281.25 && winddirection < 303.75) return TXT_WNW;
  if (winddirection >= 303.75 && winddirection < 326.25) return TXT_NW;
  if (winddirection >= 326.25 && winddirection < 348.75) return TXT_NNW;
  return "?";
}
//#########################################################################################
void DisplayTemperatureSection(int x, int y, int twidth, int tdepth) {
  display.drawRect(x - 75, y, twidth, tdepth, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x, y + 20, TXT_TEMPERATURES, CENTER);
  u8g2Fonts.setFont(u8g2_font_fub35_tf);
  drawString(x - 30, y + 100, String(WxConditions[0].Temperature, 1) + "°", CENTER); // Show current Temperature
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  drawString(x + 73, y + 100, Units == "M" ? "C" : "F", LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);                                       
  drawString(x + 10, y + 140, String(WxConditions[0].High, 0) + "° | " + String(WxConditions[0].Low, 0) + "°", CENTER); // Show forecast high and Low
}
//#########################################################################################
void DisplayForecastTextSection(int x, int y , int fwidth, int fdepth) {
  String Wx_Description;
  display.drawRect(x, y, fwidth, fdepth, GxEPD_BLACK); // forecast text outline
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  if (Language == "DE")
    Wx_Description = WxConditions[0].Forecast0;
  else {
    Wx_Description = WxConditions[0].Main0;
    if (WxConditions[0].Forecast0 != "") Wx_Description += " (" + WxConditions[0].Forecast0;
  }
  if (WxConditions[0].Forecast1 != "") Wx_Description += ", " + WxConditions[0].Forecast1;
  if (WxConditions[0].Forecast2 != "") Wx_Description += ", " + WxConditions[0].Forecast2;
  if (Wx_Description.indexOf("(") > 0) Wx_Description += ")";
  int MsgWidth = 43; // Using proportional fonts, so be aware of making it too wide!
  if (Language == "DE") drawStringMaxWidth(x + 30, y + 40, MsgWidth, Wx_Description, LEFT); // Leave German text in original format, 28 character screen width at this font size
  else                  drawStringMaxWidth(x + 30, y + 40, MsgWidth, TitleCase(Wx_Description), LEFT); // 28 character screen width at this font size
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
}
//#########################################################################################
void DisplayForecastWeather(int x, int y, int index) {
  int fwidth = 103;
  x = x + fwidth * index;
  display.drawRect(x, y, fwidth - 2, 150, GxEPD_BLACK);
  display.drawLine(x, y + 40, x + fwidth - 3, y + 40, GxEPD_BLACK);
  DisplayConditionsSection(x + fwidth / 2, y + 90, WxForecast[index].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + fwidth / 2 - 10, y + 20, String(WxForecast[index].Period.substring(11, 16)), CENTER);
  drawString(x + fwidth / 2 + 0, y + 130, String(WxForecast[index].High, 0) + "°/" + String(WxForecast[index].Low, 0) + "°", CENTER);
}
//#########################################################################################
void DisplayPressureSection(int x, int y, int pwidth, int pdepth, float pressure, String slope) {
  display.drawRect(x - 56, y, pwidth, pdepth, GxEPD_BLACK); // pressure outline
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + 8, y + 20, TXT_PRESSURE, CENTER);
  String slope_direction = TXT_PRESSURE_STEADY;
  if (slope == "+") slope_direction = TXT_PRESSURE_RISING;
  if (slope == "-") slope_direction = TXT_PRESSURE_FALLING;
  u8g2Fonts.setFont(u8g2_font_fub35_tf);
  if (Units == "I") drawString(x - 22, y + 100, String(pressure, 2), CENTER); // "Imperial"
  else              drawString(x - 22, y + 100, String(pressure, 0), CENTER); // "Metric"
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x - 3, y + 150, slope_direction, CENTER);
  display.drawRect(x + 75, y + 135, 49, 35, GxEPD_BLACK);
  drawString(x + 90, y + 150, (Units == "M" ? "hPa" : "in"), CENTER);
}
//#########################################################################################
void DisplayPrecipitationSection(int x, int y, int pwidth, int pdepth) {
  display.drawRect(x - 48, y, pwidth, pdepth, GxEPD_BLACK); // precipitation outline
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + 10, y + 20, TXT_PRECIPITATION_SOON, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  if (WxForecast[1].Rainfall >= 0.005) { // Ignore small amounts
    drawString(x - 15, y + 80, String(WxForecast[1].Rainfall, 2) + (Units == "M" ? "mm" : "in"), LEFT); // Only display rainfall total today if > 0
    addraindrop(x + 70, y + 80, 7);
  }
  if (WxForecast[1].Snowfall >= 0.005)  // Ignore small amounts
    drawString(x - 15, y + 110, String(WxForecast[1].Snowfall, 2) + (Units == "M" ? "mm" : "in") + " **", LEFT); // Only display snowfall total today if > 0
}
//#########################################################################################
void DisplayAstronomySection(int x, int y) {
  display.drawRect(x, y + 20, 365, 110, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + 14, y + 54, ConvertUnixTime(WxConditions[0].Sunrise).substring(0, 5) + " " + TXT_SUNRISE, LEFT);
  drawString(x + 14, y + 79, ConvertUnixTime(WxConditions[0].Sunset).substring(0, 5) + " " + TXT_SUNSET, LEFT);
  time_t now = time(NULL);
  struct tm * now_utc  = gmtime(&now);
  const int day_utc = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc = now_utc->tm_year + 1900;
  drawString(x + 14, y + 104, MoonPhase(day_utc, month_utc, year_utc, Hemisphere), LEFT);
  DrawMoon(x + 225, y + 5, day_utc, month_utc, year_utc, Hemisphere);
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  const int diameter = 75;
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "south") Phase = 1 - Phase;
  // Draw dark part of moon
  display.fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++) {
    double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);
    // Determine the edges of the lighted part of the moon
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5) {
      Xpos1 = -Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    display.drawLine(pW1x, pW1y, pW2x, pW2y, GxEPD_WHITE);
    display.drawLine(pW3x, pW3y, pW4x, pW4y, GxEPD_WHITE);
  }
  display.drawCircle(x + diameter - 1, y + diameter, diameter / 2, GxEPD_BLACK);
}
//#########################################################################################
String MoonPhase(int d, int m, int y, String hemisphere) {
  int c, e;
  double jd;
  int b;
  if (m < 3) {
    y--;
    m += 12;
  }
  ++m;
  c   = 365.25 * y;
  e   = 30.6  * m;
  jd  = c + e + d - 694039.09;     /* jd is total days elapsed */
  jd /= 29.53059;                        /* divide by the moon cycle (29.53 days) */
  b   = jd;                              /* int(jd) -> b, take integer part of jd */
  jd -= b;                               /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;                /* scale fraction from 0-8 and round by adding 0.5 */
  b   = b & 7;                           /* 0 and 8 are the same phase so modulo 8 for 0 */
  if (hemisphere == "south") b = 7 - b;
  if (b == 0) return TXT_MOON_NEW;              // New;              0%  illuminated
  if (b == 1) return TXT_MOON_WAXING_CRESCENT;  // Waxing crescent; 25%  illuminated
  if (b == 2) return TXT_MOON_FIRST_QUARTER;    // First quarter;   50%  illuminated
  if (b == 3) return TXT_MOON_WAXING_GIBBOUS;   // Waxing gibbous;  75%  illuminated
  if (b == 4) return TXT_MOON_FULL;             // Full;            100% illuminated
  if (b == 5) return TXT_MOON_WANING_GIBBOUS;   // Waning gibbous;  75%  illuminated
  if (b == 6) return TXT_MOON_THIRD_QUARTER;    // Third quarter;   50%  illuminated
  if (b == 7) return TXT_MOON_WANING_CRESCENT;  // Waning crescent; 25%  illuminated
  return "";
}
//#########################################################################################
void DisplayForecastSection(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  int f = 0;
  do {
    DisplayForecastWeather(x, y, f);
    f++;
  } while (f <= 7);
  // Pre-load temporary arrays with with data - because C parses by reference
  int r = 1;
  do {
    if (Units == "I") pressure_readings[r] = WxForecast[r].Pressure * 0.02953;   else pressure_readings[r] = WxForecast[r].Pressure;
    if (Units == "I") rain_readings[r]     = WxForecast[r].Rainfall * 0.0393701; else rain_readings[r]     = WxForecast[r].Rainfall;
    if (Units == "I") snow_readings[r]     = WxForecast[r].Snowfall * 0.0393701; else snow_readings[r]     = WxForecast[r].Snowfall;
    temperature_readings[r] = WxForecast[r].Temperature;
    humidity_readings[r]    = WxForecast[r].Humidity;
    r++;
  } while (r <= max_readings);
  
  int gwidth = 230, gheight = 150;
  int gx = (SCREEN_WIDTH - gwidth * 4) / 5 + 7;
  int gy = (SCREEN_HEIGHT - gheight - 50);
  int gap = gwidth + gx;
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  drawString(SCREEN_WIDTH / 2 - 50, gy - 50, TXT_FORECAST_VALUES, CENTER); // Based on a graph height of 60
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  // (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  DrawGraph(gx + 0 * gap, gy, gwidth, gheight, 900, 1050, Units == "M" ? TXT_PRESSURE_HPA : TXT_PRESSURE_IN, pressure_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 10, 30,    Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx + 2 * gap, gy, gwidth, gheight, 0, 100,   TXT_HUMIDITY_PERCENT, humidity_readings, max_readings, autoscale_off, barchart_off);
  if (SumOfPrecip(rain_readings, max_readings) >= SumOfPrecip(snow_readings, max_readings))
    DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_readings, max_readings, autoscale_on, barchart_on);
  else DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_SNOWFALL_MM : TXT_SNOWFALL_IN, snow_readings, max_readings, autoscale_on, barchart_on);
}
//#############################################################################################
void DisplayConditionsSection(int x, int y, String IconName, bool IconSize) {
  Serial.println("Icon name: " + IconName);
  if      (IconName == "01d" || IconName == "01n")  Sunny(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n")  MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n")  Cloudy(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n")  MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n")  ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n")  Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n")  Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n")  Snow(x, y, IconSize, IconName);
  else if (IconName == "50d")                       Haze(x, y, IconSize, IconName);
  else if (IconName == "50n")                       Fog(x, y, IconSize, IconName);
  else                                              Nodata(x, y, IconSize, IconName);
  if (IconSize == LargeIcon) {
    display.drawRect(x - 130, y - 200, 270, 350, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(x - 20, y - 180, TXT_CONDITIONS, CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    drawString(x - 55, y + 120, String(WxConditions[0].Humidity, 0) + "%", CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(x + 35, y + 120, "RH", CENTER);
    if (WxConditions[0].Visibility > 0) Visibility(x - 100, y - 130, String(WxConditions[0].Visibility) + "M");
    if (WxConditions[0].Cloudcover > 0) CloudCover(x + 60, y - 130, WxConditions[0].Cloudcover);
  } 
}
//#########################################################################################
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  float dx = (asize - 10) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize - 10) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;         float y1 = plength;
  float x2 = pwidth / 2;  float y2 = pwidth / 2;
  float x3 = -pwidth / 2; float y3 = pwidth / 2;
  float angle = aangle * PI / 180 - 135;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  display.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, GxEPD_BLACK);
}
//#########################################################################################
uint8_t StartWiFi() {
  Serial.print("\r\nConnecting to: "); Serial.println(String(ssid));
  IPAddress dns(8, 8, 8, 8); // Google DNS
  WiFi.disconnect(true);
  delay(500);
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  uint8_t connectionStatus;
  bool AttemptConnection = true;
  while (AttemptConnection) {
    connectionStatus = WiFi.status();
    if (millis() > start + 15000) { // Wait 15-secs maximum
      AttemptConnection = false;
    }
    if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED) {
      AttemptConnection = false;
    }
    delay(1000);
    Serial.printf("WiFi connection status in loop: %d, millis=%d\n", connectionStatus, millis());
  }
  Serial.printf("WiFi connection status: %d\r\n", connectionStatus);
  if (connectionStatus == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    Serial.println("WiFi connected at: " + WiFi.localIP().toString() + ", MAC: " + WiFi.macAddress());
    LocalIP = WiFi.localIP().toString();
    Serial.println(LocalIP);
    if (!MDNS.begin("Weather-1200x825")) {
      Serial.println("Error setting up MDNS responder!");
      while (true);
    }
  }
  else
  {
    // khl add print event message to serial
    AddToEventLog("*** WiFi connection FAILED ***");
    Serial.println(EventMessage[EventCnt]);
      switch (connectionStatus) {
      case  0: AddToEventLog("*** WL_IDLE_STATUS ***"); break;      // temporary status assigned when WiFi.begin() is called and remains active until the number of attempts expires
      case  1: AddToEventLog("*** WL_NO_SSID_AVAIL ***"); break;    // assigned when no SSID or requested SSID are available
      case  2: AddToEventLog("*** WL_SCAN_COMPLETED ***"); break;   // assigned when the scan networks is completed
      case  3: AddToEventLog("*** WL_CONNECTED ***"); break;        // assigned when connected to a WiFi network
      case  4: AddToEventLog("*** WL_CONNECT_FAILED ***"); break;   // assigned when the connection fails for all the attempts
      case  5: AddToEventLog("*** WL_CONNECTION_LOST ***"); break;  // assigned when the connection is lost
      case  6: AddToEventLog("*** WL_DISCONNECTED ***"); break;     // assigned when disconnected from a network;
      default: AddToEventLog("*** WL_UNKNOWN_CONNECTION_STATUS ***"); break;     // khl: Unknown connection status
    }
  }
  // else Serial.println("WiFi connection *** FAILED ***");
  return connectionStatus;
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
//#########################################################################################
void DisplayStatusSection(int x, int y, int rssi) {
  int size = 204;
  display.drawRect(x ,y , size, 87, GxEPD_BLACK);
  display.drawLine(x ,y + 25, x + size, y + 25, GxEPD_BLACK);
  display.drawLine(x + size / 2, y, x + size / 2, y + 25, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(x + 45, y + 10, TXT_WIFI, CENTER);
  drawString(x + 150, y + 10, TXT_POWER, CENTER);
  DrawRSSI(x + 25, y + 60, rssi);
  DrawBattery(x + 130, y + 65);;
}
//#########################################################################################
void DrawRSSI(int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20)  WIFIsignal = 30; //            <-20dbm displays 5-bars
    if (_rssi <= -40)  WIFIsignal = 24; //  -40dbm to  -21dbm displays 4-bars
    if (_rssi <= -60)  WIFIsignal = 18; //  -60dbm to  -41dbm displays 3-bars
    if (_rssi <= -80)  WIFIsignal = 12; //  -80dbm to  -61dbm displays 2-bars
    if (_rssi <= -100) WIFIsignal = 6;  // -100dbm to  -81dbm displays 1-bar
    display.fillRect(x + xpos * 8, y - WIFIsignal, 7, WIFIsignal, GxEPD_BLACK);
    xpos++;
  }
  display.fillRect(x, y - 1, 5, 1, GxEPD_BLACK);
  drawString(x + 6,  y + 10, String(rssi) + "dBm", CENTER);
}
//#########################################################################################
boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "de.pool.ntp.org"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset(); // Set the TZ environment variable
  delay(100);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//#########################################################################################
boolean UpdateLocalTime() {
  struct tm timeinfo;
  char   time_output[30], day_output[30], update_time[30];
  while (!getLocalTime(&timeinfo, 15000)) { // Wait for 15-sec for time to synchronise
    AddToEventLog("*** Failed to obtain time ***");
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");      // Displays: Saturday, June 24 2017 14:05:49
  if (Units == "M") {
    if ((Language == "CZ") || (Language == "DE") || (Language == "PL") || (Language == "NL")) {
      sprintf(day_output, "%s, %02u. %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900); // day_output >> So., 23. Juni 2019 <<
    }
    else
    {
      sprintf(day_output, "%s %02u-%s-%04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    }
    strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo);  // Creates: '14:05:49'
    sprintf(time_output, " %s", update_time);
  }
  else
  {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo); // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);        // Creates: '02:05:49pm'
    sprintf(time_output, "%s %s", TXT_UPDATED, update_time);
  }
  Date_str = day_output;
  Time_str = time_output;
  return true;
}
//#########################################################################################
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.46;
  if (voltage > 1 ) { // Only display if there is a valid reading
    Serial.println("Voltage = " + String(voltage));
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.20) percentage = 0;  // orig 3.5
    display.drawRect(x + 25, y - 20, 40, 15, GxEPD_BLACK);
    display.fillRect(x + 65, y - 16, 4, 6, GxEPD_BLACK);
    display.fillRect(x + 27, y - 18, 36 * percentage / 100.0, 11, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(x + 5, y - 15, String(percentage) + "%", RIGHT);
    drawString(x + 13, y + 5,  String(voltage, 2) + "v", CENTER);
  }
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);    // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK); // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  //Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);            // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);            // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE);  // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE); // Right middle upper circle
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE); // Upper and lower lines
}
//#########################################################################################
void addraindrop(int x, int y, int scale) {
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
  x = x + scale * 1.6; y = y + scale / 3;
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) scale *= 1.34;
  for (int d = 0; d < 4; d++) {
    addraindrop(x + scale * (7.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale, bool IconSize) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.1;
      display.drawLine(dxo + x + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 5; i++) {
    display.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, GxEPD_BLACK);
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addsun(int x, int y, int scale, bool IconSize) {
  int linesize = 3;
  if (IconSize == SmallIcon) linesize = 1;
  display.fillRect(x - scale * 2, y, scale * 4, linesize, GxEPD_BLACK);
  display.fillRect(x, y - scale * 2, linesize, scale * 4, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  if (IconSize == LargeIcon) {
    display.drawLine(1 + x - scale * 1.3, y - scale * 1.3, 1 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y - scale * 1.3, 2 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y - scale * 1.3, 3 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(1 + x - scale * 1.3, y + scale * 1.3, 1 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y + scale * 1.3, 2 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y + scale * 1.3, 3 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  }
  display.fillCircle(x, y, scale * 1.3, GxEPD_WHITE);
  display.fillCircle(x, y, scale, GxEPD_BLACK);
  display.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
}
//#########################################################################################
void addfog(int x, int y, int scale, int linesize, bool IconSize) {
  if (IconSize == SmallIcon) {
    y -= 10;
    linesize = 1;
  }
  for (int i = 0; i < 6; i++) {
    display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize, GxEPD_BLACK);
  }
}
//#########################################################################################
void Sunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  else y = y - 3; // Shift up small sun icon
  if (IconName.endsWith("n")) addmoon(x, y + 3, scale, IconSize);
  scale = scale * 1.6;
  addsun(x, y, scale, IconSize);
}
//#########################################################################################
void MostlySunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3, offset = 5;
  if (IconSize == LargeIcon) {
    scale = Large;
    offset = 10;
  }
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
  addcloud(x, y + offset, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
}
//#########################################################################################
void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3;
  if (IconSize == LargeIcon) {
    scale = Large;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void Cloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    linesize = 1;
    addcloud(x, y, scale, linesize);
  }
  else {
    y += 10;
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    addcloud(x + 30, y - 45, 5, linesize); // Cloud top right
    addcloud(x - 20, y - 30, 7, linesize); // Cloud top left
    addcloud(x, y, scale, linesize);       // Main cloud
  }
}
//#########################################################################################
void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ExpectRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void Tstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addtstorm(x, y, scale);
}
//#########################################################################################
void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsnow(x, y, scale, IconSize);
}
//#########################################################################################
void Fog(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y - 5, scale, linesize);
  addfog(x, y - 5, scale, linesize, IconSize);
}
//#########################################################################################
void Haze(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x, y - 5, scale * 1.4, IconSize);
  addfog(x, y - 5, scale * 1.4, linesize, IconSize);
}
//#########################################################################################
void CloudCover(int x, int y, int CCover) {
  addcloud(x - 9, y - 3, Small * 0.8, 2); // Cloud top left
  addcloud(x + 3, y - 3, Small * 0.8, 2); // Cloud top right
  addcloud(x, y,         Small * 0.8, 2); // Main cloud
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(x + 40, y - 5, String(CCover) + "%", LEFT);
}
//#########################################################################################
void Visibility(int x, int y, String Visi) {
  y = y - 3; //
  float start_angle = 0.52, end_angle = 2.61;
  int r = 14;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    display.drawPixel(x + r * cos(i), y - r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i), GxEPD_BLACK);
  }
  start_angle = 3.61; end_angle = 5.78;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    display.drawPixel(x + r * cos(i), y + r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y + r / 2 + r * sin(i), GxEPD_BLACK);
  }
  display.fillCircle(x, y, r / 4, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(x + 12, y - 3, Visi, LEFT);
}
//#########################################################################################
void addmoon(int x, int y, int scale, bool IconSize) {
  if (IconSize == LargeIcon) {
    display.fillCircle(x - 85, y - 100, uint16_t(scale * 0.8), GxEPD_BLACK);
    display.fillCircle(x - 57, y - 100, uint16_t(scale * 1.6), GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 28, y - 37, uint16_t(scale * 1.0), GxEPD_BLACK);
    display.fillCircle(x - 20, y - 37, uint16_t(scale * 1.6), GxEPD_WHITE);
  }
}
//#########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) u8g2Fonts.setFont(u8g2_font_helvB24_tf); else u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 10, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}
//#########################################################################################
/* (C) D L BIRD
    This function will draw a graph on a ePaper/TFT/LCD display using data from an array containing data to be graphed.
    The variable 'max_readings' determines the maximum number of data elements for each array. Call it with the following parametric data:
    x_pos-the x axis top-left position of the graph
    y_pos-the y-axis top-left position of the graph, e.g. 100, 200 would draw the graph 100 pixels along and 200 pixels down from the top-left of the screen
    width-the width of the graph in pixels
    height-height of the graph in pixels
    Y1_Max-sets the scale of plotted data, for example 5000 would scale all data to a Y-axis of 5000 maximum
    data_array1 is parsed by value, externally they can be called anything else, e.g. within the routine it is called data_array1, but externally could be temperature_readings
    auto_scale-a logical value (TRUE or FALSE) that switches the Y-axis autoscale On or Off
    barchart_on-a logical value (TRUE or FALSE) that switches the drawing mode between barhcart and line graph
    barchart_colour-a sets the title and graph plotting colour
    If called with Y!_Max value of 500 and the data never goes above 500, then autoscale will retain a 0-500 Y scale, if on, the scale increases/decreases to match the data.
    auto_scale_margin, e.g. if set to 1000 then autoscale increments the scale by 1000 steps.
*/
void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int readings, boolean auto_scale, boolean barchart_mode) {
#define auto_scale_margin 0 // Sets the autoscale increment, so axis steps up in units of e.g. 3
#define y_minor_axis 5      // 5 y-axis division markers
  int maxYscale = -10000;
  int minYscale =  10000;
  int last_x, last_y;
  float x2, y2;
  if (auto_scale == true) {
    for (int i = 1; i < readings; i++ ) {
      if (DataArray[i] >= maxYscale) maxYscale = DataArray[i];
      if (DataArray[i] <= minYscale) minYscale = DataArray[i];
    }
    maxYscale = round(maxYscale + auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Max
    Y1Max = round(maxYscale + 0.5);
    if (minYscale != 0) minYscale = round(minYscale - auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Min
    Y1Min = round(minYscale);
  }
  // Draw the graph
  last_x = x_pos + 1;
  last_y = y_pos + (Y1Max - constrain(DataArray[1], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;
  display.drawRect(x_pos, y_pos, gwidth + 3, gheight + 2, GxEPD_BLACK);
  drawString(x_pos - 20 + gwidth / 2, y_pos - 16, title, CENTER);
  // Draw the data
  // x_pos = x_pos + 10;
  for (int gx = 1; gx < readings; gx++) {
    x2 = x_pos + gx * gwidth / (readings - 1) - 1 ; // max_readings is the global variable that sets the maximum data that can be plotted
    y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
    if (barchart_mode) {
      display.fillRect(x2 - 6, y2, (gwidth / readings) - 1, y_pos + gheight - y2 + 2, GxEPD_BLACK);
    } else {
      display.drawLine(last_x, last_y, x2, y2, GxEPD_BLACK);
    }
    last_x = x2;
    last_y = y2;
  }
  //Draw the Y-axis scale
#define number_of_dashes 20
  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
    for (int j = 0; j < number_of_dashes; j++) { // Draw dashed graph grid lines
      if (spacing < y_minor_axis) display.drawFastHLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes), GxEPD_BLACK);
    }
    if ((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing) < 5 || title == TXT_PRESSURE_IN) {
        drawString(x_pos - 10, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
    }
    else
    {
      if (Y1Min < 1 && Y1Max < 10) {
        drawString(x_pos - 10, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
      }
      else {
        drawString(x_pos - 18, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0), RIGHT);
      }   
    }
  }
  for (int i = 0; i <= 2; i++) {
    drawString(5 + x_pos + gwidth / 3 * i, y_pos + gheight + 10, String(i), LEFT);
  }
  drawString(x_pos + gwidth / 2, y_pos + gheight + 25, TXT_DAYS, CENTER);
}

//#########################################################################################
void drawString(int x, int y, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}
//#########################################################################################
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y);
  if (text.length() > text_width * 2) {
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    text_width = 42;
    y = y - 3;
  }
  u8g2Fonts.println(text.substring(0, text_width));
  if (text.length() > text_width) {
    u8g2Fonts.setCursor(x, y + h + 15);
    String secondLine = text.substring(text_width);
    secondLine.trim(); // Remove any leading spaces
    u8g2Fonts.println(secondLine);
  }
}
//#########################################################################################
void InitialiseDisplay() {
  display.init(115200);
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  u8g2Fonts.begin(display); // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}
//#########################################################################################
void ReportEvent(String EventMessage[]) {
  int y = int(SCREEN_HEIGHT - 40 * (EventCnt + 1) - 2 * 40);
  int wx = int (SCREEN_WIDTH * 0.1); 
  int wy = y + int (SCREEN_WIDTH * 0.1);
  int tbw = int (SCREEN_WIDTH * 0.8);
  int tbh = int ((EventCnt + 1) * 40);
  if (EventCnt > EventThreshold) {
    display.setPartialWindow(wx, wy, tbw, tbh);
    display.fillRect(wx, wy, tbw, tbh, GxEPD_WHITE);
    display.drawRect(wx, wy, tbw, tbh, GxEPD_BLACK);
  }
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  for (byte Event = 1; Event <= EventCnt; Event++) {
    if (EventCnt > EventThreshold) drawString(wx + 3, wy + 20 + (Event - 1) * 30, 
                                   "Evt#" + String(Event < 10 ? "0" : "") + String(Event) + " : " + EventMessage[Event], LEFT);
    Serial.println("Evnt#" + String(Event < 10 ? "0" : "") + String(Event) + " : " + EventMessage[Event]);
  }
}
//#########################################################################################
void AddToEventLog(String message) {
  EventCnt++;
  EventMessage[EventCnt] = message;
}
//#########################################################################################
void VerboseRecordOfResetReason(RESET_REASON reason) {
  switch ( reason)  {
    case 1  : AddToEventLog("Vbat power on reset"); break;
    case 3  : AddToEventLog("Software reset digital core"); break;
    case 4  : AddToEventLog("Legacy watch dog reset digital core"); break;
    case 5  : AddToEventLog("Deep Sleep reset digital core"); break;
    case 6  : AddToEventLog("Reset by SLC module, reset digital core"); break;
    case 7  : AddToEventLog("Timer Group0 Watch dog reset digital core"); break;
    case 8  : AddToEventLog("Timer Group1 Watch dog reset digital core"); break;
    case 9  : AddToEventLog("RTC Watch dog Reset digital core"); break;
    case 10 : AddToEventLog("Instrusion tested to reset CPU"); break;
    case 11 : AddToEventLog("Time Group reset CPU"); break;
    case 12 : AddToEventLog("Software reset CPU"); break;
    case 13 : AddToEventLog("RTC Watch dog Reset CPU"); break;
    case 14 : AddToEventLog("APP CPU reset by PRO CPU"); break;
    case 15 : AddToEventLog("Reset when Vdd voltage is not stable"); break;
    case 16 : AddToEventLog("RTC Watch dog reset digital core and rtc module"); break;
    default : AddToEventLog("Unknown reason");
  }
}
//#########################################################################################
/*
  Version 1.0 / 9.7in
   1. First Version of the weather display on an 9.7 Waveshare display
      Based on version 7_5_T7 for the 7.5 inch display written by G6EJD
   2. Trying to get accurate 30Min intervals. (the 'diff' variable added)
   3. added mDNS to assign to 'speaking' hostname
   4. The report event stuff from former weather display versions added
      Due to sporadic WiFi connection error or RX receive errors the InitialiseDisplay() 
      is moved to the begin of the setup function. If it is not moved, the ESP32 crashes,
      because the event report can't be written to the not initialized display.
*/
