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
#include "owm_credentials2.h"  // See 'owm_credentials' tab and enter your OWM API key and set the Wifi SSID and PASSWORD
#include <ArduinoJson.h>       // https://github.com/bblanchon/ArduinoJson this version requires v 6 or above
#include <WiFi.h>              // Built-in
#include "time.h"              // Built-in
#include <SPI.h>               // Built-in 
#include "EPD_WaveShare.h"     // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "EPD_WaveShare_75.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "MiniGrafx.h"         // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "DisplayDriver.h"     // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "ArialRounded.h"      // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx

#define SCREEN_WIDTH  640.0    // Set for landscape mode, don't remove the decimal place!
#define SCREEN_HEIGHT 384.0
#define BITS_PER_PIXEL 1
#define EPD_BLACK 0
#define EPD_WHITE 1
uint16_t palette[] = { 0, 1 };

// pins_arduino.h, e.g. LOLIN32 LITE
static const uint8_t EPD_BUSY = 4;
static const uint8_t EPD_SS   = 5;
static const uint8_t EPD_RST  = 16;
static const uint8_t EPD_DC   = 17;
static const uint8_t EPD_SCK  = 18;
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23;

EPD_WaveShare75 epd(EPD_SS, EPD_RST, EPD_DC, EPD_BUSY);
MiniGrafx gfx = MiniGrafx(&epd, BITS_PER_PIXEL, palette);

//################  VERSION  ##########################
String version = "8.0";      // Version of this program
//################ VARIABLES ##########################

const unsigned long long UpdateInterval = (30LL * 60LL - 03) * 1000000LL; // Update delay in microseconds, 3-secs is the time to update so compensate for that
bool LargeIcon =  true;
bool SmallIcon =  false;
#define Large  14
#define Small  4
String time_str, date_str, rxtext; // strings to hold time and received weather data;
int    wifi_signal,wifisection, displaysection, MoonDay, MoonMonth, MoonYear, start_time, wakeup_hour;
int    Sunrise, Sunset;

//################ PROGRAM VARIABLES and OBJECTS ################

typedef struct { // For current Day and Day 1, 2, 3, etc
  String   Dt;
  String   Period;
  String   Icon;
  String   Trend;
  String   Main0;
  String   Forecast0;
  String   Forecast1;
  String   Forecast2;
  String   Description;
  String   Time;
  String   Country;
  float    lat;
  float    lon;
  float    Temperature;
  float    Humidity;
  float    High;
  float    Low;
  float    Winddir;
  float    Windspeed;
  float    Rainfall;
  float    Snowfall;
  float    Pressure;
  int      Cloudcover;
  int      Visibility;
  int      Sunrise;
  int      Sunset;
} Forecast_record_type;

#define max_readings 24

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings]    = {0};
float rain_readings[max_readings]        = {0};
float snow_readings[max_readings]        = {0};

WiFiClient client; // wifi client object

//#########################################################################################
void setup() {
  start_time = millis();
  Serial.begin(115200);
  StartWiFi();
  wifi_signal = WiFi_Signal();
  SetupTime();
  bool Received_WxData_OK = false;
  if (wakeup_hour >=8 && wakeup_hour <=23) {
   Received_WxData_OK = (obtain_wx_data("weather") && obtain_wx_data("forecast"));
  }
  // Now only refresh the screen if all the data was received OK, otherwise wait until the next timed check otherwise wait until the next timed check
  if (Received_WxData_OK) {
    StopWiFi(); // Reduces power consumption
    gfx.init();
    gfx.setRotation(0);
    gfx.setColor(EPD_BLACK);
    gfx.fillBuffer(EPD_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    Display_Weather();
    gfx.commit();
    delay(1000);
    Serial.println("Total time to update = "+String(millis()-start_time)+"mS");
  }
  begin_sleep();
}
//#########################################################################################
void loop() { // this will never run!
}
//#########################################################################################
void begin_sleep(){
  esp_sleep_enable_timer_wakeup(UpdateInterval);
  #ifdef BUILTIN_LED
    pinMode(BUILTIN_LED,INPUT);     // In case it's on, turn output off, sometimes PIN-5 on some boards is used for SPI-SS
    digitalWrite(BUILTIN_LED,HIGH); // In case it's on, turn LED off, as sometimes PIN-5 on some boards is used for SPI-SS
  #endif
  Serial.println("Awake for : "+String(millis()-start_time)+"mS");
  Serial.println(F("Starting deep-sleep period..."));
  delay(200);
  esp_deep_sleep_start();           // Sleep for e.g. 30 minutes
}
//#########################################################################################
void Display_Weather() {                         // 7.5" e-paper display is 640x384 resolution
  Display_GeneralInfo_Section();                 // Top line of the display
  Display_Wind_Section(87,117, WxConditions[0].Winddir, WxConditions[0].Windspeed, 65);
  Display_Main_Weather_Section(241, 80);         // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
  Display_Forecast_Section(174, 196);            // 3hr forecast boxes
  Display_Astronomy_Section(0, 196);             // Astronomy section Sun rise/set, Moon phase and Moon icon
  Display_Status_Section(548, 170, wifi_signal); // Wi-Fi signal strength and Battery voltage
}
//#########################################################################################
void Display_GeneralInfo_Section() {
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(5,0,"[ Version: "+version+" ]"); // Programme version
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(SCREEN_WIDTH / 2, -2, City);
  gfx.drawString(415, 153, date_str);
  gfx.drawString(415, 175, time_str);
  gfx.drawLine(0,15, SCREEN_WIDTH, 15);
}
//#########################################################################################
void Display_Main_Weather_Section(int x, int y) {
  gfx.drawRect(x-67, y-65, 140,182);
  gfx.drawRect(x+74, y-65, 100,80); // temp outline
  gfx.drawRect(x+175,y-65, 90,80);  // pressure outline
  gfx.drawRect(x+266,y-65, 130,80); // precipitation outline
  gfx.drawRect(x+74, y+14, 322,51); // forecast text outline
  gfx.drawLine(0,30,SCREEN_WIDTH,30);
  Display_Conditions_Section(x+2, y+35, WxConditions[0].Icon, LargeIcon);
  Display_Pressure_Section(x+220, y-40, WxConditions[0].Pressure, WxConditions[0].Trend);
  Display_Precip_Section(x+330, y-40);
  Display_ForecastText_Section(x + 80, y + 17);
  Display_Temperature_Section(x+125,y-64);
}
//#########################################################################################
String TitleCase(String text) { // Not currently used
  if (text.length() > 0) {
    String temp_text = text.substring(0, 1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1); // Title-case the string
  }
  return "";
}
//#########################################################################################
void Display_Temperature_Section(int x, int y){
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x, y, "Temperatures");
  gfx.drawString(x,y+63, String(WxConditions[0].High,0) + "° | " + String(WxConditions[0].Low,0) + "°"); // Show forecast high and Low
  gfx.setFont(ArialRoundedMTBold_36);
  gfx.drawString(x,y+20, String(WxConditions[0].Temperature,1)); // Show current Temperature
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);  
  gfx.drawString(x+String(WxConditions[0].Temperature,1).length()*19/2,y+25,"°"); // Add in smaller Temperature unit
  gfx.drawString(x+33,y+60,Units=="M"?"°C":"°F"); // Add in smaller Temperature unit
  gfx.drawRect(x+32,y+60,17,19);
}
//#########################################################################################
void Display_Forecast_Section(int x, int y) {
  gfx.setFont(ArialMT_Plain_10);
  Display_Forecast_Weather(x, y, 0);
  Display_Forecast_Weather(x, y, 1);
  Display_Forecast_Weather(x, y, 2);
  Display_Forecast_Weather(x, y, 3);
  Display_Forecast_Weather(x, y, 4);
  Display_Forecast_Weather(x, y, 5);
  Display_Forecast_Weather(x, y, 6);
  Display_Forecast_Weather(x, y, 7);
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
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  int gwidth = 120;
  int gheight = 58;
  int gx = (SCREEN_WIDTH - gwidth*4)/5 + 3;
  int gy = 305;
  int gap = gwidth + gx;
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(SCREEN_WIDTH/2, gy-35, "3-Day Forecast Values"); // Based on a graph height of 60
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  DrawGraph(gx+0*gap, gy, gwidth, gheight, 900,1050,Units=="M"?"Pressure (hpa)":"Pressure (in)", pressure_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx+1*gap, gy, gwidth, gheight, 10,30,   Units=="M"?"Temperature (°C)":"Temperature (°F)", temperature_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx+2*gap, gy, gwidth, gheight, 0,100,   "Humidity (%)", humidity_readings, max_readings, autoscale_off, barchart_off);
  if (SumOfPrecip(rain_readings,max_readings) >= SumOfPrecip(snow_readings,max_readings))
       DrawGraph(gx+3*gap, gy, gwidth, gheight, 0,30, Units=="M"?"Rainfall (mm)":"Rainfall (in)", rain_readings, max_readings, autoscale_on, barchart_on);
  else DrawGraph(gx+3*gap, gy, gwidth, gheight, 0,30, Units=="M"?"Snowfall (mm)":"Snowfall (in)", snow_readings, max_readings, autoscale_on, barchart_on);
}
//#########################################################################################
float SumOfPrecip(float DataArray[],int readings){
  float sum = 0;
  for (int i=0; i <= readings; i++){ sum += DataArray[i]; }
  return sum;
}
//#########################################################################################
void Display_ForecastText_Section(int x, int y){
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  WxConditions[0].Main0.toLowerCase();
  WxConditions[0].Forecast0.toLowerCase();
  WxConditions[0].Forecast1.toLowerCase();
  WxConditions[0].Forecast2.toLowerCase();
  String Wx_Description = WxConditions[0].Main0;
  if (WxConditions[0].Forecast1 != "") { // Clear & clear Sky)
    Wx_Description += " (" +  TitleCase(WxConditions[0].Forecast0) + " & " +  TitleCase(WxConditions[0].Forecast1) + ")";
  }
  gfx.drawStringMaxWidth(x, y, 280, TitleCase(Wx_Description)); // Limit text display to the 600 pixel wide section, then wrap
  gfx.setFont(ArialMT_Plain_10);
}
//#########################################################################################
void Display_Forecast_Weather(int x, int y, int index) {
  int fwidth = 58;
  x = x + fwidth * index;
  gfx.setFont(ArialMT_Plain_10);
  gfx.setColor(EPD_BLACK); // Sometimes gets set to WHITE, so change back
  gfx.drawRect(x, y, fwidth-1, 65);
  gfx.drawLine(x, y + 13, x + fwidth, y + 13);
  Display_Conditions_Section(x + fwidth/2, y + 35, WxForecast[index].Icon, SmallIcon);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(x + fwidth/2, y, String(WxForecast[index].Period.substring(11,16)));
  gfx.drawString(x + fwidth/2, y + 50, String(WxForecast[index].High,0) + "° / " + String(WxForecast[index].Low,0) + "°");
}
//#########################################################################################
void Display_Wind_Section(int x, int y, float angle, float windspeed, int Cradius) {
  arrow(x, y, Cradius - 15, angle, 15, 27); // Show wind direction on outer circle of width and length
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x,y-Cradius-35,"Wind Speed & Direction");
  int dxo, dyo, dxi, dyi;
  gfx.drawLine(0,15,0,y+Cradius+30);
  gfx.drawCircle(x,y,Cradius);     // Draw compass circle
  gfx.drawCircle(x,y,Cradius+1);   // Draw compass circle
  gfx.drawCircle(x,y,Cradius*0.7); // Draw compass inner circle
  for (float a = 0; a <360; a = a + 22.5) {
    dxo = Cradius * cos((a-90)*PI/180);
    dyo = Cradius * sin((a-90)*PI/180);
    if (a == 45)  gfx.drawString(dxo+x+10,dyo+y-10,"NE");
    if (a == 135) gfx.drawString(dxo+x+5,dyo+y+5,"SE");
    if (a == 225) gfx.drawString(dxo+x-10,dyo+y,"SW");
    if (a == 315) gfx.drawString(dxo+x-10,dyo+y-10,"NW");
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    gfx.drawLine(dxo+x,dyo+y,dxi+x,dyi+y);   
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    gfx.drawLine(dxo+x,dyo+y,dxi+x,dyi+y);   
  }
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x,y-Cradius-18,"N");
  gfx.drawString(x,y+Cradius+2,"S");
  gfx.drawString(x-Cradius-10,y-10,"W");
  gfx.drawString(x+Cradius+10,y-10,"E");
  gfx.drawString(x, y-35, WindDegToDirection(angle));
  gfx.drawString(x, y+24, String(angle,0) + "°");
  gfx.setFont(ArialMT_Plain_24);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x, y-12, String(windspeed,1));
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x-3,y+8,(Units == "M" ? " m/s" : " mph"));
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
void Display_Pressure_Section(int x, int y, float pressure, String slope) {
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x,y-24,"Pressure");
  gfx.setFont(ArialRoundedMTBold_14);
  String slope_direction = "\nSteady";
  if (slope == "+") slope_direction = "\nRising";
  if (slope == "-") slope_direction = "\nFalling";
  gfx.drawString(x, y+5, String(pressure,1) + (Units == "M" ? " mb" : " in")+slope_direction);
}
//#########################################################################################
void Display_Precip_Section(int x, int y) {
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x,y-24,"Precipitation (soon)");
  gfx.setFont(ArialRoundedMTBold_14);
  if (String(WxForecast[1].Rainfall,2) > "0.00") {
    gfx.drawString(x, y+3, String(WxForecast[1].Rainfall,2) + (Units == "M" ? "mm" : "in") + " Rain");  // Only display rainfall total today if > 0
  }
  else gfx.drawString(x, y+3, "= Rain"); // If no rainfall forecast
  if (String(WxForecast[1].Snowfall,2) > "0.00") { //Sometimes very small amounts of snow are forecast, so ignore them
    gfx.drawString(x, y+23, String(WxForecast[1].Snowfall,2) + (Units == "M" ? "mm" : "in") + " Snow");; // Only display snowfall total today if > 0
  }
  else gfx.drawString(x, y+23, "= Snow"); // If no snowfall forecast
  gfx.setFont(ArialMT_Plain_10);
}
//#########################################################################################
void Display_Astronomy_Section(int x, int y) {
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawRect(x, y+13, 173, 52);
  gfx.drawRect(x, y+13, 82, 29);
  gfx.drawString(x+5,  y+15, "Sun Rise: "+ ConvertUnixTime(WxConditions[0].Sunrise).substring(0,5));
  gfx.drawString(x+16, y+28, "     Set: "+ ConvertUnixTime(WxConditions[0].Sunset).substring(0,5));
  gfx.drawString(x+88, y+15, "Moon");
  gfx.drawString(x+5,  y+47, "Phase: "   + MoonPhase(MoonDay, MoonMonth, MoonYear, Hemisphere));
  DrawMoon(x+104, y, MoonDay, MoonMonth, MoonYear, Hemisphere);
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  int diameter = 38;
  double Xpos, Ypos, Rpos, Xpos1, Xpos2, ip, ag;
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
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
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
  e   = 30.6   * m;
  jd  = c + e + d - 694039.09;           /* jd is total days elapsed */
  jd /= 29.53059;                        /* divide by the moon cycle (29.53 days) */
  b   = jd;                              /* int(jd) -> b, take integer part of jd */
  jd -= b;                               /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;                    /* scale fraction from 0-8 and round by adding 0.5 */
  b   = b & 7;                           /* 0 and 8 are the same phase so modulo 8 for 0 */
  if (hemisphere == "south") b = 7 - b;
  if (b == 0) return "New";              // New;              0%  illuminated
  if (b == 1) return "Waxing Crescent";  // Waxing crescent; 25%  illuminated
  if (b == 2) return "First Quarter";    // First quarter;   50%  illuminated
  if (b == 3) return "Waxing Gibbous";   // Waxing gibbous;  75%  illuminated
  if (b == 4) return "Full";             // Full;            100% illuminated
  if (b == 5) return "Waning Gibbous";   // Waning gibbous;  75%  illuminated
  if (b == 6) return "Third Quarter";    // Third quarter;   50%  illuminated
  if (b == 7) return "Waning Crescent";  // Waning crescent; 25%  illuminated
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
  gfx.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2);
}
//#########################################################################################
void Display_Conditions_Section(int x, int y, String IconName, bool IconSize) {
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  if (IconSize == LargeIcon) gfx.drawString(x, y-99, "Conditions");
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
bool obtain_wx_data(String RequestType) {
  rxtext = "";
  String units = (Units == "M"?"metric":"imperial");
  client.stop(); // close connection before sending a new request
  if (client.connect(server, 80)) { // if the connection succeeds
    // Serial.println("connecting...");
    // send the HTTP PUT request:
    if (RequestType == "weather")
      client.println("GET /data/2.5/" + RequestType + "?q=" + City + "," + Country + "&APPID=" + apikey + "&mode=json&units="+units+"&lang="+Language+" HTTP/1.1");
    else
      client.println("GET /data/2.5/" + RequestType + "?q=" + City + "," + Country + "&APPID=" + apikey + "&mode=json&units="+units+"&lang="+Language+"&cnt=24 HTTP/1.1");
    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ESP OWM Receiver/1.1");
    client.println("Connection: close");
    client.println();
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(F(">>> Client Timeout !"));
        client.stop();
        return false;
      }
    }
    char c = 0;
    bool startJson = false;
    int jsonend = 0;
    while (client.available()) {
      c = client.read();
      // JSON formats contain an equal number of open and close curly brackets, so check that JSON is received correctly by counting open and close brackets
      if (c == '{') {
        startJson = true; // set true to indicate JSON message has started
        jsonend++;
      }
      if (c == '}') { jsonend--; }
      if (startJson == true) { rxtext += c; } // Add in the received character
      // if jsonend = 0 then we have have received equal number of curly braces
      if (jsonend == 0 && startJson == true) {
        Serial.println(F("Received OK..."));
        //Serial.println(rxtext);
        if (!DecodeWeather(rxtext,RequestType)) return false;
        client.stop();
        return true;
      }
    }
  }
  else {
    // if no connection was made:
    Serial.println(F("connection failed"));
    return false;
  }
  rxtext = "";
  return true;
}
//#########################################################################################
// Problems with stucturing JSON decodes, see here: https://arduinojson.org/assistant/
bool DecodeWeather(String json, String Type) {
  Serial.println("Received a JSON string of size : "+String(json.length()));
  Serial.print(F("Creating object...and "));
  // allocate the JsonDocument
  DynamicJsonDocument doc(20*1024);
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, json);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  Serial.println(" Decoding " + Type + " data");
  if (Type == "weather") {
    // All Serial.println statements are for diagnostic purposes and not required, remove if not needed 
    WxConditions[0].lon         = root["coord"]["lon"].as<float>();              Serial.println(WxConditions[0].lon);
    WxConditions[0].lat         = root["coord"]["lat"].as<float>();              Serial.println(WxConditions[0].lat);
    WxConditions[0].Main0       = root["weather"][0]["main"].as<char*>();        Serial.println(WxConditions[0].Main0);
    WxConditions[0].Forecast0   = root["weather"][0]["description"].as<char*>(); Serial.println(WxConditions[0].Forecast0);
    WxConditions[0].Icon        = root["weather"][0]["icon"].as<char*>();        Serial.println(WxConditions[0].Icon);
    WxConditions[0].Forecast1   = root["weather"][1]["main"].as<char*>();        Serial.println(WxConditions[0].Forecast1);
    WxConditions[0].Forecast2   = root["weather"][2]["main"].as<char*>();        Serial.println(WxConditions[0].Forecast2);
    WxConditions[0].Temperature = root["main"]["temp"].as<float>();              Serial.println(WxConditions[0].Temperature);
    WxConditions[0].Pressure    = root["main"]["pressure"].as<float>();          Serial.println(WxConditions[0].Pressure);
    WxConditions[0].Humidity    = root["main"]["humidity"].as<float>();          Serial.println(WxConditions[0].Humidity);
    WxConditions[0].Low         = root["main"]["temp_min"].as<float>();          Serial.println(WxConditions[0].Low);
    WxConditions[0].High        = root["main"]["temp_max"].as<float>();          Serial.println(WxConditions[0].High);
    WxConditions[0].Windspeed   = root["wind"]["speed"].as<float>();             Serial.println(WxConditions[0].Windspeed);
    WxConditions[0].Winddir     = root["wind"]["deg"].as<float>();               Serial.println(WxConditions[0].Winddir);
    WxConditions[0].Cloudcover  = root["clouds"]["all"].as<int>();               Serial.println(WxConditions[0].Cloudcover); // in % of cloud cover
    WxConditions[0].Visibility  = root["visibility"].as<int>();                  Serial.println(WxConditions[0].Visibility); // in metres
    WxConditions[0].Country     = root["sys"]["country"].as<char*>();            Serial.println(WxConditions[0].Country);
    WxConditions[0].Sunrise     = root["sys"]["sunrise"].as<int>();              Serial.println(WxConditions[0].Sunrise);
    WxConditions[0].Sunset      = root["sys"]["sunset"].as<int>();               Serial.println(WxConditions[0].Sunset);
  }
  if (Type == "forecast") {
    //Serial.println(json);
    const char* cod                 = root["cod"]; // "200"
    float message                   = root["message"]; 
    int cnt                         = root["cnt"]; 
    JsonArray list                  = root["list"];
    Serial.print(F("\nReceiving Forecast period - ")); //------------------------------------------------
    for (byte r=0; r < max_readings; r++) {
      Serial.println("\nPeriod-"+String(r)+"--------------"); 
      WxForecast[r].Dt                = list[r]["dt"].as<char*>(); 
      WxForecast[r].Temperature       = list[r]["main"]["temp"].as<float>();              Serial.println(WxForecast[r].Temperature);
      WxForecast[r].Low               = list[r]["main"]["temp_min"].as<float>();          Serial.println(WxForecast[r].Low);
      WxForecast[r].High              = list[r]["main"]["temp_max"].as<float>();          Serial.println(WxForecast[r].High);
      WxForecast[r].Pressure          = list[r]["main"]["pressure"].as<float>();          Serial.println(WxForecast[r].Pressure);
      WxForecast[r].Humidity          = list[r]["main"]["humidity"].as<float>();          Serial.println(WxForecast[r].Humidity);
      WxForecast[r].Forecast0         = list[r]["weather"][0]["main"].as<char*>();        Serial.println(WxForecast[r].Forecast0);
      WxForecast[r].Forecast0         = list[r]["weather"][1]["main"].as<char*>();        Serial.println(WxForecast[r].Forecast1);
      WxForecast[r].Forecast0         = list[r]["weather"][2]["main"].as<char*>();        Serial.println(WxForecast[r].Forecast2);
      WxForecast[r].Description       = list[r]["weather"][0]["description"].as<char*>(); Serial.println(WxForecast[r].Description);
      WxForecast[r].Icon              = list[r]["weather"][0]["icon"].as<char*>();        Serial.println(WxForecast[r].Icon);
      WxForecast[r].Cloudcover        = list[r]["clouds"]["all"].as<int>();               Serial.println(WxForecast[0].Cloudcover); // in % of cloud cover
      WxForecast[r].Windspeed         = list[r]["wind"]["speed"].as<float>();             Serial.println(WxForecast[r].Windspeed);
      WxForecast[r].Winddir           = list[r]["wind"]["deg"].as<float>();               Serial.println(WxForecast[r].Winddir);
      WxForecast[r].Rainfall          = list[r]["rain"]["3h"].as<float>();                Serial.println(WxForecast[r].Rainfall);
      WxForecast[r].Snowfall          = list[r]["snow"]["3h"].as<float>();                Serial.println(WxForecast[r].Snowfall);
      WxForecast[r].Period            = list[r]["dt_txt"].as<char*>();                    Serial.println(WxForecast[r].Period);
    }
    //------------------------------------------
    float pressure_trend = WxForecast[0].Pressure - WxForecast[2].Pressure; // Measure pressure slope between ~now and later
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
  WxConditions[0].Pressure = WxConditions[0].Pressure * 0.02953;   // hPa to ins
  WxForecast[1].Rainfall   = WxForecast[1].Rainfall   * 0.0393701; // mm to inches of rainfall
  WxForecast[1].Snowfall   = WxForecast[1].Snowfall   * 0.0393701; // mm to inches of snowfall
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
      begin_sleep();
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
int WiFi_Signal() {
  return WiFi.RSSI();
}
//#########################################################################################
void Display_Status_Section(int x, int y, int rssi){
  gfx.drawRect(x-32,y-26,121,53);
  gfx.drawLine(x-32,y-14,x-32+121,y-14);
  gfx.drawLine(x-32+121/2,y-15,x-32+121/2,y-26);
  gfx.drawString(x-15,y-26,"WiFi");
  gfx.drawString(x+45,y-26,"Power");
  DrawRSSI(x-20,y+5,rssi);
  DrawBattery(x+55, y+3);;
}
//#########################################################################################
void DrawRSSI(int x, int y, int rssi){
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20)  WIFIsignal = 20; //            <-20dbm displays 5-bars
    if (_rssi <= -40)  WIFIsignal = 16; //  -40dbm to  -21dbm displays 4-bars
    if (_rssi <= -60)  WIFIsignal = 12; //  -60dbm to  -41dbm displays 3-bars
    if (_rssi <= -80)  WIFIsignal = 8;  //  -80dbm to  -61dbm displays 2-bars
    if (_rssi <= -100) WIFIsignal = 4;  // -100dbm to  -81dbm displays 1-bar
    gfx.fillRect(x+xpos*5,y-WIFIsignal,4,WIFIsignal);
    xpos++;
  }
  gfx.fillRect(x,y-1,4,1);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x+5,y+2,String(rssi));
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(x+30,y+5,"dbm");
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
  char output[30], day_output[30];
  while (!getLocalTime(&timeinfo)) {
    Serial.println(F("Failed to obtain time"));
  }
  strftime(output, 30, "%H", &timeinfo);
  wakeup_hour = String(output).toInt();
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");      // Displays: Saturday, June 24 2017 14:05:49
  Serial.println(&timeinfo, "%H:%M:%S");                      // Displays: 14:05:49
  if (Units == "M") {
    strftime(day_output, 30, "%a  %d-%b-%y", &timeinfo);      // Displays: Sat 24/Jun/17
    strftime(output, 30, "( Updated: %H:%M:%S )", &timeinfo); // Creates: '@ 14:05:49'
  }
  else {
    strftime(day_output, 30, "%a  %b-%d-%y", &timeinfo);      // Creates: Sat Jun/24/17
    strftime(output, 30, "( Updated: %r )", &timeinfo);       // Creates: '@ 2:05:49pm'
  }
  date_str = day_output;
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
    time_str =  (hour < 10 ? "0" + String(hour) : String(hour)) + ":" + (min < 10 ? "0" + String(min) : String(min)) + ":" + "  ";  // HH:MM   05/07/17
    time_str += (day < 10 ? "0" + String(day) : String(day)) + "/" + (month < 10 ? "0" + String(month) : String(month)) + "/" + (year < 10 ? "0" + String(year) : String(year)); // HH:MM   05/07/17
  }
  else {
    String ampm = "am";
    if (hour > 11) ampm = "pm";
    hour = hour % 12; if (hour == 0) hour = 12;
    time_str =  (hour % 12 < 10 ? "0" + String(hour % 12) : String(hour % 12)) + ":" + (min < 10 ? "0" + String(min) : String(min)) + ampm + " ";      // HH:MMam 07/05/17
    time_str += (month < 10 ? "0" + String(month) : String(month)) + "/" + (day < 10 ? "0" + String(day) : String(day)) + "/" + "/" + (year < 10 ? "0" + String(year) : String(year)); // HH:MMpm 07/05/17
  }
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  //Serial.println(time_str);
  return time_str;
}
//#########################################################################################
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.485;
  if (voltage > 1 ) { // Only display if there is a valid reading
    if (voltage >= 4.19) percentage = 100;
    else if (voltage < 3.20) percentage = 0;
    else percentage = voltage/4.2 * 100;
    gfx.setColor(EPD_BLACK);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawRect(x-22, y+5, 19, 10);
    gfx.fillRect(x-2, y+7, 3, 6);
    gfx.fillRect(x-20, y+7, 17 * percentage / 100.0, 6);
    gfx.setFont(ArialMT_Plain_10);
    gfx.drawString(x+5, y-10,   String(percentage) + "%");
    gfx.drawString(x+18, y+4, String(voltage, 2) + "v");
  }
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  gfx.fillCircle(x - scale * 3, y, scale);                              // Left most circle
  gfx.fillCircle(x + scale * 3, y, scale);                              // Right most circle
  gfx.fillCircle(x - scale, y - scale, scale * 1.4);                    // left middle upper circle
  gfx.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75);       // Right middle upper circle
  gfx.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1); // Upper and lower lines
  //Clear cloud inner
  gfx.setColor(EPD_WHITE);
  gfx.fillCircle(x - scale * 3, y, scale - linesize);                   // Clear left most circle
  gfx.fillCircle(x + scale * 3, y, scale - linesize);                   // Clear right most circle
  gfx.fillCircle(x - scale, y - scale, scale * 1.4 - linesize);         // left middle upper circle
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
    gfx.fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize);
  }
}
//#########################################################################################
void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void MostlySunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
}
//#########################################################################################
void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale);
}
//#########################################################################################
void Cloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) {
    if (IconName.endsWith("n")) addmoon(x,y,scale);
    linesize = 1;
    addcloud(x, y, scale, linesize);
  }
  else {
    y += 10;
    if (IconName.endsWith("n")) addmoon(x,y,scale);
    addcloud(x+30, y-45, 5, linesize); // Cloud top right
    addcloud(x-20, y-30, 7, linesize); // Cloud top left
    addcloud(x, y, scale, linesize);   // Main cloud
  }
}
//#########################################################################################
void Sunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  scale = scale * 1.6;
  addsun(x, y, scale);
}
//#########################################################################################
void ExpectRain(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
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
  if (IconName.endsWith("n")) addmoon(x,y,scale);
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
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addtstorm(x, y, scale);
}
//#########################################################################################
void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addsnow(x, y, scale);
}
//#########################################################################################
void Fog(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y-5, scale, linesize);
  addfog(x, y-5, scale, linesize);
}
//#########################################################################################
void Haze(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addsun(x, y-5, scale*1.4);
  addfog(x, y-5, scale*1.4, linesize);
}
//#########################################################################################
void addmoon(int x, int y, int scale){
  if (scale == Large) {
    gfx.fillCircle(x-50,y-60,scale);
    gfx.setColor(EPD_WHITE);
    gfx.fillCircle(x-35,y-60,scale*1.6);
    gfx.setColor(EPD_BLACK);
  }
  else
  {
    gfx.fillCircle(x-20,y-15,scale);
    gfx.setColor(EPD_WHITE);
    gfx.fillCircle(x-15,y-15,scale*1.6);
    gfx.setColor(EPD_BLACK);
  }
}
//#########################################################################################
void Nodata(int x, int y, int scale) {
  if (scale == Large) gfx.setFont(ArialMT_Plain_24); else gfx.setFont(ArialMT_Plain_16);
  gfx.drawString(x, y-10, "N/A");
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
  float x1, y1, x2, y2;
  if (auto_scale == true) {
    for (int i = 1; i < readings; i++ ) {
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
  gfx.drawRect(x_pos, y_pos, gwidth + 3, gheight + 2);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x_pos + gwidth / 2, y_pos - 19, title);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  // Draw the data
  for (int gx = 1; gx < readings; gx++) {
      x1 = last_x;
      y1 = last_y;
      x2 = x_pos + gx * gwidth/(readings-1)-1 ; // max_readings is the global variable that sets the maximum data that can be plotted
      y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
      if (barchart_mode) {
        gfx.fillRect(x2, y2, (gwidth/readings)-1, y_pos + gheight - y2 + 1);
      } else {
        gfx.drawLine(last_x, last_y, x2, y2);
      }
      last_x = x2;
      last_y = y2;
  }
  //Draw the Y-axis scale
  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
  #define number_of_dashes 20
    for (int j = 0; j < number_of_dashes; j++) { // Draw dashed graph grid lines
      if (spacing < y_minor_axis) gfx.drawHorizontalLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes));
    }
    if ( (Y1Max-(float)(Y1Max-Y1Min)/y_minor_axis*spacing) < 10) {gfx.drawString(x_pos-2, y_pos+gheight*spacing/y_minor_axis-5, String((Y1Max-(float)(Y1Max-Y1Min)/y_minor_axis*spacing+0.01), 1));}
    else {
      if (Y1Min < 1 && Y1Max < 10) gfx.drawString(x_pos - 2, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing+0.01), 1));
      else gfx.drawString(x_pos - 2, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0)); // +0.01 prevents -0.00 occurring
    }
  }
  for (int i = 0; i <= 3; i++) {
    gfx.drawString(5 + x_pos + gwidth / 3 * i, y_pos + gheight + 3, String(i));
  }
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x_pos+gwidth/2,y_pos+gheight+7,"(Days)");
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}
