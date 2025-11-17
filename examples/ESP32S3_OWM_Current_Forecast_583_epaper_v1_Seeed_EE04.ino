/* ESP32 Weather Display using an EPD 5.83" 648 x 480 Display, obtains data from Open Weather Map, decodes then displays it.
// Board XIAO ESP32S3
  ####################################################################################################################################
  This software, the ideas and concepts is Copyright (c) David Bird 2025. All rights to this software are reserved.

  Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
  1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
  2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
  3. You may not, except with my express written permission, distribute or commercially exploit the content.
  4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.
  5. You may not use this software to create YouTube or other video content, or for any purposes of monetisation.

  The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
  software use is visible to an end-user.

  THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY
  OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  See more at http://www.dsbird.org.uk
*/

// https://wiki.seeedstudio.com/epaper_ee04/

#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson needs version v6 or above
#include <WiFi.h>         // Built-in
#include <WiFiClient.h>
#include <HTTPClient.h>
#include "owm_credentials.h"  // See 'owm_credentials' tab and enter your OWM API key and set the Wifi SSID and PASSWORD
#include "common.h"
#include "EN_lang.h"  // Localisation (English)

#include <time.h>  // Built-in
#include <SPI.h>   // Built-in
#define ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
//#include "lang_cz.h"                  // Localisation (Czech)
//#include "lang_fr.h"                  // Localisation (French)
//#include "lang_gr.h"                  // Localisation (German)
//#include "lang_it.h"                  // Localisation (Italian)
//#include "lang_nl.h"
//#include "lang_pl.h"                  // Localisation (Polish)

#define SCREEN_WIDTH  648  // Set for landscape mode
#define SCREEN_HEIGHT 480

enum alignment { LEFT,
                 RIGHT,
                 CENTER };

// Connections for XIAO ESP32S3 epaper board driver board
//#define EPD_BUSY  A3
//#define EPD_CS    D7
//#define EPD_RST   38
//#define EPD_DC    10
//#define EPD_SCK    7
//#define EPD_MISO  12  // Master-In Slave-Out not used, as no data from display
//#define EPD_MOSI  11

#define EPD_SCK   D8
#define EPD_MISO  -1
#define EPD_MOSI D10
#define EPD_CS    44   // D7
#define EPD_DC    10   // D16
#define EPD_BUSY   4  // D3
#define EPD_RST   38  // D11

GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT> display(GxEPD2_583_T8(/*CS=5*/ EPD_CS, /*DC=*/EPD_DC, /*RST=*/EPD_RST, /*BUSY=*/EPD_BUSY));  // GDEW0583T8 648x480, EK79655 (GD7965)
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;  // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall

// Using fonts:
// u8g2_font_helvB08_tf
// u8g2_font_helvB10_tf
// u8g2_font_helvB12_tf
// u8g2_font_helvB14_tf
// u8g2_font_helvB18_tf
// u8g2_font_helvB24_tf

//################  VERSION  #####################################################
String version = "1.0 (16/11/25)";  // Programme version, see change log at end
//################ VARIABLES #####################################################

boolean LargeIcon = true, SmallIcon = false;
#define Large 12                          // For icon drawing, needs to be odd number for best effect
#define Small 5                           // For icon drawing, needs to be odd number for best effect
String Time_str, Date_str, Forecast_day;  // strings to hold time and received weather data
int wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long StartTime = 0;

//################ PROGRAM VARIABLES and OBJECTS ################
#define autoscale_on true
#define autoscale_off false
#define barchart_on true
#define barchart_off false

float pressure_readings[max_readings] = { 0 };
float temperature_readings[max_readings] = { 0 };
float humidity_readings[max_readings] = { 0 };
float rain_readings[max_readings] = { 0 };
float snow_readings[max_readings] = { 0 };

long SleepDuration = 60;  // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int WakeupTime = 7;       // Don't wakeup until after 07:00 to save battery power
int SleepTime  = 23;      // Sleep after (23+1) 00:00 to save battery power

//#########################################################################################
void setup() {
  StartTime = millis();
  Serial.begin(115200);
  delay(200);
  Serial.println(__FILE__);
  esp_sleep_enable_touchpad_wakeup();
  if (StartWiFi() == WL_CONNECTED && SetupTime() == true) {
    if ((CurrentHour >= WakeupTime && CurrentHour <= SleepTime)) {
      InitialiseDisplay();  // Give screen time to initialise by getting weather data!
      byte Attempts = 1;
      bool RxWeather = false;
      WiFiClient client;
      while (RxWeather == false && Attempts <= 2) {  // Try up-to 2 time for Weather and Forecast data
        Serial.println("Getting Weather Data");
        RxWeather = ReceiveOneCallWeather(client, true);
        Attempts++;
      }
      if (RxWeather) {  // Only if received Weather
        Serial.println("Obtained Weather Data");
        StopWiFi();  // Reduces power consumption
        DisplayWeather();
        display.display(false);  // Full screen update mode
      }
    }
  }
  BeginSleep();
}
//#########################################################################################
void loop() {  // this will never run!
}
//#########################################################################################
void BeginSleep() {  // Wake up with a Touch pin to refresh the weather data, just needs a wire on the chosen pin
  display.powerOff();
  long SleepTimer = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec));
  esp_sleep_enable_timer_wakeup((SleepTimer + 60) * 1000000LL);  // Added + 60 seconds to cover ESP32 RTC timer inaccuracies
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT);  // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Entering " + String(SleepTimer) + "-secs of sleep time");
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();  // Sleep for e.g. 60 minutes
}
//#########################################################################################
void DisplayWeather() {                         // 5.83" e-paper display is 648 x 480 resolution
  DisplayGeneralInfoSection(315, 178);          // Top line of the display
  DisplayDisplayWindSection(80, 126, WxConditions[0].Winddir, WxConditions[0].Windspeed, 60);
  DisplayMainWeatherSection(228, 97);           // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
  DisplayForecastSection(170, 260);             // 1-day forecast boxes
  DisplayAstronomySection(0, 245);              // Astronomy section Sun rise/set, Moon phase and Moon icon
  DisplayStatusSection(545, 211, wifi_signal);  // Wi-Fi signal strength and Battery voltage
}
//#########################################################################################
void DisplayGeneralInfoSection(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x - 309, y - 175, "[Version: " + version + "]", LEFT);  // Programme version
  drawString(SCREEN_WIDTH / 2, 3, City, CENTER);
  display.drawRect(x - 30, y, 218, 76, GxEPD_BLACK);  // Box around time and date
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + 45, y + 17, Date_str, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(x + 55, y + 42, Time_str, CENTER);
  display.drawLine(0, 18, SCREEN_WIDTH - 2, 18, GxEPD_BLACK);
}
//#########################################################################################
void DisplayMainWeatherSection(int x, int y) {
  display.drawLine(0, 38, SCREEN_WIDTH - 2, 38, GxEPD_BLACK);
  DisplayConditionsSection(x - 10, y + 50, WxConditions[0].Icon, LargeIcon);
  DisplayTemperatureSection(x + 120, y - 75, 120, 95);
  DisplayPressureSection(x + 235, y - 75, WxConditions[0].Pressure, WxConditions[0].Trend, 120, 95);
  DisplayPrecipitationSection(x + 348, y - 75, 120, 95);
  display.drawRect(x + 57, y + 20, 363, 60, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  Serial.println(Daily[0].Description);
  // 123456789.123456789.123456789.123456789.123456789.
  // The day will start with clear sky through the late morning hours, transitioning to partly cloudy
  // The day will start with partly cloudy through the late morning hours, transitioning to rain
  // The day will start with partly cloudy through the late morning hours, transitioning to clearing
  // You can expect clear sky in the morning, with partly cloudy in the afternoon
  // You can expect partly cloudy in the morning, with clearing in the afternoon
  // Expect a day of partly cloudy with rain
  // Expect a day that is partly cloudy with rain
  // There will be clear sky today
  // There will be partly cloudy today
  // There will be partly cloudy until morning, then rain
  // There will be clear sky until morning, then partly cloudy
  // There will be partly cloudy until morning, then clearing
  // There will be rain until morning, then partly cloudy
  // Expect a day of partly cloudy with clear spells
  Daily[0].Description.replace("There", "It");
  Daily[0].Description.replace("of", "that is");
  Daily[0].Description.replace("with partly", "partly");
  Daily[0].Description.replace("with clearing", "then clearing");
  String Line1, Line2, Line3;
  WordWrap(Daily[0].Description, Line1, Line2, Line3, 49);
  if (Line2.length() == 0) drawString(x + 60, y + 45, Line1, LEFT);
  else {
    drawString(x + 60, y + 30, Line1, LEFT);
    drawString(x + 60, y + 48, Line2, LEFT);
    drawString(x + 60, y + 66, Line3, LEFT);
  }
}
//#########################################################################################
void WordWrap(String line, String &line1, String &line2, String &line3, int length) {
  String words[100];
  int ptr = 0;
  int ArrayLen = line.length() + 1;  //The +1 is for the 0x00h Terminator
  char str[ArrayLen];
  line.toCharArray(str, ArrayLen);
  char *pch;                // a pointer variable
  pch = strtok(str, " -");  // Find words seperated by SPACE
  while (pch != NULL) {
    String word = String(pch);
    words[ptr] = word;
    ptr++;
    pch = strtok(NULL, " -");
  }
  int numWords = ptr;  // - 1;
  ptr = 0;
  line1 = "";
  line2 = "";
  line3 = "";
  line1 = formatline(line1, length, numWords, ptr, words);
  line2 = formatline(line2, length, numWords, ptr, words);
  line3 = formatline(line3, length, numWords, ptr, words);
}
//#########################################################################################
String formatline(String &line, int &length, int numWords, int &ptr, String words[]) {
  while (line.length() < length && ptr <= numWords) {
    if (line.length() + words[ptr].length() < length) {
      line += words[ptr] + " ";
    } else break;
    ptr++;
  }
  return line;
}
//#########################################################################################
void DisplayDisplayWindSection(int x, int y, float angle, float windspeed, int Cradius) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y - Cradius - 41, TXT_WIND_SPEED_DIRECTION, CENTER);
  display.drawRect(x - 80, y - 105, 160, 233, GxEPD_BLACK);
  arrow(x, y, Cradius - 20, angle, 18, 25);  // Show wind direction on outer circle of width and length
  int dxo, dyo, dxi, dyi;
  display.drawLine(0, 18, 0, y + Cradius + 37, GxEPD_BLACK);
  display.drawCircle(x, y, Cradius, GxEPD_BLACK);        // Draw compass circle
  display.drawCircle(x, y, Cradius + 1, GxEPD_BLACK);    // Draw compass circle
  display.drawCircle(x, y, Cradius * 0.7, GxEPD_BLACK);  // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45) drawString(dxo + x + 10, dyo + y - 12, TXT_NE, CENTER);
    if (a == 135) drawString(dxo + x + 7, dyo + y + 6, TXT_SE, CENTER);
    if (a == 225) drawString(dxo + x - 18, dyo + y, TXT_SW, CENTER);
    if (a == 315) drawString(dxo + x - 16, dyo + y - 12, TXT_NW, CENTER);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
  }
  drawString(x, y - Cradius - 12, TXT_N, CENTER);
  drawString(x, y + Cradius + 6, TXT_S, CENTER);
  drawString(x - Cradius - 12, y - 3, TXT_W, CENTER);
  drawString(x + Cradius + 10, y - 3, TXT_E, CENTER);
  drawString(x + 7, y + 25, String(angle, 0) + "°", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(x - 8, y - 30, WindDegToDirection(angle), CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  drawString(x - 8, y - 2, String(windspeed, 1), CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y + 12, (Units == "M" ? "m/s" : "mph"), CENTER);
}
//#########################################################################################
String WindDegToDirection(float winddirection) {
  String Ord_direction[16] = { TXT_N, TXT_NNE, TXT_NE, TXT_ENE, TXT_E, TXT_ESE, TXT_SE, TXT_SSE, TXT_S, TXT_SSW, TXT_SW, TXT_WSW, TXT_W, TXT_WNW, TXT_NW, TXT_NNW };
  return Ord_direction[int((winddirection / 22.5) + 0.5) % 16];
}
//#########################################################################################
void DisplayTemperatureSection(int x, int y, int twidth, int tdepth) {
  display.drawRect(x - 63, y - 1, twidth, tdepth, GxEPD_BLACK);  // temp outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x - 5, y + 5, TXT_TEMPERATURES, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x, y + 82, String(Daily[0].High, 0) + "° | " + String(Daily[0].Low, 0) + "°", CENTER);  // Show forecast high and Low
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  drawString(x - 18, y + 53, String(WxConditions[0].Temperature, 1) + "°", CENTER);  // Show current Temperature
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  display.drawRect(x + 32, y + tdepth - 22, 25, 21, GxEPD_BLACK);  // temp outline
  drawString(x + 38, y + 83, Units == "M" ? "C" : "F", LEFT);
}
//#########################################################################################
void DisplayForecastWeather(int x, int y) {
  for (int Forecast = 0; Forecast < 8; Forecast++) {
    DisplayForecastDailyWeather(x, y, Forecast);
  }
}
//#########################################################################################
void DisplayForecastDailyWeather(int x, int y, int Forecast) {
  int Fwidth = 58, FDepth = 85;
  x = x + Fwidth * Forecast;
  display.drawRect(x, y + 50, Fwidth - 1, FDepth, GxEPD_BLACK);
  DisplayConditionsSection(x + Fwidth / 2 - 1, y + 95, Daily[Forecast].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  ConvertUnixTimeToDay(Daily[Forecast].Dt);
  if (Forecast == 0) Forecast_day = TXT_TODAY;
  drawString(x + Fwidth / 2 - 5, y + 60, Forecast_day, CENTER);
  drawString(x + Fwidth / 2 + 5, y + 120, String(Daily[Forecast].High, 0) + "°/" + String(Daily[Forecast].Low, 0) + "°", CENTER);
}
//#########################################################################################
void DisplayPressureSection(int x, int y, float pressure, String slope, int pwidth, int pdepth) {
  display.drawRect(x - 56, y - 1, pwidth, pdepth, GxEPD_BLACK);  // pressure outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 3, y + 5, TXT_PRESSURE, CENTER);
  String slope_direction = TXT_PRESSURE_STEADY;
  if (slope == "+") slope_direction = TXT_PRESSURE_RISING;
  if (slope == "-") slope_direction = TXT_PRESSURE_FALLING;
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  if (Units == "I") drawString(x - 20, y + 55, String(pressure, 2), CENTER);  // "Imperial"
  else drawString(x - 18, y + 55, String(pressure, 0), CENTER);               // "Metric"
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  display.drawRect(x + 29, y + 75, 35, 19, GxEPD_BLACK);
  drawString(x + 45, y + 80, (Units == "M" ? "hPa" : "in"), CENTER);
  drawString(x - 10, y + 80, slope_direction, CENTER);
}
//#########################################################################################
void DisplayPrecipitationSection(int x, int y, int pwidth, int pdepth) {
  display.drawRect(x - 48, y - 1, pwidth, pdepth, GxEPD_BLACK);  // precipitation outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 11, y + 5, TXT_PRECIPITATION, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  if (WxForecast[1].Rainfall >= 0.005) {                                                                 // Ignore small amounts
    drawString(x - 30, y + 35, String(WxForecast[1].Rainfall, 2) + (Units == "M" ? "mm" : "in"), LEFT);  // Only display rainfall total today if > 0
    addraindrop(x + 40, y + 30, 5);
  }
  if (WxForecast[1].Snowfall >= 0.005)                                                                           // Ignore small amounts
    drawString(x - 30, y + 60, String(WxForecast[1].Snowfall, 2) + (Units == "M" ? "mm" : "in") + " **", LEFT);  // Only display snowfall total today if > 0
  drawString(x - 30, y + 81, String(Daily[1].PoP * 100, 0) + "% PoP", LEFT);                                     // Only display pop if > 0
}
//#########################################################################################
void DisplayAstronomySection(int x, int y) {
  display.drawRect(x, y + 10, 184, 85, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 6, y + 24, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNRISE, LEFT);
  drawString(x + 6, y + 45, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNSET, LEFT);
  time_t now = time(NULL);
  struct tm *now_utc = gmtime(&now);
  const int day_utc = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc = now_utc->tm_year + 1900;
  drawString(x + 6, y + 70, MoonPhase(day_utc, month_utc, year_utc, Hemisphere), LEFT);
  DrawMoon(x + 101, y, day_utc, month_utc, year_utc, Hemisphere);
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  const int diameter = 47;
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
    } else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
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
  c = 365.25 * y;
  e = 30.6 * m;
  jd = c + e + d - 694039.09; /* jd is total days elapsed */
  jd /= 29.53059;             /* divide by the moon cycle (29.53 days) */
  b = jd;                     /* int(jd) -> b, take integer part of jd */
  jd -= b;                    /* subtract integer part to leave fractional part of original jd */
  b = jd * 8 + 0.5;           /* scale fraction from 0-8 and round by adding 0.5 */
  b = b & 7;                  /* 0 and 8 are the same phase so modulo 8 for 0 */
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
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  DisplayForecastWeather(x + 15, y - 55);
  // Pre-load temporary arrays with with data - because C parses by reference
  int r = 0;
  do {
    if (Units == "I") pressure_readings[r] = WxForecast[r].Pressure * 0.02953;
    else pressure_readings[r] = WxForecast[r].Pressure;
    if (Units == "I") rain_readings[r] = WxForecast[r].Rainfall * 0.0393701;
    else rain_readings[r] = WxForecast[r].Rainfall;
    if (Units == "I") snow_readings[r] = WxForecast[r].Snowfall * 0.0393701;
    else snow_readings[r] = WxForecast[r].Snowfall;
    temperature_readings[r] = WxForecast[r].Temperature;
    humidity_readings[r] = WxForecast[r].Humidity;
    r++;
  } while (r < max_readings);
  int gwidth = 100, gheight = 53;
  int gx = (SCREEN_WIDTH - gwidth * 4) / 5 + 5;
  int gy = 400;
  int gap = gwidth + gx;
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(SCREEN_WIDTH / 2, gy - 40, TXT_FORECAST_VALUES, CENTER);  // Based on a graph height of 60
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  // (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  DrawGraph(gx + 0 * gap, gy, gwidth, gheight, 900, 1050, Units == "M" ? TXT_PRESSURE_HPA : TXT_PRESSURE_IN, pressure_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 10, 30, Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx + 2 * gap, gy, gwidth, gheight, 0, 100, TXT_HUMIDITY_PERCENT, humidity_readings, max_readings, autoscale_off, barchart_off);
  const int Rain_array_size = sizeof(rain_readings) / sizeof(float);
  const int Snow_array_size = sizeof(snow_readings) / sizeof(float);
  if (SumOfPrecip(rain_readings, Rain_array_size) >= SumOfPrecip(snow_readings, Snow_array_size))
    DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_readings, Rain_array_size, autoscale_on, barchart_on);
  else DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_SNOWFALL_MM : TXT_SNOWFALL_IN, snow_readings, Snow_array_size, autoscale_on, barchart_on);
}
//#########################################################################################
void DisplayConditionsSection(int x, int y, String IconName, bool IconSize) {
  Serial.println("Icon name: " + IconName);
  if (IconName == "01d" || IconName == "01n") Sunny(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n") MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n") Cloudy(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n") MostlyCloudy(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n") ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n") Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n") Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n") Snow(x, y, IconSize, IconName);
  else if (IconName == "50d") Haze(x, y, IconSize, IconName);
  else if (IconName == "50n") Fog(x, y, IconSize, IconName);
  else Nodata(x, y, IconSize, IconName);
  if (IconSize == LargeIcon) {
    display.drawRect(x - 57, y - 126, 123, 233, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(x, y - 120, TXT_CONDITIONS, CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB12_tf);
    DisplayVisiCCoverUVISection(x + 25, y - 5);
    drawString(x - 3, y + 40, String(WxConditions[0].Humidity, 0) + "% RH", CENTER);
  }
}
//#########################################################################################
void DisplayVisiCCoverUVISection(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  Visibility(x - 65, y - 87, String(WxConditions[0].Visibility) + "M");
  if (WxConditions[0].Cloudcover > 0) CloudCover(x - 3, y - 70, WxConditions[0].Cloudcover);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  if (WxConditions[0].UVI >= 0) Display_UVIndexLevel(x - 15, y + 50, WxConditions[0].UVI);
}
//#########################################################################################
void Display_UVIndexLevel(int x, int y, float UVI) {
  String Level = "";
  if (UVI < 2) Level = " (L)";
  if (UVI >= 2 && UVI < 5) Level = " (M)";
  if (UVI >= 5 && UVI < 7) Level = " (H)";
  if (UVI >= 7 && UVI < 10) Level = " (VH)";
  if (UVI >= 10) Level = " (EX)";
  drawString(x - 50, y + 15, "UVI: " + String(UVI, (UVI < 1 ? 1 : 0)) + Level, LEFT);
}
//#########################################################################################
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x;  // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y;  // calculate Y position
  float x1 = 0;
  float y1 = plength;
  float x2 = pwidth / 2;
  float y2 = pwidth / 2;
  float x3 = -pwidth / 2;
  float y3 = pwidth / 2;
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
uint8_t StartWiFi() {
  Serial.print("\r\nConnecting to: ");
  Serial.println(String(ssid));
  IPAddress dns(8, 8, 8, 8);  // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);  // switch off AP
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  uint8_t connectionStatus;
  bool AttemptConnection = true;
  while (AttemptConnection) {
    connectionStatus = WiFi.status();
    if (millis() > start + 15000) {  // Wait 15-secs maximum
      AttemptConnection = false;
    }
    if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED) {
      AttemptConnection = false;
    }
    delay(50);
  }
  if (connectionStatus == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI();  // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  } else Serial.println("WiFi connection *** FAILED ***");
  return connectionStatus;
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
//#########################################################################################
void DisplayStatusSection(int x, int y, int rssi) {
  display.drawRect(x - 41, y - 32, 143, 75, GxEPD_BLACK);
  display.drawRect(x - 41, y - 32, 143 / 2, 17, GxEPD_BLACK);
  display.drawRect(x - 41 + (143 / 2) - 1, y - 32, 143 / 2 + 2, 17, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y - 28, TXT_WIFI, CENTER);
  drawString(x + 68, y - 28, TXT_POWER, CENTER);
  DrawRSSI(x - 10, y + 10, rssi);
  DrawBattery(x + 50, y + 10);
}
//#########################################################################################
void DrawRSSI(int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20) WIFIsignal = 20;  //            <-20dbm displays 5-bars
    if (_rssi <= -40) WIFIsignal = 16;  //  -40dbm to  -21dbm displays 4-bars
    if (_rssi <= -60) WIFIsignal = 12;  //  -60dbm to  -41dbm displays 3-bars
    if (_rssi <= -80) WIFIsignal = 8;   //  -80dbm to  -61dbm displays 2-bars
    if (_rssi <= -100) WIFIsignal = 4;  // -100dbm to  -81dbm displays 1-bar
    display.fillRect(x + xpos * 6, y - WIFIsignal, 5, WIFIsignal, GxEPD_BLACK);
    xpos++;
  }
  display.fillRect(x, y - 1, 5, 1, GxEPD_BLACK);
  drawString(x + 6, y + 6, String(rssi) + "dBm", CENTER);
}
//#########################################################################################
boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");  //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);                                                  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                    // Set the TZ environment variable
  delay(100);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//#########################################################################################
boolean UpdateLocalTime() {
  struct tm timeinfo;
  char time_output[40], day_output[30], update_time[30], forecast_day[30];
  while (!getLocalTime(&timeinfo, 10000)) {  // Wait for 10-sec for time to synchronise
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin = timeinfo.tm_min;
  CurrentSec = timeinfo.tm_sec;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");  // Displays: Saturday, June 24 2017 14:05:49
  if (Units == "M") {
    if ((Language == "CZ") || (Language == "DE") || (Language == "PL") || (Language == "NL")) {
      sprintf(day_output, "%s, %02u. %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);  // day_output >> So., 23. Juni 2019 <<
    } else {
      sprintf(day_output, "%s %02u-%s-%04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    }
    strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo);  // Creates: '14:05:49'
    sprintf(time_output, "%s %s", TXT_UPDATED, update_time);
  } else {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo);  // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);         // Creates: '02:05:49pm'
    sprintf(time_output, "%s %s", TXT_UPDATED, update_time);
  }
  strftime(forecast_day, sizeof(forecast_day), "%w", &timeinfo);
  Forecast_day = forecast_day;
  Date_str = day_output;
  Time_str = time_output;
  return true;
}
//#########################################################################################
String ConvertUnixTimeToDay(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char FDay[40];
  strftime(FDay, sizeof(FDay), "%w", now_tm);
  Forecast_day = weekday_D[String(FDay).toInt()];
  return Forecast_day;
}
//#########################################################################################
void DrawBattery(int x, int y) {
  pinMode(A0, INPUT);     // Enable VBAT Measurement
  pinMode(D5, OUTPUT);
  digitalWrite(D5, HIGH); // 6 or D5 Battery voltage ADC enable
  delay(10); // To allow switch time to actuate (10uS)
  analogReadResolution(12);  // 12-bit resolution
  uint8_t percentage = 100;
  float voltage = 0;
  for (int r = 1; r <= 10; r++) {
    voltage += analogRead(A0) / 4096.0 * 7.0;
  }
  voltage /= 10;
  Serial.println("Voltage = " + String(voltage));
  if (voltage > 1) {  // Only display if there is a valid reading
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.50) percentage = 0;
    int BatteryWidth = 19;
    int BatteryHeight = 10;
    int BatteryBarLen = BatteryWidth - 4;
    int BatteryBarHeight = BatteryHeight - 4;
    display.drawRect(x + 15, y - 12, BatteryWidth, BatteryHeight, GxEPD_BLACK);  // Body
    display.fillRect(x + 34, y - 10, 2, 5, GxEPD_BLACK);                         // Connector button
    display.fillRect(x + 17, y - 10, BatteryBarLen * percentage / 100.0, BatteryBarHeight, GxEPD_BLACK);
    drawString(x + 10, y - 11, String(percentage) + "%", RIGHT);
    drawString(x + 13, y + 5, String(voltage, 2) + "v", CENTER);
  }
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                               // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                               // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);                     // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK);        // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK);  // Upper and lower lines
  //Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);                                                    // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);                                                    // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE);                                          // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE);                             // Right middle upper circle
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE);  // Upper and lower lines
}
//#########################################################################################
void addraindrop(int x, int y, int scale) {
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y, GxEPD_BLACK);
  x = x + scale * 1.6;
  y = y + scale / 3;
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y, GxEPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) scale *= 1.34;
  for (int d = 0; d < 3; d++) {
    addraindrop(x + scale * (6.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale, bool IconSize) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180);
      dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180);
      dyi = dyo * 0.1;
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
  else y = y - 3;  // Shift up small sun icon
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
  } else {
    y += 10;
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    addcloud(x + 30, y - 45, 5, linesize);  // Cloud top right
    addcloud(x - 20, y - 30, 7, linesize);  // Cloud top left
    addcloud(x, y, scale, linesize);        // Main cloud
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
  addcloud(x - 9, y - 3, Small * 0.5, 2);  // Cloud top left
  addcloud(x + 3, y - 3, Small * 0.5, 2);  // Cloud top right
  addcloud(x, y, Small * 0.5, 2);          // Main cloud
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 15, y - 5, String(CCover) + "%", LEFT);
}
//#########################################################################################
void Visibility(int x, int y, String Visi) {
  y = y - 3;  //
  float start_angle = 0.52, end_angle = 2.61;
  int r = 10;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    display.drawPixel(x + r * cos(i), y - r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i), GxEPD_BLACK);
  }
  start_angle = 3.61;
  end_angle = 5.78;
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
    display.fillCircle(x - 36, y - 60, scale, GxEPD_BLACK);
    display.fillCircle(x - 22, y - 60, scale * 1.6, GxEPD_WHITE);
  } else {
    display.fillCircle(x - 18, y - 11, scale, GxEPD_BLACK);
    display.fillCircle(x - 13, y - 11, scale * 1.6, GxEPD_WHITE);
  }
}
//#########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  else u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 10, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}
//#########################################################################################
/* (C) D L BIRD 2025
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
#define auto_scale_margin 0  // Sets the autoscale increment, so axis steps up in units of e.g. 3
#define y_minor_axis 5       // 5 y-axis division markers
  float maxYscale = -10000;
  float minYscale = 10000;
  float last_x, last_y;
  float x2, y2;
  if (auto_scale == true) {
    for (int i = 1; i < readings; i++) {
      if (DataArray[i] >= maxYscale) maxYscale = DataArray[i];
      if (DataArray[i] <= minYscale) minYscale = DataArray[i];
    }
    maxYscale = round(maxYscale + auto_scale_margin);  // Auto scale the graph and round to the nearest value defined, default was Y1Max
    Y1Max = round(maxYscale + 0.5);
    if (minYscale != 0) minYscale = round(minYscale - auto_scale_margin);  // Auto scale the graph and round to the nearest value defined, default was Y1Min
    Y1Min = round(minYscale);
  }
  // Draw the graph
  last_x = x_pos;
  last_y = y_pos + (Y1Max - constrain(DataArray[1], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;
  display.drawRect(x_pos, y_pos, gwidth + 3, gheight + 2, GxEPD_BLACK);
  drawString(x_pos + gwidth / 2, y_pos - 15, title, CENTER);
  // Draw the data
  for (int gx = 0; gx < readings; gx++) {
    y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
    if (barchart_mode) {
      x2 = x_pos + gx * (gwidth / readings) + 2;
      display.fillRect(x2, y2, (gwidth / readings) - 1, y_pos + gheight - y2 + 2, GxEPD_BLACK);
      } else {
      x2 = x_pos + gx * gwidth / (readings - 1) + 1;  // max_readings is the global variable that sets the maximum data that can be plotted
      display.drawLine(last_x, last_y, x2, y2, GxEPD_BLACK);
    }
    last_x = x2;
    last_y = y2;
  }
  //Draw the Y-axis scale
#define number_of_dashes 20
  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
    for (int j = 0; j < number_of_dashes; j++) {  // Draw dashed graph grid lines
      if (spacing < y_minor_axis) display.drawFastHLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes), GxEPD_BLACK);
    }
    if ((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing) < 5 || title == TXT_PRESSURE_IN) {
      drawString(x_pos - 1, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
    } else {
      if (Y1Min < 1 && Y1Max < 10)
        drawString(x_pos - 1, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
      else
        drawString(x_pos - 3, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0), RIGHT);
    }
  }
  int Days = 2;
  drawString(x_pos + gwidth / (Days * 2) * 1, y_pos + gheight + 3, "1", LEFT);
  drawString(x_pos + gwidth / (Days * 2) * 3, y_pos + gheight + 3, "2", LEFT);
  drawString(x_pos + gwidth / 2, y_pos + gheight + 14, TXT_DAYS, CENTER);
}

//#########################################################################################
void drawString(int x, int y, String text, alignment align) {
  int16_t x1, y1;  //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT) x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}
//#########################################################################################
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align) {
  int16_t x1, y1;  //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT) x = x - w;
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
    secondLine.trim();  // Remove any leading spaces
    u8g2Fonts.println(secondLine);
  }
}
//#########################################################################################
void InitialiseDisplay() {
  display.init(115200, true, 2, false);  // init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode)
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  u8g2Fonts.begin(display);                   // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                   // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);              // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);  // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);  // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);    // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.setRotation(2);                     // 2 for inverted display, 0 for normal
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}
