/* ESP32 Weather Display using an EPD 4.2" Display, obtains data from Open Weather Map, decodes it and then displays it.
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
*/

//#define DRAW_GRID 1
#define DEBUG 0 //0 off, 1 on

#include "owm_credentials.h"  // See 'owm_credentials' tab and enter your OWM API key and set the Wifi SSID and PASSWORD
#include <ArduinoJson.h>       // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>              // Built-in
#include "time.h"              // Built-in
#include <SPI.h>               // Built-in
#define  ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <driver/adc.h>
#include "esp_adc_cal.h"
#include "epaper_fonts.h"
#include "forecast_record.h"
#include "lang.h"
//#include "lang_cz.h"                // Localisation (Czech)
//#include "lang_fr.h"                // Localisation (French)
//#include "lang_gr.h"                // Localisation (German)
//#include "lang_it.h"                // Localisation (Italian)
//#include "lang_nl.h"                // Localisation (Dutch)
//#include "lang_pl.h"                // Localisation (Polish)

#define SCREEN_WIDTH  480.0    // Set for landscape mode, don't remove the decimal place!
#define SCREEN_HEIGHT 280.0

enum alignment {LEFT, RIGHT, CENTER};

// Connections for Firebeetle DFR0478
static const uint8_t EPD_BUSY = 25;  // D2 to EPD BUSY
static const uint8_t EPD_CS   = 5;  // D8 to EPD CS
static const uint8_t EPD_RST  = 26; // D3 to EPD RST
static const uint8_t EPD_DC   = 27; // D4 to EPD DC
static const uint8_t EPD_SCK  = 18; // SCK to EPD CLK
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23; // MOSI to EPD DIN

GxEPD2_BW<GxEPD2_370_TC1, GxEPD2_370_TC1::HEIGHT> display(GxEPD2_370_TC1(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;  // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall

// Using fonts:
// u8g2_font_helvB08_tf
// u8g2_font_helvB10_tf
// u8g2_font_helvB12_tf
// u8g2_font_helvB14_tf
// u8g2_font_helvB18_tf
// u8g2_font_helvB24_tf

//################  VERSION  ##########################
String version = "12.5";     // Version of this program
//################ VARIABLES ###########################

boolean LargeIcon = true, SmallIcon = false;
#define Large  11           // For icon drawing, needs to be odd number for best effect
#define Small  5            // For icon drawing, needs to be odd number for best effect
String  time_str, date_str; // strings to hold time and received weather data
int     wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long    StartTime = 0;

//################ PROGRAM VARIABLES and OBJECTS ################

#define max_readings 24

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];

//Our defined 100/0% battery range for our single cell lipo.
// 3.7v is somewhat conservative, but better we trigger early than
// too late. Also, under 3.7v (which is around 10% of capacity left), the
// voltage drops off pretty fast, so is hard to measure.
// 4.2v is also the battery charger voltage when charging, although my firebeetle
// measured it as 4.27v, that is just about in spec. of the 1.5% variance on the charger
// chip, and probably affected more by the ADC generic inaccuracies.
#define LIPO_MAX_V  4.2
#define LIPO_MIN_V  3.7

#include <common.h>

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings]    = {0};
float rain_readings[max_readings]        = {0};
float snow_readings[max_readings]        = {0};

const long SleepDuration = 60; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
const int  WakeupTime    = 7;  // Don't wakeup until after 07:00 to save battery power
const int  SleepTime     = 23; // Sleep after 11pm to save battery power

//#########################################################################################
void setup() {
  if( DEBUG ) {
    //Re-initialise the serial port after clock speed change, if we are using it.
    Serial.begin(115200);
    Serial.println("Debug enabled");
  } else {
    //testing
    //Serial.begin(115200);
    //Serial.println("Debug disabled");
  }
  StartTime = millis();
  //If we have woken up then the presumption is we should do a refresh
  // The 'back to sleep' code should work out when we have the 'long snooze' period and do the right thing.
  if (StartWiFi() == WL_CONNECTED && SetupTime() == true) {
    if (DEBUG) Serial.println("Wifi connected");
    InitialiseDisplay(); // Give screen time to initialise by getting weather data!
    byte Attempts = 1;
    bool RxWeather = false, RxForecast = false;
    WiFiClient client;   // wifi client object
    while ((RxWeather == false || RxForecast == false) && Attempts <= 2) { // Try up-to 2 time for Weather and Forecast data
      if (RxWeather  == false) RxWeather  = obtain_wx_data(client, "weather");
      if (RxForecast == false) RxForecast = obtain_wx_data(client, "forecast");
      Attempts++;
    }
    StopWiFi(); //Turn off as soon as we can to reduce power consumption
    if (RxWeather && RxForecast) { // Only if received both Weather or Forecast proceed
      if (DEBUG) Serial.println("Got weather");
      DisplayWeather();
      display.display(false); // Full screen update mode
    } else {
      if (DEBUG) Serial.println("Failed to get weather");
    }
  }
  if (DEBUG) Serial.println("and go to sleep");
  BeginSleep();
}
//#########################################################################################
void loop() { // this will never run!
}
//#########################################################################################
void BeginSleep() {
  long SleepSeconds;
  long number_of_hours;

  //Even though the firebeetle has a 32.768KHz xtal installed on the board, you have to enable
  // CONFIG_ESP32_RTC_CLK_SRC in the esp32 SDK library build to enable it, and there is afaik no
  // easy way to do that from the Arduino land - so, for now, we'll just run with the default clock.

  display.powerOff();

  // There are two 'versions' of the sleep/wake times - one which covers a day change (so SleepTime>WakeTime),
  // and one that is during the same day (so SleepTime<WakeTime). To help figure this out, here is a visual
  // representation of the two types:

  //Day             |          Day 0                                 |          Day 1                                 |
  //Hour            |000102030405060708091011121314151617181920212223|000102030405060708091011121314151617181920212223|
  //Sleep 11pm-7am  |SSSSSSSSSSSSSSSS                              SS|SSSSSSSSSSSSSSSS                              SS|
  //Sleep 1am-8am   |  SSSSSSSSSSSSSSSS                              |  SSSSSSSSSSSSSSSS                              |

  //Check which type of sleep range we have, as we have to cater for the extra day changeover in the sleep math.
  if ( SleepTime > WakeupTime ) {
    // The sleep period covers midnight, and thus 'two days'

    // When the sleep period spans two days, over midnight, the easiest way to check if we need a long nap is... to actually check if we are in a short sleep
    // period. Note, we only do a check of the Hour - just makes things much easier to code up
    if ( (CurrentHour >= WakeupTime) && (CurrentHour < SleepTime) ) {
      //Short sleep
      SleepSeconds = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec));
    } else {
      //Long sleep

      //Cater for the possibility of the current hour being before or after midnight...
      if (CurrentHour >= SleepTime ) //Still in the first day
        number_of_hours = (24 - CurrentHour) + WakeupTime;  //How many hours until we are meant to next awake
      else
        number_of_hours = WakeupTime - CurrentHour;  //How many hours until we are meant to next awake

      SleepSeconds = (number_of_hours * 60 * 60) - (((CurrentMin % 60) * 60) + CurrentSec);
    }
  } else {
    // The sleep period is all in the same day - presumably then, SleepTime is 'after midnight', and always
    // smaller than WakeupTime, which makes the math easier.

    // Check if we are in the deep sleep period
    if ( (CurrentHour >= SleepTime) && (CurrentHour < WakeupTime) ) {
      //Long sleep
      number_of_hours = WakeupTime - CurrentHour;  //How many hours until we are meant to next awake
      SleepSeconds = (number_of_hours * 60 * 60) - (((CurrentMin % 60) * 60) + CurrentSec);
    } else {
      //Short sleep
      SleepSeconds = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec));
    }
  }

  //I tried to check what the longest we can sleep is for. The API docs say this function can return an error, but if you look in
  // the actual source code, it always returns OK. So, well, we'll just have to see...
  //Right, I found a few details. Looks like the RTC timer register is 48bit, and by default it will be following the slow_ck clock in the
  // ESP32 (especially as we've not enabled the 32KHz xtal)...  slow_ck runs at 150KHz by default.
  // 2^48 is about 2.8x10^14. 2.8x10^14/150,000 == ~1876499844 - seconds?
  // Well, there are about 31.5million seconds a year - which indicates to me we can sleep for... >59 years???
  // If somebody would like to check that math - please let's update this if we need to ;-)
  esp_sleep_enable_timer_wakeup((SleepSeconds+20) * 1000000LL); // Added +20 seconnds to cover ESP32 RTC timer source inaccuracies

#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  if (DEBUG) Serial.println("Entering " + String(SleepSeconds) + "-secs of sleep time");
  if (DEBUG) Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  if (DEBUG) Serial.println("Starting deep-sleep period...");

  esp_deep_sleep_start();      // Sleep for e.g. 30 minutes
}
//#########################################################################################
void DisplayWeather() {                 // 3.7" e-paper display is 280x480 resolution
  if (DEBUG) Serial.println("and draw weather");
#if DRAW_GRID
  Draw_Grid();
#endif
  DrawHeadingSection();                 // Top line of the display
  DrawMainWeatherSection(0, 20);      // current weather section, top 1/3rd of the display, roughly
  DrawAstronomySection(320, 20);        // Astronomy section Sun rise/set, Moon phase and Moon icon
  DrawForecastSection(0, 120);         // 3hr forecast boxes

  //We draw the battery last, as if it needs to put up a big warning for low battery it can now
  // overlay it over all other stuff on the screen.
  DrawBattery(SCREEN_WIDTH-100, 18);
}
//#########################################################################################
// Help debug screen layout by drawing a grid of little crosses
void Draw_Grid() {
  int x, y;
  const int grid_step = 10;

  //Draw the screen border so we know how far we can push things out
  display.drawLine(0, 0, SCREEN_WIDTH-1, 0, GxEPD_BLACK);  //across top
  display.drawLine(0, SCREEN_HEIGHT-1, SCREEN_WIDTH-1, SCREEN_HEIGHT-1, GxEPD_BLACK); //across bottom
  display.drawLine(0, 0, 0, SCREEN_HEIGHT-1, GxEPD_BLACK);  //lhs
  display.drawLine(SCREEN_WIDTH-1, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1, GxEPD_BLACK);  //rhs

  for( x=grid_step; x<SCREEN_WIDTH; x+=grid_step ) {
    for( y=grid_step; y<SCREEN_HEIGHT; y+=grid_step ) {
      display.drawLine(x-1, y, x+1, y, GxEPD_BLACK);  //Horizontal line
      display.drawLine(x, y-1, x, y+1, GxEPD_BLACK);  //Vertical line
    }
  }
}
//#########################################################################################
void DrawHeadingSection() {
  //Header bar is 20pix high, including the underline
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(SCREEN_WIDTH / 2, 0, City, CENTER);  //City in the middle
  drawString(1, 0, date_str, LEFT);   //Date on the left
  // I don't like having the time on the screen - it's not the 'current' time, it's the
  // time of last update, so can be a bit confusing unless clarified
  //drawString(4, 9, time_str, LEFT);
  display.drawLine(0, 19, SCREEN_WIDTH, 19, GxEPD_BLACK);
}
//#########################################################################################
void DrawMainWeatherSection(int x, int y) {
  DrawMainWx(x, y);

  //Current weather description - rainy, sunny etc.
  String Wx_Description = WxConditions[0].Forecast0;
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  if (WxConditions[0].Forecast1 != "")
    Wx_Description += " & " + WxConditions[0].Forecast1;

  drawStringMaxWidth(x + 5, y + 60, 150, TitleCase(Wx_Description), LEFT);

  display.drawRect(x, y, 159, 99, GxEPD_BLACK); //first LHS 3rd

  DisplayWXicon(x + 160 +80, y+55, WxConditions[0].Icon, LargeIcon);
  display.drawRect(x+160, y, 159, 99, GxEPD_BLACK); //middle 3rd
}
//#########################################################################################
void DrawForecastSection(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  DrawForecastWeather(x, y, 0);
  DrawForecastWeather(x + 80, y, 1);
  DrawForecastWeather(x + 80*2, y, 2);
  DrawForecastWeather(x + 80*3, y, 3);
  DrawForecastWeather(x + 80*4, y, 4);
  DrawForecastWeather(x + 80*5, y, 5);

  //Long range 3day trend graphs
  for (int r = 0; r < max_readings; r++) {
    if (Units == "I") {
      pressure_readings[r] = WxForecast[r].Pressure * 0.02953;
      rain_readings[r]     = WxForecast[r].Rainfall * 0.0393701;
    }
    else {
      pressure_readings[r] = WxForecast[r].Pressure;
      rain_readings[r]     = WxForecast[r].Rainfall;
    }
    temperature_readings[r] = WxForecast[r].Temperature;
  }
  display.drawLine(0, y + 172, SCREEN_WIDTH, y + 172, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(SCREEN_WIDTH / 2, y + 180, TXT_FORECAST_VALUES, CENTER);

  u8g2Fonts.setFont(u8g2_font_helvB10_tf);

  //       (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  //DrawGraph(SCREEN_WIDTH / 400 * 30,  SCREEN_HEIGHT / 300 * 221, SCREEN_WIDTH / 4, SCREEN_HEIGHT / 5, 900, 1050, Units == "M" ? TXT_PRESSURE_HPA : TXT_PRESSURE_IN, pressure_readings, max_readings, autoscale_on, barchart_off);

//  DrawGraph(SCREEN_WIDTH / 400 * 158, SCREEN_HEIGHT / 300 * 221, SCREEN_WIDTH / 4, SCREEN_HEIGHT / 5, 10, 30, Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature_readings, max_readings, autoscale_on, barchart_off);
//  DrawGraph(SCREEN_WIDTH / 400 * 288, SCREEN_HEIGHT / 300 * 221, SCREEN_WIDTH / 4, SCREEN_HEIGHT / 5, 0, 30, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_readings, max_readings, autoscale_on, barchart_on);
  DrawGraph(0+20, 220, 220-15, 40, 10, 30, Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(240+20, 220, 220-15, 40, 0, 30, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_readings, max_readings, autoscale_on, barchart_on);

}
//#########################################################################################
void DrawForecastWeather(int x, int y, int index) {
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  display.drawRect(x, y, 80, 80, GxEPD_BLACK);   //outer rectangle
  display.drawLine(x + 1, y + 20, x + 79, y + 20, GxEPD_BLACK);
  DisplayWXicon(x + 40, y + 40, WxForecast[index].Icon, SmallIcon);
  drawString(x + 40, y, String(ConvertUnixTime(WxForecast[index].Dt + WxConditions[0].Timezone).substring(0,5)), CENTER);
  drawString(x + 40, y + 55, String(WxForecast[index].High, 0) + "° / " + String(WxForecast[index].Low, 0) + "°", CENTER);
}
//#########################################################################################
void DrawMainWx(int x, int y) {
  String Combined;
  Combined = String(WxConditions[0].Temperature, 1) + "°" + (Units == "M" ? "C" : "F"); // Show current Temperature
  Combined += " / " + String(WxConditions[0].Humidity, 0) + "%";

  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  drawString(x+5, y + 5, Combined, LEFT);
}
//#########################################################################################
void DisplayDisplayWindSection(int x, int y, float angle, float windspeed, int Cradius) {
  arrow(x, y, Cradius - 7, angle, 12, 18); // Show wind direction on outer circle of width and length
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  int dxo, dyo, dxi, dyi;
  display.drawLine(0, 15, 0, y + Cradius + 30, GxEPD_BLACK);
  display.drawCircle(x, y, Cradius, GxEPD_BLACK);     // Draw compass circle
  display.drawCircle(x, y, Cradius + 1, GxEPD_BLACK); // Draw compass circle
  display.drawCircle(x, y, Cradius * 0.7, GxEPD_BLACK); // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45)  drawString(dxo + x + 10, dyo + y - 10, TXT_NE, CENTER);
    if (a == 135) drawString(dxo + x + 7,  dyo + y + 5,  TXT_SE, CENTER);
    if (a == 225) drawString(dxo + x - 15, dyo + y,      TXT_SW, CENTER);
    if (a == 315) drawString(dxo + x - 15, dyo + y - 10, TXT_NW, CENTER);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
  }
  drawString(x, y - Cradius - 10,     TXT_N, CENTER);
  drawString(x, y + Cradius + 5,      TXT_S, CENTER);
  drawString(x - Cradius - 10, y - 3, TXT_W, CENTER);
  drawString(x + Cradius + 8,  y - 3, TXT_E, CENTER);
  drawString(x - 2, y - 20, WindDegToDirection(angle), CENTER);
  drawString(x + 3, y + 12, String(angle, 0) + "°", CENTER);
  drawString(x + 3, y - 3, String(windspeed, 1) + (Units == "M" ? "m/s" : "mph"), CENTER);
}
//#########################################################################################
String WindDegToDirection(float winddirection) {
  int dir = int((winddirection / 22.5) + 0.5);
  String Ord_direction[16] = {TXT_N, TXT_NNE, TXT_NE, TXT_ENE, TXT_E, TXT_ESE, TXT_SE, TXT_SSE, TXT_S, TXT_SSW, TXT_SW, TXT_WSW, TXT_W, TXT_WNW, TXT_NW, TXT_NNW};
  return Ord_direction[(dir % 16)];
}
//#########################################################################################
void DrawPressureAndTrend(int x, int y, float pressure, String slope) {
  drawString(x, y, String(pressure, (Units == "M" ? 0 : 1)) + (Units == "M" ? "mb" : "in"), CENTER);
  x = x + 40; y = y + 2;
  if      (slope == "+") {
    display.drawLine(x,  y, x + 4, y - 4, GxEPD_BLACK);
    display.drawLine(x + 4, y - 4, x + 8, y, GxEPD_BLACK);
  }
  else if (slope == "0") {
    display.drawLine(x + 4, y - 4, x + 8, y, GxEPD_BLACK);
    display.drawLine(x + 4, y + 4, x + 8, y, GxEPD_BLACK);
  }
  else if (slope == "-") {
    display.drawLine(x,  y, x + 4, y + 4, GxEPD_BLACK);
    display.drawLine(x + 4, y + 4, x + 8, y, GxEPD_BLACK);
  }
}
//#########################################################################################
void DisplayPrecipitationSection(int x, int y) {
  display.drawRect(x, y - 1, 167, 56, GxEPD_BLACK); // precipitation outline
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  if (WxForecast[1].Rainfall > 0.005) { // Ignore small amounts
    drawString(x + 5, y + 15, String(WxForecast[1].Rainfall, 2) + (Units == "M" ? "mm" : "in"), LEFT); // Only display rainfall total today if > 0
    addraindrop(x + 65 - (Units == "I" ? 10 : 0), y + 16, 7);
  }
  if (WxForecast[1].Snowfall > 0.005)  // Ignore small amounts
    drawString(x + 5, y + 35, String(WxForecast[1].Snowfall, 2) + (Units == "M" ? "mm" : "in") + " * *", LEFT); // Only display snowfall total today if > 0
}
//#########################################################################################
void DrawAstronomySection(int x, int y) {
  display.drawRect(x, y, 159, 99, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + 5, y + 2, "Sun Rise/Set", LEFT);
  drawString(x + 20, y + 30, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, (Units == "M" ? 5 : 7)), LEFT);
  drawString(x + 20, y + 50, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, (Units == "M" ? 5 : 7)), LEFT);
  time_t now = time(NULL);
  struct tm * now_utc = gmtime(&now);
  const int day_utc   = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc  = now_utc->tm_year + 1900;
  DrawMoon(x + 80, y, day_utc, month_utc, year_utc, Hemisphere);
  drawString(x + 5, y + 75, MoonPhase(day_utc, month_utc, year_utc), LEFT);
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  const int diameter = 50;
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "south") Phase = 1 - Phase;
  // Draw dark part of moon
  display.fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= 45; Ypos++) {
    double Xpos = sqrt(45 * 45 - Ypos * Ypos);
    // Determine the edges of the lighted part of the moon
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5) {
      Xpos1 = - Xpos;
      Xpos2 = (Rpos - 2 * Phase * Rpos - Xpos);
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = (Xpos - 2 * Phase * Rpos + Rpos);
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
  display.drawCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);
}
//#########################################################################################
String MoonPhase(int d, int m, int y) {
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
  jd /= 29.53059;                  /* divide by the moon cycle (29.53 days) */
  b   = jd;                        /* int(jd) -> b, take integer part of jd */
  jd -= b;                         /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;              /* scale fraction from 0-8 and round by adding 0.5 */
  b   = b & 7;                     /* 0 and 8 are the same phase so modulo 8 for 0 */
  Hemisphere.toLowerCase();
  if (Hemisphere == "south") b = 7 - b;
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
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;           float y1 = plength;
  float x2 = pwidth / 2;  float y2 = pwidth / 2;
  float x3 = -pwidth / 2; float y3 = pwidth / 2;
  float angle = aangle * PI / 180;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  display.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, GxEPD_BLACK);
}
//#########################################################################################
void DisplayWXicon(int x, int y, String IconName, bool IconSize) {
  if (DEBUG) Serial.println(IconName);
  if      (IconName == "01d" || IconName == "01n")  Sunny(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n")  MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n")  Cloudy(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n")  MostlyCloudy(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n")  ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n")  Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n")  Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n")  Snow(x, y, IconSize, IconName);
  else if (IconName == "50d")                       Haze(x, y, IconSize, IconName);
  else if (IconName == "50n")                       Fog(x, y, IconSize, IconName);
  else                                              Nodata(x, y, IconSize, IconName);
}
//#########################################################################################
uint8_t StartWiFi() {
  if (DEBUG) Serial.println("Connecting to: " + String(ssid));
  IPAddress dns(8, 8, 8, 8); // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
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
    delay(50);
  }
  if (connectionStatus == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    if (DEBUG) Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else log_e("WiFi connection *** FAILED ***");
  return connectionStatus;
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
//#########################################################################################
boolean SetupTime() {
  static bool TimeStatus;
  if (DEBUG) Serial.println("SetupTime()");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  if (DEBUG) Serial.println("Setting TZ to " + String(Timezone));
  tzset(); // Set the TZ environment variable
  delay(100); //why? - but it seems to help 'time settle' ??
  if (DEBUG) Serial.println("Update local time");
  TimeStatus = UpdateLocalTime();
  if (DEBUG) Serial.println("Time done");
  return TimeStatus;
}
//#########################################################################################
boolean UpdateLocalTime() {
  struct tm timeinfo;
  char   time_output[30], day_output[30], update_time[30];
  while (!getLocalTime(&timeinfo, 10000)) { // Wait for 5-sec for time to synchronise
    if (DEBUG) Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S"); // Displays: Saturday, June 24 2017 14:05:49
  if (Units == "M") {
    if ((Language == "CZ") || (Language == "DE") || (Language == "NL") || (Language == "PL")) {
      sprintf(day_output, "%s, %02u. %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900); // day_output >> So., 23. Juni 2019 <<
    }
    else
    {
      sprintf(day_output, "%s  %02u-%s-%04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    }
    strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo);  // Creates: '@ 14:05:49'   and change from 30 to 8 <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    sprintf(time_output, "%s", update_time);
  }
  else
  {
    strftime(day_output, sizeof(day_output), "%a  %b-%d-%Y", &timeinfo); // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);         // Creates: '@ 02:05:49pm'
    sprintf(time_output, "%s", update_time);
  }
  date_str = day_output;
  time_str = time_output;
  return true;
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                      // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                      // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);            // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK); // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  //Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);           // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);           // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE); // left middle upper circle
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
  int scale = Small, offset = 3;
  if (IconSize == LargeIcon) {
    scale = Large;
    y = y - 8;
    offset = 18;
  } else y = y - 3; // Shift up small sun icon
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
  scale = scale * 1.6;
  addsun(x, y, scale, IconSize);
}
//#########################################################################################
void MostlySunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3, offset = 3;
  if (IconSize == LargeIcon) {
    scale = Large;
    offset = 10;
  } else linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
  addcloud(x, y + offset, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
}
//#########################################################################################
void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
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
    addcloud(x + 30, y - 35, 5, linesize); // Cloud top right
    addcloud(x - 20, y - 25, 7, linesize); // Cloud top left
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
  if (IconName.endsWith("n")) addmoon(x, y + 10, scale, IconSize);
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
  if (IconName.endsWith("n")) addmoon(x, y + 15, scale, IconSize);
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
  addcloud(x - 9, y - 3, Small * 0.5, 2); // Cloud top left
  addcloud(x + 3, y - 3, Small * 0.5, 2); // Cloud top right
  addcloud(x, y,         Small * 0.5, 2); // Main cloud
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 15, y - 5, String(CCover) + "%", LEFT);
}
//#########################################################################################
void Visibility(int x, int y, String Visi) {
  y = y - 3; //
  float start_angle = 0.52, end_angle = 2.61;
  int r = 10;
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
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 12, y - 3, Visi, LEFT);
}
//#########################################################################################
void addmoon(int x, int y, int scale, bool IconSize) {
  if (IconSize == LargeIcon) {
    x = x + 12; y = y + 12;
    display.fillCircle(x - 50, y - 55, scale, GxEPD_BLACK);
    display.fillCircle(x - 35, y - 55, scale * 1.6, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 20, y - 12, scale, GxEPD_BLACK);
    display.fillCircle(x - 15, y - 12, scale * 1.6, GxEPD_WHITE);
  }
}
//#########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) u8g2Fonts.setFont(u8g2_font_helvB24_tf); else u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 8, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}

//#########################################################################################
// Calculate the capacity % using a 'sigmoidal' calculation that tries to emulate the
// battery discharge curve from 'full' to 'empty' (which, is not the same as **flat** 0v for a lipo!)
// See this site for more details and some GPLv3 code, where I pulled the algo from:
// https://github.com/rlogiacco/BatterySense
/**
 * Symmetric sigmoidal approximation
 * https://www.desmos.com/calculator/7m9lu26vpy
 *
 * c - c / (1 + k*x/v)^3
 */
uint8_t battery_sigmoidal_capacity(uint16_t voltage, uint16_t minVoltage, uint16_t maxVoltage) {
  // slow
  // uint8_t result = 110 - (110 / (1 + pow(1.468 * (voltage - minVoltage)/(maxVoltage - minVoltage), 6)));

  // steep
  // uint8_t result = 102 - (102 / (1 + pow(1.621 * (voltage - minVoltage)/(maxVoltage - minVoltage), 8.1)));

  // normal
  uint8_t result = 105 - (105 / (1 + pow(1.724 * (voltage - minVoltage)/(maxVoltage - minVoltage), 5.5)));
  return result >= 100 ? 100 : result;
}
//#########################################################################################
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  int adc_value;
  float voltage;
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type;

  // The firebeetle has adc calibration data in its ee - let's try to use it.
  // The firebeetle has a pair of 1M resistors, with a 100nF decoupling cap, placed
  // across VB to GND. VB is the BAT output of the TP4056 charger chip.
  // Sooo, we are looking at *half* the voltage of the battery on the analog pin.
  // The ADC is 12 bit, and uses the default 11db attenuator, which sets its measurable
  // input from 150mV to 2450mV.

  adc_power_acquire();
  adc_value = analogRead(36);
  adc_power_release();

  if (DEBUG) Serial.println(" adc read = " + String(adc_value));

  //Characterize ADC at particular atten.
  // There is a library we could use to do this: https://github.com/madhephaestus/ESP32AnalogRead
  // but we open code it here.
  // GPIO36 is ADC0.
  // Attenuation is 11db by default.
  // ESP32 default vref is 1100mv - but, the real value might be stored in the calibration data.
  val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  //Check type of calibration value used to characterize ADC
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
      if (DEBUG) Serial.println(" Cal eFuse Vref");
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
      if (DEBUG) Serial.println(" Cal Two Point");
  } else {
      if (DEBUG) Serial.println(" Cal Default");
  }

  voltage = esp_adc_cal_raw_to_voltage(adc_value, &adc_chars);
  if (DEBUG) Serial.println("Direct voltage: " + String(voltage) + "v");
  voltage *= 2; //double, as we measure halfway across a voltage divider on pin36
  if (DEBUG) Serial.println(" Double would be: " + String(voltage) + "v");
  percentage = battery_sigmoidal_capacity(voltage, LIPO_MAX_V, LIPO_MIN_V);

  if (DEBUG) Serial.println(" Which translates to percentage: " + String(percentage) + "%");

  display.drawRect(x, y - 16, 34, 14, GxEPD_BLACK);    //outer box
  display.fillRect(x + 34, y - 14, 2, 10, GxEPD_BLACK);      //battery button!
  display.fillRect(x + 1, y - 16, 33 * percentage / 100.0, 14, GxEPD_BLACK);  //battery % bar
  drawString(x + 65, y - 11, String(percentage) + "%", RIGHT);

  //If the battery is dangerously low, show an obvious warning on the screen!
  if (voltage < LIPO_MIN_V) {
    int w, h;
    if (DEBUG) Serial.println("Battery low voltage!");

    w = display.width();
    h = display.height();
    display.fillRect(w/4, h/4, w/2, h/2, GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    drawString(w/2, h/2 , String("BATTERY!"), CENTER);
  }
}
//#########################################################################################
/* (C) D L BIRD
    This function will draw a graph on a ePaper/TFT/LCD display using data from an array containing data to be graphed.
    The variable 'max_readings' determines the maximum number of data elements for each array. Call it with the following parametric data:
    x_pos - the x axis top-left position of the graph
    y_pos - the y-axis top-left position of the graph, e.g. 100, 200 would draw the graph 100 pixels along and 200 pixels down from the top-left of the screen
    width - the width of the graph in pixels
    height - height of the graph in pixels
    Y1_Max - sets the scale of plotted data, for example 5000 would scale all data to a Y-axis of 5000 maximum
    data_array1 is parsed by value, externally they can be called anything else, e.g. within the routine it is called data_array1, but externally could be temperature_readings
    auto_scale - a logical value (TRUE or FALSE) that switches the Y-axis autoscale On or Off
    barchart_on - a logical value (TRUE or FALSE) that switches the drawing mode between bar and line graphs
    barchart_colour - a sets the title and graph plotting colour
    If called with Y!_Max value of 500 and the data never goes above 500, then autoscale will retain a 0-500 Y scale, if on, the scale increases/decreases to match the data.
    auto_scale_margin, e.g. if set to 1000 then autoscale increments the scale by 1000 steps.
*/
void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int readings, boolean auto_scale, boolean barchart_mode) {
#define auto_scale_margin 0 // Sets the autoscale increment, so axis steps up in units of e.g. 3
#define y_minor_axis 5      // 5 y-axis division markers
  float maxYscale = -10000;
  float minYscale =  10000;
  int last_x, last_y;
  float x1, y1, x2, y2;
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
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x_pos + gwidth / 2, y_pos - 12, title, CENTER);
  // Draw the graph
  last_x = x_pos;
  last_y = y_pos + (Y1Max - constrain(DataArray[1], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;
  display.drawRect(x_pos, y_pos, gwidth + 3, gheight + 2, GxEPD_BLACK);
  drawString(x_pos + gwidth / 2, y_pos - 13, title, CENTER);
  // Draw the data
  for (int gx = 0; gx < readings; gx++) {
    y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
    if (barchart_mode) {
      x2 = x_pos + gx * (gwidth / readings) + 2;
      display.fillRect(x2, y2, (gwidth / readings) - 2, y_pos + gheight - y2 + 2, GxEPD_BLACK);
    } 
    else
    {
      x2 = x_pos + gx * gwidth / (readings - 1) + 1; // max_readings is the global variable that sets the maximum data that can be plotted
      display.drawLine(last_x, last_y, x2, y2, GxEPD_BLACK);
    }
    last_x = x2;
    last_y = y2;
  }
  //Draw the Y-axis scale
#define number_of_dashes 15
  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
    for (int j = 0; j < number_of_dashes; j++) { // Draw dashed graph grid lines
      if (spacing < y_minor_axis) display.drawFastHLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes), GxEPD_BLACK);
    }
    if ((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing) < 5 || title == TXT_PRESSURE_IN) {
      drawString(x_pos, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
    }
    else
    {
      if (Y1Min < 1 && Y1Max < 10)
        drawString(x_pos - 3, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
      else
        drawString(x_pos - 3, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0), RIGHT);
    }
  }
  for (int i = 0; i <= 2; i++) {
    drawString(15 + x_pos + gwidth / 3 * i, y_pos + gheight + 3, String(i), LEFT);
  }
  drawString(x_pos + gwidth / 2, y_pos + gheight + 10, TXT_DAYS, CENTER);
}
//#########################################################################################
// y - the top pixel of the string (font)
// text - the string to print/display
// x - depends on the align argument:
//  align:
//    - LEFT : x is the lhs starting point of the string
//    - CENTER : x is the dead centre of the string
//    - RIGHT: x is the rhs pixel of the string
void drawString(int x, int y, String text, alignment align) {
  uint16_t w, h;
  display.setTextWrap(false);
  w = u8g2Fonts.getUTF8Width(text.c_str());
  h = u8g2Fonts.getFontAscent() + abs(u8g2Fonts.getFontDescent());

  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}
//#########################################################################################
void drawStringMaxWidth(int x, int y, int text_width, String text, alignment align) {
  int max_lines = 2;
  int lines_done = 0;
  int first_char = 0;
  int last_char = text.length()+1;  //+1 as in substring 'to' is exclusive
  int stringwidth, charheight, descender;
  int printat_x;

  if (DEBUG) Serial.println("drawStringMaxWidth([" + text + "])");

  descender = abs(u8g2Fonts.getFontDescent());  //abs() as descender tends to be negative
  charheight = u8g2Fonts.getFontAscent();
  // If we have a descender value, add it again for the inter-line gap
  if (descender > 0 ) charheight += descender * 2;
  // otherwise, add a quarter of the char height as the inter-line gap
  else charheight += charheight/4;

  if (DEBUG) Serial.println("charheight:" + String(charheight));

  while( (lines_done < max_lines) && (first_char <= text.length()) && (last_char >= first_char) ) {
    if (DEBUG) Serial.println(" Line:" + String(lines_done) + ". Check if we need to trim the string");
    while( (stringwidth = u8g2Fonts.getUTF8Width(text.substring(first_char, last_char).c_str())) > text_width ) {
      last_char--;
      if (DEBUG) Serial.println("  trim to " + String(last_char));
    }

    if (align == RIGHT)  printat_x = x - stringwidth;
    if (align == CENTER) printat_x = x - (stringwidth / 2);
    if (align == LEFT) printat_x = x;

    if (DEBUG) Serial.println(" Print from " + String(first_char) + " to " + String(last_char) );
    if (DEBUG) Serial.println(" Print [" + text.substring(first_char, last_char) + "]");
    u8g2Fonts.setCursor(printat_x, y + (lines_done * charheight));
    u8g2Fonts.println(text.substring(first_char, last_char));
    first_char = last_char;   //last_char in substring is exclusive, so no '+1' on it.
    last_char = text.length();
    lines_done++;
  }

  if (lines_done >= max_lines) if (DEBUG) Serial.println("Stopped at line limit");
  if (first_char >= text.length() ) if (DEBUG) Serial.println("Stopped as string all printed");
  if (DEBUG) Serial.println("drawStringMaxWidth() done");
}
//#########################################################################################
void InitialiseDisplay() {
  display.init(115200, true, 2, false);
  // display.init(); for older Waveshare HAT's
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.setRotation(3);
  u8g2Fonts.begin(display); // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}
/*
  Version 12.0 reformatted to use u8g2 fonts
  1.  Screen layout revised
  2.  Made consitent with other versions specifically 7x5 variant
  3.  Introduced Visibility in Metres, Cloud cover in % and RH in %
  4.  Correct sunrise/sunset time when in imperial mode.

  Version 12.1 Clarified Waveshare ESP32 driver board connections

  Version 12.2 Changed GxEPD2 initialisation from 115200 to 0
  1.  display.init(115200); becomes display.init(0); to stop blank screen following update to GxEPD2

  Version 12.3
  1. Added 20-secs to allow for slow ESP32 RTC timers

  Version 12.4
  1. Improved graph drawing function for negative numbers Line 808

  Version 12.5
  1. Modified for GxEPD2 changes
*/
