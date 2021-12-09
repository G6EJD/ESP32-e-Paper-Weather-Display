/* ESP Weather Display using an EPD 2.9" Display, obtains data from Open Weather Map, decodes it and then displays it.
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

#include "owm_credentials.h"
#include <ArduinoJson.h>     // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>            // Built-in
#include "time.h"
#include <SPI.h>
#define  ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "forecast_record.h"
#include "lang.h"                     // Localisation (English)
//#include "lang_cz.h"                // Localisation (Czech)
//#include "lang_es.h"                // Localisation (Spanish)
//#include "lang_fr.h"                // Localisation (French)
//#include "lang_gr.h"                // Localisation (German)
//#include "lang_it.h"                // Localisation (Italian)
//#include "lang_nl.h"                // Localisation (Dutch)
//#include "lang_no.h"                // Localisation (Norwegian)
//#include "lang_pl.h"                // Localisation (Polish)
//#include "lang_pt.h"                // Localisation (Portugese)

#define SCREEN_WIDTH   296
#define SCREEN_HEIGHT  128

enum alignmentType {LEFT, RIGHT, CENTER};

// Connections for e.g. LOLIN D32
static const uint8_t EPD_BUSY = 4;  // to EPD BUSY
static const uint8_t EPD_CS   = 5;  // to EPD CS
static const uint8_t EPD_RST  = 16; // to EPD RST
static const uint8_t EPD_DC   = 17; // to EPD DC
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

GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> display(GxEPD2_290(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;  // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
// Using fonts:
// u8g2_font_helvB08_tf
// u8g2_font_helvB10_tf
// u8g2_font_helvB12_tf
// u8g2_font_helvB14_tf
// u8g2_font_helvB18_tf
// u8g2_font_helvB24_tf

//################# LIBRARIES ##########################
String version = "7.0";       // Version of this program
//################ VARIABLES ###########################

bool    LargeIcon = true, SmallIcon = false;
#define Large 7    // For best results use odd numbers
#define Small 3    // For best results use odd numbers
String  time_str, date_str, ForecastDay; // strings to hold time and date
int     wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long    StartTime = 0;

//################ PROGRAM VARIABLES and OBJECTS ##########################################
#define max_readings 25 // In groups of 3-hours (3-days = 3 x 8 = 24)
Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];

#include <common.h>

float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings]    = {0};
float rain_readings[max_readings]        = {0};
float snow_readings[max_readings]        = {0};

long SleepDuration = 30; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int  WakeupHour    = 7;  // Don't wakeup until after 07:00 to save battery power
int  SleepHour     = 23; // Sleep after (23+1) 00:00 to save battery power

typedef struct { // For current Day and Day 1, 2, 3, etc
  String Time;
  float  High;
  float  Low;
} HL_record_type;

HL_record_type  HLReadings[max_readings];

//#########################################################################################
void setup() {
  StartTime = millis();
  Serial.begin(115200);
  while (!Serial);
  Serial.println(__FILE__);
  if (StartWiFi() == WL_CONNECTED && SetupTime() == true) {
    bool WakeUp = false;
    if (WakeupHour > SleepHour)
      WakeUp = (CurrentHour >= WakeupHour || CurrentHour <= SleepHour);
    else
      WakeUp = (CurrentHour >= WakeupHour && CurrentHour <= SleepHour);
    if (WakeUp) {
      InitialiseDisplay(); // Give screen time to initialise by getting weather data!
      byte Attempts = 1;
      bool RxWeather = false, RxForecast = false;
      WiFiClient client;   // wifi client object
      while ((RxWeather == false || RxForecast == false) && Attempts <= 2) { // Try up-to 2 time for Weather and Forecast data
        if (RxWeather  == false) RxWeather  = obtain_wx_data(client, "weather");
        if (RxForecast == false) RxForecast = obtain_wx_data(client, "forecast");
        Attempts++;
      }
      if (RxWeather && RxForecast) { // Only if received both Weather or Forecast proceed
        GetHighsandLows();;
        StopWiFi(); // Reduces power consumption
        DisplayWeather();
        display.display(false); // Full screen update mode
      }
    }
  }
  BeginSleep();
}
//#########################################################################################
void loop() { // this will never run!
}
//#########################################################################################
void BeginSleep() {
  display.powerOff();
  long SleepTimer = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec)); //Some ESP32 are too fast to maintain accurate time
  esp_sleep_enable_timer_wakeup((SleepTimer + 20) * 1000000LL); // Added +20 seconnds to cover ESP32 RTC timer source inaccuracies
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Entering " + String(SleepTimer) + "-secs of sleep time");
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();  // Sleep for e.g. 30 minutes
}
//#########################################################################################
void DisplayWeather() {             // 2.9" e-paper display is 296x128 resolution
  UpdateLocalTime();
  Draw_Heading_Section();           // Top line of the display
  Draw_Main_Weather_Section();      // Centre section of display for Location, temperature, Weather report, Wx Symbol and wind direction
  int Forecast = 2, Dposition = 0;
  String StartTime  = "08:00" + String((Units == "M" ? "" : "a"));
  String MidTime    = "09:00" + String((Units == "M" ? "" : "a"));
  String FinishTime = "10:00" + String((Units == "M" ? "" : "a"));
  do {
    String Ftime = ConvertUnixTime(WxForecast[Forecast].Dt + WxForecast[Forecast].Timezone).substring(0, (Units == "M" ? 5 : 6));
    if (Ftime == StartTime || Ftime == MidTime || Ftime == FinishTime) {
      DisplayForecastWeather(18, 104, Forecast, Dposition, 57); // x,y coordinates, forecast number, position, spacing width
      Dposition++;
    }
    Forecast++;
  } while (Forecast < max_readings);
}
//#########################################################################################
void Draw_Heading_Section() {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(27, 15, City, CENTER);
  drawString(2, 1, time_str + "   " + date_str, LEFT);
  display.drawLine(0, 11, 150 + (Units == "I"?15:0), 11, GxEPD_BLACK);
}
//#########################################################################################
void DisplayForecastWeather(int x, int y, int forecast, int Dposition, int fwidth) {
  GetForecastDay(WxForecast[forecast].Dt);
  x += fwidth * Dposition;
  DisplayConditionsSection(x + 10, y, WxForecast[forecast].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 8, y - 24, ForecastDay, CENTER);
  drawString(x + 18, y + 12, String(HLReadings[Dposition].High, 0) + "°/" + String(HLReadings[Dposition].Low, 0) + "°", CENTER);
  display.drawRect(x - 18, y - 27, fwidth, 51, GxEPD_BLACK);
}
//#########################################################################################
void Draw_Main_Weather_Section() {
  DisplayConditionsSection(205, 45, WxConditions[0].Icon, LargeIcon);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(3, 33, String(WxConditions[0].Temperature, 1) + "° / " + String(WxConditions[0].Humidity, 0) + "%", LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  DrawWind(270, 37, WxConditions[0].Winddir, WxConditions[0].Windspeed);
  if (WxConditions[0].Rainfall > 0.005 || WxConditions[0].Snowfall > 0.005) {
    if (WxConditions[0].Rainfall > 0.005) drawString(170, 66, String(WxConditions[0].Rainfall, 1) + (Units == "M" ? "mm " : "in ") + TXT_PRECIPITATION_SOON, LEFT);
    else drawString(170, 66, String(WxConditions[0].Snowfall, 1) + (Units == "M" ? "mm " : "in ") + TXT_PRECIPITATION_SOON, LEFT); // Rain has precedence over snow if both reported!
  }
  DrawPressureTrend(3, 49, WxConditions[0].Pressure, WxConditions[0].Trend);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  String Wx_Description = WxConditions[0].Forecast0;
  drawString(2, 63, TitleCase(Wx_Description), LEFT);
  display.drawLine(0, 77, 296, 77, GxEPD_BLACK);
  DisplayAstronomySection(170, 64); // Astronomy section Sun rise/set and Moon phase plus icon
}
//#########################################################################################
void DisplayAstronomySection(int x, int y) {
  display.drawRect(x, y + 13, 126, 51, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  SunRise(x + 64, y + 23);
  drawString(x + 80, y + 20, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, (Units == "M" ? 5 : 7)), LEFT);
  SunSet(x + 64, y + 38);
  drawString(x + 80, y + 35, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, (Units == "M" ? 5 : 7)), LEFT);
  time_t now = time(NULL);
  struct tm * now_utc = gmtime(&now);
  const int day_utc   = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc  = now_utc->tm_year + 1900;
  drawString(x + 6, y + 53, MoonPhase(day_utc, month_utc, year_utc, Hemisphere), LEFT);
  DrawMoon(x - 12, y - 1, day_utc, month_utc, year_utc, Hemisphere);
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
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  const int diameter = 34;
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "south") Phase = 1 - Phase;
  // Draw dark part of moon
  display.fillCircle(x + diameter - 1, y + diameter, diameter / 2, GxEPD_BLACK);
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
void DrawWind(int x, int y, float angle, float windspeed) {
#define Cradius 18
  float dx = Cradius * cos((angle - 90) * PI / 180) + x; // calculate X position
  float dy = Cradius * sin((angle - 90) * PI / 180) + y; // calculate Y position
  arrow(x, y, Cradius - 10, angle, 10, 20); // Show wind direction on outer circle
  display.drawCircle(x, y, Cradius + 2, GxEPD_BLACK);
  //display.drawCircle(x, y, Cradius + 3, GxEPD_BLACK);
  for (int m = 0; m < 360; m = m + 45) {
    dx = Cradius * cos(m * PI / 180); // calculate X position
    dy = Cradius * sin(m * PI / 180); // calculate Y position
    display.drawLine(x + dx, y + dy, x + dx * 0.8, y + dy * 0.8, GxEPD_BLACK);
  }
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 10 + (WindDegToDirection(angle).length() < 2 ? 10 : 0), y + Cradius + 12, WindDegToDirection(angle), CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 5, y - Cradius - 15, String(windspeed, 1) + (Units == "M" ? " m/s" : " mph"), CENTER);
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
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  // x,y is the centre poistion of the arrow and asize is the radius out from the x,y position
  // aangle is angle to draw the pointer at e.g. at 45° for NW
  // pwidth is the pointer width in pixels
  // plength is the pointer length in pixels
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;         float y1 = plength;
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
void DrawPressureTrend(int x, int y, float pressure, String slope) {
  drawString(x, y - 3, String(pressure, (Units == "M" ? 0 : 1)) + (Units == "M" ? " hPa" : " in"), LEFT);
  x = x + 52 - (Units == "M" ? 0 : 15); y = y + 3;
  if      (slope == "+") {
    display.drawLine(x,  y, x + 4, y - 4, GxEPD_BLACK);
    display.drawLine(x + 4, y - 4, x + 8, y, GxEPD_BLACK);
  }
  else if (slope == "0") {
    display.drawLine(x + 3, y - 4, x + 8, y, GxEPD_BLACK);
    display.drawLine(x + 3, y + 4, x + 8, y, GxEPD_BLACK);
  }
  else if (slope == "-") {
    display.drawLine(x,  y, x + 4, y + 4, GxEPD_BLACK);
    display.drawLine(x + 4, y + 4, x + 8, y, GxEPD_BLACK);
  }
}
//#########################################################################################
void DisplayConditionsSection(int x, int y, String IconName, bool IconSize) {
  Serial.println("Icon name: " + IconName);
  if      (IconName == "01d" || IconName == "01n")  ClearSky(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n")  FewClouds(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n")  ScatteredClouds(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n")  BrokenClouds(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n")  ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n")  Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n")  Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n")  Snow(x, y, IconSize, IconName);
  else if (IconName == "50d" || IconName == "50n")  Mist(x, y, IconSize, IconName);
  else                                              Nodata(x, y, IconSize, IconName);
}
//#########################################################################################
void GetHighsandLows() {
  for (int d = 0; d < max_readings; d++) {
    HLReadings[d].Time = "";
    HLReadings[d].High = (Units == "M"?-50:-58);
    HLReadings[d].Low  = (Units == "M"?70:158);
  }
  int Day = 0;
  String StartTime  = "08:00" + String((Units == "M" ? "" : "a"));
  String FinishTime = "10:00" + String((Units == "M" ? "" : "a"));
  for (int r = 0; r < max_readings; r++) {
    if (ConvertUnixTime(WxForecast[r].Dt + WxForecast[r].Timezone).substring(0, (Units == "M" ? 5 : 6)) >= StartTime && ConvertUnixTime(WxForecast[r].Dt + WxForecast[r].Timezone).substring(0, (Units == "M" ? 5 : 6)) <= FinishTime) { // found first period in day
      HLReadings[Day].Time = ConvertUnixTime(WxForecast[r].Dt + WxForecast[r].Timezone).substring(0, (Units == "M" ? 5 : 6));
      for (int InDay = 0; InDay < 8; InDay++) { // 00:00 to 21:00 is 8 readings
        if (r + InDay < max_readings) {
          if (WxForecast[r + InDay].High > HLReadings[Day].High) {
            HLReadings[Day].High = WxForecast[r + InDay].High;
          }
          if (WxForecast[r + InDay].Low  < HLReadings[Day].Low)  {
            HLReadings[Day].Low  = WxForecast[r + InDay].Low;
          }
        }
      }
      //Serial.println("Cnt= " + String(Day) + "Day=" + HLReadings[Day].Time + " " + String(HLReadings[Day].High) + " " + String(HLReadings[Day].Low));
      Day++;
    }
  }
} // Now the array HLReadings has 5-days of Highs and Lows
//#########################################################################################
uint8_t StartWiFi() {
  Serial.println("\r\nConnecting to: " + String(ssid));
  IPAddress dns(8, 8, 8, 8); // Use Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("STA: Failed!\n");
    WiFi.disconnect(false);
    delay(500);
    WiFi.begin(ssid, password);
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else Serial.println("WiFi connection *** FAILED ***");
  return WiFi.status();
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
//#########################################################################################
boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
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
  while (!getLocalTime(&timeinfo, 5000)) { // Wait for 5-sec for time to synchronise
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");      // Displays: Saturday, June 24 2017 14:05:49
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
    strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo); // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);        // Creates: '02:05:49pm'
    sprintf(time_output, "%s", update_time);
  }
  date_str = day_output;
  time_str = time_output;
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
    if (voltage <= 3.50) percentage = 0;
    display.drawRect(x + 15, y - 12, 19, 10, GxEPD_BLACK);
    display.fillRect(x + 34, y - 10, 2, 5, GxEPD_BLACK);
    display.fillRect(x + 17, y - 10, 15 * percentage / 100.0, 6, GxEPD_BLACK);
    drawString(x + 60, y - 11, String(percentage) + "%", RIGHT);
    //drawString(x + 13, y + 5,  String(voltage, 2) + "v", CENTER);
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
    linesize = 1;
    y = y - 1;
  } else y = y - 3;
  for (int i = 0; i < 6; i++) {
    display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.6, scale * 6, linesize, GxEPD_BLACK);
  }
}
//#########################################################################################
void ClearSky(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) {
    scale = Large;
    y = y - 6; // Shift up large sun
  }
  else {
    y = y - 5; // Shift down small sun icon
  }
  if (IconName.endsWith("n")) addmoon(x, y + 3, scale, IconSize);
  scale = scale * 1.6;
  addsun(x, y, scale, IconSize);
}
//#########################################################################################
void FewClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 1;
  if (IconSize == LargeIcon) {
    scale = Large;
    linesize = 3;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
}//#########################################################################################
void ScatteredClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3, offset = 10;
  if (IconSize == LargeIcon) {
    scale = Large;
  }
  else
  {
    linesize = 1;
    offset   = 5;
  }
  if (IconName.endsWith("n")) addmoon(x, y + offset + (IconSize ? -8 : 0), scale, IconSize);
  addcloud(x + offset, y - offset * 1.2, scale / 1.5, linesize);
  addcloud(x, y + offset, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
}
//#########################################################################################
void BrokenClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3, offset  = 12;
  if (IconSize == LargeIcon) {
    scale = Large;
  }
  else
  {
    linesize = 1;
    offset   = 6;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x - offset, y - offset, scale / 1.5, linesize);
  addcloud(x + offset, y - offset * 1.2, scale / 1.5, linesize);
  addcloud(x, y, scale, linesize);
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
void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x - (IconSize ? 8 : 0), y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
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
void Mist(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
    y = y + 5;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y - 5, scale, linesize);
  addfog(x, y - 2, scale, linesize, IconSize);
}
//#########################################################################################
void Haze(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x, y - 2, scale * 1.4, IconSize);
  addfog(x, y + 3 - (IconSize ? 12 : 0), scale * 1.4, linesize, IconSize);
}
//#########################################################################################
void CloudCover(int x, int y, int CCover) {
  addcloud(x - 9, y - 3, Small * 0.6, 2); // Cloud top left
  addcloud(x + 3, y - 3, Small * 0.6, 2); // Cloud top right
  addcloud(x, y,         Small * 0.6, 2); // Main cloud
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
    x = x - 5; y = y + 5;
    display.fillCircle(x - 21, y - 23, scale, GxEPD_BLACK);
    display.fillCircle(x - 14, y - 23, scale * 1.7, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 16, y - 11, scale, GxEPD_BLACK);
    display.fillCircle(x - 11, y - 11, scale * 1.7, GxEPD_WHITE);
  }
}
//#########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) u8g2Fonts.setFont(u8g2_font_helvB24_tf); else u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 8, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}
//#########################################################################################
void SunRise(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  addsun(x, y, 3, SmallIcon);
  display.drawFastVLine(x + 10, y - 6, 10, GxEPD_BLACK);
  display.drawLine(x + 10 - 3, y - 6 + 3, x + 10, y - 6, GxEPD_BLACK);
  display.drawLine(x + 10 + 3, y - 6 + 3, x + 10, y - 6, GxEPD_BLACK);
}
//#########################################################################################
void SunSet(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  addsun(x, y, 3, SmallIcon);
  display.drawFastVLine(x + 10, y - 6, 10, GxEPD_BLACK);
  display.drawLine(x + 10 - 3, y + 4 - 3, x + 10, y + 4, GxEPD_BLACK);
  display.drawLine(x + 10 + 3, y + 4 - 3, x + 10, y + 4, GxEPD_BLACK);
}
//#########################################################################################
void drawString(int x, int y, String text, alignmentType alignment) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT)  x = x - w;
  if (alignment == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}
//#########################################################################################
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignmentType alignment) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT)  x = x - w;
  if (alignment == CENTER) x = x - w / 2;
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
String GetForecastDay(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40], FDay[40];
  if (Units == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
    strftime(FDay, sizeof(FDay), "%w", now_tm);
  }
  else {
    strftime(output, sizeof(output), "%I:%M%p %m/%d/%y", now_tm);
    strftime(FDay, sizeof(FDay), "%w", now_tm);
  }
  ForecastDay = weekday_D[String(FDay).toInt()];
  return output;
}
//#########################################################################################
void InitialiseDisplay() {
  //display.init(115200, true, 2, false);
  display.init(115200); //for older Waveshare HAT's
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.setRotation(1);                    // Use 1 or 3 for landscape modes
  u8g2Fonts.begin(display);                  // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);   // Explore u8g2 fonts from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}

/*
  Version 6.0 reformatted to use u8g2 fonts
  1.  Screen layout revised
  2.  Made consitent with other versions specifically 7x5 variant
  3.  Introduced Visibility in Metres, Cloud cover in % and RH in %
  4.  Correct sunrise/sunset time when in imperial mode.

  Version 6.1 Provided connection support for Waveshare ESP32 driver board

  Version 6.2 Changed GxEPD2 initialisation from 115200 to 0
  1.  display.init(115200); becomes display.init(0); to stop blank screen following update to GxEPD2

  Version 6.3 changed u8g2 fonts selection
   1.  Omitted 'FONT(' and added _tf to font names either Regular (R) or Bold (B)

  Version 6.4
   1. Added an extra 20-secs sleep delay to allow for slow ESP32 RTC timers

  Version 6.5
  1. Modified for GxEPD2

  Version 7.0
  1. Added 3-dayy forecasting

*/
