/* ESP32 Weather Display using an EPD 7.5" Display, obtains data from Open Weather Map, decodes it and then displays it.
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
#include "owm_credentials.h"          // See 'owm_credentials' tab and enter your OWM API key and set the Wifi SSID and PASSWORD
#include <ArduinoJson.h>              // https://github.com/bblanchon/ArduinoJson needs version v6 or above
#include <WiFi.h>                     // Built-in
#include "time.h"                     // Built-in
#include <SPI.h>                      // Built-in 
#define   ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold12pt7b.h> // Needs Adafruit_GFX library
#include "epaper_fonts.h"
#include "forecast_record.h"
#include "lang.h"                     // Localisation (English)
//#include "lang_fr.h"                  // Localisation (French)
//#include "lang_gr.h"                  // Localisation (German)
//#include "lang_it.h"                  // Localisation (Italian)

#define SCREEN_WIDTH  640             // Set for landscape mode
#define SCREEN_HEIGHT 384

enum alignment {LEFT, RIGHT, CENTER};

// E-Paper pins to ESP32 GPIO pins e.g. LOLIN32 D32 or most ESP32 development boards
static const uint8_t EPD_BUSY = 4;
static const uint8_t EPD_CS   = 5;
static const uint8_t EPD_RST  = 16; // Lolin D32 Pro pin N/A, suggest use 12
static const uint8_t EPD_DC   = 17; // Lolin D32 Pro pin N/A, suggest use 13
static const uint8_t EPD_SCK  = 18;
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23;

GxEPD2_BW<GxEPD2_750, GxEPD2_750::HEIGHT> display(GxEPD2_750(/*CS=5*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY));

//################  VERSION  ##########################
String version = "12.0";     // Programme version
//################ VARIABLES ##########################

const unsigned long long UpdateInterval = (30LL * 60LL - 8) * 1000000LL; // Update delay in microseconds, 8-secs is the time to update so compensate for that
bool    LargeIcon = true;
bool    SmallIcon = false;
#define Large  14   // For icon drawing
#define Small  4    // For icon drawing
String  time_str, date_str; // strings to hold time and received weather data;wi
int     wifi_signal, wifisection, displaysection, MoonDay, MoonMonth, MoonYear, start_time, wakeup_hour;
int     Sunrise, Sunset;

//################ PROGRAM VARIABLES and OBJECTS ################

#define max_readings 24

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];

#include "common.h"

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings]    = {0};
float rain_readings[max_readings]        = {0};
float snow_readings[max_readings]        = {0};

int wakeup_time = 7;  // Don't wakeup until after 07:00
int sleep_time  = 23; // Don't Sleep until after 23:00 to save battery

//#########################################################################################
void setup() {
  start_time = millis();
  Serial.begin(115200);
  StartWiFi();
  wifi_signal = WiFiSignal();
  SetupTime();
  InitialiseDisplay();
  if (wakeup_hour >= wakeup_time && wakeup_hour <= sleep_time) {
    // Now only refresh the screen if all the data was received OK, otherwise wait until the next timed check
    if ((obtain_wx_data(client, "weather") && obtain_wx_data(client, "forecast"))) {
      StopWiFi(); // Reduces power consumption
      display.fillScreen(GxEPD_WHITE);
      DisplayWeather();
      display.display(false); // Full screen update mode
      delay(1200);
      Serial.println("Total time to update = " + String(millis() - start_time) + "mS");
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
  esp_sleep_enable_timer_wakeup(UpdateInterval);
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT);    // In case it's on, turn output off, sometimes PIN-5 on some boards is used for SPI-SS
  digitalWrite(BUILTIN_LED, HIGH); // In case it's on, turn LED off, as sometimes PIN-5 on some boards is used for SPI-SS
#endif
  Serial.println("Awake for : " + String(millis() - start_time) + "mS");
  Serial.println(F("Starting deep-sleep period..."));
  delay(200);
  esp_deep_sleep_start();           // Sleep for e.g. 30 minutes
}
//#########################################################################################
void DisplayWeather() {                         // 7.5" e-paper display is 640x384 resolution
  DisplayGeneralInfoSection();                 // Top line of the display
  DisplayDisplayWindSection(87, 117, WxConditions[0].Winddir, WxConditions[0].Windspeed, 65);
  DisplayMainWeatherSection(241, 80);         // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
  DisplayForecastSection(174, 196);            // 3hr forecast boxes
  DisplayAstronomySection(0, 196);             // Astronomy section Sun rise/set, Moon phase and Moon icon
  DisplayStatusSection(548, 170, wifi_signal); // Wi-Fi signal strength and Battery voltage
}
//#########################################################################################
void DisplayGeneralInfoSection() {
  drawString(5, 2, "[Version: " + version + "]", LEFT); // Programme version
  drawString(SCREEN_WIDTH / 2, 3, City, CENTER);
  drawString(415, 153, date_str, CENTER);
  drawString(415, 175, time_str, CENTER);
  display.drawLine(0, 15, SCREEN_WIDTH, 15, GxEPD_BLACK);
}
//#########################################################################################
void DisplayMainWeatherSection(int x, int y) {
  display.drawRect(x - 67, y - 65, 140, 182, GxEPD_BLACK);
  display.drawRect(x + 74, y - 65, 100, 80, GxEPD_BLACK); // temp outline
  display.drawRect(x + 175, y - 65, 90, 80, GxEPD_BLACK); // pressure outline
  display.drawRect(x + 266, y - 65, 130, 80, GxEPD_BLACK); // precipitation outline
  display.drawRect(x + 74, y + 14, 322, 51, GxEPD_BLACK); // forecast text outline
  display.drawLine(0, 30, SCREEN_WIDTH, 30, GxEPD_BLACK);
  DisplayConditionsSection(x + 2, y + 35, WxConditions[0].Icon, LargeIcon);
  DisplayPressureSection(x + 220, y - 40, WxConditions[0].Pressure, WxConditions[0].Trend);
  DisplayPrecipitationSection(x + 330, y - 40);
  DisplayForecastTextSection(x + 80, y + 17);
  DisplayTemperatureSection(x + 125, y - 64);
}
//#########################################################################################
void DisplayTemperatureSection(int x, int y) {
  drawString(x, y + 2, TXT_TEMPERATURES, CENTER);
  drawString(x, y + 63, String(WxConditions[0].High, 0) + " | " + String(WxConditions[0].Low, 0), CENTER); // Show forecast high and Low
  display.setFont(&DSEG7_Classic_Bold_21);
  drawString(x - 5, y + 20, String(WxConditions[0].Temperature, 1) + "'", CENTER); // Show current Temperature
  display.setFont(&DejaVu_Sans_Bold_11);
  //drawString(x + String(WxConditions[0].Temperature, 1).length() * 19 / 2, y + 25, "*",LEFT); // Add in smaller Temperature unit in this font * is °
  drawString(x + 25, y + 32, Units == "M" ? "C" : "F", LEFT);
}
//#########################################################################################
void DisplayForecastSection(int x, int y) {
  DisplayForecastWeather(x, y, 0);
  DisplayForecastWeather(x, y, 1);
  DisplayForecastWeather(x, y, 2);
  DisplayForecastWeather(x, y, 3);
  DisplayForecastWeather(x, y, 4);
  DisplayForecastWeather(x, y, 5);
  DisplayForecastWeather(x, y, 6);
  DisplayForecastWeather(x, y, 7);
  // (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  // Pre-load temporary arrays with with data - because C parses by reference
  for (int r = 1; r <= max_readings; r++) {
    if (Units == "I") {
      pressure_readings[r]  = WxForecast[r].Pressure * 0.02953;
      rain_readings[r]      = WxForecast[r].Rainfall * 0.0393701;
      snow_readings[r]      = WxForecast[r].Snowfall * 0.0393701;
    }
    else {
      pressure_readings[r]  = WxForecast[r].Pressure;
      rain_readings[r]      = WxForecast[r].Rainfall;
      snow_readings[r]      = WxForecast[r].Snowfall;
    }
    temperature_readings[r] = WxForecast[r].Temperature;
    humidity_readings[r]    = WxForecast[r].Humidity;
  }
  int gwidth = 120;
  int gheight = 58;
  int gx = (SCREEN_WIDTH - gwidth * 4) / 5 + 5;
  int gy = 305;
  int gap = gwidth + gx;
  display.setFont(&DejaVu_Sans_Bold_11);
  drawString(SCREEN_WIDTH / 2, gy - 43, TXT_FORECAST_VALUES, CENTER); // Based on a graph height of 60
  DrawGraph(gx + 0 * gap, gy, gwidth, gheight, 900, 1050, Units == "M" ? TXT_PRESSURE_HPA : TXT_PRESSURE_IN, pressure_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 10, 30,   Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx + 2 * gap, gy, gwidth, gheight, 0, 100,   TXT_HUMIDITY_PERCENT, humidity_readings, max_readings, autoscale_off, barchart_off);
  if (SumOfPrecip(rain_readings, max_readings) >= SumOfPrecip(snow_readings, max_readings))
    DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_readings, max_readings, autoscale_on, barchart_on);
  else DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_SNOWFALL_MM : TXT_SNOWFALL_IN, snow_readings, max_readings, autoscale_on, barchart_on);
}
//#########################################################################################
void DisplayForecastTextSection(int x, int y) {
  if (Language != "DE") {
    WxConditions[0].Forecast0.toLowerCase();
    WxConditions[0].Forecast1.toLowerCase();
    WxConditions[0].Forecast2.toLowerCase();
  }
  display.setFont(&FreeMonoBold12pt7b);
  String Wx_Description = WxConditions[0].Forecast0;
  if (WxConditions[0].Forecast1 != "") {
    Wx_Description += " (" +  TitleCase(WxConditions[0].Forecast1);
    if (WxConditions[0].Forecast2 != "" && WxConditions[0].Forecast1 != WxConditions[0].Forecast2) Wx_Description += ", " +  TitleCase(WxConditions[0].Forecast2);
    Wx_Description += ")";
  }
  if (Language != "DE") {
    drawStringMaxWidth(x, y+15, 22, TitleCase(Wx_Description), LEFT); // Limit text display to the 22 character wide section, then wrap
  }
  else
    drawStringMaxWidth(x, y+13, 22, Wx_Description, LEFT); // Limit text display to the 22 character wide section, then wrap
  display.setFont(&DejaVu_Sans_Bold_11);
}
//#########################################################################################
void DisplayForecastWeather(int x, int y, int index) {
  int fwidth = 58;
  x = x + fwidth * index;
  display.drawRect(x, y, fwidth - 1, 65, GxEPD_BLACK);
  display.drawLine(x, y + 13, x + fwidth - 3, y + 13, GxEPD_BLACK);
  DisplayConditionsSection(x + fwidth / 2, y + 35, WxForecast[index].Icon, SmallIcon);
  drawString(x + fwidth / 2, y + 3, String(WxForecast[index].Period.substring(11, 16)), CENTER);
  drawString(x + fwidth / 2, y + 50, String(WxForecast[index].High, 0) + "/" + String(WxForecast[index].Low, 0), CENTER);
}
//#########################################################################################
void DisplayDisplayWindSection(int x, int y, float angle, float windspeed, int Cradius) {
  arrow(x, y, Cradius - 17, angle, 15, 27); // Show wind direction on outer circle of width and length
  drawString(x, y - Cradius - 35, TXT_WIND_SPEED_DIRECTION, CENTER);
  int dxo, dyo, dxi, dyi;
  display.drawLine(0, 15, 0, y + Cradius + 30, GxEPD_BLACK);
  display.drawCircle(x, y, Cradius, GxEPD_BLACK);  // Draw compass circle
  display.drawCircle(x, y, Cradius + 1, GxEPD_BLACK); // Draw compass circle
  display.drawCircle(x, y, Cradius * 0.7, GxEPD_BLACK); // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45)  drawString(dxo + x + 10, dyo + y - 10, TXT_NE, CENTER);
    if (a == 135) drawString(dxo + x + 7, dyo + y + 5, TXT_SE, CENTER);
    if (a == 225) drawString(dxo + x - 12, dyo + y, TXT_SW, CENTER);
    if (a == 315) drawString(dxo + x - 10, dyo + y - 10, TXT_NW, CENTER);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
  }
  drawString(x, y - Cradius - 10, TXT_N, CENTER);
  drawString(x, y + Cradius + 5,  TXT_S, CENTER);
  drawString(x - Cradius - 8, y - 5, TXT_W, CENTER);
  drawString(x + Cradius + 6, y - 5, TXT_E, CENTER);
  drawString(x, y - 35, WindDegToDirection(angle), CENTER);
  drawString(x, y + 24, String(angle, 0) + "'", CENTER);
  display.setFont(&DSEG7_Classic_Bold_18);
  drawString(x, y - 10, String(windspeed, 1), CENTER);
  display.setFont(&DejaVu_Sans_Bold_11);
  drawString(x, y + 10, (Units == "M" ? "m/s" : "mph"), CENTER);
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
void DisplayPressureSection(int x, int y, float pressure, String slope) {
  drawString(x - 35, y - 21, TXT_PRESSURE, LEFT);
  String slope_direction = TXT_PRESSURE_STEADY;
  if (slope == "+") slope_direction = TXT_PRESSURE_RISING;
  if (slope == "-") slope_direction = TXT_PRESSURE_FALLING;
  display.setFont(&FreeMonoBold12pt7b);
  display.drawRect(x + 12, y + 39, 33, 16, GxEPD_BLACK);
  display.setFont(&DSEG7_Classic_Bold_21);
  if (Units == "I") drawString(x, y - 5, String(pressure, 2), CENTER); // "Imperial"
  else              drawString(x - 10, y - 5, String(pressure, 0), CENTER); // "Metric"
  display.setFont(&DejaVu_Sans_Bold_11);
  drawString(x + 27, y + 43, (Units == "M" ? "hPa" : "in"), CENTER);
  drawString(x + 22, y + 20, slope_direction, RIGHT);
}
//#########################################################################################
void DisplayPrecipitationSection(int x, int y) {
  drawString(x, y - 24, TXT_PRECIPITATION_SOON, CENTER);
  if (String(WxForecast[1].Rainfall, 2) > "0.00") {
    drawString(x, y + 3, String(WxForecast[1].Rainfall, 2) + (Units == "M" ? "mm" : "in") + TXT_RAIN, CENTER); // Only display rainfall total today if > 0
  }
  else drawString(x, y + 3, TXT_EQUAL_RAIN, CENTER); // If no rainfall forecast
  if (String(WxForecast[1].Snowfall, 2) > "0.00") { //Sometimes very small amounts of snow are forecast, so ignore them
    drawString(x, y + 23, String(WxForecast[1].Snowfall, 2) + (Units == "M" ? "mm" : "in") + TXT_SNOW, CENTER);; // Only display snowfall total today if > 0
  }
  else drawString(x, y + 23, TXT_EQUAL_SNOW, CENTER); // If no snowfall forecast
}
//#########################################################################################
void DisplayAstronomySection(int x, int y) {
  display.drawRect(x, y + 13, 173, 52, GxEPD_BLACK);
  display.drawRect(x, y + 13, 115, 29, GxEPD_BLACK);
  drawString(x + 5, y + 15, ConvertUnixTime(WxConditions[0].Sunrise).substring(0, 5) + " " + TXT_SUNRISE, LEFT);
  drawString(x + 5, y + 28, ConvertUnixTime(WxConditions[0].Sunset).substring(0, 5) + " " + TXT_SUNSET, LEFT);
  drawString(x + 5, y + 47, MoonPhase(MoonDay, MoonMonth, MoonYear, Hemisphere), LEFT);
  DrawMoon(x + 109, y, MoonDay, MoonMonth, MoonYear, Hemisphere);
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  int diameter = 38;
  double Xpos, Ypos, Rpos, Xpos1, Xpos2;//, ip, ag;
  for (Ypos = 0; Ypos <= 45; Ypos++) {
    Xpos = sqrt(45 * 45 - Ypos * Ypos);
    // Draw dark part of moon
    double pB1x = (90   - Xpos) / 90 * diameter + x;
    double pB1y = (Ypos + 90) / 90   * diameter + y;
    double pB2x = (Xpos + 90) / 90   * diameter + x;
    double pB2y = (Ypos + 90) / 90   * diameter + y;
    double pB3x = (90   - Xpos) / 90 * diameter + x;
    double pB3y = (90   - Ypos) / 90 * diameter + y;
    double pB4x = (Xpos + 90) / 90   * diameter + x;
    double pB4y = (90   - Ypos) / 90 * diameter + y;
    display.drawLine(pB1x, pB1y, pB2x, pB2y, GxEPD_BLACK);
    display.drawLine(pB3x, pB3y, pB4x, pB4y, GxEPD_BLACK);
    // Determine the edges of the lighted part of the moon
    int j = JulianDate(dd, mm, yy);
    //Calculate the approximate phase of the moon
    double Phase = (j + 4.867) / 29.53059;
    Phase = Phase - (int)Phase;
    //if (Phase < 0.5) ag = Phase * 29.53059 + 29.53059 / 2; else ag = Phase * 29.53059 - 29.53059 / 2; // Moon's age in days if required
    if (hemisphere == "south") Phase = 1 - Phase;
    Rpos = 2 * Xpos;
    if (Phase < 0.5) {
      Xpos1 = - Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + 90) / 90 * diameter + x;
    double pW1y = (90 - Ypos)  / 90 * diameter + y;
    double pW2x = (Xpos2 + 90) / 90 * diameter + x;
    double pW2y = (90 - Ypos)  / 90 * diameter + y;
    double pW3x = (Xpos1 + 90) / 90 * diameter + x;
    double pW3y = (Ypos + 90)  / 90 * diameter + y;
    double pW4x = (Xpos2 + 90) / 90 * diameter + x;
    double pW4y = (Ypos + 90)  / 90 * diameter + y;
    display.drawLine(pW1x, pW1y, pW2x, pW2y, GxEPD_WHITE);
    display.drawLine(pW3x, pW3y, pW4x, pW4y, GxEPD_WHITE);
  }
  display.drawCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);
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
  e   = 30.6   * m;
  jd  = c + e + d - 694039.09;           /* jd is total days elapsed */
  jd /= 29.53059;                        /* divide by the moon cycle (29.53 days) */
  b   = jd;                              /* int(jd) -> b, take integer part of jd */
  jd -= b;                               /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;                    /* scale fraction from 0-8 and round by adding 0.5 */
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
void DisplayConditionsSection(int x, int y, String IconName, bool IconSize) {
  if (IconSize == LargeIcon) {
    drawString(x, y - 97, TXT_CONDITIONS, CENTER);
    display.setFont(&DSEG7_Classic_Bold_18);
    drawString(x - 5, y + 55, String(WxConditions[0].Humidity, 0) + "% RH", CENTER);
    display.setFont(&DejaVu_Sans_Bold_11);
  }
  Serial.println(IconName);
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
  else                                              Nodata(x, y, IconSize);
}
//#########################################################################################
int StartWiFi() {
  int connAttempts = 0;
  Serial.print(F("\r\nConnecting to: ")); Serial.println(String(ssid1));
  IPAddress dns(8, 8, 8, 8); // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid1, password1);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500); Serial.print(".");
    if (connAttempts > 20) {
      WiFi.disconnect();
      BeginSleep();
    }
    connAttempts++;
  }
  Serial.println("WiFi connected at: " + String(WiFi.localIP()));
  return 1;
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  wifisection    = millis() - wifisection;
}
//#########################################################################################
int WiFiSignal() {
  return WiFi.RSSI();
}
//#########################################################################################
void DisplayStatusSection(int x, int y, int rssi) {
  display.drawRect(x - 35, y - 26, 124, 53, GxEPD_BLACK);
  display.drawLine(x - 35, y - 14, x - 35 + 121, y - 14, GxEPD_BLACK);
  display.drawLine(x - 35 + 121 / 2, y - 15, x - 35 + 121 / 2, y - 26, GxEPD_BLACK);
  drawString(x - 5, y - 24, TXT_WIFI, CENTER);
  drawString(x + 55, y - 24, TXT_POWER, CENTER);
  DrawRSSI(x - 8, y + 5, rssi);
  DrawBattery(x + 50, y + 3);;
}
//#########################################################################################
void DrawRSSI(int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20)  WIFIsignal = 20; //            <-20dbm displays 5-bars
    if (_rssi <= -40)  WIFIsignal = 16; //  -40dbm to  -21dbm displays 4-bars
    if (_rssi <= -60)  WIFIsignal = 12; //  -60dbm to  -41dbm displays 3-bars
    if (_rssi <= -80)  WIFIsignal = 8;  //  -80dbm to  -61dbm displays 2-bars
    if (_rssi <= -100) WIFIsignal = 4;  // -100dbm to  -81dbm displays 1-bar
    display.fillRect(x + xpos * 5, y - WIFIsignal, 4, WIFIsignal, GxEPD_BLACK);
    xpos++;
  }
  display.fillRect(x, y - 1, 4, 1, GxEPD_BLACK);
  drawString(x + 5,  y + 2, String(rssi) + "dBm", CENTER);
}
//#########################################################################################
void SetupTime() {
  configTime(0, 0, "0.uk.pool.ntp.org", "time.nist.gov");
  setenv("TZ", Timezone, 1);
  delay(50);
  UpdateLocalTime();
}
//#########################################################################################
void UpdateLocalTime() {
  struct tm timeinfo;
  char   output[30], day_output[30];
  char   day_number[30], day_year[30];
  char   day_lang[4], month_lang[4];
  char   update_time[30];
  while (!getLocalTime(&timeinfo)) {
    Serial.println(F("Failed to obtain time"));
  }
  strftime(output, 30, "%H", &timeinfo);
  wakeup_hour = String(output).toInt();
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");      // Displays: Saturday, June 24 2017 14:05:49
  Serial.println(&timeinfo, "%H:%M:%S");                      // Displays: 14:05:49
  if (Units == "M") {
    strftime(day_lang, 4, "%a", &timeinfo);
    if (strcmp(day_lang, "Mon") == 0) strcpy(day_lang, TXT_MONDAY);
    if (strcmp(day_lang, "Tue") == 0) strcpy(day_lang, TXT_TUESDAY);
    if (strcmp(day_lang, "Wed") == 0) strcpy(day_lang, TXT_WEDNESDAY);
    if (strcmp(day_lang, "Thu") == 0) strcpy(day_lang, TXT_THURSDAY);
    if (strcmp(day_lang, "Fri") == 0) strcpy(day_lang, TXT_FRIDAY);
    if (strcmp(day_lang, "Sat") == 0) strcpy(day_lang, TXT_SATURDAY);
    if (strcmp(day_lang, "Sun") == 0) strcpy(day_lang, TXT_SUNDAY);
    strftime(month_lang, 4, "%b", &timeinfo);
    if (strcmp(month_lang, "Jan") == 0) strcpy(month_lang, TXT_JANUARY);
    if (strcmp(month_lang, "Feb") == 0) strcpy(month_lang, TXT_FEBRUARY);
    if (strcmp(month_lang, "Mar") == 0) strcpy(month_lang, TXT_MARCH);
    if (strcmp(month_lang, "Apr") == 0) strcpy(month_lang, TXT_APRIL);
    if (strcmp(month_lang, "May") == 0) strcpy(month_lang, TXT_MAY);
    if (strcmp(month_lang, "Jun") == 0) strcpy(month_lang, TXT_JUNE);
    if (strcmp(month_lang, "Jul") == 0) strcpy(month_lang, TXT_JULY);
    if (strcmp(month_lang, "Aug") == 0) strcpy(month_lang, TXT_AUGUST);
    if (strcmp(month_lang, "Sep") == 0) strcpy(month_lang, TXT_SEPTEMBER);
    if (strcmp(month_lang, "Oct") == 0) strcpy(month_lang, TXT_OCTOBER);
    if (strcmp(month_lang, "Nov") == 0) strcpy(month_lang, TXT_NOVEMBER);
    if (strcmp(month_lang, "Dec") == 0) strcpy(month_lang, TXT_DECEMBER);

    strftime(day_number, 30, "%d", &timeinfo);
    strftime(day_year, 30, "%y", &timeinfo);           // Displays: Sat 24/Jun/17
    sprintf(day_output, "%s %s-%s-%s", day_lang, day_number, month_lang, day_year);
    strftime(update_time, 30, "%H:%M:%S", &timeinfo);  // Creates: '@ 14:05:49'
    sprintf(output, "( %s %s )", TXT_UPDATED, update_time);
  }
  else
  {
    strftime(day_output, 30, "%a %b-%d-%y", &timeinfo); // Creates  'Sat May-31-19'
    strftime(output, 30, "( Updated: %r )", &timeinfo); // Creates: '@ 2:05:49pm'
  }
  date_str = day_output;
  time_str = output;
}
//#########################################################################################
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.485;
  if (voltage > 1 ) { // Only display if there is a valid reading
    Serial.println("Voltage = " + String(voltage));
    percentage = 2808.3808 * pow(voltage, 4) - 43560.9157 * pow(voltage, 3) + 252848.5888 * pow(voltage, 2) - 650767.4615 * voltage + 626532.5703;
    if (voltage > 4.19) percentage = 100;
    else if (voltage <= 3.50) percentage = 0;
    display.drawRect(x - 22, y + 5, 19, 10, GxEPD_BLACK);
    display.fillRect(x - 3, y + 7, 2, 5, GxEPD_BLACK);
    display.fillRect(x - 20, y + 7, 15 * percentage / 100.0, 6, GxEPD_BLACK);
    drawString(x + 5, y - 10, String(percentage) + "%", CENTER);
    drawString(x + 19, y + 4, String(voltage, 1) + "v", CENTER);
  }
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                             // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                             // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);                   // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK);      // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  //Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);                  // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);                  // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE);        // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE); // Right middle upper circle
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE); // Upper and lower lines
}
//#########################################################################################
void addrain(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 6; i++) {
    display.drawLine(x - scale * 4 + scale * i * 1.3 + 0, y + scale * 1.9, x - scale * 3.5 + scale * i * 1.3 + 0, y + scale, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.3 + 1, y + scale * 1.9, x - scale * 3.5 + scale * i * 1.3 + 1, y + scale, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.3 + 2, y + scale * 1.9, x - scale * 3.5 + scale * i * 1.3 + 2, y + scale, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.1;
      display.drawLine(dxo + x + 0 + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
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
void addsun(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  int dxo, dyo, dxi, dyi;
  display.fillCircle(x, y, scale, GxEPD_BLACK);
  display.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
  for (float i = 0; i < 360; i = i + 45) {
    dxo = 2.2 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.6;
    dyo = 2.2 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.6;
    if (i == 0   || i == 180) {
      display.drawLine(dxo + x - 1, dyo + y, dxi + x - 1, dyi + y, GxEPD_BLACK);
      if (scale != Small) {
        display.drawLine(dxo + x + 0, dyo + y, dxi + x + 0, dyi + y, GxEPD_BLACK);
        display.drawLine(dxo + x + 1, dyo + y, dxi + x + 1, dyi + y, GxEPD_BLACK);
      }
    }
    if (i == 90  || i == 270) {
      display.drawLine(dxo + x, dyo + y - 1, dxi + x, dyi + y - 1, GxEPD_BLACK);
      if (scale != Small) {
        display.drawLine(dxo + x, dyo + y + 0, dxi + x, dyi + y + 0, GxEPD_BLACK);
        display.drawLine(dxo + x, dyo + y + 1, dxi + x, dyi + y + 1, GxEPD_BLACK);
      }
    }
    if (i == 45  || i == 135 || i == 225 || i == 315) {
      display.drawLine(dxo + x - 1, dyo + y, dxi + x - 1, dyi + y, GxEPD_BLACK);
      if (scale != Small) {
        display.drawLine(dxo + x + 0, dyo + y, dxi + x + 0, dyi + y, GxEPD_BLACK);
        display.drawLine(dxo + x + 1, dyo + y, dxi + x + 1, dyi + y, GxEPD_BLACK);
      }
    }
  }
}
//#########################################################################################
void addfog(int x, int y, int scale, int linesize) {
  if (scale == Small) y -= 10;
  if (scale == Small) linesize = 1;
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
  if (IconName.endsWith("n")) addmoon(x, y + 3, scale);
  scale = scale * 1.6;
  addsun(x, y, scale);
}
//#########################################################################################
void MostlySunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y, scale);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
}
//#########################################################################################
void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y, scale);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void Cloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) {
    if (IconName.endsWith("n")) addmoon(x, y, scale);
    linesize = 1;
    addcloud(x, y, scale, linesize);
  }
  else {
    y += 10;
    if (IconName.endsWith("n")) addmoon(x, y, scale);
    addcloud(x + 30, y - 45, 5, linesize); // Cloud top right
    addcloud(x - 20, y - 30, 7, linesize); // Cloud top left
    addcloud(x, y, scale, linesize);   // Main cloud
  }
}
//#########################################################################################
void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y, scale);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale);
}
//#########################################################################################
void ExpectRain(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y, scale);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale);
}
//#########################################################################################
void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y, scale);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale);
}
//#########################################################################################
void Tstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y, scale);
  addcloud(x, y, scale, linesize);
  addtstorm(x, y, scale);
}
//#########################################################################################
void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y, scale);
  addcloud(x, y, scale, linesize);
  addsnow(x, y, scale);
}
//#########################################################################################
void Fog(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y, scale);
  addcloud(x, y - 5, scale, linesize);
  addfog(x, y - 5, scale, linesize);
}
//#########################################################################################
void Haze(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y, scale);
  addsun(x, y - 5, scale * 1.4);
  addfog(x, y - 5, scale * 1.4, linesize);
}
//#########################################################################################
void addmoon(int x, int y, int scale) {
  if (scale == Large) {
    display.fillCircle(x - 50, y - 60, scale, GxEPD_BLACK);
    display.fillCircle(x - 35, y - 60, scale * 1.6, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 20, y - 15, scale, GxEPD_BLACK);
    display.fillCircle(x - 15, y - 15, scale * 1.6, GxEPD_WHITE);
  }
}
//#########################################################################################
void Nodata(int x, int y, int scale) {
  //if (scale == Large) display.setFont(ArialMT_Plain_24); else display.setFont(ArialMT_Plain_16);
  if (scale == Large) display.setFont(&FreeMonoBold12pt7b); else display.setFont(&DejaVu_Sans_Bold_11);
  drawString(x, y - 10, "N/A", CENTER);
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
    barchart_on - a logical value (TRUE or FALSE) that switches the drawing mode between barhcart and line graph
    barchart_colour - a sets the title and graph plotting colour
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
  drawString(x_pos + gwidth / 2, y_pos - 19, title, CENTER);
  // Draw the data
  for (int gx = 1; gx < readings; gx++) {
    x2 = x_pos + gx * gwidth / (readings - 1) - 1 ; // max_readings is the global variable that sets the maximum data that can be plotted
    y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
    if (barchart_mode) {
      display.fillRect(x2, y2, (gwidth / readings) - 1, y_pos + gheight - y2 + 1, GxEPD_BLACK);
    } else {
      display.drawLine(last_x, last_y, x2, y2, GxEPD_BLACK);
    }
    last_x = x2;
    last_y = y2;
  }
  //Draw the Y-axis scale
  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
#define number_of_dashes 20
    for (int j = 0; j < number_of_dashes; j++) { // Draw dashed graph grid lines
      if (spacing < y_minor_axis) display.drawFastHLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes), GxEPD_BLACK);
    }
    if ((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing) < 10 || title == TXT_PRESSURE_IN) {
      drawString(x_pos - 2, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
    }
    else {
      if (Y1Min < 1 && Y1Max < 10) drawString(x_pos - 2, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
      else
        drawString(x_pos - 2, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0), RIGHT);
    }
  }
  for (int i = 0; i <= 2; i++) {
    drawString(15 + x_pos + gwidth / 3 * i, y_pos + gheight + 3, String(i), LEFT);
  }
  drawString(x_pos + gwidth / 2, y_pos + gheight + 7, TXT_DAYS, CENTER);
}

//#########################################################################################
void drawString(int x, int y, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  display.setCursor(x, y + h);
  display.print(text);
}
//#########################################################################################
void drawStringMaxWidth(int x, int y, int text_width, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  display.setCursor(x, y);
  display.println(text.substring(0, text_width));
  if (text.length() > text_width) {
    display.setCursor(x, y+h+7);
    String secondLine = text.substring(text_width);
    secondLine.trim(); // Remove any leading spaces
    display.println(secondLine);
  }
}

void InitialiseDisplay() {
  display.init(115200);
  display.setRotation(0);
  display.setTextSize(0);
  display.setFont(&DejaVu_Sans_Bold_11);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
}
