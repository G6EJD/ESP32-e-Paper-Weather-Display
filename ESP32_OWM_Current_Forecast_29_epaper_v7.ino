/* ESP Weather Display using an EPD 2.9" Display, obtains data from Weather Underground and then decodes then displays it.
 *####################################################################################################################################  
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
#include "EPD_WaveShare.h"   // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "MiniGrafx.h"       // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "DisplayDriver.h"   // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "ArialRounded.h"    // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx

#define SCREEN_HEIGHT  128
#define SCREEN_WIDTH   296
#define BITS_PER_PIXEL 1

// defines the colors usable in the paletted 16 color frame buffer
#define EPD_BLACK 0 
#define EPD_WHITE 1 
uint16_t palette[] = { EPD_BLACK, EPD_WHITE };

// Display connectiopins_arduino.h, e.g. LOLIN32 LITE
static const uint8_t EPD_BUSY  = 4;
static const uint8_t EPD_SS    = 5;
static const uint8_t EPD_RST   = 16;
static const uint8_t EPD_DC    = 17;
static const uint8_t EPD_SCK   = 18;
static const uint8_t EPD_MISO  = 19; // Not used
static const uint8_t EPD_MOSI  = 23;

EPD_WaveShare epd(EPD2_9, EPD_SS, EPD_RST, EPD_DC, EPD_BUSY); // //EPD_WaveShare epd(EPD2_9, CS, RST, DC, BUSY);

MiniGrafx gfx = MiniGrafx(&epd, BITS_PER_PIXEL, palette);

//################# LIBRARIES ##########################
String version = "6";       // Version of this program
//################ VARIABLES ###########################

//------ NETWORK VARIABLES-----------
// Use your own API key by signing up for a free developer account at http://www.wunderground.com/weather/api/
unsigned long       lastConnectionTime = 0;                       // Last time you connected to the server, in milliseconds
const unsigned long UpdateInterval     = (30L*60L - 12)*1000000L; // Delay between updates, in microseconds, WU allows 500 requests per-day maximum, set to every 15-mins or more
bool    Largesize  = true;
bool    Smallsize  = false;
#define Large 7
#define Small 3
String  time_str, Day_time_str, rxtext; // strings to hold time and received weather data;
int     wifisection, displaysection, MoonDay, MoonMonth, MoonYear;

//################ PROGRAM VARIABLES and OBJECTS ##########################################
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
  String   lat;
  String   lon;
  String   Temperature;
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

#define max_readings 6

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];

WiFiClient client; // wifi client object

// Wifi on section takes : 7.37 secs to complete at 119mA using a Lolin 32 (wifi on)
// Display section takes : 10.16 secs to complete at 40mA using a Lolin 32 (wifi off)
// Sleep current is 175uA using a Lolin 32
// Total power consumption per-hour based on 2 updates at 30-min intervals = (2x(7.37x119/3600 + 10.16x40/3600) + 0.15 x (3600-2x(7.37+10.16))/3600)/3600 = 0.887mAHr 
// A 2600mAhr battery will last for 2600/0.88 = 122 days

//#########################################################################################
void setup() {
  wifisection = millis();
  Serial.begin(115200);
  StartWiFi();
  SetupTime();
  lastConnectionTime = millis();
  bool Received_WxData_OK = false;
  Received_WxData_OK = (obtain_wx_data("weather") && obtain_wx_data("forecast"));
  // Now only refresh the screen if all the data was received OK, otherwise wait until the next timed check otherwise wait until the next timed check
  if (Received_WxData_OK) {
    //Received data OK at this point so turn off the WiFi to save power
    StopWiFi(); // Reduces power consumption
    displaysection = millis();
    gfx.init();
    gfx.setRotation(3);
    gfx.setFont(ArialMT_Plain_10);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.fillBuffer(EPD_BLACK);
    gfx.setColor(EPD_WHITE);
    gfx.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, "Decoding data from OpenWeatherMap...");
    gfx.commit();
    delay(2500);
    gfx.setColor(EPD_BLACK);
    gfx.fillBuffer(EPD_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);  
    DisplayWeather();
    gfx.commit();
    delay(2500);
  }
  Serial.println("   Wifi section took: "+ String(wifisection/1000.0)+" secs");
  Serial.println("Display section took: "+ String((millis()-displaysection)/1000.0)+" secs");
  esp_sleep_enable_timer_wakeup(UpdateInterval);
  esp_deep_sleep_start(); // Sleep for e.g. 30 minutes
}

//#########################################################################################
void loop() { // this will never run!
}

//#########################################################################################
void DisplayWeather(){ // 2.9" e-paper display is 296x122 resolution
  UpdateLocalTime();
  Draw_Heading_Section();          // Top line of the display
  Draw_Main_Weather_Section();     // Centre section of display for Location, temperature, Weather report, cWx Symbol and wind direction
  Draw_3hr_Forecast(20,102, 1);    // First  3hr forecast box
  Draw_3hr_Forecast(70,102, 2);    // Second 3hr forecast box
  Draw_3hr_Forecast(120,102,3);    // Third  3hr forecast box
  Draw_Astronomy_Section();        // Astronomy section Sun rise/set, Moon phase and Moon icon
}
//#########################################################################################
void Draw_Heading_Section(){
  gfx.setFont(ArialMT_Plain_16);
  gfx.drawString(5,15,String(City));
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(5,0,time_str);
  gfx.drawString(226,0,Day_time_str);
  gfx.drawLine(0,14,296,14);  
}
//#########################################################################################
void Draw_Main_Weather_Section(){
  //Period-0 (Main Icon/Report)
  DisplayWXicon(205,45,WxConditions[0].Icon,Largesize); 
  gfx.setFont(ArialMT_Plain_24);
  gfx.drawString(5,32,String(WxConditions[0].Temperature)+"°");
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(String(WxConditions[0].Temperature).length()*13+8,33,(Units=="I"?"F":"C"));
  gfx.setFont(ArialMT_Plain_10);
  DrawWind(265,42,WxConditions[0].Winddir,WxConditions[0].Windspeed);
  if (WxConditions[0].Rainfall > 0) gfx.drawString(90,35,String(WxConditions[0].Rainfall,1)+(Units=="M"?"mm":"in")+" of rain");
  DrawPressureTrend(130,52,WxConditions[0].Pressure,WxConditions[0].Trend);
  gfx.setFont(ArialRoundedMTBold_14);
  String Wx_Description = WxConditions[0].Forecast0;
  if (WxConditions[0].Forecast1 != "") Wx_Description += " & " +  WxConditions[0].Forecast1;
    if (WxConditions[0].Forecast2 != "" && WxConditions[0].Forecast1 != WxConditions[0].Forecast2) Wx_Description += " & " +  WxConditions[0].Forecast2;
  gfx.drawString(5,59, Wx_Description);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawLine(0,77,296,77);
}
//#########################################################################################
String TitleCase(String text){
  if (text.length() > 0) {
    String temp_text = text.substring(0,1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1); // Title-case the string
  }
}//#########################################################################################
void Draw_3hr_Forecast(int x, int y, int index){
  DisplayWXicon(x+2,y,WxForecast[index].Icon,Smallsize);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x+3,y-25,WxForecast[index].Period.substring(11,16));
  gfx.drawString(x+2,y+11,String(WxForecast[index].High,0)+"° / "+String(WxForecast[index].Low,0)+"°");
  gfx.drawLine(x+28,77,x+28,129);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}
//#########################################################################################
void Draw_Astronomy_Section(){
  gfx.drawString(152,76,"Sun Rise: " + ConvertUnixTime(WxConditions[0].Sunrise).substring(0,5));
  gfx.drawString(178,88,"Set: "      + ConvertUnixTime(WxConditions[0].Sunset).substring(0,5));
  DrawMoon(230,65,MoonDay,MoonMonth,MoonYear,Hemisphere);
  gfx.drawString(152,100,"Moon phase:");
  gfx.drawString(152,112,MoonPhase(MoonDay,MoonMonth,MoonYear,Hemisphere));
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
  j = k1 + k2 + d + 59+1;
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
  if (b == 6) return "Third quarter";     // Last quarter; 50% illuminated
  if (b == 7) return "Waning crescent";  // Waning crescent; 25% illuminated
  return "";
}
//#########################################################################################
float MoonIllumination(int nDay, int nMonth, int nYear) { // calculate the current phase of the moon
  float age, phase, frac, days_since;
  long YY, MM, K1, K2, K3, JD;
  YY = nYear - floor((12 - nMonth) / 10);
  MM = nMonth + 9;
  if (MM >= 12) { MM = MM - 12;  }
  K1 = floor(365.25 * (YY + 4712));
  K2 = floor(30.6 * MM + 0.5);
  K3 = floor(floor((YY / 100) + 49) * 0.75) - 38;
  JD = K1 + K2 + nDay + 59;  //Julian day
  if (JD > 2299160) {JD = JD - K3; } //1582, Gregorian calendar
  days_since = JD - 2451550L; //since new moon on Jan. 6, 2000 (@18:00)
  phase = (days_since - 0.25) / 29.53059; //0.25 = correct for 6 pm that day
  phase -= floor(phase);  //phase in cycle
  age = phase * 29.53059;
  // calculate fraction full
  frac = (1.0 - cos(phase * 2 * PI)) * 0.5;
  if (frac > 1.0) frac = 2.0 - frac;  //illumination, accurate to about 5%
  return frac; //phase or age or frac, as desired
}
//#########################################################################################
void DrawWind(int x, int y, float angle, float windspeed){
  #define Cradius 15
  float dx = Cradius*cos((angle-90)*PI/180)+x;  // calculate X position  
  float dy = Cradius*sin((angle-90)*PI/180)+y;  // calculate Y position  
  arrow(x,y,Cradius-3,angle,10,12);   // Show wind direction on outer circle
  gfx.drawCircle(x,y,Cradius+2);
  gfx.drawCircle(x,y,Cradius+3);
  for (int m=0;m<360;m=m+45){
    dx = Cradius*cos(m*PI/180); // calculate X position  
    dy = Cradius*sin(m*PI/180); // calculate Y position  
    gfx.drawLine(x+dx,y+dy,x+dx*0.8,y+dy*0.8);
  }
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x,y+Cradius+3,WindDegToDirection(angle));
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(x,y-Cradius-14,String(windspeed,1)+(Units=="M"?" m/s":" mph"));
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}
//#########################################################################################
String WindDegToDirection(float winddirection){
  if (winddirection >= 348.75 || winddirection < 11.25)  return "N";
  if (winddirection >= 11.25  && winddirection < 33.75)  return "NNE";
  if (winddirection >= 33.75  && winddirection < 56.25)  return "NE";
  if (winddirection >= 56.25  && winddirection < 78.75)  return "ENE";
  if (winddirection >= 78.75  && winddirection < 101.25) return "E";
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
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength){
  // x,y is the centre poistion of the arrow and asize is the radius out from the x,y position
  // aangle is angle to draw the pointer at e.g. at 45° for NW
  // pwidth is the pointer width in pixels
  // plength is the pointer length in pixels
  float dx = (asize-10)*cos((aangle-90)*PI/180)+x; // calculate X position  
  float dy = (asize-10)*sin((aangle-90)*PI/180)+y; // calculate Y position  
  float x1 = 0;         float y1 = plength;
  float x2 = pwidth/2;  float y2 = pwidth/2;
  float x3 = -pwidth/2; float y3 = pwidth/2;
  float angle = aangle*PI/180-135;
  float xx1 = x1*cos(angle)-y1*sin(angle)+dx;
  float yy1 = y1*cos(angle)+x1*sin(angle)+dy;
  float xx2 = x2*cos(angle)-y2*sin(angle)+dx;
  float yy2 = y2*cos(angle)+x2*sin(angle)+dy;
  float xx3 = x3*cos(angle)-y3*sin(angle)+dx;
  float yy3 = y3*cos(angle)+x3*sin(angle)+dy;
  gfx.fillTriangle(xx1,yy1,xx3,yy3,xx2,yy2);
}
//#########################################################################################
void DrawPressureTrend(int x, int y, float pressure, String slope){
  gfx.drawString(90,47,String(pressure,1)+(Units=="M"?"mb":"in"));
  x = x + 8;
  if      (slope == "+") {
    gfx.drawLine(x,  y,  x+4,y-4);
    gfx.drawLine(x+4,y-4,x+8,y);
  }
  else if (slope == "0") {
    gfx.drawLine(x+3,y-4,x+8,y);
    gfx.drawLine(x+3,y+4,x+8,y);
  }
  else if (slope == "-") {
    gfx.drawLine(x,  y,  x+4,y+4);
    gfx.drawLine(x+4,y+4,x+8,y);
  }
}
//#########################################################################################
void DisplayWXicon(int x, int y, String IconName, bool LargeSize){
  IconName.toLowerCase(); IconName.trim();
  Serial.println(IconName);
  if      (IconName == "01d" || IconName == "01n")  if (LargeSize) Sunny(x,y,Large); else Sunny(x,y,Small);
  else if (IconName == "02d" || IconName == "02n")  if (LargeSize) MostlySunny(x,y,Large); else MostlySunny(x,y,Small);
  else if (IconName == "03d" || IconName == "03n")  if (LargeSize) Cloudy(x,y,Large); else Cloudy(x,y,Small);
  else if (IconName == "04d" || IconName == "04n")  if (LargeSize) MostlySunny(x,y,Large); else MostlySunny(x,y,Small);
  else if (IconName == "09d" || IconName == "09n")  if (LargeSize) ChanceRain(x,y,Large); else ChanceRain(x,y,Small);
  else if (IconName == "10d" || IconName == "10n")  if (LargeSize) Rain(x,y,Large); else Rain(x,y,Small);
  else if (IconName == "11d" || IconName == "11n")  if (LargeSize) Tstorms(x,y,Large); else Tstorms(x,y,Small);
  else if (IconName == "13d" || IconName == "13n")  if (LargeSize) Snow(x,y,Large); else Snow(x,y,Small);
  else if (IconName == "50d")                       if (LargeSize) Haze(x,y-5,Large); else Haze(x,y,Small);
  else if (IconName == "50n")                       if (LargeSize) Fog(x,y-5,Large); else Fog(x,y,Small); 
  else if (IconName == "probrain")                  if (LargeSize) ProbRain(x,y,Large); else ProbRain(x,y,Small);
  else                                              if (LargeSize) Nodata(x,y,Large); else Nodata(x,y,Small);
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
      client.println("GET /data/2.5/" + RequestType + "?q=" + City + "," + Country + "&APPID=" + apikey + "&mode=json&units="+units+"&lang="+Language+"&cnt=6 HTTP/1.1");
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
      // JSON formats contain an equal number of open and close curly brackets, so check that JSON is received correctly by counting open and close brackets
      if (c == '{') {
        startJson = true; // set true to indicate JSON message has started
        jsonend++;
      }
      if (c == '}') { jsonend--; }
      if (startJson == true) { rxtext += c; } // Add in the received character
      // if jsonend = 0 then we have have received equal number of curly braces
      if (jsonend == 0 && startJson == true) {
        Serial.println("Received OK...");
        //Serial.println(rxtext);
        if (!DecodeWeather(rxtext,RequestType)) return false;
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
  rxtext = "";
  return true;
}
//#########################################################################################
// Problems with stucturing JSON decodes, see here: https://arduinojson.org/assistant/
bool DecodeWeather(String json, String Type) {
  Serial.print(F("Creating object...and "));
  DynamicJsonBuffer jsonBuffer (50*1024);
  JsonObject& root = jsonBuffer.parseObject(const_cast<char*>(json.c_str()));
  if (!root.success()) {
    Serial.print("ParseObject() failed");
    return false;
  }
  Serial.println(" Decoding " + Type + " data");
  if (Type == "weather") {
    // All Serial.println statements are for diagnostic purposes and not required, remove if not needed 
    WxConditions[0].lon         = root["coord"]["lon"].as<String>();             Serial.println(WxConditions[0].lon);
    WxConditions[0].lat         = root["coord"]["lat"].as<String>();             Serial.println(WxConditions[0].lat);
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
    JsonArray& list                 = root["list"];
    Serial.print("\nReceiving Forecast period - "); //------------------------------------------------
    for (byte r=0; r < max_readings; r++) {
      Serial.println("\nPeriod-"+String(r)+"--------------"); 
      WxForecast[r].Dt                = list[r]["dt"].as<char*>(); 
      WxForecast[r].Temperature       = list[r]["main"]["temp"].as<float>();               Serial.println(WxForecast[r].Temperature);
      WxForecast[r].Low               = list[r]["main"]["temp_min"].as<float>();           Serial.println(WxForecast[r].Low);
      WxForecast[r].High              = list[r]["main"]["temp_max"].as<float>();           Serial.println(WxForecast[r].High);
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
      WxForecast[r].Snowfall          = list[r]["snow"]["3h"].as<float>();                Serial.println(WxForecast[r].Rainfall);
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
  WxConditions[0].Pressure = WxConditions[0].Pressure * 0.02953;   //  hPa to ins
  WxForecast[1].Rainfall   = WxForecast[1].Rainfall   * 0.0393701; // mm to inches of rainfall
}
//#########################################################################################
int StartWiFi(){
  int connAttempts = 0;
  Serial.print(F("\r\nConnecting to: ")); Serial.println(String(ssid1));
  WiFi.disconnect();   
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid1, password1);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500); Serial.print(F("."));
    if(connAttempts > 20) {
      Serial.println("Attempting to connect to 2nd Access Point");
      connAttempts = 0;
      while (WiFi.status() != WL_CONNECTED ) {
        connAttempts++;
        WiFi.mode(WIFI_STA); 
        WiFi.begin(ssid2, password2);
        if(connAttempts > 20) ESP.restart();
        delay(500); Serial.print(F("."));
      }
    }
    connAttempts++;
  }
  Serial.println("WiFi connected at: "+String(WiFi.localIP()));
  return 1;
}
//#########################################################################################
void StopWiFi(){
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  wifisection    = millis() - wifisection;
}
//#########################################################################################
void SetupTime(){
  configTime(0, 0, "0.uk.pool.ntp.org", "time.nist.gov");
  setenv("TZ", Timezone, 1);
  delay(200);
  UpdateLocalTime();
}
//#########################################################################################
void UpdateLocalTime(){
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)){
    Serial.println(F("Failed to obtain time"));
  }
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S"); // Displays: Saturday, June 24 2017 14:05:49
  Serial.println(&timeinfo, "%H:%M:%S"); // Displays: 14:05:49
  char output[30], day_output[30];
  if (Units == "M") {
    strftime(day_output, 30, "%a  %d-%m-%y", &timeinfo); // Creates: Sat 24-Jun-17
    strftime(output, 30, "(@ %H:%M:%S )", &timeinfo);    // Creates: 14:05:49
  }
  else {
    strftime(day_output, 30, "%a  %m-%d-%y", &timeinfo); // Creates: Sat Jun-24-17
    strftime(output, 30, "(@ %r )", &timeinfo);          // Creates: 2:05:49pm
  }
  Day_time_str = day_output;
  time_str     = output;
}
//#########################################################################################
String ConvertUnixTime(int unix_time){  
  struct tm *now_tm;  
  int hour, min, second, day, month, year; 
  // timeval tv = {unix_time,0}; 
  time_t tm = unix_time; 
  now_tm = localtime(&tm);  
  hour   = now_tm->tm_hour;  
  min    = now_tm->tm_min;   
  second = now_tm->tm_sec; 
  //wday   = now_tm->tm_wday;
  day    = now_tm->tm_mday;  
  month  = now_tm->tm_mon+1;  
  year   = 1900 + now_tm->tm_year; // To get just YY information
  MoonDay   = day;
  MoonMonth = month;
  MoonYear  = year;
  time_str  =  (hour<10?"0"+String(hour):String(hour))+":"+(min<10?"0"+String(min):String(min))+":"+(second<10?"0"+String(second):String(second))+" ";  
  time_str += (day<10?"0"+String(day):String(day))+"/"+ (month<10?"0"+String(month):String(month))+"/"+(year<10?"0"+String(year):String(year));      // HH:MM:SS  05/07/17  
  //Serial.println(time_str);  
  return time_str;  
}  
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  gfx.fillCircle(x-scale*3, y, scale);                           // Left most circle
  gfx.fillCircle(x+scale*3, y, scale);                           // Right most circle
  gfx.fillCircle(x-scale, y-scale, scale*1.4);                   // left middle upper circle
  gfx.fillCircle(x+scale*1.5, y-scale*1.3, scale*1.75);          // Right middle upper circle
  gfx.fillRect(x-scale*3-1, y-scale, scale*6, scale*2+1);         // Upper and lower lines
  //Clear cloud inner
  gfx.setColor(EPD_WHITE);
  gfx.fillCircle(x-scale*3, y, scale-linesize);                  // Clear left most circle
  gfx.fillCircle(x+scale*3, y, scale-linesize);                  // Clear right most circle
  gfx.fillCircle(x-scale, y-scale, scale*1.4-linesize);          // left middle upper circle
  gfx.fillCircle(x+scale*1.5, y-scale*1.3, scale*1.75-linesize); // Right middle upper circle
  gfx.fillRect(x-scale*3+2, y-scale+linesize-1, scale*5.9, scale*2-linesize*2+2); // Upper and lower lines
  gfx.setColor(EPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale){
  y = y + scale/2;
  for (int i = 0; i < 6; i++){
    gfx.drawLine(x-scale*4+scale*i*1.3+0, y+scale*1.9, x-scale*3.5+scale*i*1.3+0, y+scale);
    if (scale != Small) {
      gfx.drawLine(x-scale*4+scale*i*1.3+1, y+scale*1.9, x-scale*3.5+scale*i*1.3+1, y+scale);
      gfx.drawLine(x-scale*4+scale*i*1.3+2, y+scale*1.9, x-scale*3.5+scale*i*1.3+2, y+scale);
    }
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale){
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5;flakes++){
    for (int i = 0; i <360; i = i + 45) {
      dxo = 0.5*scale * cos((i-90)*3.14/180); dxi = dxo*0.1;
      dyo = 0.5*scale * sin((i-90)*3.14/180); dyi = dyo*0.1;
      gfx.drawLine(dxo+x+0+flakes*1.5*scale-scale*3,dyo+y+scale*2,dxi+x+0+flakes*1.5*scale-scale*3,dyi+y+scale*2); 
    }
  }
}
//#########################################################################################
void addtstorm(int x, int y, int scale){
  y = y + scale/2;
  for (int i = 0; i < 5; i++){
    gfx.drawLine(x-scale*4+scale*i*1.5+0, y+scale*1.5, x-scale*3.5+scale*i*1.5+0, y+scale);
    if (scale != Small) {
      gfx.drawLine(x-scale*4+scale*i*1.5+1, y+scale*1.5, x-scale*3.5+scale*i*1.5+1, y+scale);
      gfx.drawLine(x-scale*4+scale*i*1.5+2, y+scale*1.5, x-scale*3.5+scale*i*1.5+2, y+scale);
    }
    gfx.drawLine(x-scale*4+scale*i*1.5, y+scale*1.5+0, x-scale*3+scale*i*1.5+0, y+scale*1.5+0);
    if (scale != Small) {
      gfx.drawLine(x-scale*4+scale*i*1.5, y+scale*1.5+1, x-scale*3+scale*i*1.5+0, y+scale*1.5+1);
      gfx.drawLine(x-scale*4+scale*i*1.5, y+scale*1.5+2, x-scale*3+scale*i*1.5+0, y+scale*1.5+2);
    }
    gfx.drawLine(x-scale*3.5+scale*i*1.4+0, y+scale*2.5, x-scale*3+scale*i*1.5+0, y+scale*1.5);
    if (scale != Small) {
      gfx.drawLine(x-scale*3.5+scale*i*1.4+1, y+scale*2.5, x-scale*3+scale*i*1.5+1, y+scale*1.5);
      gfx.drawLine(x-scale*3.5+scale*i*1.4+2, y+scale*2.5, x-scale*3+scale*i*1.5+2, y+scale*1.5);
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
  gfx.fillCircle(x, y, scale-linesize);
  gfx.setColor(EPD_BLACK);
  for (float i = 0; i <360; i = i + 45) {
    dxo = 2.2*scale * cos((i-90)*3.14/180); dxi = dxo * 0.6;
    dyo = 2.2*scale * sin((i-90)*3.14/180); dyi = dyo * 0.6;
    if (i == 0   || i == 180) {
      gfx.drawLine(dxo+x-1,dyo+y,dxi+x-1,dyi+y);
      if (scale != Small) {
        gfx.drawLine(dxo+x+0,dyo+y,dxi+x+0,dyi+y); 
        gfx.drawLine(dxo+x+1,dyo+y,dxi+x+1,dyi+y);
      }
    }
    if (i == 90  || i == 270) {
      gfx.drawLine(dxo+x,dyo+y-1,dxi+x,dyi+y-1);
      if (scale != Small) {
        gfx.drawLine(dxo+x,dyo+y+0,dxi+x,dyi+y+0); 
        gfx.drawLine(dxo+x,dyo+y+1,dxi+x,dyi+y+1); 
      }
    }
    if (i == 45  || i == 135 || i == 225 || i == 315) {
      gfx.drawLine(dxo+x-1,dyo+y,dxi+x-1,dyi+y);
      if (scale != Small) {
        gfx.drawLine(dxo+x+0,dyo+y,dxi+x+0,dyi+y); 
        gfx.drawLine(dxo+x+1,dyo+y,dxi+x+1,dyi+y); 
      }
    }
  }
}
//#########################################################################################
void addfog(int x, int y, int scale){
  int linesize = 2;  
  if (scale == Small) linesize = 1;
  for (int i = 0; i < 6; i++){
    gfx.fillRect(x-scale*3, y+scale*1.5, scale*6, linesize); 
    gfx.fillRect(x-scale*3, y+scale*2.0, scale*6, linesize); 
    gfx.fillRect(x-scale*3, y+scale*2.7, scale*6, linesize); 
  }
}
//#########################################################################################
void MostlyCloudy(int x, int y, int scale){ 
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize); 
  addsun(x-scale*1.8,y-scale*1.8,scale); 
  addcloud(x,y,scale,linesize); 
}
//#########################################################################################
void MostlySunny(int x, int y, int scale){ 
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize); 
  addsun(x-scale*1.8,y-scale*1.8,scale); 
}
//#########################################################################################
void Rain(int x, int y, int scale){ 
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize); 
  addrain(x,y,scale); 
} 
//#########################################################################################
void ProbRain(int x, int y, int scale){
  x = x + 20;
  y = y + 15;
  addcloud(x,y,scale,1);
  y = y + scale/2;
  for (int i = 0; i < 6; i++){
    gfx.drawLine(x-scale*4+scale*i*1.3+0, y+scale*1.9, x-scale*3.5+scale*i*1.3+0, y+scale);
  }
}
//#########################################################################################
void Cloudy(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
}
//#########################################################################################
void Sunny(int x, int y, int scale){
  scale = scale * 1.45;
  addsun(x,y-4,scale);
}
//#########################################################################################
void ExpectRain(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addsun(x-scale*1.8,y-scale*1.8,scale); 
  addcloud(x,y,scale,linesize);
  addrain(x,y,scale);
}
//#########################################################################################
void ChanceRain(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addsun(x-scale*1.8,y-scale*1.8,scale); 
  addcloud(x,y,scale,linesize);
  addrain(x,y,scale);
}
//#########################################################################################
void Tstorms(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
  addtstorm(x,y,scale);
}
//#########################################################################################
void Snow(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
  addsnow(x,y,scale);
}
//#########################################################################################
void Fog(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
  addfog(x,y,scale);
}
//#########################################################################################
void Haze(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addsun(x,y,scale);
  addfog(x,y,scale);
}
//#########################################################################################
void Nodata(int x, int y, int scale){
  if (scale > Small) {
    gfx.setFont(ArialMT_Plain_24);
    gfx.drawString(x-10,y-18,"N/A");
  }
  else 
  {
    gfx.setFont(ArialMT_Plain_10);
    gfx.drawString(x-8,y-8,"N/A");
  }
  gfx.setFont(ArialMT_Plain_10);
}
