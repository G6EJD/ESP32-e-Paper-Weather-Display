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
#include "owm_credentials.h"   // See 'owm_credentials' tab and enter your OWM API key and set the Wifi SSID and PASSWORD
#include <ArduinoJson.h>       // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>              // Built-in
#include "time.h"              // Built-in
#include <SPI.h>               // Built-in 
#include "EPD_WaveShare.h"     // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "EPD_WaveShare_42.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "MiniGrafx.h"         // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "DisplayDriver.h"     // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "ArialRounded.h"      // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx

#define SCREEN_WIDTH  400      // Set for landscape mode
#define SCREEN_HEIGHT 300      // Set for landscape mode
#define BITS_PER_PIXEL 1
#define EPD_BLACK 0
#define EPD_WHITE 1
uint16_t palette[] = { 0, 1 };

// pins_arduino.h, e.g. LOLIN32 LITE
static const uint8_t EPD_BUSY  = 4;
static const uint8_t EPD_SS    = 5;
static const uint8_t EPD_RST   = 16;
static const uint8_t EPD_DC    = 17;
static const uint8_t EPD_SCK   = 18;
static const uint8_t EPD_MISO  = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI  = 23;

EPD_WaveShare42 epd(EPD_SS, EPD_RST, EPD_DC, EPD_BUSY);
MiniGrafx gfx = MiniGrafx(&epd, BITS_PER_PIXEL, palette);

//################  VERSION  ##########################
String version = "6";       // Version of this program
//################ VARIABLES ###########################

const unsigned long UpdateInterval     = 30L * 60L * 1000000L; // Delay between updates, in milliseconds, WU allows 500 requests per-day maximum, set to every 15-mins or more
bool LargeIcon   =  true;
bool SmallIcon   =  false;
#define Large  11
#define Small  4
String time_str, Day_time_str, rxtext; // strings to hold time and received weather data;
int    wifisection, displaysection, MoonDay, MoonMonth, MoonYear;
int    Sunrise, Sunset;

//################ PROGRAM VARIABLES and OBJECTS ################

typedef struct { // For current Day and Day 1, 2, 3
  String   Period;
  float    Temperature;
  float    Humidity;
  String   Icon;
  float    High;
  float    Low;
  float    Rainfall;
  float    Pressure;
  int      Cloudcover;
  String   Trend;
  float    Winddir;
  float    Windspeed;
  String   Forecast;
  String   Description;
  String   Time;
} Forecast_record_type;

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[40];

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

#define max_readings 41
float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float rain_readings[max_readings]        = {0};

WiFiClient client; // wifi client object

//#########################################################################################
void setup() {
  Serial.begin(115200);
  StartWiFi();
  SetupTime();
  bool Received_Forecast_OK = false;
  obtain_wx_data("weather");  Received_Forecast_OK = DecodeWeather(rxtext, "weather");
  obtain_wx_data("forecast"); Received_Forecast_OK = DecodeWeather(rxtext, "forecast");
  // Now only refresh the screen if all the data was received OK, otherwise wait until the next timed check otherwise wait until the next timed check
  if (Received_Forecast_OK) {
    StopWiFi(); // Reduces power consumption
    gfx.init();
    gfx.setRotation(0);
    gfx.setColor(EPD_BLACK);
    gfx.fillBuffer(EPD_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    Display_Weather();
    DrawBattery(320, 0);
    gfx.commit();
    delay(2000);
  }
  Serial.println("Starting deep-sleep period...");
  esp_sleep_enable_timer_wakeup(UpdateInterval);
  esp_deep_sleep_start(); // Sleep for e.g. 30 minutes
}
//#########################################################################################
void loop() { // this will never run!

}
//#########################################################################################
void Display_Weather() {              // 4.2" e-paper display is 400x300 resolution
  Draw_Heading_Section();             // Top line of the display
  Draw_Main_Weather_Section(165, 70); // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
  Draw_Forecast_Section(230, 18);     // 3hr forecast boxes
  Draw_Astronomy_Section(230, 20);    // Astronomy section Sun rise/set, Moon phase and Moon icon
}
//#########################################################################################
void Draw_Heading_Section() {
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(SCREEN_WIDTH / 2, -2, City);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(SCREEN_WIDTH - 8, 0, Day_time_str);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(5, 0, time_str);
  gfx.drawLine(0, 15, SCREEN_WIDTH, 15);
}
//#########################################################################################
void Draw_Main_Weather_Section(int x, int y) {
  DisplayWXicon(x + 5, y - 8, WxConditions[0].Icon, LargeIcon);
  gfx.setFont(ArialRoundedMTBold_14);
  DrawPressureTrend(x, y + 50, WxConditions[0].Pressure, WxConditions[0].Trend);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  Draw_Rain(x - 100, y + 35);
  gfx.setFont(ArialMT_Plain_24);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(x - 155, y + 70, TitleCase(WxConditions[0].Description));
  Draw_Main_Wx(x - 98, y - 1);
  gfx.drawLine(0, y + 68, SCREEN_WIDTH, y + 68);
}
//#########################################################################################
String TitleCase(String text) {
  if (text.length() > 0) {
    String temp_text = text.substring(0, 1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1); // Title-case the string
  }
  return "";
}
//#########################################################################################
void Draw_Forecast_Section(int x, int y) {
  gfx.setFont(ArialMT_Plain_10);
  Draw_Forecast_Weather(x, y, 0);
  Draw_Forecast_Weather(x + 56, y, 1);
  Draw_Forecast_Weather(x + 112, y, 2);
  //       (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  for (int r = 1; r <= max_readings; r++) {
    pressure_readings[r]    = WxForecast[r].Pressure;
    temperature_readings[r] = WxForecast[r].Temperature;
    rain_readings[r]        = WxForecast[r].Rainfall;
  }
  gfx.drawLine(0, y + 173, SCREEN_WIDTH, y + 173);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x - 40, y + 173, "5-Day Forecast Values");
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  DrawGraph(30,  222, 100, 60, 900, 1050,"Pressure",    pressure_readings,    40, autoscale_on, barchart_off);
  DrawGraph(158, 222, 100, 60, 10 ,  30, "Temperature", temperature_readings, 40, autoscale_on, barchart_off);
  DrawGraph(288, 222, 100, 60, 0  ,  30, "Rainfall",    rain_readings,        40, autoscale_on, barchart_on);
}
//#########################################################################################
void Draw_Forecast_Weather(int x, int y, int index) {
  gfx.setFont(ArialMT_Plain_10);
  gfx.setColor(EPD_BLACK); // Sometimes gets set to WHITE, so change back
  gfx.drawRect(x, y, 55, 65);
  gfx.drawLine(x + 1, y + 13, x + 55, y + 13);
  DisplayWXicon(x + 28, y + 35, WxForecast[index].Icon, SmallIcon);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(x + 28, y, String(WxForecast[index].Period.substring(11,16)));
  gfx.drawString(x + 28, y + 50, String(WxForecast[index].High,0) + "° / " + String(WxForecast[index].Low,0) + "°");
}
//#########################################################################################
void Draw_Main_Wx(int x, int y) {
  DrawWind(x, y, WxConditions[0].Winddir, WxConditions[0].Windspeed);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x, y - 28, String(WxConditions[0].High,0) + "° | " + String(WxConditions[0].Low,0) + "°"); // Show forecast high and Low
  gfx.setFont(ArialMT_Plain_24);
  gfx.drawString(x - 5, y - 10, String(WxConditions[0].Temperature,1) + "°"); // Show current Temperature
    gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);  
  gfx.drawString(x+String(WxConditions[0].Temperature,1).length()*11/2,y-9,Units=="M"?"C":"F"); // Add in smaller Temperature unit
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}
//#########################################################################################
void DrawWind(int x, int y, float angle, float windspeed) {
  int Cradius = 44;
  float dx = Cradius * cos((angle - 90) * PI / 180) + x; // calculate X position
  float dy = Cradius * sin((angle - 90) * PI / 180) + y; // calculate Y position
  arrow(x, y, Cradius - 3, angle, 15, 15); // Show wind direction on outer circle
  gfx.drawCircle(x, y, Cradius + 2);
  gfx.drawCircle(x, y, Cradius + 3);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x, y + Cradius - 25, WindDegToDirection(angle));
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(x - Cradius + 5, y - Cradius - 6, String(windspeed,1) + (Units == "M" ? " m/s" : " mph"));
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}
//#########################################################################################
String WindDegToDirection(float winddirection) {
  if (winddirection >= 348.75 || winddirection < 11.25)  return "N";
  if (winddirection >=  11.25 && winddirection < 33.75)  return "NNE";
  if (winddirection >=  33.75 && winddirection < 56.25)  return "NE";
  if (winddirection >=  56.25 && winddirection < 78.75)  return "ENE";
  if (winddirection >=  78.75 && winddirection < 101.25) return "E";
  if (winddirection >= 101.25 && winddirection < 123.75) return "ESE";
  if (winddirection >= 123.75 && winddirection < 146.25) return "SE";
  if (winddirection >= 146.25 && winddirection < 168.75) return "SSE";
  if (winddirection >= 168.75 && winddirection < 191.25) return "S";
  if (winddirection >= 191.25 && winddirection < 213.75) return "SSW";
  if (winddirection >= 213.75 && winddirection < 236.25) return "SW";
  if (winddirection >= 236.25 && winddirection < 258.75) return "WSW";
  if (winddirection >= 258.75 && winddirection < 281.25) return "W";
  if (winddirection >= 281.25 && winddirection < 303.75) return "WNW";
  if (winddirection >= 303.75 && winddirection < 326.25) return "NW";
  if (winddirection >= 326.25 && winddirection < 348.75) return "NNW";
  return "?";
}
//#########################################################################################
void DrawPressureTrend(int x, int y, float pressure, String slope) {
  gfx.drawString(x - 25, y, String(pressure,1) + (Units == "M" ? "mb" : "in"));
  x = x + 45; y = y + 8;
  if      (slope == "+") {
    gfx.drawLine(x,  y,  x + 4, y - 4);
    gfx.drawLine(x + 4, y - 4, x + 8, y);
  }
  else if (slope == "0") {
    gfx.drawLine(x + 3, y - 4, x + 8, y);
    gfx.drawLine(x + 3, y + 4, x + 8, y);
  }
  else if (slope == "-") {
    gfx.drawLine(x,  y,  x + 4, y + 4);
    gfx.drawLine(x + 4, y + 4, x + 8, y);
  }
}
//#########################################################################################
void Draw_Rain(int x, int y) {
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  if (WxForecast[1].Rainfall > 0) gfx.drawString(x, y + 14, String(WxForecast[1].Rainfall,1) + (Units == "M" ? "mm" : "in") + " Rainfall"); // Only display rainfall total today if > 0
  gfx.setFont(ArialMT_Plain_10);
}
//#########################################################################################
void Draw_Astronomy_Section(int x, int y) {
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawRect(x, y + 64, 167, 53);
  gfx.drawString(x + 4, y + 65, "Sun Rise/Set");
  gfx.drawString(x + 20, y + 75, ConvertUnixTime(Sunrise).substring(0, 5));
  gfx.drawString(x + 20, y + 85, ConvertUnixTime(Sunset).substring(0, 5));
  gfx.drawString(x + 4, y + 100, "Moon:");
  gfx.drawString(x + 35, y + 100, MoonPhase(MoonDay, MoonMonth, MoonYear, Hemisphere));
  DrawMoon(x + 103, y + 51, MoonDay, MoonMonth, MoonYear, Hemisphere);
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
#define pi 3.141592654
  int diameter = 38;
  float Xpos, Ypos, Rpos, Xpos1, Xpos2, ip, ag;
  gfx.setColor(EPD_BLACK);
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
    gfx.setColor(EPD_BLACK);
    gfx.drawLine(pB1x, pB1y, pB2x, pB2y);
    gfx.drawLine(pB3x, pB3y, pB4x, pB4y);
    // Determine the edges of the lighted part of the moon
    int j = JulianDate(dd, mm, yy);
    //Calculate the approximate phase of the moon
    double Phase = (j + 4.867) / 29.53059;
    Phase = Phase - (int)Phase;
    if (Phase < 0.5) ag = Phase * 29.53059 + 29.53059 / 2; else ag = Phase * 29.53059 - 29.53059 / 2; // Moon's age in days
    if (hemisphere == "south") Phase = 1 - Phase;
    Rpos = 2 * Xpos;
    if (Phase < 0.5) {
      Xpos1 = - Xpos;
      Xpos2 = (Rpos - 2 * Phase * Rpos - Xpos);
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = (Xpos - 2 * Phase * Rpos + Rpos);
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + 90) / 90 * diameter + x;
    double pW1y = (90 - Ypos) / 90  * diameter + y;
    double pW2x = (Xpos2 + 90) / 90 * diameter + x;
    double pW2y = (90 - Ypos) / 90  * diameter + y;
    double pW3x = (Xpos1 + 90) / 90 * diameter + x;
    double pW3y = (Ypos + 90) / 90  * diameter + y;
    double pW4x = (Xpos2 + 90) / 90 * diameter + x;
    double pW4y = (Ypos + 90) / 90  * diameter + y;
    gfx.setColor(EPD_WHITE);
    gfx.drawLine(pW1x, pW1y, pW2x, pW2y);
    gfx.drawLine(pW3x, pW3y, pW4x, pW4y);
  }
  gfx.setColor(EPD_BLACK);
  gfx.drawCircle(x + diameter - 1, y + diameter, diameter / 2 + 1);
}
//#########################################################################################
int JulianDate(int d, int m, int y) {
  int mm, yy, k1, k2, k3, j;
  yy = y - (int)((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12) mm = mm - 12;
  k1 = (int)(365.25 * (yy + 4712));
  k2 = (int)(30.6001 * mm + 0.5);
  k3 = (int)((int)((yy / 100) + 49) * 0.75) - 38;
  // 'j' for dates in Julian calendar:
  j = k1 + k2 + d + 59 + 1;
  if (j > 2299160) j = j - k3; // 'j' is the Julian date at 12h UT (Universal Time) For Gregorian calendar:
  return j;
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
  e   = 30.6 * m;
  jd  = c + e + d - 694039.09;           /* jd is total days elapsed */
  jd /= 29.53059;                        /* divide by the moon cycle (29.53 days) */
  b   = jd;                              /* int(jd) -> b, take integer part of jd */
  jd -= b;                               /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;                    /* scale fraction from 0-8 and round by adding 0.5 */
  b   = b & 7;                           /* 0 and 8 are the same phase so modulo 8 for 0 */
  if (hemisphere == "south") b = 7 - b;
  if (b == 0) return "New";              // New; 0% illuminated
  if (b == 1) return "Waxing crescent";  // Waxing crescent; 25% illuminated
  if (b == 2) return "First quarter";    // First quarter; 50% illuminated
  if (b == 3) return "Waxing gibbous";   // Waxing gibbous; 75% illuminated
  if (b == 4) return "Full";             // Full; 100% illuminated
  if (b == 5) return "Waning gibbous";   // Waning gibbous; 75% illuminated
  if (b == 6) return "Last quarter";     // Last quarter; 50% illuminated
  if (b == 7) return "Waning crescent";  // Waning crescent; 25% illuminated
  return "";
}
//#########################################################################################
void DrawCircle(int x, int y, int Cstart, int Cend, int Cradius, int Coffset_radius, int Coffset) {
  gfx.setColor(EPD_BLACK);
  float dx, dy;
  for (int i = Cstart; i < Cend; i++) {
    dx = (Cradius + Coffset_radius) * cos((i - 90) * PI / 180) + x + Coffset / 2; // calculate X position
    dy = (Cradius + Coffset_radius) * sin((i - 90) * PI / 180) + y; // calculate Y position
    gfx.setPixel(dx, dy);
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
  gfx.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2);
}
//#########################################################################################
void DisplayWXicon(int x, int y, String IconName, bool LargeSize) {
  Serial.println(IconName);
    if      (IconName == "01d" || IconName == "01n")  if (LargeSize) Sunny(x, y, Large); else Sunny(x, y, Small);
    else if (IconName == "02d" || IconName == "02n")  if (LargeSize) MostlySunny(x, y, Large); else MostlySunny(x, y, Small);
    else if (IconName == "03d" || IconName == "03n")  if (LargeSize) Cloudy(x, y, Large); else Cloudy(x, y, Small);
    else if (IconName == "04d" || IconName == "04n")  if (LargeSize) MostlySunny(x, y, Large); else MostlySunny(x, y, Small);
    else if (IconName == "09d" || IconName == "09n")  if (LargeSize) ChanceRain(x, y, Large); else ChanceRain(x, y, Small);
    else if (IconName == "10d" || IconName == "10n")  if (LargeSize) Rain(x, y, Large); else Rain(x, y, Small);
    else if (IconName == "11d" || IconName == "11n")  if (LargeSize) Tstorms(x, y, Large); else Tstorms(x, y, Small);
    else if (IconName == "13d" || IconName == "13n")  if (LargeSize) Snow(x, y, Large); else Snow(x, y, Small);
    else if (IconName == "50d")                       if (LargeSize) Haze(x, y - 5, Large); else Haze(x, y, Small);
    else if (IconName == "50n")                       if (LargeSize) Fog(x, y - 5, Large); else Fog(x, y, Small);
    else if (IconName == "probrain")                  if (LargeSize) ProbRain(x, y, Large); else ProbRain(x, y, Small);
  else                                              if (LargeSize) Nodata(x, y, Large); else Nodata(x, y, Small);
}
//#########################################################################################
bool obtain_wx_data(String RequestType) {
  rxtext = "";
  client.stop(); // close connection before sending a new request
  if (client.connect(server, 80)) { // if the connection succeeds
    // Serial.println("connecting...");
    // send the HTTP PUT request:
    client.println("GET /data/2.5/" + RequestType + "?q=" + City + "," + Country + "&APPID=" + apikey + "&mode=json&units=metric&cnt=40 HTTP/1.1");
    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ESP OWM Receiver/1.1");
    client.println("Connection: close");
    client.println();
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return false;
      }
    }
    char c = 0;
    bool startJson = false;
    int jsonend = 0;
    while (client.available()) {
      c = client.read();
      // since json contains equal number of open and close curly brackets, this means we can determine when a json is completely received  by counting
      // the open and close occurences,
      //Serial.print(c);
      if (c == '{') {
        startJson = true;         // set startJson true to indicate json message has started
        jsonend++;
      }
      if (c == '}') {
        jsonend--;
      }
      if (startJson == true) {
        rxtext += c; // Add in the received character
      }
      // if jsonend = 0 then we have have received equal number of curly braces
      if (jsonend == 0 && startJson == true) {
        Serial.println("Received OK...");
        //Serial.println(rxtext);
        client.stop();
        return true;
      }
    }
  }
  else {
    // if no connction was made:
    Serial.println("connection failed");
    return false;
  }
  return true;
}
//#########################################################################################
bool DecodeWeather(String json, String Type) {
  Serial.print(F("Creating object...and "));
  DynamicJsonBuffer jsonBuffer (50 * 1024);
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    Serial.print("ParseObject() failed");
    return false;
  }
  Serial.println("Decoding " + Type + " data");
  if (Type == "weather") {
    //Serial.println(json);
    JsonObject& location = root["coord"];
    // All Serial.println statements are for diagnostic purposes and not required, remove if not needed 
    String lon                  = location["lon"];        Serial.println("Lon:" + lon);
    String lat                  = location["lat"];        Serial.println("Lat:" + lat);
    JsonObject& weather         = root["weather"][0];
    const char* description     = weather["main"];        WxConditions[0].Description = String(description); Serial.println(WxConditions[0].Description);
    const char * icon           = weather["icon"];        WxConditions[0].Icon = String(icon);   Serial.println(WxConditions[0].Icon);
    JsonObject& main            = root["main"];
    WxConditions[0].Temperature = main["temp"];           Serial.println(WxConditions[0].Temperature);
    WxConditions[0].Pressure    = main["pressure"];       Serial.println(WxConditions[0].Pressure);
    WxConditions[0].Humidity    = main["humidity"];       Serial.println(WxConditions[0].Humidity);
    WxConditions[0].Low         = main["temp_min"];       Serial.println(WxConditions[0].Low);
    WxConditions[0].High        = main["temp_max"];       Serial.println(WxConditions[0].High);
    WxConditions[0].Windspeed   = root["wind"]["speed"];  Serial.println(WxConditions[0].Windspeed);
    WxConditions[0].Winddir     = root["wind"]["deg"];    Serial.println(WxConditions[0].Winddir);
    WxConditions[0].Cloudcover  = root["clouds"]["all"];  Serial.println(WxConditions[0].Cloudcover); // Result in % of cloud cover
    JsonObject& sys             = root["sys"];
    String country              = sys["country"];         Serial.println(country);
    int    sunrise              = sys["sunrise"];         Serial.println(sunrise);       Sunrise      = sunrise;
    int    sunset               = sys["sunset"];          Serial.println(sunset);        Sunset       = sunset;
  }
  if (Type == "forecast") {
    //Serial.println(json);
    Serial.print("\nReceiving Forecast period - 0,"); //------------------------------------------------
    const char* cod                 = root["cod"]; // "200"
    float message                   = root["message"]; 
    int cnt                         = root["cnt"]; 
    JsonArray& list                 = root["list"];

    JsonObject& list0               = list[0];
    int list0_dt                    = list0["dt"]; 
    JsonObject& list0_main          = list0["main"];
    WxForecast[0].Temperature       = list0_main["temp"];
    WxForecast[0].Low               = list0_main["temp_min"];
    WxForecast[0].High              = list0_main["temp_max"];
    WxForecast[0].Pressure          = list0_main["pressure"];
    WxForecast[0].Humidity          = list0_main["humidity"];
    JsonObject& list0_weather0      = list0["weather"][0];
    const char * list0_forecast     = list0_weather0["main"];        WxForecast[0].Forecast = String(list0_forecast);
    const char * list0_description  = list0_weather0["description"]; WxForecast[0].Description = String(list0_description);
    const char * list0_icon         = list0_weather0["icon"];        WxForecast[0].Icon = String(list0_icon);
    WxForecast[0].Windspeed         = list0["wind"]["speed"];
    WxForecast[0].Winddir           = list0["wind"]["deg"];
    WxForecast[0].Rainfall          = list0["rain"]["3h"];
    const char * list0_period       = list0["dt_txt"];               WxForecast[0].Period = String(list0_period);

    Serial.print("1,"); //------------------------------------------------
    JsonObject& list1               = list[1];
    long list1_dt                   = list1["dt"]; 
    JsonObject& list1_main          = list1["main"];
    WxForecast[1].Temperature       = list1_main["temp"];
    WxForecast[1].Low               = list1_main["temp_min"];
    WxForecast[1].High              = list1_main["temp_max"];
    WxForecast[1].Pressure          = list1_main["pressure"];
    WxForecast[1].Humidity          = list1_main["humidity"];
    JsonObject& list1_weather0      = list1["weather"][0];
    const char * list1_forecast     = list1_weather0["main"];        WxForecast[1].Forecast = String(list1_forecast);
    const char * list1_description  = list1_weather0["description"]; WxForecast[1].Description = String(list1_description);
    const char * list1_icon         = list1_weather0["icon"];        WxForecast[1].Icon = String(list1_icon);
    WxForecast[1].Windspeed         = list1["wind"]["speed"];
    WxForecast[1].Winddir           = list1["wind"]["deg"];
    WxForecast[1].Rainfall          = list1["rain"]["3h"];
    const char * list1_period       = list1["dt_txt"];               WxForecast[1].Period = String(list1_period);

    Serial.print("2,"); //------------------------------------------------
    JsonObject& list2               = list[2];
    long list2_dt                   = list2["dt"]; 
    JsonObject& list2_main          = list2["main"];
    WxForecast[2].Temperature       = list2_main["temp"];
    WxForecast[2].Low               = list2_main["temp_min"];
    WxForecast[2].High              = list2_main["temp_max"];
    WxForecast[2].Pressure          = list2_main["pressure"];
    WxForecast[2].Humidity          = list2_main["humidity"]; 
    JsonObject& list2_weather0      = list2["weather"][0];
    const char * list2_forecast     = list2_weather0["main"];        WxForecast[2].Forecast = String(list2_forecast);
    const char * list2_description  = list2_weather0["description"]; WxForecast[2].Description = String(list2_description);
    const char * list2_icon         = list2_weather0["icon"];        WxForecast[2].Icon = String(list2_icon);
    WxForecast[2].Windspeed         = list2["wind"]["speed"]; 
    WxForecast[2].Winddir           = list2["wind"]["deg"]; 
    WxForecast[2].Rainfall          = list2["rain"]["3h"]; 
    const char * list2_period       = list2["dt_txt"];               WxForecast[2].Period = String(list2_period);

    Serial.print("3,"); //------------------------------------------------
    JsonObject& list3               = list[3];
    long list3_dt                   = list3["dt"]; 
    JsonObject& list3_main          = list3["main"];
    WxForecast[3].Temperature       = list3_main["temp"];
    WxForecast[3].Low               = list3_main["temp_min"];
    WxForecast[3].High              = list3_main["temp_max"];
    WxForecast[3].Pressure          = list3_main["pressure"];
    WxForecast[3].Humidity          = list3_main["humidity"]; 
    JsonObject& list3_weather0      = list3["weather"][0];
    const char * list3_forecast     = list3_weather0["main"];        WxForecast[3].Forecast = String(list3_forecast);
    const char * list3_description  = list3_weather0["description"]; WxForecast[3].Description = String(list3_description);
    const char * list3_icon         = list3_weather0["icon"];        WxForecast[3].Icon = String(list3_icon);
    WxForecast[3].Windspeed         = list3["wind"]["speed"]; 
    WxForecast[3].Winddir           = list3["wind"]["deg"]; 
    WxForecast[3].Rainfall          = list3["rain"]["3h"]; 
    const char * list3_period       = list3["dt_txt"];               WxForecast[3].Period = String(list3_period);

    Serial.print("4,");  //---------------------------
    JsonObject& list4               = list[4];
    long list4_dt                   = list4["dt"];
    JsonObject& list4_main          = list4["main"];
    WxForecast[4].Temperature       = list4_main["temp"];
    WxForecast[4].Low               = list4_main["temp_min"];
    WxForecast[4].High              = list4_main["temp_max"];
    WxForecast[4].Pressure          = list4_main["pressure"];
    WxForecast[4].Humidity          = list4_main["humidity"];
    JsonObject& list4_weather0      = list4["weather"][0];
    const char * list4_forecast     = list4_weather0["main"];        WxForecast[4].Forecast = String(list4_forecast);
    const char * list4_description  = list4_weather0["description"]; WxForecast[4].Description = String(list4_description);
    const char * list4_icon         = list4_weather0["icon"];        WxForecast[4].Icon = String(list4_icon);
    WxForecast[4].Windspeed         = list4["wind"]["speed"];
    WxForecast[4].Winddir           = list4["wind"]["deg"];
    WxForecast[4].Rainfall          = list4["rain"]["3h"];
    const char * list4_period       = list4["dt_txt"];               WxForecast[4].Period = String(list4_period);

    Serial.print("5,");  //---------------------------
    JsonObject& list5               = list[5];
    long list5_dt                   = list5["dt"];
    JsonObject& list5_main          = list5["main"];
    WxForecast[5].Temperature       = list5_main["temp"];
    WxForecast[5].Low               = list5_main["temp_min"];
    WxForecast[5].High              = list5_main["temp_max"];
    WxForecast[5].Pressure          = list5_main["pressure"];
    WxForecast[5].Humidity          = list5_main["humidity"];
    JsonObject& list5_weather0      = list5["weather"][0];
    const char * list5_forecast     = list5_weather0["main"];        WxForecast[5].Forecast = String(list5_forecast);
    const char * list5_description  = list5_weather0["description"]; WxForecast[5].Description = String(list5_description);
    const char * list5_icon         = list5_weather0["icon"];        WxForecast[5].Icon = String(list5_icon);
    WxForecast[5].Windspeed         = list5["wind"]["speed"];
    WxForecast[5].Winddir           = list5["wind"]["deg"];
    WxForecast[5].Rainfall          = list5["rain"]["3h"];
    const char * list5_period       = list5["dt_txt"];               WxForecast[5].Period = String(list5_period);

    Serial.print("6,");  //---------------------------
    JsonObject& list6               = list[6];
    long list6_dt                   = list6["dt"];
    JsonObject& list6_main          = list6["main"];
    WxForecast[6].Temperature       = list6_main["temp"];
    WxForecast[6].Low               = list6_main["temp_min"];
    WxForecast[6].High              = list6_main["temp_max"];
    WxForecast[6].Pressure          = list6_main["pressure"];
    WxForecast[6].Humidity          = list6_main["humidity"];
    JsonObject& list6_weather0      = list6["weather"][0];
    const char * list6_forecast     = list6_weather0["main"];        WxForecast[6].Forecast = String(list6_forecast);
    const char * list6_description  = list6_weather0["description"]; WxForecast[6].Description = String(list6_description);
    const char * list6_icon         = list6_weather0["icon"];        WxForecast[6].Icon = String(list6_icon);
    WxForecast[6].Windspeed         = list6["wind"]["speed"];
    WxForecast[6].Winddir           = list6["wind"]["deg"];
    WxForecast[6].Rainfall          = list6["rain"]["3h"];
    const char * list6_period       = list6["dt_txt"];               WxForecast[6].Period = String(list6_period);

    Serial.print("7,");  //---------------------------
    JsonObject& list7               = list[7];
    long list7_dt                   = list7["dt"];
    JsonObject& list7_main          = list7["main"];
    WxForecast[7].Temperature       = list7_main["temp"];
    WxForecast[7].Low               = list7_main["temp_min"];
    WxForecast[7].High              = list7_main["temp_max"];
    WxForecast[7].Pressure          = list7_main["pressure"];
    WxForecast[7].Humidity          = list7_main["humidity"];
    JsonObject& list7_weather0      = list7["weather"][0];
    const char * list7_forecast     = list7_weather0["main"];        WxForecast[7].Forecast = String(list7_forecast);
    const char * list7_description  = list7_weather0["description"]; WxForecast[7].Description = String(list7_description);
    const char * list7_icon         = list7_weather0["icon"];        WxForecast[7].Icon = String(list7_icon);
    WxForecast[7].Windspeed         = list7["wind"]["speed"];
    WxForecast[7].Winddir           = list7["wind"]["deg"];
    WxForecast[7].Rainfall          = list7["rain"]["3h"];
    const char * list7_period       = list7["dt_txt"];               WxForecast[7].Period = String(list7_period);

    Serial.print("8,");  //---------------------------
    JsonObject& list8               = list[8];
    long list8_dt                   = list8["dt"];
    JsonObject& list8_main          = list8["main"];
    WxForecast[8].Temperature       = list8_main["temp"];
    WxForecast[8].Low               = list8_main["temp_min"];
    WxForecast[8].High              = list8_main["temp_max"];
    WxForecast[8].Pressure          = list8_main["pressure"];
    WxForecast[8].Humidity          = list8_main["humidity"];
    JsonObject& list8_weather0      = list8["weather"][0];
    const char * list8_forecast     = list8_weather0["main"];        WxForecast[8].Forecast = String(list8_forecast);
    const char * list8_description  = list8_weather0["description"]; WxForecast[8].Description = String(list8_description);
    const char * list8_icon         = list8_weather0["icon"];        WxForecast[8].Icon = String(list8_icon);
    WxForecast[8].Windspeed         = list8["wind"]["speed"];
    WxForecast[8].Winddir           = list8["wind"]["deg"];
    WxForecast[8].Rainfall          = list8["rain"]["3h"];
    const char * list8_period       = list8["dt_txt"];               WxForecast[8].Period = String(list8_period);

    Serial.print("9,");  //---------------------------
    JsonObject& list9               = list[9];
    long list9_dt                   = list9["dt"];
    JsonObject& list9_main          = list9["main"];
    WxForecast[9].Temperature       = list9_main["temp"];
    WxForecast[9].Low               = list9_main["temp_min"];
    WxForecast[9].High              = list9_main["temp_max"];
    WxForecast[9].Pressure          = list9_main["pressure"];
    WxForecast[9].Humidity          = list9_main["humidity"];
    JsonObject& list9_weather0      = list9["weather"][0];
    const char * list9_forecast     = list9_weather0["main"];        WxForecast[9].Forecast = String(list9_forecast);
    const char * list9_description  = list9_weather0["description"]; WxForecast[9].Description = String(list9_description);
    const char * list9_icon         = list9_weather0["icon"];        WxForecast[9].Icon = String(list9_icon);
    WxForecast[9].Windspeed         = list9["wind"]["speed"];
    WxForecast[9].Winddir           = list9["wind"]["deg"];
    WxForecast[9].Rainfall          = list9["rain"]["3h"];
    const char * list9_period       = list9["dt_txt"];               WxForecast[9].Period = String(list9_period);

    Serial.print("10,");  //---------------------------
    JsonObject& list10               = list[10];
    long list10_dt                   = list10["dt"];
    JsonObject& list10_main          = list10["main"];
    WxForecast[10].Temperature       = list10_main["temp"];
    WxForecast[10].Low               = list10_main["temp_min"];
    WxForecast[10].High              = list10_main["temp_max"];
    WxForecast[10].Pressure          = list10_main["pressure"];
    WxForecast[10].Humidity          = list10_main["humidity"];
    JsonObject& list10_weather0      = list10["weather"][0];
    const char * list10_forecast     = list10_weather0["main"];        WxForecast[10].Forecast = String(list10_forecast);
    const char * list10_description  = list10_weather0["description"]; WxForecast[10].Description = String(list10_description);
    const char * list10_icon         = list10_weather0["icon"];        WxForecast[10].Icon = String(list10_icon);
    WxForecast[10].Windspeed         = list10["wind"]["speed"];
    WxForecast[10].Winddir           = list10["wind"]["deg"];
    WxForecast[10].Rainfall          = list10["rain"]["3h"];
    const char * list10_period       = list10["dt_txt"];               WxForecast[10].Period = String(list10_period);

    Serial.print("11,");  //---------------------------
    JsonObject& list11               = list[11];
    long list11_dt                   = list11["dt"];
    JsonObject& list11_main          = list11["main"];
    WxForecast[11].Temperature       = list11_main["temp"];
    WxForecast[11].Low               = list11_main["temp_min"];
    WxForecast[11].High              = list11_main["temp_max"];
    WxForecast[11].Pressure          = list11_main["pressure"];
    WxForecast[11].Humidity          = list11_main["humidity"];
    JsonObject& list11_weather0      = list11["weather"][0];
    const char * list11_forecast     = list11_weather0["main"];        WxForecast[11].Forecast = String(list11_forecast);
    const char * list11_description  = list11_weather0["description"]; WxForecast[11].Description = String(list11_description);
    const char * list11_icon         = list11_weather0["icon"];        WxForecast[11].Icon = String(list11_icon);
    WxForecast[11].Windspeed         = list11["wind"]["speed"];
    WxForecast[11].Winddir           = list11["wind"]["deg"];
    WxForecast[11].Rainfall          = list11["rain"]["3h"];
    const char * list11_period       = list11["dt_txt"];               WxForecast[11].Period = String(list11_period);

    Serial.print("12,");  //---------------------------
    JsonObject& list12               = list[12];
    long list12_dt                   = list12["dt"];
    JsonObject& list12_main          = list12["main"];
    WxForecast[12].Temperature       = list12_main["temp"];
    WxForecast[12].Low               = list12_main["temp_min"];
    WxForecast[12].High              = list12_main["temp_max"];
    WxForecast[12].Pressure          = list12_main["pressure"];
    WxForecast[12].Humidity          = list12_main["humidity"];
    JsonObject& list12_weather0      = list12["weather"][0];
    const char * list12_forecast     = list12_weather0["main"];        WxForecast[12].Forecast = String(list12_forecast);
    const char * list12_description  = list12_weather0["description"]; WxForecast[12].Description = String(list12_description);
    const char * list12_icon         = list12_weather0["icon"];        WxForecast[12].Icon = String(list12_icon);
    WxForecast[12].Windspeed         = list12["wind"]["speed"];
    WxForecast[12].Winddir           = list12["wind"]["deg"];
    WxForecast[12].Rainfall          = list12["rain"]["3h"];
    const char * list12_period       = list12["dt_txt"];               WxForecast[12].Period = String(list12_period);

    Serial.print("13,");  //---------------------------
    JsonObject& list13               = list[13];
    long list13_dt                   = list13["dt"];
    JsonObject& list13_main          = list13["main"];
    WxForecast[13].Temperature       = list13_main["temp"];
    WxForecast[13].Low               = list13_main["temp_min"];
    WxForecast[13].High              = list13_main["temp_max"];
    WxForecast[13].Pressure          = list13_main["pressure"];
    WxForecast[13].Humidity          = list13_main["humidity"];
    JsonObject& list13_weather0      = list13["weather"][0];
    const char * list13_forecast     = list13_weather0["main"];        WxForecast[13].Forecast = String(list13_forecast);
    const char * list13_description  = list13_weather0["description"]; WxForecast[13].Description = String(list13_description);
    const char * list13_icon         = list13_weather0["icon"];        WxForecast[13].Icon = String(list13_icon);
    WxForecast[13].Windspeed         = list13["wind"]["speed"];
    WxForecast[13].Winddir           = list13["wind"]["deg"];
    WxForecast[13].Rainfall          = list13["rain"]["3h"];
    const char * list13_period       = list13["dt_txt"];               WxForecast[13].Period = String(list13_period);

    Serial.print("14,");  //---------------------------
    JsonObject& list14               = list[14];
    long list14_dt                   = list14["dt"];
    JsonObject& list14_main          = list14["main"];
    WxForecast[14].Temperature       = list14_main["temp"];
    WxForecast[14].Low               = list14_main["temp_min"];
    WxForecast[14].High              = list14_main["temp_max"];
    WxForecast[14].Pressure          = list14_main["pressure"];
    WxForecast[14].Humidity          = list14_main["humidity"];
    JsonObject& list14_weather0      = list14["weather"][0];
    const char * list14_forecast     = list14_weather0["main"];        WxForecast[14].Forecast = String(list14_forecast);
    const char * list14_description  = list14_weather0["description"]; WxForecast[14].Description = String(list14_description);
    const char * list14_icon         = list14_weather0["icon"];        WxForecast[14].Icon = String(list14_icon);
    WxForecast[14].Windspeed         = list14["wind"]["speed"];
    WxForecast[14].Winddir           = list14["wind"]["deg"];
    WxForecast[14].Rainfall          = list14["rain"]["3h"];
    const char * list14_period       = list14["dt_txt"];               WxForecast[14].Period = String(list14_period);

    Serial.print("15,");  //---------------------------
    JsonObject& list15               = list[15];
    long list15_dt                   = list15["dt"];
    JsonObject& list15_main          = list15["main"];
    WxForecast[15].Temperature       = list15_main["temp"];
    WxForecast[15].Low               = list15_main["temp_min"];
    WxForecast[15].High              = list15_main["temp_max"];
    WxForecast[15].Pressure          = list15_main["pressure"];
    WxForecast[15].Humidity          = list15_main["humidity"];
    JsonObject& list15_weather0      = list15["weather"][0];
    const char * list15_forecast     = list15_weather0["main"];        WxForecast[15].Forecast = String(list15_forecast);
    const char * list15_description  = list15_weather0["description"]; WxForecast[15].Description = String(list15_description);
    const char * list15_icon         = list15_weather0["icon"];        WxForecast[15].Icon = String(list15_icon);
    WxForecast[15].Windspeed         = list15["wind"]["speed"];
    WxForecast[15].Winddir           = list15["wind"]["deg"];
    WxForecast[15].Rainfall          = list15["rain"]["3h"];
    const char * list15_period       = list15["dt_txt"];               WxForecast[15].Period = String(list15_period);

    Serial.print("16,");  //---------------------------
    JsonObject& list16               = list[16];
    long list16_dt                   = list16["dt"];
    JsonObject& list16_main          = list16["main"];
    WxForecast[16].Temperature       = list16_main["temp"];
    WxForecast[16].Low               = list16_main["temp_min"];
    WxForecast[16].High              = list16_main["temp_max"];
    WxForecast[16].Pressure          = list16_main["pressure"];
    WxForecast[16].Humidity          = list16_main["humidity"];
    JsonObject& list16_weather0      = list16["weather"][0];
    const char * list16_forecast     = list16_weather0["main"];        WxForecast[16].Forecast = String(list16_forecast);
    const char * list16_description  = list16_weather0["description"]; WxForecast[16].Description = String(list16_description);
    const char * list16_icon         = list16_weather0["icon"];        WxForecast[16].Icon = String(list16_icon);
    WxForecast[16].Windspeed         = list16["wind"]["speed"];
    WxForecast[16].Winddir           = list16["wind"]["deg"];
    WxForecast[16].Rainfall          = list16["rain"]["3h"];
    const char * list16_period       = list16["dt_txt"];               WxForecast[16].Period = String(list16_period);

    Serial.print("17,");  //---------------------------
    JsonObject& list17               = list[17];
    long list17_dt                   = list17["dt"];
    JsonObject& list17_main          = list17["main"];
    WxForecast[17].Temperature       = list17_main["temp"];
    WxForecast[17].Low               = list17_main["temp_min"];
    WxForecast[17].High              = list17_main["temp_max"];
    WxForecast[17].Pressure          = list17_main["pressure"];
    WxForecast[17].Humidity          = list17_main["humidity"];
    JsonObject& list17_weather0      = list17["weather"][0];
    const char * list17_forecast     = list17_weather0["main"];        WxForecast[17].Forecast = String(list17_forecast);
    const char * list17_description  = list17_weather0["description"]; WxForecast[17].Description = String(list17_description);
    const char * list17_icon         = list17_weather0["icon"];        WxForecast[17].Icon = String(list17_icon);
    WxForecast[17].Windspeed         = list17["wind"]["speed"];
    WxForecast[17].Winddir           = list17["wind"]["deg"];
    WxForecast[17].Rainfall          = list17["rain"]["3h"];
    const char * list17_period       = list17["dt_txt"];               WxForecast[17].Period = String(list17_period);

    Serial.print("18,");  //---------------------------
    JsonObject& list18               = list[18];
    long list18_dt                   = list18["dt"];
    JsonObject& list18_main          = list18["main"];
    WxForecast[18].Temperature       = list18_main["temp"];
    WxForecast[18].Low               = list18_main["temp_min"];
    WxForecast[18].High              = list18_main["temp_max"];
    WxForecast[18].Pressure          = list18_main["pressure"];
    WxForecast[18].Humidity          = list18_main["humidity"];
    JsonObject& list18_weather0      = list18["weather"][0];
    const char * list18_forecast     = list18_weather0["main"];        WxForecast[18].Forecast = String(list18_forecast);
    const char * list18_description  = list18_weather0["description"]; WxForecast[18].Description = String(list18_description);
    const char * list18_icon         = list18_weather0["icon"];        WxForecast[18].Icon = String(list18_icon);
    WxForecast[18].Windspeed         = list18["wind"]["speed"];
    WxForecast[18].Winddir           = list18["wind"]["deg"];
    WxForecast[18].Rainfall          = list18["rain"]["3h"];
    const char * list18_period       = list18["dt_txt"];               WxForecast[18].Period = String(list18_period);

    Serial.print("19,");  //---------------------------
    JsonObject& list19               = list[19];
    long list19_dt                   = list19["dt"];
    JsonObject& list19_main          = list19["main"];
    WxForecast[19].Temperature       = list19_main["temp"];
    WxForecast[19].Low               = list19_main["temp_min"];
    WxForecast[19].High              = list19_main["temp_max"];
    WxForecast[19].Pressure          = list19_main["pressure"];
    WxForecast[19].Humidity          = list19_main["humidity"];
    JsonObject& list19_weather0      = list19["weather"][0];
    const char * list19_forecast     = list19_weather0["main"];        WxForecast[19].Forecast = String(list19_forecast);
    const char * list19_description  = list19_weather0["description"]; WxForecast[19].Description = String(list19_description);
    const char * list19_icon         = list19_weather0["icon"];        WxForecast[19].Icon = String(list19_icon);
    WxForecast[19].Windspeed         = list19["wind"]["speed"];
    WxForecast[19].Winddir           = list19["wind"]["deg"];
    WxForecast[19].Rainfall          = list19["rain"]["3h"];
    const char * list19_period       = list19["dt_txt"];               WxForecast[19].Period = String(list19_period);

    Serial.print("20,");  //---------------------------
    JsonObject& list20               = list[20];
    long list20_dt                   = list20["dt"];
    JsonObject& list20_main          = list20["main"];
    WxForecast[20].Temperature       = list20_main["temp"];
    WxForecast[20].Low               = list20_main["temp_min"];
    WxForecast[20].High              = list20_main["temp_max"];
    WxForecast[20].Pressure          = list20_main["pressure"];
    WxForecast[20].Humidity          = list20_main["humidity"];
    JsonObject& list20_weather0      = list20["weather"][0];
    const char * list20_forecast     = list20_weather0["main"];        WxForecast[20].Forecast = String(list20_forecast);
    const char * list20_description  = list20_weather0["description"]; WxForecast[20].Description = String(list20_description);
    const char * list20_icon         = list20_weather0["icon"];        WxForecast[20].Icon = String(list20_icon);
    WxForecast[20].Windspeed         = list20["wind"]["speed"];
    WxForecast[20].Winddir           = list20["wind"]["deg"];
    WxForecast[20].Rainfall          = list20["rain"]["3h"];
    const char * list20_period       = list20["dt_txt"];               WxForecast[20].Period = String(list20_period);

    Serial.print("21,");  //---------------------------
    JsonObject& list21               = list[21];
    long list21_dt                   = list21["dt"];
    JsonObject& list21_main          = list21["main"];
    WxForecast[21].Temperature       = list21_main["temp"];
    WxForecast[21].Low               = list21_main["temp_min"];
    WxForecast[21].High              = list21_main["temp_max"];
    WxForecast[21].Pressure          = list21_main["pressure"];
    WxForecast[21].Humidity          = list21_main["humidity"];
    JsonObject& list21_weather0      = list21["weather"][0];
    const char * list21_forecast     = list21_weather0["main"]; WxForecast[21].Forecast = String(list21_forecast);
    const char * list21_description  = list21_weather0["description"]; WxForecast[21].Description = String(list21_description);
    const char * list21_icon         = list21_weather0["icon"]; WxForecast[21].Icon = String(list21_icon);
    WxForecast[21].Windspeed         = list21["wind"]["speed"];
    WxForecast[21].Winddir           = list21["wind"]["deg"];
    WxForecast[21].Rainfall          = list21["rain"]["3h"];
    const char * list21_period       = list21["dt_txt"]; WxForecast[21].Period = String(list21_period);

    Serial.print("22,");  //---------------------------
    JsonObject& list22               = list[22];
    long list22_dt                   = list22["dt"];
    JsonObject& list22_main          = list22["main"];
    WxForecast[22].Temperature       = list22_main["temp"];
    WxForecast[22].Low               = list22_main["temp_min"];
    WxForecast[22].High              = list22_main["temp_max"];
    WxForecast[22].Pressure          = list22_main["pressure"];
    WxForecast[22].Humidity          = list22_main["humidity"];
    JsonObject& list22_weather0      = list22["weather"][0];
    const char * list22_forecast     = list22_weather0["main"]; WxForecast[22].Forecast = String(list22_forecast);
    const char * list22_description  = list22_weather0["description"]; WxForecast[22].Description = String(list22_description);
    const char * list22_icon         = list22_weather0["icon"]; WxForecast[22].Icon = String(list22_icon);
    WxForecast[22].Windspeed         = list22["wind"]["speed"];
    WxForecast[22].Winddir           = list22["wind"]["deg"];
    WxForecast[22].Rainfall          = list22["rain"]["3h"];
    const char * list22_period       = list22["dt_txt"]; WxForecast[22].Period = String(list22_period);

    Serial.print("23,");  //---------------------------
    JsonObject& list23               = list[23];
    long list23_dt                   = list23["dt"];
    JsonObject& list23_main          = list23["main"];
    WxForecast[23].Temperature       = list23_main["temp"];
    WxForecast[23].Low               = list23_main["temp_min"];
    WxForecast[23].High              = list23_main["temp_max"];
    WxForecast[23].Pressure          = list23_main["pressure"];
    WxForecast[23].Humidity          = list23_main["humidity"];
    JsonObject& list23_weather0      = list23["weather"][0];
    const char * list23_forecast     = list23_weather0["main"]; WxForecast[23].Forecast = String(list23_forecast);
    const char * list23_description  = list23_weather0["description"]; WxForecast[23].Description = String(list23_description);
    const char * list23_icon         = list23_weather0["icon"]; WxForecast[23].Icon = String(list23_icon);
    WxForecast[23].Windspeed         = list23["wind"]["speed"];
    WxForecast[23].Winddir           = list23["wind"]["deg"];
    WxForecast[23].Rainfall          = list23["rain"]["3h"];
    const char * list23_period       = list23["dt_txt"]; WxForecast[23].Period = String(list23_period);

    Serial.print("24,");  //---------------------------
    JsonObject& list24               = list[24];
    long list24_dt                   = list24["dt"];
    JsonObject& list24_main          = list24["main"];
    WxForecast[24].Temperature       = list24_main["temp"];
    WxForecast[24].Low               = list24_main["temp_min"];
    WxForecast[24].High              = list24_main["temp_max"];
    WxForecast[24].Pressure          = list24_main["pressure"];
    WxForecast[24].Humidity          = list24_main["humidity"];
    JsonObject& list24_weather0      = list24["weather"][0];
    const char * list24_forecast     = list24_weather0["main"]; WxForecast[24].Forecast = String(list24_forecast);
    const char * list24_description  = list24_weather0["description"]; WxForecast[24].Description = String(list24_description);
    const char * list24_icon         = list24_weather0["icon"]; WxForecast[24].Icon = String(list24_icon);
    WxForecast[24].Windspeed         = list24["wind"]["speed"];
    WxForecast[24].Winddir           = list24["wind"]["deg"];
    WxForecast[24].Rainfall          = list24["rain"]["3h"];
    const char * list24_period       = list24["dt_txt"]; WxForecast[24].Period = String(list24_period);

    Serial.print("25,");  //---------------------------
    JsonObject& list25               = list[25];
    long list25_dt                   = list25["dt"];
    JsonObject& list25_main          = list25["main"];
    WxForecast[25].Temperature       = list25_main["temp"];
    WxForecast[25].Low               = list25_main["temp_min"];
    WxForecast[25].High              = list25_main["temp_max"];
    WxForecast[25].Pressure          = list25_main["pressure"];
    WxForecast[25].Humidity          = list25_main["humidity"];
    JsonObject& list25_weather0      = list25["weather"][0];
    const char * list25_forecast     = list25_weather0["main"]; WxForecast[25].Forecast = String(list25_forecast);
    const char * list25_description  = list25_weather0["description"]; WxForecast[25].Description = String(list25_description);
    const char * list25_icon         = list25_weather0["icon"]; WxForecast[25].Icon = String(list25_icon);
    WxForecast[25].Windspeed         = list25["wind"]["speed"];
    WxForecast[25].Winddir           = list25["wind"]["deg"];
    WxForecast[25].Rainfall          = list25["rain"]["3h"];
    const char * list25_period       = list25["dt_txt"]; WxForecast[25].Period = String(list25_period);

    Serial.print("26,");  //---------------------------
    JsonObject& list26               = list[26];
    long list26_dt                   = list26["dt"];
    JsonObject& list26_main          = list26["main"];
    WxForecast[26].Temperature       = list26_main["temp"];
    WxForecast[26].Low               = list26_main["temp_min"];
    WxForecast[26].High              = list26_main["temp_max"];
    WxForecast[26].Pressure          = list26_main["pressure"];
    WxForecast[26].Humidity          = list26_main["humidity"];
    JsonObject& list26_weather0      = list26["weather"][0];
    const char * list26_forecast     = list26_weather0["main"]; WxForecast[26].Forecast = String(list26_forecast);
    const char * list26_description  = list26_weather0["description"]; WxForecast[26].Description = String(list26_description);
    const char * list26_icon         = list26_weather0["icon"]; WxForecast[26].Icon = String(list26_icon);
    WxForecast[26].Windspeed         = list26["wind"]["speed"];
    WxForecast[26].Winddir           = list26["wind"]["deg"];
    WxForecast[26].Rainfall          = list26["rain"]["3h"];
    const char * list26_period       = list26["dt_txt"]; WxForecast[26].Period = String(list26_period);

    Serial.print("27,");  //---------------------------
    JsonObject& list27               = list[27];
    long list27_dt                   = list27["dt"];
    JsonObject& list27_main          = list27["main"];
    WxForecast[27].Temperature       = list27_main["temp"];
    WxForecast[27].Low               = list27_main["temp_min"];
    WxForecast[27].High              = list27_main["temp_max"];
    WxForecast[27].Pressure          = list27_main["pressure"];
    WxForecast[27].Humidity          = list27_main["humidity"];
    JsonObject& list27_weather0      = list27["weather"][0];
    const char * list27_forecast     = list27_weather0["main"]; WxForecast[27].Forecast = String(list27_forecast);
    const char * list27_description  = list27_weather0["description"]; WxForecast[27].Description = String(list27_description);
    const char * list27_icon         = list27_weather0["icon"]; WxForecast[27].Icon = String(list27_icon);
    WxForecast[27].Windspeed         = list27["wind"]["speed"];
    WxForecast[27].Winddir           = list27["wind"]["deg"];
    WxForecast[27].Rainfall          = list27["rain"]["3h"];
    const char * list27_period       = list27["dt_txt"]; WxForecast[27].Period = String(list27_period);

    Serial.print("28,");  //---------------------------
    JsonObject& list28               = list[28];
    long list28_dt                   = list28["dt"];
    JsonObject& list28_main          = list28["main"];
    WxForecast[28].Temperature       = list28_main["temp"];
    WxForecast[28].Low               = list28_main["temp_min"];
    WxForecast[28].High              = list28_main["temp_max"];
    WxForecast[28].Pressure          = list28_main["pressure"];
    WxForecast[28].Humidity          = list28_main["humidity"];
    JsonObject& list28_weather0      = list28["weather"][0];
    const char * list28_forecast     = list28_weather0["main"]; WxForecast[28].Forecast = String(list28_forecast);
    const char * list28_description  = list28_weather0["description"]; WxForecast[28].Description = String(list28_description);
    const char * list28_icon         = list28_weather0["icon"]; WxForecast[28].Icon = String(list28_icon);
    WxForecast[28].Windspeed         = list28["wind"]["speed"];
    WxForecast[28].Winddir           = list28["wind"]["deg"];
    WxForecast[28].Rainfall          = list28["rain"]["3h"];
    const char * list28_period       = list28["dt_txt"]; WxForecast[28].Period = String(list28_period);

    Serial.print("29,");  //---------------------------
    JsonObject& list29               = list[29];
    long list29_dt                   = list29["dt"];
    JsonObject& list29_main          = list29["main"];
    WxForecast[29].Temperature       = list29_main["temp"];
    WxForecast[29].Low               = list29_main["temp_min"];
    WxForecast[29].High              = list29_main["temp_max"];
    WxForecast[29].Pressure          = list29_main["pressure"];
    WxForecast[29].Humidity          = list29_main["humidity"];
    JsonObject& list29_weather0      = list29["weather"][0];
    const char * list29_forecast     = list29_weather0["main"]; WxForecast[29].Forecast = String(list29_forecast);
    const char * list29_description  = list29_weather0["description"]; WxForecast[29].Description = String(list29_description);
    const char * list29_icon         = list29_weather0["icon"]; WxForecast[29].Icon = String(list29_icon);
    WxForecast[29].Windspeed         = list29["wind"]["speed"];
    WxForecast[29].Winddir           = list29["wind"]["deg"];
    WxForecast[29].Rainfall          = list29["rain"]["3h"];
    const char * list29_period       = list29["dt_txt"]; WxForecast[29].Period = String(list29_period);

    Serial.print("30,");  //---------------------------
    JsonObject& list30               = list[30];
    long list30_dt                   = list30["dt"];
    JsonObject& list30_main          = list30["main"];
    WxForecast[30].Temperature       = list30_main["temp"];
    WxForecast[30].Low               = list30_main["temp_min"];
    WxForecast[30].High              = list30_main["temp_max"];
    WxForecast[30].Pressure          = list30_main["pressure"];
    WxForecast[30].Humidity          = list30_main["humidity"];
    JsonObject& list30_weather0      = list30["weather"][0];
    const char * list30_forecast     = list30_weather0["main"]; WxForecast[30].Forecast = String(list30_forecast);
    const char * list30_description  = list30_weather0["description"]; WxForecast[30].Description = String(list30_description);
    const char * list30_icon         = list30_weather0["icon"]; WxForecast[30].Icon = String(list30_icon);
    WxForecast[30].Windspeed         = list30["wind"]["speed"];
    WxForecast[30].Winddir           = list30["wind"]["deg"];
    WxForecast[30].Rainfall          = list30["rain"]["3h"];
    const char * list30_period       = list30["dt_txt"]; WxForecast[30].Period = String(list30_period);

    Serial.print("31,");  //---------------------------
    JsonObject& list31               = list[31];
    long list31_dt                   = list31["dt"];
    JsonObject& list31_main          = list31["main"];
    WxForecast[31].Temperature       = list31_main["temp"];
    WxForecast[31].Low               = list31_main["temp_min"];
    WxForecast[31].High              = list31_main["temp_max"];
    WxForecast[31].Pressure          = list31_main["pressure"];
    WxForecast[31].Humidity          = list31_main["humidity"];
    JsonObject& list31_weather0      = list31["weather"][0];
    const char * list31_forecast     = list31_weather0["main"]; WxForecast[31].Forecast = String(list31_forecast);
    const char * list31_description  = list31_weather0["description"]; WxForecast[31].Description = String(list31_description);
    const char * list31_icon         = list31_weather0["icon"]; WxForecast[31].Icon = String(list31_icon);
    WxForecast[31].Windspeed         = list31["wind"]["speed"];
    WxForecast[31].Winddir           = list31["wind"]["deg"];
    WxForecast[31].Rainfall          = list31["rain"]["3h"];
    const char * list31_period       = list31["dt_txt"]; WxForecast[31].Period = String(list31_period);

    Serial.print("32,");  //---------------------------
    JsonObject& list32               = list[32];
    long list32_dt                   = list32["dt"];
    JsonObject& list32_main          = list32["main"];
    WxForecast[32].Temperature       = list32_main["temp"];
    WxForecast[32].Low               = list32_main["temp_min"];
    WxForecast[32].High              = list32_main["temp_max"];
    WxForecast[32].Pressure          = list32_main["pressure"];
    WxForecast[32].Humidity          = list32_main["humidity"];
    JsonObject& list32_weather0      = list32["weather"][0];
    const char * list32_forecast     = list32_weather0["main"]; WxForecast[32].Forecast = String(list32_forecast);
    const char * list32_description  = list32_weather0["description"]; WxForecast[32].Description = String(list32_description);
    const char * list32_icon         = list32_weather0["icon"]; WxForecast[32].Icon = String(list32_icon);
    WxForecast[32].Windspeed         = list32["wind"]["speed"];
    WxForecast[32].Winddir           = list32["wind"]["deg"];
    WxForecast[32].Rainfall          = list32["rain"]["3h"];
    const char * list32_period       = list32["dt_txt"]; WxForecast[32].Period = String(list32_period);

    Serial.print("33,");  //---------------------------
    JsonObject& list33               = list[33];
    long list33_dt                   = list33["dt"];
    JsonObject& list33_main          = list33["main"];
    WxForecast[33].Temperature       = list33_main["temp"];
    WxForecast[33].Low               = list33_main["temp_min"];
    WxForecast[33].High              = list33_main["temp_max"];
    WxForecast[33].Pressure          = list33_main["pressure"];
    WxForecast[33].Humidity          = list33_main["humidity"];
    JsonObject& list33_weather0      = list33["weather"][0];
    const char * list33_forecast     = list33_weather0["main"]; WxForecast[33].Forecast = String(list33_forecast);
    const char * list33_description  = list33_weather0["description"]; WxForecast[33].Description = String(list33_description);
    const char * list33_icon         = list33_weather0["icon"]; WxForecast[33].Icon = String(list33_icon);
    WxForecast[33].Windspeed         = list33["wind"]["speed"];
    WxForecast[33].Winddir           = list33["wind"]["deg"];
    WxForecast[33].Rainfall          = list33["rain"]["3h"];
    const char * list33_period       = list33["dt_txt"]; WxForecast[33].Period = String(list33_period);

    Serial.print("34,");  //---------------------------
    JsonObject& list34               = list[34];
    long list34_dt                   = list34["dt"];
    JsonObject& list34_main          = list34["main"];
    WxForecast[34].Temperature       = list34_main["temp"];
    WxForecast[34].Low               = list34_main["temp_min"];
    WxForecast[34].High              = list34_main["temp_max"];
    WxForecast[34].Pressure          = list34_main["pressure"];
    WxForecast[34].Humidity          = list34_main["humidity"];
    JsonObject& list34_weather0      = list34["weather"][0];
    const char * list34_forecast     = list34_weather0["main"]; WxForecast[34].Forecast = String(list34_forecast);
    const char * list34_description  = list34_weather0["description"]; WxForecast[34].Description = String(list34_description);
    const char * list34_icon         = list34_weather0["icon"]; WxForecast[34].Icon = String(list34_icon);
    WxForecast[34].Windspeed         = list34["wind"]["speed"];
    WxForecast[34].Winddir           = list34["wind"]["deg"];
    WxForecast[34].Rainfall          = list34["rain"]["3h"];
    const char * list34_period       = list34["dt_txt"]; WxForecast[34].Period = String(list34_period);

    Serial.print("35,");  //---------------------------
    JsonObject& list35               = list[35];
    long list35_dt                   = list35["dt"];
    JsonObject& list35_main          = list35["main"];
    WxForecast[35].Temperature       = list35_main["temp"];
    WxForecast[35].Low               = list35_main["temp_min"];
    WxForecast[35].High              = list35_main["temp_max"];
    WxForecast[35].Pressure          = list35_main["pressure"];
    WxForecast[35].Humidity          = list35_main["humidity"];
    JsonObject& list35_weather0      = list35["weather"][0];
    const char * list35_forecast     = list35_weather0["main"]; WxForecast[35].Forecast = String(list35_forecast);
    const char * list35_description  = list35_weather0["description"]; WxForecast[35].Description = String(list35_description);
    const char * list35_icon         = list35_weather0["icon"]; WxForecast[35].Icon = String(list35_icon);
    WxForecast[35].Windspeed         = list35["wind"]["speed"];
    WxForecast[35].Winddir           = list35["wind"]["deg"];
    WxForecast[35].Rainfall          = list35["rain"]["3h"];
    const char * list35_period       = list35["dt_txt"]; WxForecast[35].Period = String(list35_period);
    
    Serial.print("36,");  //---------------------------
    JsonObject& list36               = list[36];
    long list36_dt                   = list36["dt"];
    JsonObject& list36_main          = list36["main"];
    WxForecast[36].Temperature       = list36_main["temp"];
    WxForecast[36].Low               = list36_main["temp_min"];
    WxForecast[36].High              = list36_main["temp_max"];
    WxForecast[36].Pressure          = list36_main["pressure"];
    WxForecast[36].Humidity          = list36_main["humidity"];
    JsonObject& list36_weather0      = list36["weather"][0];
    const char * list36_forecast     = list36_weather0["main"]; WxForecast[36].Forecast = String(list36_forecast);
    const char * list36_description  = list36_weather0["description"]; WxForecast[36].Description = String(list36_description);
    const char * list36_icon         = list36_weather0["icon"]; WxForecast[36].Icon = String(list36_icon);
    WxForecast[36].Windspeed         = list36["wind"]["speed"];
    WxForecast[36].Winddir           = list36["wind"]["deg"];
    WxForecast[36].Rainfall          = list36["rain"]["3h"];
    const char * list36_period       = list36["dt_txt"]; WxForecast[36].Period = String(list36_period);

    Serial.print("37,");  //---------------------------
    JsonObject& list37               = list[37];
    long list37_dt                   = list37["dt"];
    JsonObject& list37_main          = list37["main"];
    WxForecast[37].Temperature       = list37_main["temp"];
    WxForecast[37].Low               = list37_main["temp_min"];
    WxForecast[37].High              = list37_main["temp_max"];
    WxForecast[37].Pressure          = list37_main["pressure"];
    WxForecast[37].Humidity          = list37_main["humidity"];
    JsonObject& list37_weather0      = list37["weather"][0];
    const char * list37_forecast     = list37_weather0["main"]; WxForecast[37].Forecast = String(list37_forecast);
    const char * list37_description  = list37_weather0["description"]; WxForecast[37].Description = String(list37_description);
    const char * list37_icon         = list37_weather0["icon"]; WxForecast[37].Icon = String(list37_icon);
    WxForecast[37].Windspeed         = list37["wind"]["speed"];
    WxForecast[37].Winddir           = list37["wind"]["deg"];
    WxForecast[37].Rainfall          = list37["rain"]["3h"];
    const char * list37_period       = list37["dt_txt"]; WxForecast[37].Period = String(list37_period);
    
    Serial.print("38,");  //---------------------------
    JsonObject& list38               = list[38];
    long list38_dt                   = list38["dt"];
    JsonObject& list38_main          = list38["main"];
    WxForecast[38].Temperature       = list38_main["temp"];
    WxForecast[38].Low               = list38_main["temp_min"];
    WxForecast[38].High              = list38_main["temp_max"];
    WxForecast[38].Pressure          = list38_main["pressure"];
    WxForecast[38].Humidity          = list38_main["humidity"];
    JsonObject& list38_weather0      = list38["weather"][0];
    const char * list38_forecast     = list38_weather0["main"]; WxForecast[38].Forecast = String(list38_forecast);
    const char * list38_description  = list38_weather0["description"]; WxForecast[38].Description = String(list38_description);
    const char * list38_icon         = list38_weather0["icon"]; WxForecast[38].Icon = String(list38_icon);
    WxForecast[38].Windspeed         = list38["wind"]["speed"];
    WxForecast[38].Winddir           = list38["wind"]["deg"];
    WxForecast[38].Rainfall          = list38["rain"]["3h"];
    const char * list38_period       = list38["dt_txt"]; WxForecast[38].Period = String(list38_period);
    
    Serial.print("39,"); //------------------------------------------------
    JsonObject& list39               = list[39];
    long list39_dt                   = list39["dt"]; 
    JsonObject& list39_main          = list39["main"];
    WxForecast[39].Temperature       = list39_main["temp"];
    WxForecast[39].Low               = list39_main["temp_min"];
    WxForecast[39].High              = list39_main["temp_max"];
    WxForecast[39].Pressure          = list39_main["pressure"];
    WxForecast[39].Humidity          = list39_main["humidity"]; 
    JsonObject& list39_weather0      = list39["weather"][0];
    const char * list39_forecast     = list39_weather0["main"]; WxForecast[39].Forecast = String(list39_forecast);
    const char * list39_description  = list39_weather0["description"]; WxForecast[39].Description = String(list39_description);
    const char * list39_icon         = list39_weather0["icon"]; WxForecast[39].Icon = String(list39_icon);
    WxForecast[39].Windspeed         = list39["wind"]["speed"]; 
    WxForecast[39].Winddir           = list39["wind"]["deg"]; 
    WxForecast[39].Rainfall          = list39["rain"]["3h"]; 
    const char * list39_period       = list39["dt_txt"]; WxForecast[39].Period = String(list39_period);
/*
    for (int i = 0; i < 40; i++){
      Serial.println("\nPeriod-"+String(i));
      Serial.println(WxForecast[i].Temperature);
      Serial.println(WxForecast[i].Low);
      Serial.println(WxForecast[i].High);
      Serial.println(WxForecast[i].Pressure);
      Serial.println(WxForecast[i].Humidity);
      Serial.println(WxForecast[i].Forecast);
      Serial.println(WxForecast[i].Description);
      Serial.println(WxForecast[i].Icon);
      Serial.println(WxForecast[i].Windspeed);
      Serial.println(WxForecast[i].Winddir);
      Serial.println(WxForecast[i].Rainfall);
      Serial.println(WxForecast[i].Period);
    }    
*/
    //------------------------------------------
    float pressure_trend = WxForecast[1].Pressure - WxForecast[5].Pressure; // Measure pressure slope between ~now and later
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0; // Remove any small variations less than 0.1
    WxConditions[0].Trend = "0";
    if (pressure_trend > 0)  WxConditions[0].Trend = "+";
    if (pressure_trend < 0)  WxConditions[0].Trend = "-";
    if (pressure_trend == 0) WxConditions[0].Trend = "0";

    if (Units == "I") Convert_Readings_to_Imperial();
  }
  return true;
}
//#########################################################################################
void Convert_Readings_to_Imperial() {
  WxConditions[0].Temperature = WxConditions[0].Temperature * 9 / 5.0 + 32;
  WxConditions[0].High        = WxConditions[0].High * 9 / 5.0 + 32;
  WxConditions[0].Low         = WxConditions[0].Low * 9 / 5.0 + 32;
  WxConditions[0].Pressure    = WxConditions[0].Pressure * 9 / 5.0 + 32;
  WxForecast[1].Rainfall      = WxForecast[1].Rainfall * 9 / 5.0 + 32;
  WxConditions[0].Windspeed   = WxConditions[0].Windspeed * 9 / 5.0 + 32;
  WxForecast[1].Low           = WxForecast[1].Low * 1.8 + 32;
  WxForecast[1].High          = WxForecast[1].High * 1.8 + 32;
  WxForecast[2].Low           = WxForecast[2].Low * 1.8 + 32;
  WxForecast[2].High          = WxForecast[2].High * 1.8 + 32;
  WxForecast[3].Low           = WxForecast[3].Low * 1.8 + 32;
  WxForecast[3].High          = WxForecast[3].High * 1.8 + 32;
}
//#########################################################################################
int StartWiFi() {
  int connAttempts = 0;
  Serial.print(F("\r\nConnecting to: ")); Serial.println(String(ssid1));
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid1, password1);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500); Serial.print(".");
    if (connAttempts > 20) {
      WiFi.disconnect();
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid2, password2);
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
void SetupTime() {
  configTime(0, 0, "0.uk.pool.ntp.org", "time.nist.gov");
  setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 1);
  delay(500);
  UpdateLocalTime();
}
//#########################################################################################
void UpdateLocalTime() {
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println(F("Failed to obtain time"));
  }
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");     // Displays: Saturday, June 24 2017 14:05:49
  Serial.println(&timeinfo, "%H:%M:%S");                     // Displays: 14:05:49
  char output[30], day_output[30];
  if (Units == "M") {
    strftime(day_output, 30, "%a  %d-%m-%y", &timeinfo);     // Displays: Sat 24-Jun-17
    strftime(output, 30, "(@ %H:%M:%S )", &timeinfo); // Creates: '@ 14:05:49'
  }
  else {
    strftime(day_output, 30, "%a  %m-%d-%y", &timeinfo);     // Creates: Sat Jun-24-17
    strftime(output, 30, "(@ %r )", &timeinfo);       // Creates: '@ 2:05:49pm'
  }
  Day_time_str = day_output;
  time_str     = output;
}
//#########################################################################################
String ConvertUnixTime(int unix_time) {
  struct tm *now_tm;
  int hour, min, second, day, month, year, wday;
  // timeval tv = {unix_time,0};
  time_t tm = unix_time;
  now_tm = localtime(&tm);
  hour   = now_tm->tm_hour;
  min    = now_tm->tm_min;
  second = now_tm->tm_sec;
  wday   = now_tm->tm_wday;
  day    = now_tm->tm_mday;
  month  = now_tm->tm_mon + 1;
  year   = 1900 + now_tm->tm_year; // To get just YY information
  MoonDay   = day;
  MoonMonth = month;
  MoonYear  = year;
  if (Units == "M") {
    time_str =  (hour < 10 ? "0" + String(hour) : String(hour)) + ":" + (min < 10 ? "0" + String(min) : String(min)) + ":" + "  ";                     // HH:MM   05/07/17
    time_str += (day < 10 ? "0" + String(day) : String(day)) + "/" + (month < 10 ? "0" + String(month) : String(month)) + "/" + (year < 10 ? "0" + String(year) : String(year)); // HH:MM   05/07/17
  }
  else {
    String ampm = "am";
    if (hour > 11) ampm = "pm";
    hour = hour % 12; if (hour == 0) hour = 12;
    time_str =  (hour % 12 < 10 ? "0" + String(hour % 12) : String(hour % 12)) + ":" + (min < 10 ? "0" + String(min) : String(min)) + ampm + " ";      // HH:MMam 07/05/17
    time_str += (month < 10 ? "0" + String(month) : String(month)) + "/" + (day < 10 ? "0" + String(day) : String(day)) + "/" + "/" + (year < 10 ? "0" + String(year) : String(year)); // HH:MMpm 07/05/17
  }
  // Returns either '21:12  ' or ' 09:12pm' depending on Units
  //Serial.println(time_str);
  return time_str;
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  gfx.fillCircle(x - scale * 3, y, scale);                       // Left most circle
  gfx.fillCircle(x + scale * 3, y, scale);                       // Right most circle
  gfx.fillCircle(x - scale, y - scale, scale * 1.4);             // left middle upper circle
  gfx.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75); // Right middle upper circle
  gfx.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1); // Upper and lower lines
  //Clear cloud inner
  gfx.setColor(EPD_WHITE);
  gfx.fillCircle(x - scale * 3, y, scale - linesize);            // Clear left most circle
  gfx.fillCircle(x + scale * 3, y, scale - linesize);            // Clear right most circle
  gfx.fillCircle(x - scale, y - scale, scale * 1.4 - linesize);  // left middle upper circle
  gfx.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize); // Right middle upper circle
  gfx.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2); // Upper and lower lines
  gfx.setColor(EPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 6; i++) {
    gfx.drawLine(x - scale * 4 + scale * i * 1.3 + 0, y + scale * 1.9, x - scale * 3.5 + scale * i * 1.3 + 0, y + scale);
    if (scale != Small) {
      gfx.drawLine(x - scale * 4 + scale * i * 1.3 + 1, y + scale * 1.9, x - scale * 3.5 + scale * i * 1.3 + 1, y + scale);
      gfx.drawLine(x - scale * 4 + scale * i * 1.3 + 2, y + scale * 1.9, x - scale * 3.5 + scale * i * 1.3 + 2, y + scale);
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
      gfx.drawLine(dxo + x + 0 + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2);
    }
  }
}
//#########################################################################################
void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 5; i++) {
    gfx.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale);
    if (scale != Small) {
      gfx.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale);
      gfx.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale);
    }
    gfx.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0);
    if (scale != Small) {
      gfx.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1);
      gfx.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2);
    }
    gfx.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5);
    if (scale != Small) {
      gfx.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5);
      gfx.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5);
    }
  }
}
//#########################################################################################
void addsun(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  int dxo, dyo, dxi, dyi;
  gfx.fillCircle(x, y, scale);
  gfx.setColor(EPD_WHITE);
  gfx.fillCircle(x, y, scale - linesize);
  gfx.setColor(EPD_BLACK);
  for (float i = 0; i < 360; i = i + 45) {
    dxo = 2.2 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.6;
    dyo = 2.2 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.6;
    if (i == 0   || i == 180) {
      gfx.drawLine(dxo + x - 1, dyo + y, dxi + x - 1, dyi + y);
      if (scale != Small) {
        gfx.drawLine(dxo + x + 0, dyo + y, dxi + x + 0, dyi + y);
        gfx.drawLine(dxo + x + 1, dyo + y, dxi + x + 1, dyi + y);
      }
    }
    if (i == 90  || i == 270) {
      gfx.drawLine(dxo + x, dyo + y - 1, dxi + x, dyi + y - 1);
      if (scale != Small) {
        gfx.drawLine(dxo + x, dyo + y + 0, dxi + x, dyi + y + 0);
        gfx.drawLine(dxo + x, dyo + y + 1, dxi + x, dyi + y + 1);
      }
    }
    if (i == 45  || i == 135 || i == 225 || i == 315) {
      gfx.drawLine(dxo + x - 1, dyo + y, dxi + x - 1, dyi + y);
      if (scale != Small) {
        gfx.drawLine(dxo + x + 0, dyo + y, dxi + x + 0, dyi + y);
        gfx.drawLine(dxo + x + 1, dyo + y, dxi + x + 1, dyi + y);
      }
    }
  }
}
//#########################################################################################
void addfog(int x, int y, int scale, int linesize) {
  if (scale == Small) y -= 10;
  if (scale == Small) linesize = 1;
  for (int i = 0; i < 6; i++) {
    gfx.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize);
    gfx.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize);
    gfx.fillRect(x - scale * 3, y + scale * 2.7, scale * 6, linesize);
  }
}
//#########################################################################################
void MostlyCloudy(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void MostlySunny(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
}
//#########################################################################################
void Rain(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale);
}
//#########################################################################################
void ProbRain(int x, int y, int scale) {
  x = x + 20;
  y = y + 15;
  addcloud(x, y, scale, 1);
  y = y + scale / 2;
  for (int i = 0; i < 6; i++) {
    gfx.drawLine(x - scale * 4 + scale * i * 1.3 + 0, y + scale * 1.9, x - scale * 3.5 + scale * i * 1.3 + 0, y + scale);
  }
}
//#########################################################################################
void Cloudy(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void Sunny(int x, int y, int scale) {
  scale = scale * 1.5;
  addsun(x, y, scale);
}
//#########################################################################################
void ExpectRain(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale);
}
//#########################################################################################
void ChanceRain(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale);
}
//#########################################################################################
void Tstorms(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  addcloud(x, y, scale, linesize);
  addtstorm(x, y, scale);
}
//#########################################################################################
void Snow(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  addcloud(x, y, scale, linesize);
  addsnow(x, y, scale);
}
//#########################################################################################
void Fog(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  addcloud(x, y, scale, linesize);
  addfog(x, y, scale, linesize);
}
//#########################################################################################
void Haze(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  addsun(x, y, scale);
  addfog(x, y, scale, linesize);
}
//#########################################################################################
void Nodata(int x, int y, int scale) {
  if (scale > 1) gfx.setFont(ArialMT_Plain_24); else gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(x - 10, y, "N/A");
}
//#########################################################################################
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.485;
  if (voltage > 4.21) percentage = 100;
  else if (voltage < 3.20) percentage = 0;
  else percentage = (voltage - 3.20) * 100 / (4.21 - 3.20);
  gfx.setColor(EPD_BLACK);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(x - 25, y, String(voltage, 2) + "V");
  gfx.drawRect(x - 22, y + 2, 19, 10);
  gfx.fillRect(x - 2, y + 4, 2, 6);
  gfx.fillRect(x - 20, y + 4, 16 * percentage / 100, 6);
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
#define yticks 5                  // 5 y-axis division markers
  int maxYscale = -10000;
  int minYscale =  10000;
  int last_x, last_y;
  int x1, y1, x2, y2;
  if (auto_scale == true) {
    for (int i = 1; i <= readings; i++ ) {
      if (DataArray[i] >= maxYscale) maxYscale = DataArray[i];
      if (DataArray[i] <= minYscale) minYscale = DataArray[i];
    }
    maxYscale = round(maxYscale + auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Max
    Y1Max = round(maxYscale+0.5);
    if (minYscale != 0) minYscale = round(minYscale - auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Min
    Y1Min = round(minYscale);
  }
  // Draw the graph
  last_x = x_pos + 1;
  last_y = y_pos + (Y1Max - constrain(DataArray[1], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;
  gfx.setColor(EPD_BLACK);
  gfx.drawRect(x_pos, y_pos, gwidth + 2, gheight + 2);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x_pos + gwidth / 2, y_pos - 17, title);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  // Draw the data
  for (int gx = 1; gx <= readings; gx++) {
    if (gx <= readings) {
      x1 = last_x;
      y1 = last_y;
      x2 = x_pos + gx * gwidth / readings; // max_readings is the global variable that sets the maximum data that can be plotted
      y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 3;
      if (barchart_mode) {
        gfx.fillRect(x2, y2, gwidth / readings+2, y_pos + gheight - y2 + 1);
      } else {
        gfx.drawLine(last_x, last_y, x2, y2);
      }
      last_x = x2;
      last_y = y2;
    }
  }
  //Draw the Y-axis scale
  for (int spacing = 0; spacing <= yticks; spacing++) {
  #define number_of_dashes 20
    for (int j = 0; j < number_of_dashes; j++) { // Draw dashed graph grid lines
      if (spacing < yticks) gfx.drawHorizontalLine((x_pos + 1 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / yticks), gwidth / (2 * number_of_dashes));
    }
    if ( (Y1Max - (float)(Y1Max - Y1Min) / yticks * spacing) < 10) gfx.drawString(x_pos - 2, y_pos + gheight * spacing / yticks - 5, String((Y1Max - (float)(Y1Max - Y1Min) / yticks * spacing+0.01), 1));
    else  gfx.drawString(x_pos - 2, y_pos + gheight * spacing / yticks - 5, String((Y1Max - (float)(Y1Max - Y1Min) / yticks * spacing + 0.01), 0)); // +0.01 prevents -0.00 occurring
  }
  for (int i = 0; i <= 5; i++) {
    gfx.drawString(5 + x_pos + gwidth / 5 * i, y_pos + gheight + 3, String(i));
  }
}

