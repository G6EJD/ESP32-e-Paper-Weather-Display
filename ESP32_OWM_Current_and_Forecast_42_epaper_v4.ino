/* ESP32 Weather Display using an EPD 4.2" Display, obtains data from Weather Underground and then decodes then displays it.
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
String version = "1";       // Version of this program
//################ VARIABLES ###########################

const unsigned long UpdateInterval     = 30L*60L*1000000L; // Delay between updates, in milliseconds, WU allows 500 requests per-day maximum, set to every 15-mins or more
unsigned long       lastConnectionTime = 0;                // Last time connected to the server, in milliseconds
bool LargeIcon   =  true;
bool SmallIcon   =  false;
#define Large  11
#define Small  4
String time_str, Day_time_str, rxtext; // strings to hold time and received weather data;
int    wifisection, displaysection, MoonDay, MoonMonth, MoonYear;
int    Sunrise, Sunset;

//################ PROGRAM VARIABLES and OBJECTS ################
typedef struct { // For current Day and Day 1, 2, 3
  String Period,
         Temperature,
         Icon,       
         High,
         Low,
         Rain,
         Pressure,
         Trend,
         Winddir,
         Windspeed,
         Forecast,
         Time;
} Conditions_record_type;  
 
Conditions_record_type WxConditions[17];

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

#define max_readings 16
float pressure_readings[max_readings+1]    = {0};
float temperature_readings[max_readings+1] = {0};
float rain_readings[max_readings+1]        = {0};

WiFiClient client; // wifi client object

//#########################################################################################
void setup() { 
  Serial.begin(115200);
  StartWiFi();
  SetupTime();
  lastConnectionTime = millis();
  bool Received_Forecast_OK = false;
  obtain_wx_data("weather");  Received_Forecast_OK = DecodeWeather(rxtext,"weather");
  obtain_wx_data("forecast"); Received_Forecast_OK = DecodeWeather(rxtext,"forecast");
  // Now only refresh the screen if all the data was received OK, otherwise wait until the next timed check otherwise wait until the next timed check
  if (Received_Forecast_OK) { 
    StopWiFi(); // Reduces power consumption
    gfx.init();
    gfx.setRotation(0);
    gfx.setColor(EPD_BLACK);
    gfx.fillBuffer(EPD_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);  
    Display_Weather();
    DrawBattery(320,0);
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
void Display_Weather(){              // 4.2" e-paper display is 400x300 resolution
  Draw_Heading_Section();            // Top line of the display
  Draw_Main_Weather_Section(165,70); // Centre section of display for Location, temperature, Weather report, cWx Symbol and wind direction
  Draw_Forecast_Section(230,18);     // 3hr forecast boxes
  Draw_Astronomy_Section(230,20);    // Astronomy section Sun rise/set, Moon phase and Moon icon
}
//#########################################################################################
void Draw_Heading_Section(){
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(SCREEN_WIDTH/2,-2,City);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(SCREEN_WIDTH-8,0,Day_time_str);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(5,0,time_str);
  gfx.drawLine(0,15,SCREEN_WIDTH,15);  
}
//#########################################################################################
void Draw_Main_Weather_Section(int x, int y){
  DisplayWXicon(x+5,y-8,WxConditions[0].Icon,LargeIcon);
  gfx.setFont(ArialRoundedMTBold_14);
  DrawPressureTrend(x,y+50,WxConditions[0].Pressure,WxConditions[0].Trend);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);  
  Draw_Rain(x-100,y+35);
  gfx.setFont(ArialMT_Plain_24);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(x-155,y+70,TitleCase(WxConditions[0].Forecast));
  Draw_Main_Wx(x-98,y-1);
  gfx.drawLine(0,y+68,SCREEN_WIDTH,y+68);
}
//#########################################################################################
String TitleCase(String text){
  if (text.length() > 0) {
    String temp_text = text.substring(0,1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1); // Title-case the string
  }
}
//#########################################################################################
void Draw_Forecast_Section(int x, int y){
  gfx.setFont(ArialMT_Plain_10);
  Draw_Forecast_Weather(x,y,1);
  Draw_Forecast_Weather(x+56,y,2);
  Draw_Forecast_Weather(x+112,y,3);
  //       (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  for (int p=1; p < 16; p++){pressure_readings[p]    = WxConditions[p].Pressure.toFloat();}
  for (int t=1; t < 16; t++){temperature_readings[t] = WxConditions[t].Temperature.toFloat();}
  for (int r=1; r < 16; r++){rain_readings[r]        = WxConditions[r].Rain.toFloat();}
  gfx.drawLine(0,y+173,SCREEN_WIDTH,y+173);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);  
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x-40,y+173,"5-Day Forecast Values");
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  DrawGraph(30, 222,100,60,900,1050,"Pressure",    pressure_readings,    15, autoscale_on, barchart_off); 
  DrawGraph(158,222,100,60,10 ,  30,"Temperature", temperature_readings, 15, autoscale_on, barchart_off);
  DrawGraph(288,222,100,60,0  ,  30,"Rainfall",    rain_readings,        15, autoscale_on, barchart_on);
}
//#########################################################################################
void Draw_Forecast_Weather(int x, int y, int index){
  gfx.setFont(ArialMT_Plain_10);
  gfx.setColor(EPD_BLACK); // Sometimes gets set to WHITE, so change back
  gfx.drawRect(x,y,55,65);
  gfx.drawLine(x+1,y+13,x+55,y+13);
  DisplayWXicon(x+28,y+35,WxConditions[index].Icon,SmallIcon);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(x+28,y,WxConditions[index].Period);
  gfx.drawString(x+28,y+50,WxConditions[index].High+"° / "+WxConditions[index].Low+"°");
}
//#########################################################################################
void Draw_Main_Wx(int x, int y){
  DrawWind(x,y,WxConditions[0].Winddir, WxConditions[0].Windspeed);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);  
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x,y-28,WxConditions[0].High+"° | "+WxConditions[0].Low+"°"); // Show forecast high and Low
  gfx.setFont(ArialMT_Plain_24);
  gfx.drawString(x-5,y-10,WxConditions[0].Temperature+"°"); // Show current Temperature
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);  
  gfx.drawString(x+WxConditions[0].Temperature.length()*11/2,y-9,Units=="M"?"C":"F"); // Add in smaller Temperature unit
}
//#########################################################################################
void DrawWind(int x, int y, String angle, String windspeed){
  int Cradius = 44;
  float dx = Cradius*cos((angle.toInt()-90)*PI/180)+x;  // calculate X position  
  float dy = Cradius*sin((angle.toInt()-90)*PI/180)+y;  // calculate Y position  
  arrow(x,y,Cradius-3,angle,15,15); // Show wind direction on outer circle
  gfx.drawCircle(x,y,Cradius+2);
  gfx.drawCircle(x,y,Cradius+3);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x,y+Cradius-25,WindDegToDirection(angle.toFloat()));
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(x-Cradius+5,y-Cradius-6,windspeed+(Units=="M"?" m/s":" mph"));
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}
//#########################################################################################
String WindDegToDirection(float winddirection){
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
void DrawPressureTrend(int x, int y, String pressure, String slope){
  gfx.drawString(x-25,y,pressure+(Units=="M"?"mb":"in"));
  x = x + 45; y = y + 8;
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
void Draw_Rain(int x, int y){
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  if (WxConditions[1].Rain.toFloat() > 0) gfx.drawString(x,y+14,WxConditions[1].Rain+(Units=="M"?"mm":"in")+" Rainfall"); // Only display rainfall total today if > 0
  gfx.setFont(ArialMT_Plain_10);
}
//#########################################################################################
void Draw_Astronomy_Section(int x, int y){
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawRect(x,y+64,167,53);
  gfx.drawString(x+4,y+65,"Sun Rise/Set");
  gfx.drawString(x+20,y+75,ConvertUnixTime(Sunrise).substring(0,5));
  gfx.drawString(x+20,y+85,ConvertUnixTime(Sunset).substring(0,5));
  gfx.drawString(x+4,y+100,"Moon:");
  gfx.drawString(x+35,y+100,MoonPhase(MoonDay,MoonMonth,MoonYear,Hemisphere));
  DrawMoon(x+103,y+51,MoonDay,MoonMonth,MoonYear,Hemisphere);
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere){
  #define pi 3.141592654
  int diameter = 38;
  float Xpos, Ypos, Rpos, Xpos1, Xpos2, ip, ag;
  gfx.setColor(EPD_BLACK);
  for (Ypos = 0; Ypos <= 45; Ypos++) {
    Xpos = sqrt(45 * 45 - Ypos * Ypos);
    // Draw dark part of moon
    double pB1x = (90   - Xpos)/90 * diameter + x;
    double pB1y = (Ypos + 90)/90   * diameter + y;
    double pB2x = (Xpos + 90)/90   * diameter + x;
    double pB2y = (Ypos + 90)/90   * diameter + y;
    double pB3x = (90   - Xpos)/90 * diameter + x;
    double pB3y = (90   - Ypos)/90 * diameter + y;
    double pB4x = (Xpos + 90)/90   * diameter + x;
    double pB4y = (90   - Ypos)/90 * diameter + y;
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
    if (Phase < 0.5){
      Xpos1 = - Xpos;
      Xpos2 = (Rpos - 2 * Phase * Rpos - Xpos);
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = (Xpos - 2 * Phase * Rpos + Rpos);
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + 90)/90 * diameter + x;
    double pW1y = (90 - Ypos)/90  * diameter + y;
    double pW2x = (Xpos2 + 90)/90 * diameter + x;
    double pW2y = (90 - Ypos)/90  * diameter + y;
    double pW3x = (Xpos1 + 90)/90 * diameter + x;
    double pW3y = (Ypos + 90)/90  * diameter + y;
    double pW4x = (Xpos2 + 90)/90 * diameter + x;
    double pW4y = (Ypos + 90)/90  * diameter + y;
    gfx.setColor(EPD_WHITE);
    gfx.drawLine(pW1x, pW1y, pW2x, pW2y);
    gfx.drawLine(pW3x, pW3y, pW4x, pW4y);
  }
  gfx.setColor(EPD_BLACK);
  gfx.drawCircle(x+diameter-1,y+diameter,diameter/2+1);
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
String MoonPhase(int d, int m, int y, String hemisphere){
    int c,e;
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
void DrawCircle(int x, int y, int Cstart, int Cend, int Cradius, int Coffset_radius, int Coffset){
  gfx.setColor(EPD_BLACK);
  float dx,dy;
  for (int i=Cstart;i<Cend;i++) {
    dx = (Cradius+Coffset_radius) * cos((i-90)*PI/180) + x + Coffset/2; // calculate X position  
    dy = (Cradius+Coffset_radius) * sin((i-90)*PI/180) + y; // calculate Y position  
    gfx.setPixel(dx,dy);
  }
}
//#########################################################################################
void arrow(int x, int y, int asize, String aangle, int pwidth, int plength){
  float dx = (asize-10)*cos((aangle.toInt()-90)*PI/180)+x; // calculate X position  
  float dy = (asize-10)*sin((aangle.toInt()-90)*PI/180)+y; // calculate Y position  
  float x1 = 0;         float y1 = plength;
  float x2 = pwidth/2;  float y2 = pwidth/2;
  float x3 = -pwidth/2; float y3 = pwidth/2;
  float angle = aangle.toInt()*PI/180-135;
  float xx1 = x1*cos(angle)-y1*sin(angle)+dx;
  float yy1 = y1*cos(angle)+x1*sin(angle)+dy;
  float xx2 = x2*cos(angle)-y2*sin(angle)+dx;
  float yy2 = y2*cos(angle)+x2*sin(angle)+dy;
  float xx3 = x3*cos(angle)-y3*sin(angle)+dx;
  float yy3 = y3*cos(angle)+x3*sin(angle)+dy;
  gfx.fillTriangle(xx1,yy1,xx3,yy3,xx2,yy2);
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
  else if (IconName == "50d" || IconName == "50n")  if (LargeSize) Fog(x,y-5,Large); else Fog(x,y,Small); // Fog and Hazy images are identical
  else if (IconName == "probrain")                  if (LargeSize) ProbRain(x,y,Large); else ProbRain(x,y,Small);
  else                                              if (LargeSize) Nodata(x,y,Large); else Nodata(x,y,Small);
}
//#########################################################################################
bool obtain_wx_data(String RequestType) {
  rxtext = "";
  // close any connection before send a new request to allow client make connection to server
  client.stop();
  // if there's a successful connection:
  if (client.connect(server, 80)) {
    // Serial.println("connecting...");
    // send the HTTP PUT request:
    client.println("GET /data/2.5/"+RequestType+"?q="+City+","+Country+"&APPID="+ apikey +"&mode=json&units=metric&cnt=15 HTTP/1.1");
    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ESP OWM Receiver/1.1");
    client.println("Connection: close");
    client.println();
    lastConnectionTime = millis();
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
      if (c == '}') {jsonend--;}
      if (startJson == true) {rxtext += c;}  // Add in the received character
      // if jsonend = 0 then we have have received equal number of curly braces 
      if (jsonend == 0 && startJson == true) {
        Serial.println("Received OK...");
        Serial.println(rxtext);
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
  Serial.println(F("Creating object..."));
  DynamicJsonBuffer jsonBuffer (10*1024);
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    Serial.print("ParseObject() failed");
    return false;
  }
  Serial.println("Decoding "+Type+" data");
  if (Type == "weather") {
    Serial.println(json);
    JsonObject& location = root["coord"];
    // These Serial.println statements are all diagnostic prints and not required
    String lon           = location["lon"];        Serial.println("Lon:"+lon);
    String lat           = location["lat"];        Serial.println("Lat:"+lat);
    JsonObject& weather  = root["weather"][0];
    String description   = weather["description"]; Serial.println(description);   WxConditions[0].Forecast    = description;
    String icon          = weather["icon"];        Serial.println(icon);          WxConditions[0].Icon        = icon;
    JsonObject& main     = root["main"];
    String temp          = main["temp"];           Serial.println(temp);          WxConditions[0].Temperature = String(temp.toFloat(),1);
    String pressure      = main["pressure"];       Serial.println(pressure);      WxConditions[0].Pressure    = String(pressure.toFloat(),1);
    String temp_min      = main["temp_min"];       Serial.println(temp_min);      WxConditions[0].Low         = String(temp_min.toFloat(),0);
    String temp_max      = main["temp_max"];       Serial.println(temp_max);      WxConditions[0].High        = String(temp_max.toFloat(),0);
    JsonObject& wind     = root["wind"];
    String windspeed     = wind["speed"];          Serial.println(windspeed);     WxConditions[0].Windspeed   = windspeed;
    String winddirection = wind["deg"];            Serial.println(winddirection); WxConditions[0].Winddir     = winddirection;
    JsonObject& sys = root["sys"];
    String country       = sys["country"];         Serial.println(country);
    int    sunrise       = sys["sunrise"];         Serial.println(sunrise);       Sunrise      = sunrise;
    int    sunset        = sys["sunset"];          Serial.println(sunset);        Sunset       = sunset;
  }
  if (Type == "forecast") {
    Serial.println("\nForecast period-0"); //------------------------------------------------
    JsonObject& forecast0 = root["list"][0]["main"]; 
    String temp0          = forecast0["temp"];             Serial.println(temp0);     WxConditions[1].Temperature = String(temp0.toFloat(),1);
    String temp_min0      = forecast0["temp_min"];         Serial.println(temp_min0); WxConditions[1].Low         = String(temp_min0.toFloat(),0);
    String temp_max0      = forecast0["temp_max"];         Serial.println(temp_max0); WxConditions[1].High        = String(temp_max0.toFloat(),0);
    String pressure0      = forecast0["pressure"];         Serial.println(pressure0); WxConditions[1].Pressure    = pressure0;
    JsonObject& rain0     = root["list"][0]["rain"]; 
    String rainfall0      = rain0["3h"];                   Serial.println(rainfall0); WxConditions[1].Rain        = String(rainfall0.toFloat(),2);
    JsonObject& weather0  = root["list"][0]["weather"][0];
    String description0   = weather0["description"];       Serial.println(description0);
    String icon0          = weather0["icon"];              Serial.println(icon0);     WxConditions[1].Icon        = icon0;
    JsonObject& dttext0   = root["list"][0];
    String dt_txt0        = dttext0["dt_txt"];             Serial.println(dt_txt0);   WxConditions[1].Period      = dt_txt0.substring(11,16); //2018-05-24 00:00:00
    
    Serial.println("\nForecast period-1"); //------------------------------------------------
    JsonObject& forecast1 = root["list"][1]["main"]; 
    String temp1          = forecast1["temp"];             Serial.println(temp1);     WxConditions[2].Temperature = String(temp1.toFloat(),1);
    String temp_min1      = forecast1["temp_min"];         Serial.println(temp_min1); WxConditions[2].Low         = String(temp_min1.toFloat(),0);
    String temp_max1      = forecast1["temp_max"];         Serial.println(temp_max1); WxConditions[2].High        = String(temp_max1.toFloat(),0);
    String pressure1      = forecast1["pressure"];         Serial.println(pressure1); WxConditions[2].Pressure    = pressure1;
    JsonObject& rain1     = root["list"][1]["rain"]; 
    String rainfall1      = rain1["3h"];                   Serial.println(rainfall1); WxConditions[2].Rain        = rainfall1;
    JsonObject& weather1  = root["list"][1]["weather"][0];
    String description1   = weather1["description"];       Serial.println(description1);
    String icon1          = weather1["icon"];              Serial.println(icon1);     WxConditions[2].Icon        = icon1;
    JsonObject& dttext1   = root["list"][1];
    String dt_txt1        = dttext1["dt_txt"];             Serial.println(dt_txt1);   WxConditions[2].Period      = dt_txt1.substring(11,16); //2018-05-24 03:00:00

    Serial.println("\nForecast period-2"); //------------------------------------------------
    JsonObject& forecast2 = root["list"][2]["main"]; 
    String temp2          = forecast2["temp"];             Serial.println(temp2);     WxConditions[3].Temperature = String(temp2.toFloat(),1);
    String temp_min2      = forecast2["temp_min"];         Serial.println(temp_min2); WxConditions[3].Low         = String(temp_min2.toFloat(),0);
    String temp_max2      = forecast2["temp_max"];         Serial.println(temp_max2); WxConditions[3].High        = String(temp_max2.toFloat(),0);
    String pressure2      = forecast2["pressure"];         Serial.println(pressure2); WxConditions[3].Pressure    = pressure2;
    JsonObject& rain2     = root["list"][2]["rain"]; 
    String rainfall2      = rain1["3h"];                   Serial.println(rainfall2); WxConditions[3].Rain        = rainfall2;
    JsonObject& weather2  = root["list"][2]["weather"][0];
    String description2   = weather2["description"];       Serial.println(description2);
    String icon2          = weather2["icon"];              Serial.println(icon2);     WxConditions[3].Icon        = icon2;
    JsonObject& dttext2   = root["list"][2];
    String dt_txt2        = dttext2["dt_txt"];             Serial.println(dt_txt2);   WxConditions[3].Period      = dt_txt2.substring(11,16); //2018-05-24 06:00:00

    Serial.println("\nForecast period-3"); //------------------------------------------------
    JsonObject& forecast3 = root["list"][3]["main"]; 
    String temp3          = forecast3["temp"];             Serial.println(temp3);     WxConditions[4].Temperature = String(temp3.toFloat(),1);
    String temp_min3      = forecast3["temp_min"];         Serial.println(temp_min3); WxConditions[4].Low         = String(temp_min3.toFloat(),0);
    String temp_max3      = forecast3["temp_max"];         Serial.println(temp_max3); WxConditions[4].High        = String(temp_max3.toFloat(),0);
    String pressure3      = forecast3["pressure"];         Serial.println(pressure3); WxConditions[4].Pressure    = pressure3;
    JsonObject& rain3     = root["list"][3]["rain"]; 
    String rainfall3      = rain1["3h"];                   Serial.println(rainfall3); WxConditions[4].Rain        = rainfall3;
    JsonObject& weather3  = root["list"][3]["weather"][0];
    String description3   = weather3["description"];       Serial.println(description3);
    String icon3          = weather3["icon"];              Serial.println(icon3);     WxConditions[4].Icon        = icon3;
    JsonObject& dttext3   = root["list"][3];
    String dt_txt3        = dttext3["dt_txt"];             Serial.println(dt_txt3);   WxConditions[4].Period      = dt_txt3.substring(11,16); //2018-05-24 09:00:00

    Serial.println("\nForecast period-4"); //------------------------------------------------
    JsonObject& forecast4 = root["list"][4]["main"]; 
    String temp4          = forecast4["temp"];             Serial.println(temp4);     WxConditions[5].Temperature = String(temp4.toFloat(),1);
    String temp_min4      = forecast4["temp_min"];         Serial.println(temp_min4); WxConditions[5].Low         = String(temp_min4.toFloat(),0);
    String temp_max4      = forecast4["temp_max"];         Serial.println(temp_max4); WxConditions[5].High        = String(temp_max4.toFloat(),0);
    String pressure4      = forecast4["pressure"];         Serial.println(pressure4); WxConditions[5].Pressure    = pressure4;
    JsonObject& rain4     = root["list"][4]["rain"]; 
    String rainfall4      = rain1["3h"];                   Serial.println(rainfall4); WxConditions[5].Rain        = rainfall4;
    JsonObject& weather4  = root["list"][4]["weather"][0];
    String description4   = weather4["description"];       Serial.println(description4);
    String icon4          = weather4["icon"];              Serial.println(icon4);     WxConditions[5].Icon        = icon3;
    JsonObject& dttext4   = root["list"][4];
    String dt_txt4        = dttext4["dt_txt"];             Serial.println(dt_txt4);   WxConditions[5].Period      = dt_txt3.substring(11,16); //2018-05-24 12:00:00

    Serial.println("\nForecast period-5"); //------------------------------------------------
    JsonObject& forecast5 = root["list"][5]["main"]; 
    String temp5          = forecast5["temp"];             Serial.println(temp5);     WxConditions[6].Temperature = String(temp5.toFloat(),1);
    String temp_min5      = forecast5["temp_min"];         Serial.println(temp_min5); WxConditions[6].Low         = String(temp_min5.toFloat(),0);
    String temp_max5      = forecast5["temp_max"];         Serial.println(temp_max5); WxConditions[6].High        = String(temp_max5.toFloat(),0);
    String pressure5      = forecast5["pressure"];         Serial.println(pressure5); WxConditions[6].Pressure    = pressure5;
    JsonObject& rain5     = root["list"][5]["rain"]; 
    String rainfall5      = rain1["3h"];                   Serial.println(rainfall5); WxConditions[6].Rain        = rainfall5;
    JsonObject& weather5  = root["list"][5]["weather"][0];
    String description5   = weather5["description"];       Serial.println(description5);
    String icon5          = weather5["icon"];              Serial.println(icon5);     WxConditions[6].Icon        = icon5;
    JsonObject& dttext5   = root["list"][5];
    String dt_txt5        = dttext5["dt_txt"];             Serial.println(dt_txt5);   WxConditions[6].Period      = dt_txt5.substring(11,16); //2018-05-24 15:00:00

    Serial.println("\nForecast period-6"); //------------------------------------------------
    JsonObject& forecast6 = root["list"][6]["main"]; 
    String temp6          = forecast6["temp"];             Serial.println(temp6);     WxConditions[7].Temperature = String(temp6.toFloat(),1);
    String temp_min6      = forecast6["temp_min"];         Serial.println(temp_min6); WxConditions[7].Low         = String(temp_min6.toFloat(),0);
    String temp_max6      = forecast6["temp_max"];         Serial.println(temp_max6); WxConditions[7].High        = String(temp_max6.toFloat(),0);
    String pressure6      = forecast6["pressure"];         Serial.println(pressure6); WxConditions[7].Pressure    = pressure6;
    JsonObject& rain6     = root["list"][6]["rain"]; 
    String rainfall6      = rain1["3h"];                   Serial.println(rainfall6); WxConditions[7].Rain        = rainfall6;
    JsonObject& weather6  = root["list"][6]["weather"][0];
    String description6   = weather6["description"];       Serial.println(description6);
    String icon6          = weather6["icon"];              Serial.println(icon6);     WxConditions[7].Icon        = icon6;
    JsonObject& dttext6   = root["list"][6];
    String dt_txt6        = dttext6["dt_txt"];             Serial.println(dt_txt6);   WxConditions[7].Period      = dt_txt6.substring(11,16); //2018-05-24 15:00:00

    Serial.println("\nForecast period-7"); //------------------------------------------------
    JsonObject& forecast7 = root["list"][7]["main"]; 
    String temp7          = forecast7["temp"];             Serial.println(temp7);     WxConditions[8].Temperature = String(temp7.toFloat(),1);
    String temp_min7      = forecast7["temp_min"];         Serial.println(temp_min7); WxConditions[8].Low         = String(temp_min7.toFloat(),0);
    String temp_max7      = forecast7["temp_max"];         Serial.println(temp_max7); WxConditions[8].High        = String(temp_max7.toFloat(),0);
    String pressure7      = forecast7["pressure"];         Serial.println(pressure7); WxConditions[8].Pressure    = pressure7;
    JsonObject& rain7     = root["list"][7]["rain"]; 
    String rainfall7      = rain1["3h"];                   Serial.println(rainfall7); WxConditions[8].Rain        = rainfall7;
    JsonObject& weather7  = root["list"][7]["weather"][0];
    String description7   = weather7["description"];       Serial.println(description7);
    String icon7          = weather7["icon"];              Serial.println(icon7);     WxConditions[8].Icon        = icon7;
    JsonObject& dttext7   = root["list"][7];
    String dt_txt7        = dttext7["dt_txt"];             Serial.println(dt_txt7);   WxConditions[8].Period      = dt_txt7.substring(11,16); //2018-05-24 15:00:00

    Serial.println("\nForecast period-8"); //------------------------------------------------
    JsonObject& forecast8 = root["list"][8]["main"]; 
    String temp8          = forecast8["temp"];             Serial.println(temp8);     WxConditions[9].Temperature = String(temp8.toFloat(),1);
    String temp_min8      = forecast8["temp_min"];         Serial.println(temp_min8); WxConditions[9].Low         = String(temp_min8.toFloat(),0);
    String temp_max8      = forecast8["temp_max"];         Serial.println(temp_max8); WxConditions[9].High        = String(temp_max8.toFloat(),0);
    String pressure8      = forecast8["pressure"];         Serial.println(pressure8); WxConditions[9].Pressure    = pressure8;
    JsonObject& rain8     = root["list"][8]["rain"]; 
    String rainfall8      = rain1["3h"];                   Serial.println(rainfall8); WxConditions[9].Rain        = rainfall8;
    JsonObject& weather8  = root["list"][8]["weather"][0];
    String description8   = weather8["description"];       Serial.println(description8);
    String icon8          = weather8["icon"];              Serial.println(icon8);     WxConditions[9].Icon        = icon8;
    JsonObject& dttext8   = root["list"][8];
    String dt_txt8        = dttext8["dt_txt"];             Serial.println(dt_txt8);   WxConditions[9].Period      = dt_txt8.substring(11,16); //2018-05-24 15:00:00

    Serial.println("\nForecast period-9"); //------------------------------------------------
    JsonObject& forecast9 = root["list"][9]["main"]; 
    String temp9          = forecast9["temp"];             Serial.println(temp9);     WxConditions[10].Temperature = String(temp9.toFloat(),1);
    String temp_min9      = forecast9["temp_min"];         Serial.println(temp_min9); WxConditions[10].Low         = String(temp_min9.toFloat(),0);
    String temp_max9      = forecast9["temp_max"];         Serial.println(temp_max9); WxConditions[10].High        = String(temp_max9.toFloat(),0);
    String pressure9      = forecast9["pressure"];         Serial.println(pressure9); WxConditions[10].Pressure    = pressure9;
    JsonObject& rain9     = root["list"][9]["rain"]; 
    String rainfall9      = rain1["3h"];                   Serial.println(rainfall9); WxConditions[10].Rain        = rainfall9;
    JsonObject& weather9  = root["list"][9]["weather"][0];
    String description9   = weather9["description"];       Serial.println(description9);
    String icon9          = weather9["icon"];              Serial.println(icon9);     WxConditions[10].Icon        = icon9;
    JsonObject& dttext9   = root["list"][9];
    String dt_txt9        = dttext9["dt_txt"];             Serial.println(dt_txt9);   WxConditions[10].Period      = dt_txt9.substring(11,16); //2018-05-24 15:00:00

    Serial.println("\nForecast period-10"); //------------------------------------------------
    JsonObject& forecast10 = root["list"][10]["main"]; 
    String temp10          = forecast10["temp"];            Serial.println(temp10);     WxConditions[11].Temperature = String(temp10.toFloat(),1);
    String temp_min10      = forecast10["temp_min"];        Serial.println(temp_min10); WxConditions[11].Low         = String(temp_min10.toFloat(),0);
    String temp_max10      = forecast10["temp_max"];        Serial.println(temp_max10); WxConditions[11].High        = String(temp_max10.toFloat(),0);
    String pressure10      = forecast10["pressure"];        Serial.println(pressure10); WxConditions[11].Pressure    = pressure10;
    JsonObject& rain10     = root["list"][10]["rain"]; 
    String rainfall10      = rain1["3h"];                   Serial.println(rainfall10); WxConditions[11].Rain        = rainfall10;
    JsonObject& weather10  = root["list"][10]["weather"][0];
    String description10   = weather10["description"];      Serial.println(description10);
    String icon10          = weather10["icon"];             Serial.println(icon10);     WxConditions[11].Icon        = icon10;
    JsonObject& dttext10   = root["list"][10];
    String dt_txt10        = dttext10["dt_txt"];            Serial.println(dt_txt10);   WxConditions[11].Period      = dt_txt10.substring(11,16); //2018-05-24 15:00:00

    Serial.println("\nForecast period-11"); //------------------------------------------------
    JsonObject& forecast11 = root["list"][11]["main"]; 
    String temp11          = forecast11["temp"];            Serial.println(temp11);     WxConditions[12].Temperature = String(temp11.toFloat(),1);
    String temp_min11      = forecast11["temp_min"];        Serial.println(temp_min11); WxConditions[12].Low         = String(temp_min11.toFloat(),0);
    String temp_max11      = forecast11["temp_max"];        Serial.println(temp_max11); WxConditions[12].High        = String(temp_max11.toFloat(),0);
    String pressure11      = forecast11["pressure"];        Serial.println(pressure11); WxConditions[12].Pressure    = pressure11;
    JsonObject& rain11     = root["list"][11]["rain"]; 
    String rainfall11      = rain1["3h"];                   Serial.println(rainfall11); WxConditions[12].Rain        = rainfall11;
    JsonObject& weather11  = root["list"][11]["weather"][0];
    String description11   = weather11["description"];      Serial.println(description11);
    String icon11          = weather11["icon"];             Serial.println(icon11);     WxConditions[12].Icon        = icon11;
    JsonObject& dttext11   = root["list"][11];
    String dt_txt11        = dttext11["dt_txt"];            Serial.println(dt_txt11);   WxConditions[12].Period      = dt_txt11.substring(11,16); //2018-05-24 15:00:00

    Serial.println("\nForecast period-12"); //------------------------------------------------
    JsonObject& forecast12 = root["list"][12]["main"]; 
    String temp12          = forecast12["temp"];            Serial.println(temp12);     WxConditions[13].Temperature = String(temp12.toFloat(),1);
    String temp_min12      = forecast12["temp_min"];        Serial.println(temp_min12); WxConditions[13].Low         = String(temp_min12.toFloat(),0);
    String temp_max12      = forecast12["temp_max"];        Serial.println(temp_max12); WxConditions[13].High        = String(temp_max12.toFloat(),0);
    String pressure12      = forecast12["pressure"];        Serial.println(pressure12); WxConditions[13].Pressure    = pressure12;
    JsonObject& rain12     = root["list"][12]["rain"]; 
    String rainfall12      = rain1["3h"];                   Serial.println(rainfall12); WxConditions[13].Rain        = rainfall12;
    JsonObject& weather12  = root["list"][12]["weather"][0];
    String description12   = weather12["description"];      Serial.println(description12);
    String icon12          = weather12["icon"];             Serial.println(icon12);     WxConditions[13].Icon        = icon12;
    JsonObject& dttext12   = root["list"][12];
    String dt_txt12        = dttext12["dt_txt"];            Serial.println(dt_txt12);   WxConditions[13].Period      = dt_txt12.substring(11,16); //2018-05-24 15:00:00

    Serial.println("\nForecast period-13"); //------------------------------------------------
    JsonObject& forecast13 = root["list"][13]["main"]; 
    String temp13          = forecast13["temp"];            Serial.println(temp13);     WxConditions[14].Temperature = String(temp13.toFloat(),1);
    String temp_min13      = forecast13["temp_min"];        Serial.println(temp_min13); WxConditions[14].Low         = String(temp_min13.toFloat(),0);
    String temp_max13      = forecast13["temp_max"];        Serial.println(temp_max13); WxConditions[14].High        = String(temp_max13.toFloat(),0);
    String pressure13      = forecast13["pressure"];        Serial.println(pressure13); WxConditions[14].Pressure    = pressure13;
    JsonObject& rain13     = root["list"][13]["rain"]; 
    String rainfall13      = rain1["3h"];                   Serial.println(rainfall13); WxConditions[14].Rain        = rainfall13;
    JsonObject& weather13  = root["list"][13]["weather"][0];
    String description13   = weather13["description"];      Serial.println(description13);
    String icon13          = weather13["icon"];             Serial.println(icon13);     WxConditions[14].Icon        = icon13;
    JsonObject& dttext13   = root["list"][13];
    String dt_txt13        = dttext13["dt_txt"];            Serial.println(dt_txt13);   WxConditions[14].Period      = dt_txt13.substring(11,16); //2018-05-24 15:00:00

    Serial.println("\nForecast period-14"); //------------------------------------------------
    JsonObject& forecast14 = root["list"][14]["main"]; 
    String temp14          = forecast14["temp"];            Serial.println(temp14);     WxConditions[15].Temperature = String(temp14.toFloat(),1);
    String temp_min14      = forecast14["temp_min"];        Serial.println(temp_min14); WxConditions[15].Low         = String(temp_min14.toFloat(),0);
    String temp_max14      = forecast14["temp_max"];        Serial.println(temp_max14); WxConditions[15].High        = String(temp_max14.toFloat(),0);
    String pressure14      = forecast14["pressure"];        Serial.println(pressure14); WxConditions[15].Pressure    = pressure14;
    JsonObject& rain14     = root["list"][14]["rain"]; 
    String rainfall14      = rain1["3h"];                   Serial.println(rainfall14); WxConditions[15].Rain        = rainfall14;
    JsonObject& weather14  = root["list"][14]["weather"][0];
    String description14   = weather14["description"];      Serial.println(description14);
    String icon14          = weather14["icon"];             Serial.println(icon14);     WxConditions[15].Icon        = icon14;
    JsonObject& dttext14   = root["list"][14];
    String dt_txt14        = dttext14["dt_txt"];            Serial.println(dt_txt14);   WxConditions[15].Period      = dt_txt14.substring(11,16); //2018-05-24 15:00:00
 
    float pressure_trend = WxConditions[1].Pressure.toFloat() - WxConditions[5].Pressure.toFloat(); // Measure pressure slope between ~now and later
    pressure_trend = ((int)(pressure_trend*10))/10.0; // Remove any small variations less than 0.1
    WxConditions[0].Trend = "0";
    if (pressure_trend > 0)  WxConditions[0].Trend = "+";
    if (pressure_trend < 0)  WxConditions[0].Trend = "-";
    if (pressure_trend == 0) WxConditions[0].Trend = "0"; 
    
    if (Units == "I") Convert_Readings_to_Imperial();
  }
  return true;
}
//#########################################################################################
void Convert_Readings_to_Imperial(){
   WxConditions[0].Temperature = String(WxConditions[0].Temperature.toFloat()*9/5.0+32,1);
   WxConditions[0].High        = String(WxConditions[0].High.toFloat()*9/5.0+32,1);
   WxConditions[0].Low         = String(WxConditions[0].Low.toFloat()*9/5.0+32,1);
   WxConditions[0].Pressure    = String(WxConditions[0].Pressure.toFloat()*9/5.0+32,1);
   WxConditions[0].Rain        = String(WxConditions[0].Rain.toFloat()*9/5.0+32,1);
   WxConditions[0].Windspeed   = String(WxConditions[0].Windspeed.toFloat()*9/5.0+32,1);
   WxConditions[1].Low         = String(WxConditions[1].Low.toFloat()*1.8+32,0);
   WxConditions[1].High        = String(WxConditions[1].High.toFloat()*1.8+32,0);
   WxConditions[2].Low         = String(WxConditions[2].Low.toFloat()*1.8+32,0);
   WxConditions[2].High        = String(WxConditions[2].High.toFloat()*1.8+32,0);
   WxConditions[3].Low         = String(WxConditions[3].Low.toFloat()*1.8+32,0);
   WxConditions[3].High        = String(WxConditions[3].High.toFloat()*1.8+32,0);
}
//#########################################################################################
int StartWiFi(){
  int connAttempts = 0;
  Serial.print(F("\r\nConnecting to: ")); Serial.println(String(ssid1));
  WiFi.disconnect();   
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid1, password1);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500); Serial.print(".");
    if(connAttempts > 20) {
      WiFi.disconnect();   
      WiFi.mode(WIFI_STA); 
      WiFi.begin(ssid2, password2);
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
  setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02",1);
  delay(500);
  UpdateLocalTime();
}
//#########################################################################################
void UpdateLocalTime(){
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)){
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
String ConvertUnixTime(int unix_time){  
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
  month  = now_tm->tm_mon+1;  
  year   = 1900 + now_tm->tm_year; // To get just YY information
  MoonDay   = day;
  MoonMonth = month;
  MoonYear  = year;
  if (Units == "M") {
    time_str =  (hour<10?"0"+String(hour):String(hour))+":"+(min<10?"0"+String(min):String(min))+":"+"  ";                                             // HH:MM   05/07/17  
    time_str += (day<10?"0"+String(day):String(day))+"/"+ (month<10?"0"+String(month):String(month))+"/"+(year<10?"0"+String(year):String(year));      // HH:MM   05/07/17  
  }
  else {
    String ampm= "am";
    if (hour > 11) ampm = "pm"; 
    hour = hour%12; if (hour == 0) hour = 12;
    time_str =  (hour%12<10?"0"+String(hour%12):String(hour%12))+":"+(min<10?"0"+String(min):String(min))+ampm+" ";                                    // HH:MMam 07/05/17
    time_str += (month<10?"0"+String(month):String(month))+"/"+(day<10?"0"+String(day):String(day))+"/"+"/"+(year<10?"0"+String(year):String(year));   // HH:MMpm 07/05/17  
  }
  // Returns either '21:12  ' or ' 09:12pm' depending on Units
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
void addfog(int x, int y, int scale, int linesize){
  if (scale == Small) y -= 10;
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
  scale = scale * 1.5;
  addsun(x,y,scale);
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
  addfog(x,y,scale,linesize);
}
//#########################################################################################
void Nodata(int x, int y, int scale){
  if (scale > 1) gfx.setFont(ArialMT_Plain_24); else gfx.setFont(ArialMT_Plain_10);
   gfx.drawString(x-10,y,"N/A");
}
//#########################################################################################
void DrawBattery(int x, int y) {
   uint8_t percentage = 100;
   float voltage = analogRead(35) / 4096.0 * 7.485;
   if (voltage > 4.21) percentage = 100;
   else if (voltage < 3.20) percentage = 0;
   else percentage = (voltage - 3.20) * 100 / (4.21-3.20);
   gfx.setColor(EPD_BLACK);
   gfx.setFont(ArialMT_Plain_10);
   gfx.setTextAlignment(TEXT_ALIGN_RIGHT);  
   gfx.drawString(x-25,y, String(voltage, 2) + "V");
   gfx.drawRect(x - 22, y + 2, 19, 10);
   gfx.fillRect(x - 2, y + 4, 2, 6);   
   gfx.fillRect(x - 20, y + 4, 16 * percentage / 100, 6);
}
//#########################################################################################
/* (C) D L BIRD
 *  This function will draw a graph on a ePaper/TFT/LCD display using data from an array containing data to be graphed.
 *  The variable 'max_readings' determines the maximum number of data elements for each array. Call it with the following parametric data:
 *  x_pos - the x axis top-left position of the graph
 *  y_pos - the y-axis top-left position of the graph, e.g. 100, 200 would draw the graph 100 pixels along and 200 pixels down from the top-left of the screen
 *  width - the width of the graph in pixels
 *  height - height of the graph in pixels
 *  Y1_Max - sets the scale of plotted data, for example 5000 would scale all data to a Y-axis of 5000 maximum
 *  data_array1 is parsed by value, externally they can be called anything else, e.g. within the routine it is called data_array1, but externally could be temperature_readings
 *  auto_scale - a logical value (TRUE or FALSE) that switches the Y-axis autoscale On or Off
 *  barchart_on - a logical value (TRUE or FALSE) that switches the drawing mode between barhcart and line graph
 *  barchart_colour - a sets the title and graph plotting colour
 *  If called with Y!_Max value of 500 and the data never goes above 500, then autoscale will retain a 0-500 Y scale, if on, it will increase the scale to match the data to be displayed, and reduce it accordingly if required.
 *  auto_scale_major_tick, set to 1000 and autoscale with increment the scale in 1000 steps.
 */
 void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, int Y1Min, int Y1Max, String title, float DataArray[], int readings, boolean auto_scale, boolean barchart_mode) {
  #define auto_scale_major_tick 1 // Sets the autoscale increment, so axis steps up in units of e.g. 3
  #define yticks 5                  // 5 y-axis division markers
  int maxYscale = -10000;
  int minYscale =  10000;
  int last_x, last_y;
  int x1,y1,x2,y2;
  if (auto_scale==true) {
    for (int i=1; i <= readings; i++ ) {
      if (DataArray[i] >= maxYscale) maxYscale = DataArray[i];
      if (DataArray[i] <= minYscale) minYscale = DataArray[i];
    }
    maxYscale = round(maxYscale+auto_scale_major_tick); // Auto scale the graph and round to the nearest value defined, default was Y1Max
    Y1Max = round(maxYscale); 
    if (minYscale != 0) minYscale = round(minYscale-auto_scale_major_tick); // Auto scale the graph and round to the nearest value defined, default was Y1Min
    Y1Min = round(minYscale);
  }
  // Draw the graph
  last_x = x_pos+1; 
  last_y = y_pos+(Y1Max-constrain(DataArray[1],Y1Min,Y1Max))/(Y1Max-Y1Min)*gheight;
  gfx.setColor(EPD_BLACK);
  gfx.drawRect(x_pos,y_pos,gwidth+2,gheight+2);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x_pos + gwidth/2,y_pos-17,title);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  // Draw the data
  for(int gx = 1; gx <= readings; gx++){
    if (gx<= readings) {
      x1 = last_x;
      y1 = last_y;
      x2 = x_pos+gx*gwidth/readings; // max_readings is the global variable that sets the maximum data that can be plotted 
      y2 = y_pos+(Y1Max-constrain(DataArray[gx],Y1Min,Y1Max)) / (Y1Max-Y1Min) * gheight;
      if (barchart_mode) { gfx.fillRect(x2,y2,gwidth/readings-3,y_pos+gheight-y2);} else { gfx.drawLine(last_x,last_y,x2,y2); }
      last_x = x2;
      last_y = y2;
    }
  }
  //Draw the Y-axis scale
  for (int spacing = 0; spacing <= yticks; spacing++) {
    #define number_of_dashes 20
    for (int j=0; j < number_of_dashes; j++){ // Draw dashed graph grid lines
      if (spacing < yticks) gfx.drawHorizontalLine((x_pos+1+j*gwidth/number_of_dashes),y_pos+(gheight*spacing/yticks),gwidth/(2*number_of_dashes));
    }
    if ( (Y1Max-(float)(Y1Max-Y1Min)/yticks*spacing) < 10) gfx.drawString(x_pos-2,y_pos+gheight*spacing/yticks-5, String((Y1Max-(float)(Y1Max-Y1Min)/yticks*spacing),1));
    else       gfx.drawString(x_pos-2,y_pos+gheight*spacing/yticks-5, String((Y1Max-(float)(Y1Max-Y1Min)/yticks*spacing),0));
  }
  for (int i=0; i <= 15; i=i+3){
    gfx.drawString(5+x_pos+gwidth/readings*i*1.08,y_pos+gheight+3,String(i/3));
  }
}

