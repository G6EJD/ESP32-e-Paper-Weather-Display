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
#include "credentials.h"
#include <ArduinoJson.h>     // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>            // Built-in
#include "time.h" 
#include <SPI.h>
#include "EPD_WaveShare.h"   // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "MiniGrafx.h"       // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "DisplayDriver.h"   // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "ArialRounded.h"    // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx

#define SCREEN_HEIGHT 128
#define SCREEN_WIDTH 296
#define BITS_PER_PIXEL 1

// defines the colors usable in the paletted 16 color frame buffer
#define EPD_BLACK 0 
#define EPD_WHITE 1 
uint16_t palette[] = { EPD_BLACK, EPD_WHITE };

// pins_arduino.h, e.g. LOLIN32 LITE
//static const uint8_t BUSY  = 4;
//static const uint8_t SS    = 5;
//static const uint8_t RST   = 16;
//static const uint8_t DC    = 17;
//static const uint8_t SCK   = 18;
//static const uint8_t MISO  = 19; // Not used
//static const uint8_t MOSI  = 23;

EPD_WaveShare epd(EPD2_9, 5, 16, 17, 4); // //EPD_WaveShare epd(EPD2_9, CS, RST, DC, BUSY);

MiniGrafx gfx = MiniGrafx(&epd, BITS_PER_PIXEL, palette);

//################# LIBRARIES ##########################
String version = "1";       // Version of this program
//################ VARIABLES ###########################

//------ NETWORK VARIABLES---------
// Use your own API key by signing up for a free developer account at http://www.wunderground.com/weather/api/
unsigned long        lastConnectionTime = 0;          // Last time you connected to the server, in milliseconds
const unsigned long  UpdateInterval     = 30L*60L*1000000L; // Delay between updates, in milliseconds, WU allows 500 requests per-day maximum, set to every 15-mins or more
String Units    =  "M"; // M for Metric or I for Imperial
bool Largesize  = true;
bool Smallsize  = false;
#define Large 7
#define Small 3
String time_str, currCondString; // strings to hold time and received weather data;
String Ctemperature, Cweather, Cicon, Cwinddir, Cwinddegrees, Cwindspeed, Cpressure, Cptrend, Cprecip;

//################ PROGRAM VARIABLES and OBJECTS ################
typedef struct {  
   String WDay;
   String Day;
   String Icon;
   String High;
   String Low;
   String Conditions;
   String Pop;
   String winddir;
   String windangle;
   
} Conditions_record_type;  
 
Conditions_record_type WxConditions[5] = {};       

// Astronomy
String  Dhemisphere, DphaseofMoon, Sunrise, Sunset, Moonrise, Moonset, Moonlight;

const char* host = "api.wunderground.com";

WiFiClient client; // wifi client object

void setup() { 
  Serial.begin(115200);
  StartWiFi();
  SetupTime();
  lastConnectionTime = millis();
  bool Received_Forecast_OK = (obtain_wx_data("conditions") && obtain_wx_data("forecast/astronomy"));
  // Now only refresh the screen if all the data was received OK, otherwise wait until the next timed check
  if (Received_Forecast_OK && (WiFi.status() == WL_CONNECTED)) { 
    //Received data OK at this point so turn off the WiFi to save power
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    gfx.init();
    gfx.setRotation(3);
    gfx.setFont(ArialMT_Plain_10);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.fillBuffer(EPD_BLACK);
    gfx.setColor(EPD_WHITE);
    gfx.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, "Getting data from Weather Underground...");
    gfx.commit();
    delay(2000);
    gfx.setColor(EPD_BLACK);
    gfx.fillBuffer(EPD_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);  
    DisplayForecast();
  }
  esp_sleep_enable_timer_wakeup(UpdateInterval);
  esp_deep_sleep_start(); // Sleep for e.g. 30 minutes
}

void loop() { // this will never run!

}

void DisplayForecast(){ // 2.9" e-paper display is 296x122 resolution
  UpdateLocalTime();
  gfx.setFont(ArialMT_Plain_16);
  gfx.drawString(5,15,String(City));
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(5,0,time_str);
  gfx.drawString(220,0,WxConditions[1].WDay);
  gfx.drawString(245,0,WxConditions[1].Day);
  gfx.setFont(ArialMT_Plain_24);
  gfx.drawLine(0,14,296,14);  
  //Period-0 (Main Icon/Report)
  DisplayWXicon(205,45,Cicon,Largesize); 
  // IDisplayWXicon(205,45,WxConditions[1].Icon,Largesize); if you want the forecast icon
  gfx.drawString(5,32,Ctemperature+"°");
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(Ctemperature.length()*13+8,33,(Units=="I"?"F":"C"));
  gfx.setFont(ArialMT_Plain_10);
  DisplayWXicon(100,32, "probrain",Small);
  DrawWind(265,42,WxConditions[1].winddir,WxConditions[1].windangle);
  gfx.drawString(136,40,WxConditions[1].Pop+"%");
  if (Cprecip.toFloat() > 0) gfx.drawString(105,55,Cprecip+(Units=="M"?"mm":"in")+" total");
  gfx.drawString(180,62,Cpressure+(Units=="M"?"mb":"in"));
  DrawPressureTrend(220,69,Cptrend);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(5,59,Cweather); // or //gfx.drawString(5,62,WxConditions[1].Conditions); // If you want the forecast weather
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawLine(0,77,296,77);
  DrawDayWeather(20,102,2);
  DrawDayWeather(70,102,3);
  DrawDayWeather(120,102,4);
  gfx.drawString(152,75,"Sun:");
  gfx.drawString(152,85,Sunrise);
  gfx.drawString(152,95,Sunset);
  gfx.drawString(190,75,"Moon:");
  gfx.drawString(190,85,Moonrise);
  gfx.drawString(190,95,Moonset);
  gfx.drawString(152,105,"Phase:  ("+Moonlight+"%)");
  gfx.drawString(152,115,DphaseofMoon);
  // DrawMoon(x,y,radius,hemisphere,%illumination,Moon Phase)
  DrawMoon(270,102,18,Dhemisphere, Moonlight, DphaseofMoon); 
  gfx.commit();
  delay(5000); //Needed to draw before the CPU goes to sleep, but I don't know why? It appears there is no check of the display busy state...before commit exits.
}

void DrawDayWeather(int x, int y, int index){
  DisplayWXicon(x+2,y,WxConditions[index].Icon,Smallsize);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x+3,y-25,WxConditions[index].WDay);
  gfx.drawString(x+2,y+11,WxConditions[index].High+"° "+WxConditions[index].Pop+"%");
  gfx.drawLine(x+28,77,x+28,129);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}

void DrawMoon(int16_t x0, int16_t y0, int16_t r, String hemisphere, String illumination, String Phase) {
  Phase.toLowerCase();
  if (Phase != "new moon") if (Phase.indexOf("waxing")==0 || Phase.indexOf("first")==0) Phase = "First"; else Phase = "Second"; 
  int h = 2 * r;
  int m_illumination = 2*r-2*illumination.toInt()*r/50;
  x0 = x0 - m_illumination/2; 
  y0 = y0 - h/2;
  int16_t x1 = x0 + m_illumination; 
  int16_t y1 = y0 + h;
  //~~~~~~~~~~~~~~~~~~~~~~ based on R=50 
  gfx.setColor(EPD_BLACK);
  if (Phase == "new moon") {
    gfx.fillCircle(x0+m_illumination/2,y0+h/2,r);
  }
  else
  {
    if (illumination.toInt() > 0) gfx.drawCircle(x0+m_illumination/2,y0+h/2,r);
    gfx.setColor(EPD_WHITE); 
    //gfx.fillRect(x0+illumination/2,y0+h/2-r,r+1,h+1); = right
    if ((hemisphere == "North" && Phase == "Second") || (hemisphere == "South" && Phase == "First"))  gfx.fillRect(x0+m_illumination/2  ,y0+h/2-r,r+1,h+1); // clear right
    if ((hemisphere == "North" && Phase == "First")  || (hemisphere == "South" && Phase == "Second")) gfx.fillRect(x0+m_illumination/2-r,y0+h/2-r,r  ,h+1); // clear left
    gfx.setColor(EPD_BLACK); 
    long a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1; 
    long dx = 4 * (1 - a) * b * b, dy = 4 * (b1 + 1) * a * a; 
    long err = dx + dy + b1 * a * a, e2; 
    if (x0 > x1) { x0 = x1; x1 += a; } 
    if (y0 > y1) y0 = y1; 
    y0 += (b + 1) / 2;  
    y1 = y0 - b1; 
    a *= 8 * a; 
    b1 = 8 * b * b; 
    do { 
      if (hemisphere == "North" && illumination.toInt() <= 50 && Phase == "First" && illumination.toInt() > 0) { // Left < 50%
        gfx.setPixel(x1, y0); //  2nd Quadrant
        gfx.setPixel(x1, y1); //  3rd Quadrant 
      } 
      if (hemisphere == "North" && illumination.toInt() >= 50 && Phase == "First") { // Left < 50%
        gfx.setPixel(x0, y0); //  2nd Quadrant
        gfx.setPixel(x0, y1); //  3rd Quadrant 
      } 
      if (hemisphere == "North" && illumination.toInt() <= 50 && Phase == "Second" && illumination.toInt() > 0) { // Left < 50%
        gfx.setPixel(x0, y0); //  2nd Quadrant
        gfx.setPixel(x0, y1); //  3rd Quadrant 
      } 
      if (hemisphere == "North" && illumination.toInt() >= 50 && Phase == "Second") { // Right > 50%
        gfx.setPixel(x1, y0); //  1st Quadrant
        gfx.setPixel(x1, y1); //  4th Quadrant
      }
      //-------------------------------------------------------------------------------------
      if (hemisphere == "South" && illumination.toInt() <= 50 && Phase == "First" && illumination.toInt() > 0) { // Left < 50%
        gfx.setPixel(x0, y0); //  2nd Quadrant
        gfx.setPixel(x0, y1); //  3rd Quadrant 
      } 
      if (hemisphere == "South" && illumination.toInt() >= 50 && Phase == "First") { // Left < 50%
        gfx.setPixel(x1, y0); //  2nd Quadrant
        gfx.setPixel(x1, y1); //  3rd Quadrant 
      } 
      if (hemisphere == "South" && illumination.toInt() <= 50 && Phase == "Second" && illumination.toInt() > 0) { // Left < 50%
        gfx.setPixel(x1, y0); //  2nd Quadrant
        gfx.setPixel(x1, y1); //  3rd Quadrant 
      } 
      if (hemisphere == "South" && illumination.toInt() >= 50 && Phase == "Second") { // Right > 50%
        gfx.setPixel(x0, y0); //  1st Quadrant
        gfx.setPixel(x0, y1); //  4th Quadrant
      } 
      e2 = 2 * err; 
      if (e2 >= dx) { x0++; x1--; err += dx += b1; } 
      if (e2 <= dy) { y0++; y1--; err += dy += a; } 
    } while (x0 <= x1); 
    while (y0 - y1 < b) {  
      gfx.setPixel(x0 - 1, ++y0); 
      gfx.setPixel(x0 - 1, --y1); 
    }
  } 
} 

void DrawCircle(int x, int y, int Cstart, int Cend, int Cradius, int Coffset_radius, int Coffset){
  gfx.setColor(EPD_BLACK);
  float dx,dy;
  for (int i=Cstart;i<Cend;i++) {
    dx = (Cradius+Coffset_radius) * cos((i-90)*PI/180) + x + Coffset/2; // calculate X position  
    dy = (Cradius+Coffset_radius) * sin((i-90)*PI/180) + y; // calculate Y position  
    gfx.setPixel(dx,dy);
  }
}

void DrawWind(int x, int y, String dirtext, String angle){
  #define Cradius 15
  float dx = Cradius*cos((angle.toInt()-90)*PI/180)+x;  // calculate X position  
  float dy = Cradius*sin((angle.toInt()-90)*PI/180)+y;  // calculate Y position  
  arrow(x,y,Cradius-3,WxConditions[1].windangle,10,12); // Show wind direction on outer circle
  gfx.drawCircle(x,y,Cradius+2);
  gfx.drawCircle(x,y,Cradius+3);
  for (int m=0;m<360;m=m+45){
    dx = Cradius*cos(m*PI/180); // calculate X position  
    dy = Cradius*sin(m*PI/180); // calculate Y position  
    gfx.drawLine(x+dx,y+dy,x+dx*0.8,y+dy*0.8);
  }
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x,y+Cradius+3,dirtext);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(x,y-Cradius-14,Cwindspeed+(Units=="M"?"kph":"mph"));
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}

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

void DrawPressureTrend(int x, int y, String slope){
  if      (slope == "+") arrow(x+5,y+7,10, "0",   9,10);
  else if (slope == "0") arrow(x,y,10,     "90",  9,10);
  else if (slope == "-") arrow(x+5,y-5,10, "180", 9,10);
  else arrow(x,y,10, "90", 9,10);
}

void DisplayWXicon(int x, int y, String IconName, bool LargeSize){
  Serial.println(IconName);
  if      (IconName == "rain"            || IconName == "nt_rain")
           if (LargeSize) Rain(x,y,Large); else Rain(x,y,Small);
  else if (IconName == "chancerain"      || IconName == "nt_chancerain")
           if (LargeSize) ChanceRain(x,y,Large); else ChanceRain(x,y,Small);
  else if (IconName == "snow"            || IconName == "nt_snow"         ||
           IconName == "flurries"        || IconName == "nt_flurries"     ||
           IconName == "chancesnow"      || IconName == "nt_chancesnow"   ||
           IconName == "chanceflurries"  || IconName == "nt_chanceflurries")
           if (LargeSize) Snow(x,y,Large); else Snow(x,y,Small);
  else if (IconName == "sleet"           || IconName == "nt_sleet"        ||
           IconName == "chancesleet"     || IconName == "nt_chancesleet")
           if (LargeSize) Snow(x,y,Large); else Snow(x,y,Small);
  else if (IconName == "sunny"           || IconName == "nt_sunny"        ||
           IconName == "clear"           || IconName == "nt_clear")
           if (LargeSize) Sunny(x,y,Large); else Sunny(x,y,Small);
  else if (IconName == "partlysunny"     || IconName == "nt_partlysunny"  ||
           IconName == "mostlysunny"     || IconName == "nt_mostlysunny")
           if (LargeSize) MostlySunny(x,y,Large); else MostlySunny(x,y,Small);
  else if (IconName == "cloudy"          || IconName == "nt_cloudy")  
           if (LargeSize) Cloudy(x,y,Large); else Cloudy(x,y,Small);
  else if (IconName == "mostlycloudy"    || IconName == "nt_mostlycloudy" ||
           IconName == "partlycloudy"    || IconName == "nt_partlycloudy")  
           if (LargeSize) MostlyCloudy(x,y,Large); else MostlyCloudy(x,y,Small);
  else if (IconName == "nt_mostlycloudy" || IconName == "nt_partlycloudy")  
           if (LargeSize) MostlyCloudy(x,y,Large); else MostlyCloudy(x,y,Small);
  else if (IconName == "tstorms"         || IconName == "nt_tstorms"      ||
           IconName == "chancetstorms"   || IconName == "nt_chancetstorms")
           if (LargeSize) Tstorms(x,y,Large); else Tstorms(x,y,Small);
  else if (IconName == "fog"             || IconName == "nt_fog"          ||
           IconName == "hazy"            || IconName == "nt_hazy")
           if (LargeSize) Fog(x,y,Large); else Fog(x,y,Small); // Fog and Hazy images are identical
  else if (IconName == "probrain")
           ProbRain(x,y,3);
  else     Nodata(x,y,3);
}

bool obtain_wx_data (String forecast_type) {
  client.stop();  // Clear any current connections
  Serial.println("Connecting to "+String(host)); // start a new connection
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
   Serial.println(F("Connection failed"));
   return false;
  }
  // Weather Underground API address http://api.wunderground.com/api/YOUR_API_KEY/conditions/q/YOUR_STATE/YOUR_CITY.json
  String url = "http://api.wunderground.com/api/"+apikey+"/"+forecast_type+"/q/"+Country+"/"+City+".json";
  Serial.println("Requesting URL: "+String(url));
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
  "Host: " + host + "\r\n" +
  "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(F(">>> Client Connection Timeout...Stopping"));
      client.stop();
      return false;
    }
  }
  Serial.print(F("Receiving API weather data"));
  while(client.available()) {
    currCondString = client.readStringUntil('\r');
    Serial.print(F("."));
  }
  Serial.println(F("\r\nClosing connection"));
  //Serial.println(currCondString); // Uncomment to see all of the received data
  Serial.println("Bytes received: "+String(currCondString.length()));
  bool receive_status;
  if (forecast_type == "conditions")         { receive_status = DecodeWeather_conditions(&currCondString); }
  if (forecast_type == "forecast/astronomy") { receive_status = DecodeWeather_forecast(&currCondString); }
  if (!receive_status) {
    Serial.println(F("Failed to get Weather Data"));
    return false;
  }
  return true;
}

bool DecodeWeather_forecast(String* currCondString) {
  Serial.println(F("Creating object..."));
  DynamicJsonBuffer jsonBuffer(10*1024);
  // Create root object and parse the json file returned from the api. The API returns errors and these need to be checked to ensure successful decoding
  JsonObject& root = jsonBuffer.parseObject(*(currCondString));
  if (!root.success()) {
    Serial.println(F("jsonBuffer.parseObject() failed"));
  }
  JsonObject& forecast = root["forecast"]["simpleforecast"];
  String wday0       = forecast["forecastday"][0]["date"]["weekday_short"];                     WxConditions[1].WDay = wday0;
  String icon0       = forecast["forecastday"][0]["icon"];                                      WxConditions[1].Icon = icon0;
  String high0       = forecast["forecastday"][0]["high"][(Units=="M"?"celsius":"fahrenheit")]; WxConditions[1].High = high0;
  String low0        = forecast["forecastday"][0]["low"][(Units=="M"?"celsius":"fahrenheit")];  WxConditions[1].Low  = low0;
  String conditions0 = forecast["forecastday"][0]["conditions"];                                WxConditions[1].Conditions = conditions0;
  String pop0        = forecast["forecastday"][0]["pop"];                                       WxConditions[1].Pop  = pop0;
  int    day0        = forecast["forecastday"][0]["date"]["day"];  
  String Day0; day0<10?(Day0="0"+String(day0)):(Day0=String(day0)); // Add a leading zero if < 10
  String mon0        = forecast["forecastday"][0]["date"]["monthname_short"];
  String year0       = forecast["forecastday"][0]["date"]["year"];           
  if (Units == "M") WxConditions[1].Day = Day0+"-"+mon0+"-"+year0.substring(2); // dd-mmm-yy
  else              WxConditions[1].Day = mon0+"-"+Day0+"-"+year0.substring(2); // mmm-dd-yy
  String windDir0    = forecast["forecastday"][0]["avewind"]["dir"];
  String windAngle0  = forecast["forecastday"][0]["avewind"]["degrees"];
  WxConditions[1].winddir   = windDir0;
  WxConditions[1].windangle = windAngle0;
  //-------------------------------------------------------------------------------------------------------------------------
  String wday1       = forecast["forecastday"][1]["date"]["weekday_short"];                     WxConditions[2].WDay = wday1;
  String icon1       = forecast["forecastday"][1]["icon"];                                      WxConditions[2].Icon = icon1;
  String high1       = forecast["forecastday"][1]["high"][(Units=="M"?"celsius":"fahrenheit")]; WxConditions[2].High = high1;
  String low1        = forecast["forecastday"][1]["low"][(Units=="M"?"celsius":"fahrenheit")];  WxConditions[2].Low  = low1;
  String conditions1 = forecast["forecastday"][1]["conditions"];                                WxConditions[2].Conditions = conditions1;
  String pop1        = forecast["forecastday"][1]["pop"];                                       WxConditions[2].Pop  = pop1;
  //-------------------------------------------------------------------------------------------------------------------------
  String wday2       = forecast["forecastday"][2]["date"]["weekday_short"];                     WxConditions[3].WDay = wday2;
  String icon2       = forecast["forecastday"][2]["icon"];                                      WxConditions[3].Icon = icon2;
  String high2       = forecast["forecastday"][2]["high"][(Units=="M"?"celsius":"fahrenheit")]; WxConditions[3].High = high2;
  String low2        = forecast["forecastday"][2]["low"][(Units=="M"?"celsius":"fahrenheit")];  WxConditions[3].Low  = low2;
  String conditions2 = forecast["forecastday"][2]["conditions"];                                WxConditions[3].Conditions  = conditions2;
  String pop2        = forecast["forecastday"][2]["pop"];                                       WxConditions[3].Pop  = pop2;
  //-------------------------------------------------------------------------------------------------------------------------
  String wday3       = forecast["forecastday"][3]["date"]["weekday_short"];                     WxConditions[4].WDay = wday3;
  String icon3       = forecast["forecastday"][3]["icon"];                                      WxConditions[4].Icon = icon3;
  String high3       = forecast["forecastday"][3]["high"][(Units=="M"?"celsius":"fahrenheit")]; WxConditions[4].High = high3;
  String low3        = forecast["forecastday"][3]["low"][(Units=="M"?"celsius":"fahrenheit")];  WxConditions[4].Low  = low3;
  String conditions3 = forecast["forecastday"][3]["conditions"];                                WxConditions[4].Conditions = conditions3;
  String pop3        = forecast["forecastday"][3]["pop"];                                       WxConditions[4].Pop  = pop3;
  //-------------------------------------------------------------------------------------------------------------------------
  JsonObject& moon = root["moon_phase"];
  String percentIlluminated = moon["percentIlluminated"];
  String phaseofMoon        = moon["phaseofMoon"];
  String hemisphere         = moon["hemisphere"];
  int SRhour                = moon["sunrise"]["hour"];
  int SRminute              = moon["sunrise"]["minute"];
  int SShour                = moon["sunset"]["hour"];
  int SSminute              = moon["sunset"]["minute"];
  int MRhour                = moon["moonrise"]["hour"];
  int MRminute              = moon["moonrise"]["minute"];
  int MShour                = moon["moonset"]["hour"];
  int MSminute              = moon["moonset"]["minute"];
  Sunrise   = (SRhour<10?"0":"")+String(SRhour)+":"+(SRminute<10?"0":"")+String(SRminute);
  Sunset    = (SShour<10?"0":"")+String(SShour)+":"+(SSminute<10?"0":"")+String(SSminute);
  Moonrise  = (MRhour<10?"0":"")+String(MRhour)+":"+(MRminute<10?"0":"")+String(MRminute);
  Moonset   = (MShour<10?"0":"")+String(MShour)+":"+(MSminute<10?"0":"")+String(MSminute);
  Moonlight = percentIlluminated;
  DphaseofMoon = phaseofMoon;
  Dhemisphere  = hemisphere;
  return true;
}

bool DecodeWeather_conditions(String* currCondString) {
  Serial.println(F("Creating object..."));
  DynamicJsonBuffer jsonBuffer(13*1024);
  // Create root object and parse the json file returned from the api. The API returns errors and these need to be checked to ensure successful decoding
  JsonObject& root = jsonBuffer.parseObject(*(currCondString));
  if (!root.success()) {
    Serial.println(F("jsonBuffer.parseObject() failed"));
  }
  JsonObject& current = root["current_observation"];
  String ctemp        = current[(Units=="M"?"temp_c":"temp_f")];
  String cweather     = current["weather"];
  String cwinddir     = current["wind_dir"];
  String cwinddegrees = current["wind_degrees"];
  String cwindspeed   = current[(Units=="M"?"wind_kph":"wind_mph")];
  String cicon        = current["icon"];
  String cpressure    = current[(Units=="M"?"pressure_mb":"pressure_in")];
  String cptrend      = current["pressure_trend"];
  String cprecip      = current[(Units=="M"?"precip_today_metric":"precip_today_in")];
  Ctemperature        = ctemp;
  Cweather            = cweather;
  Cicon               = cicon;
  Cwinddir            = cwinddir;
  Cwindspeed          = cwindspeed;
  Cwinddegrees        = cwinddegrees;
  Cpressure           = cpressure;
  Cptrend             = cptrend;
  Cprecip             = cprecip;
  return true;
}

int StartWiFi(){
  int connAttempts = 0;
  Serial.print(F("\r\nConnecting to: ")); Serial.println(String(ssid1));
  WiFi.disconnect();   
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid1, password1);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500); Serial.print(F("."));
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

void SetupTime(){
  configTime(0, 0, "0.uk.pool.ntp.org", "time.nist.gov");
  setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02",1);
  delay(200);
  UpdateLocalTime();
}

void UpdateLocalTime(){
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)){
    Serial.println(F("Failed to obtain time"));
  }
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S"); // Displays: Saturday, June 24 2017 14:05:49
  Serial.println(&timeinfo, "%H:%M:%S"); // Displays: 14:05:49
  char output[30];
  if (Units == "M")
    strftime(output, 30, "( Updated: %H:%M:%S )", &timeinfo); // Displays: Sat 24-Jun-17 14:05:49
  else
    strftime(output, 30, "( Updated: %r )", &timeinfo); // Displays: Sat Jun-24-17 2:05:49pm
  time_str = output;
}

//###########################################################################
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

void addfog(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  for (int i = 0; i < 5; i++){
    gfx.fillRect(x-scale*3, y+scale*1.5, scale*6, linesize); 
    gfx.fillRect(x-scale*3, y+scale*2,   scale*6, linesize); 
    gfx.fillRect(x-scale*3, y+scale*2.5, scale*6, linesize); 
  }
}

void MostlyCloudy(int x, int y, int scale){ 
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize); 
  addsun(x-scale*1.8,y-scale*1.8,scale); 
  addcloud(x,y,scale,linesize); 
}

void MostlySunny(int x, int y, int scale){ 
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize); 
  addsun(x-scale*1.8,y-scale*1.8,scale); 
}
 
void Rain(int x, int y, int scale){ 
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize); 
  addrain(x,y,scale); 
} 

void ProbRain(int x, int y, int scale){
  x = x + 20;
  y = y + 15;
  addcloud(x,y,scale,1);
  y = y + scale/2;
  for (int i = 0; i < 6; i++){
    gfx.drawLine(x-scale*4+scale*i*1.3+0, y+scale*1.9, x-scale*3.5+scale*i*1.3+0, y+scale);
  }
}

void Cloudy(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
}

void Sunny(int x, int y, int scale){
  scale = scale * 1.5;
  addsun(x,y,scale);
}

void ExpectRain(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addsun(x-scale*1.8,y-scale*1.8,scale); 
  addcloud(x,y,scale,linesize);
  addrain(x,y,scale);
}

void ChanceRain(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addsun(x-scale*1.8,y-scale*1.8,scale); 
  addcloud(x,y,scale,linesize);
  addrain(x,y,scale);
}

void Tstorms(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
  addtstorm(x,y,scale);
}

void Snow(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
  addsnow(x,y,scale);
}

void Fog(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
  addfog(x,y,scale);
}

void Nodata(int x, int y, int scale){
  if (scale > 1)   gfx.setFont(ArialMT_Plain_16); else   gfx.setFont(ArialMT_Plain_10);
   gfx.drawString(x-10,y-40,"?");
}

//###########################################################################
/* 'Conditions' response
{
  "response": {
  "version": "0.1",
  "termsofService": "http://www.wunderground.com/weather/api/d/terms.html",
  "features": {
  "conditions": 1
  }
  },
  "current_observation": {
  "image": {
  "url": "http://icons-ak.wxug.com/graphics/wu2/logo_130x80.png",
  "title": "Weather Underground",
  "link": "http://www.wunderground.com"
  },
  "display_location": {
  "full": "San Francisco, CA",
  "city": "San Francisco",
  "state": "CA",
  "state_name": "California",
  "country": "US",
  "country_iso3166": "US",
  "zip": "94101",
  "latitude": "37.77500916",
  "longitude": "-122.41825867",
  "elevation": "47.00000000"
  },
  "observation_location": {
  "full": "SOMA - Near Van Ness, San Francisco, California",
  "city": "SOMA - Near Van Ness, San Francisco",
  "state": "California",
  "country": "US",
  "country_iso3166": "US",
  "latitude": "37.773285",
  "longitude": "-122.417725",
  "elevation": "49 ft"
  },
  "estimated": {},
  "station_id": "KCASANFR58",
  "observation_time": "Last Updated on June 27, 5:27 PM PDT",
  "observation_time_rfc822": "Wed, 27 Jun 2012 17:27:13 -0700",
  "observation_epoch": "1340843233",
  "local_time_rfc822": "Wed, 27 Jun 2012 17:27:14 -0700",
  "local_epoch": "1340843234",
  "local_tz_short": "PDT",
  "local_tz_long": "America/Los_Angeles",
  "local_tz_offset": "-0700",
  "weather": "Partly Cloudy",
  "temperature_string": "66.3 F (19.1 C)",
  "temp_f": 66.3,
  "temp_c": 19.1,
  "relative_humidity": "65%",
  "wind_string": "From the NNW at 22.0 MPH Gusting to 28.0 MPH",
  "wind_dir": "NNW",
  "wind_degrees": 346,
  "wind_mph": 22.0,
  "wind_gust_mph": "28.0",
  "wind_kph": 35.4,
  "wind_gust_kph": "45.1",
  "pressure_mb": "1013",
  "pressure_in": "29.93",
  "pressure_trend": "+",
  "dewpoint_string": "54 F (12 C)",
  "dewpoint_f": 54,
  "dewpoint_c": 12,
  "heat_index_string": "NA",
  "heat_index_f": "NA",
  "heat_index_c": "NA",
  "windchill_string": "NA",
  "windchill_f": "NA",
  "windchill_c": "NA",
  "feelslike_string": "66.3 F (19.1 C)",
  "feelslike_f": "66.3",
  "feelslike_c": "19.1",
  "visibility_mi": "10.0",
  "visibility_km": "16.1",
  "solarradiation": "",
  "UV": "5",
  "precip_1hr_string": "0.00 in ( 0 mm)",
  "precip_1hr_in": "0.00",
  "precip_1hr_metric": " 0",
  "precip_today_string": "0.00 in (0 mm)",
  "precip_today_in": "0.00",
  "precip_today_metric": "0",
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "forecast_url": "http://www.wunderground.com/US/CA/San_Francisco.html",
  "history_url": "http://www.wunderground.com/history/airport/KCASANFR58/2012/6/27/DailyHistory.html",
  "ob_url": "http://www.wunderground.com/cgi-bin/findweather/getForecast?query=37.773285,-122.417725"
  }
}
//#############################################################################
''Forecast' Response
{
  "response": {
  "version": "0.1",
  "termsofService": "http://www.wunderground.com/weather/api/d/terms.html",
  "features": {
  "forecast": 1
  }
  },
  "forecast": {
  "txt_forecast": {
  "date": "2:00 PM PDT",
  "forecastday": [{
  "period": 0,
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "title": "Tuesday",
  "fcttext": "Partly cloudy in the morning, then clear. High of 68F. Breezy. Winds from the West at 10 to 25 mph.",
  "fcttext_metric": "Partly cloudy in the morning, then clear. High of 20C. Windy. Winds from the West at 20 to 35 km/h.",
  "pop": "0"
  }, {
  "period": 1,
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "title": "Tuesday Night",
  "fcttext": "Mostly cloudy. Fog overnight. Low of 50F. Winds from the WSW at 5 to 15 mph.",
  "fcttext_metric": "Mostly cloudy. Fog overnight. Low of 10C. Breezy. Winds from the WSW at 10 to 20 km/h.",
  "pop": "0"
  }, {
  "period": 2,
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "title": "Wednesday",
  "fcttext": "Mostly cloudy. Fog early. High of 72F. Winds from the WSW at 10 to 15 mph.",
  "fcttext_metric": "Mostly cloudy. Fog early. High of 22C. Breezy. Winds from the WSW at 15 to 20 km/h.",
  "pop": "0"
  }, {
  "period": 3,
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/mostlycloudy.gif",
  "title": "Wednesday Night",
  "fcttext": "Overcast. Fog overnight. Low of 54F. Winds from the WSW at 5 to 15 mph.",
  "fcttext_metric": "Overcast. Fog overnight. Low of 12C. Breezy. Winds from the WSW at 10 to 20 km/h.",
  "pop": "0"
  }, {
  "period": 4,
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "title": "Thursday",
  "fcttext": "Overcast in the morning, then partly cloudy. Fog early. High of 72F. Winds from the WSW at 10 to 15 mph.",
  "fcttext_metric": "Overcast in the morning, then partly cloudy. Fog early. High of 22C. Breezy. Winds from the WSW at 15 to 25 km/h.",
  "pop": "0"
  }, {
  "period": 5,
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "title": "Thursday Night",
  "fcttext": "Partly cloudy in the evening, then overcast. Fog overnight. Low of 54F. Winds from the WNW at 5 to 15 mph.",
  "fcttext_metric": "Partly cloudy in the evening, then overcast. Fog overnight. Low of 12C. Breezy. Winds from the WNW at 10 to 20 km/h.",
  "pop": "0"
  }, {
  "period": 6,
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "title": "Friday",
  "fcttext": "Overcast in the morning, then partly cloudy. Fog early. High of 68F. Winds from the West at 5 to 15 mph.",
  "fcttext_metric": "Overcast in the morning, then partly cloudy. Fog early. High of 20C. Breezy. Winds from the West at 10 to 20 km/h.",
  "pop": "0"
  }, {
  "period": 7,
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "title": "Friday Night",
  "fcttext": "Mostly cloudy. Fog overnight. Low of 52F. Winds from the West at 5 to 10 mph.",
  "fcttext_metric": "Mostly cloudy. Fog overnight. Low of 11C. Winds from the West at 10 to 15 km/h.",
  "pop": "0"
  }]
  },
  "simpleforecast": {
  "forecastday": [{
  "date": {
  "epoch": "1340776800",
  "pretty": "11:00 PM PDT on June 26, 2012",
  "day": 26,
  "month": 6,
  "year": 2012,
  "yday": 177,
  "hour": 23,
  "min": "00",
  "sec": 0,
  "isdst": "1",
  "monthname": "June",
  "weekday_short": "Tue",
  "weekday": "Tuesday",
  "ampm": "PM",
  "tz_short": "PDT",
  "tz_long": "America/Los_Angeles"
  },
  "period": 1,
  "high": {
  "fahrenheit": "68",
  "celsius": "20"
  },
  "low": {
  "fahrenheit": "50",
  "celsius": "10"
  },
  "conditions": "Partly Cloudy",
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "skyicon": "mostlysunny",
  "pop": 0,
  "qpf_allday": {
  "in": 0.00,
  "mm": 0.0
  },
  "qpf_day": {
  "in": 0.00,
  "mm": 0.0
  },
  "qpf_night": {
  "in": 0.00,
  "mm": 0.0
  },
  "snow_allday": {
  "in": 0,
  "cm": 0
  },
  "snow_day": {
  "in": 0,
  "cm": 0
  },
  "snow_night": {
  "in": 0,
  "cm": 0
  },
  "maxwind": {
  "mph": 21,
  "kph": 34,
  "dir": "West",
  "degrees": 272
  },
  "avewind": {
  "mph": 17,
  "kph": 27,
  "dir": "West",
  "degrees": 272
  },
  "avehumidity": 72,
  "maxhumidity": 94,
  "minhumidity": 58
  }, {
  "date": {
  "epoch": "1340863200",
  "pretty": "11:00 PM PDT on June 27, 2012",
  "day": 27,
  "month": 6,
  "year": 2012,
  "yday": 178,
  "hour": 23,
  "min": "00",
  "sec": 0,
  "isdst": "1",
  "monthname": "June",
  "weekday_short": "Wed",
  "weekday": "Wednesday",
  "ampm": "PM",
  "tz_short": "PDT",
  "tz_long": "America/Los_Angeles"
  },
  "period": 2,
  "high": {
  "fahrenheit": "72",
  "celsius": "22"
  },
  "low": {
  "fahrenheit": "54",
  "celsius": "12"
  },
  "conditions": "Partly Cloudy",
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "skyicon": "mostlysunny",
  "pop": 0,
  "qpf_allday": {
  "in": 0.00,
  "mm": 0.0
  },
  "qpf_day": {
  "in": 0.00,
  "mm": 0.0
  },
  "qpf_night": {
  "in": 0.00,
  "mm": 0.0
  },
  "snow_allday": {
  "in": 0,
  "cm": 0
  },
  "snow_day": {
  "in": 0,
  "cm": 0
  },
  "snow_night": {
  "in": 0,
  "cm": 0
  },
  "maxwind": {
  "mph": 11,
  "kph": 18,
  "dir": "WSW",
  "degrees": 255
  },
  "avewind": {
  "mph": 9,
  "kph": 14,
  "dir": "WSW",
  "degrees": 252
  },
  "avehumidity": 70,
  "maxhumidity": 84,
  "minhumidity": 54
  }, {
  "date": {
  "epoch": "1340949600",
  "pretty": "11:00 PM PDT on June 28, 2012",
  "day": 28,
  "month": 6,
  "year": 2012,
  "yday": 179,
  "hour": 23,
  "min": "00",
  "sec": 0,
  "isdst": "1",
  "monthname": "June",
  "weekday_short": "Thu",
  "weekday": "Thursday",
  "ampm": "PM",
  "tz_short": "PDT",
  "tz_long": "America/Los_Angeles"
  },
  "period": 3,
  "high": {
  "fahrenheit": "72",
  "celsius": "22"
  },
  "low": {
  "fahrenheit": "54",
  "celsius": "12"
  },
  "conditions": "Partly Cloudy",
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "skyicon": "partlycloudy",
  "pop": 0,
  "qpf_allday": {
  "in": 0.00,
  "mm": 0.0
  },
  "qpf_day": {
  "in": 0.00,
  "mm": 0.0
  },
  "qpf_night": {
  "in": 0.00,
  "mm": 0.0
  },
  "snow_allday": {
  "in": 0,
  "cm": 0
  },
  "snow_day": {
  "in": 0,
  "cm": 0
  },
  "snow_night": {
  "in": 0,
  "cm": 0
  },
  "maxwind": {
  "mph": 14,
  "kph": 22,
  "dir": "West",
  "degrees": 265
  },
  "avewind": {
  "mph": 12,
  "kph": 19,
  "dir": "WSW",
  "degrees": 256
  },
  "avehumidity": 80,
  "maxhumidity": 91,
  "minhumidity": 56
  }, {
  "date": {
  "epoch": "1341036000",
  "pretty": "11:00 PM PDT on June 29, 2012",
  "day": 29,
  "month": 6,
  "year": 2012,
  "yday": 180,
  "hour": 23,
  "min": "00",
  "sec": 0,
  "isdst": "1",
  "monthname": "June",
  "weekday_short": "Fri",
  "weekday": "Friday",
  "ampm": "PM",
  "tz_short": "PDT",
  "tz_long": "America/Los_Angeles"
  },
  "period": 4,
  "high": {
  "fahrenheit": "68",
  "celsius": "20"
  },
  "low": {
  "fahrenheit": "52",
  "celsius": "11"
  },
  "conditions": "Fog",
  "icon": "partlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/partlycloudy.gif",
  "skyicon": "mostlysunny",
  "pop": 0,
  "qpf_allday": {
  "in": 0.00,
  "mm": 0.0
  },
  "qpf_day": {
  "in": 0.00,
  "mm": 0.0
  },
  "qpf_night": {
  "in": 0.00,
  "mm": 0.0
  },
  "snow_allday": {
  "in": 0,
  "cm": 0
  },
  "snow_day": {
  "in": 0,
  "cm": 0
  },
  "snow_night": {
  "in": 0,
  "cm": 0
  },
  "maxwind": {
  "mph": 11,
  "kph": 18,
  "dir": "West",
  "degrees": 267
  },
  "avewind": {
  "mph": 10,
  "kph": 16,
  "dir": "West",
  "degrees": 272
  },
  "avehumidity": 79,
  "maxhumidity": 93,
  "minhumidity": 63
  }]
  }
  }
}  
  
//#############################################################################
'Hourly' Response
{
  "response": {
  "version": "0.1",
  "termsofService": "http://www.wunderground.com/weather/api/d/terms.html",
  "features": {
  "hourly": 1
  }
  },
  "hourly_forecast": [{
  "FCTTIME": {
  "hour": "11",
  "hour_padded": "11",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341338400",
  "pretty": "11:00 AM PDT on July 03, 2012",
  "civil": "11:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "66",
  "metric": "19"
  },
  "dewpoint": {
  "english": "55",
  "metric": "13"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "22",
  "wspd": {
  "english": "5",
  "metric": "8"
  },
  "wdir": {
  "dir": "West",
  "degrees": "260"
  },
  "wx": "",
  "uvi": "6",
  "humidity": "65",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "66",
  "metric": "19"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "0.00",
  "metric": "0.00"
  },
  "pop": "0",
  "mslp": {
  "english": "29.90",
  "metric": "1012"
  }
  }, {
  "FCTTIME": {
  "hour": "12",
  "hour_padded": "12",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341342000",
  "pretty": "12:00 PM PDT on July 03, 2012",
  "civil": "12:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "67",
  "metric": "19"
  },
  "dewpoint": {
  "english": "55",
  "metric": "13"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "22",
  "wspd": {
  "english": "9",
  "metric": "14"
  },
  "wdir": {
  "dir": "West",
  "degrees": "260"
  },
  "wx": "",
  "uvi": "6",
  "humidity": "64",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "67",
  "metric": "19"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.90",
  "metric": "1012"
  }
  }, {
  "FCTTIME": {
  "hour": "13",
  "hour_padded": "13",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341345600",
  "pretty": "1:00 PM PDT on July 03, 2012",
  "civil": "1:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "67",
  "metric": "20"
  },
  "dewpoint": {
  "english": "55",
  "metric": "13"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "22",
  "wspd": {
  "english": "13",
  "metric": "21"
  },
  "wdir": {
  "dir": "West",
  "degrees": "260"
  },
  "wx": "",
  "uvi": "6",
  "humidity": "63",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "67",
  "metric": "20"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.90",
  "metric": "1012"
  }
  }, {
  "FCTTIME": {
  "hour": "14",
  "hour_padded": "14",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341349200",
  "pretty": "2:00 PM PDT on July 03, 2012",
  "civil": "2:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "68",
  "metric": "20"
  },
  "dewpoint": {
  "english": "55",
  "metric": "13"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "22",
  "wspd": {
  "english": "17",
  "metric": "27"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "255"
  },
  "wx": "",
  "uvi": "7",
  "humidity": "62",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "68",
  "metric": "20"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.87",
  "metric": "1011"
  }
  }, {
  "FCTTIME": {
  "hour": "15",
  "hour_padded": "15",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341352800",
  "pretty": "3:00 PM PDT on July 03, 2012",
  "civil": "3:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "68",
  "metric": "20"
  },
  "dewpoint": {
  "english": "55",
  "metric": "13"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "22",
  "wspd": {
  "english": "17",
  "metric": "27"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "255"
  },
  "wx": "",
  "uvi": "7",
  "humidity": "62",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "68",
  "metric": "20"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.87",
  "metric": "1011"
  }
  }, {
  "FCTTIME": {
  "hour": "16",
  "hour_padded": "16",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341356400",
  "pretty": "4:00 PM PDT on July 03, 2012",
  "civil": "4:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "68",
  "metric": "20"
  },
  "dewpoint": {
  "english": "55",
  "metric": "13"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "22",
  "wspd": {
  "english": "17",
  "metric": "27"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "255"
  },
  "wx": "",
  "uvi": "7",
  "humidity": "63",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "68",
  "metric": "20"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.87",
  "metric": "1011"
  }
  }, {
  "FCTTIME": {
  "hour": "17",
  "hour_padded": "17",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341360000",
  "pretty": "5:00 PM PDT on July 03, 2012",
  "civil": "5:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "68",
  "metric": "20"
  },
  "dewpoint": {
  "english": "55",
  "metric": "13"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "4",
  "wspd": {
  "english": "17",
  "metric": "27"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "247"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "63",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "68",
  "metric": "20"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "0.00",
  "metric": "0.00"
  },
  "pop": "0",
  "mslp": {
  "english": "29.82",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "18",
  "hour_padded": "18",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341363600",
  "pretty": "6:00 PM PDT on July 03, 2012",
  "civil": "6:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "67",
  "metric": "20"
  },
  "dewpoint": {
  "english": "55",
  "metric": "13"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "4",
  "wspd": {
  "english": "15",
  "metric": "24"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "247"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "63",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "67",
  "metric": "20"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.82",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "19",
  "hour_padded": "19",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341367200",
  "pretty": "7:00 PM PDT on July 03, 2012",
  "civil": "7:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "67",
  "metric": "19"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "4",
  "wspd": {
  "english": "13",
  "metric": "21"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "247"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "62",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "67",
  "metric": "19"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.82",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "20",
  "hour_padded": "20",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341370800",
  "pretty": "8:00 PM PDT on July 03, 2012",
  "civil": "8:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "66",
  "metric": "19"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "4",
  "wspd": {
  "english": "11",
  "metric": "18"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "248"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "62",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "66",
  "metric": "19"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.81",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "21",
  "hour_padded": "21",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341374400",
  "pretty": "9:00 PM PDT on July 03, 2012",
  "civil": "9:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "63",
  "metric": "17"
  },
  "dewpoint": {
  "english": "53",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_clear.gif",
  "fctcode": "1",
  "sky": "4",
  "wspd": {
  "english": "10",
  "metric": "17"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "248"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "68",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "63",
  "metric": "17"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.81",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "22",
  "hour_padded": "22",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341378000",
  "pretty": "10:00 PM PDT on July 03, 2012",
  "civil": "10:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "60",
  "metric": "16"
  },
  "dewpoint": {
  "english": "53",
  "metric": "11"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_clear.gif",
  "fctcode": "1",
  "sky": "4",
  "wspd": {
  "english": "10",
  "metric": "15"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "248"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "75",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "60",
  "metric": "16"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.81",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "23",
  "hour_padded": "23",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "3",
  "mday_padded": "03",
  "yday": "184",
  "isdst": "1",
  "epoch": "1341381600",
  "pretty": "11:00 PM PDT on July 03, 2012",
  "civil": "11:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Tuesday",
  "weekday_name_night": "Tuesday Night",
  "weekday_name_abbrev": "Tue",
  "weekday_name_unlang": "Tuesday",
  "weekday_name_night_unlang": "Tuesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "57",
  "metric": "14"
  },
  "dewpoint": {
  "english": "52",
  "metric": "11"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_mostlycloudy.gif",
  "fctcode": "3",
  "sky": "87",
  "wspd": {
  "english": "9",
  "metric": "14"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "211"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "81",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "57",
  "metric": "14"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "0.00",
  "metric": "0.00"
  },
  "pop": "0",
  "mslp": {
  "english": "29.84",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "0",
  "hour_padded": "00",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341385200",
  "pretty": "12:00 AM PDT on July 04, 2012",
  "civil": "12:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "57",
  "metric": "14"
  },
  "dewpoint": {
  "english": "52",
  "metric": "11"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_mostlycloudy.gif",
  "fctcode": "3",
  "sky": "87",
  "wspd": {
  "english": "9",
  "metric": "14"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "211"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "81",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "57",
  "metric": "14"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.84",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "1",
  "hour_padded": "01",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341388800",
  "pretty": "1:00 AM PDT on July 04, 2012",
  "civil": "1:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "57",
  "metric": "14"
  },
  "dewpoint": {
  "english": "52",
  "metric": "11"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_mostlycloudy.gif",
  "fctcode": "3",
  "sky": "87",
  "wspd": {
  "english": "9",
  "metric": "14"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "211"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "80",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "57",
  "metric": "14"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.84",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "2",
  "hour_padded": "02",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341392400",
  "pretty": "2:00 AM PDT on July 04, 2012",
  "civil": "2:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "57",
  "metric": "14"
  },
  "dewpoint": {
  "english": "52",
  "metric": "11"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_mostlycloudy.gif",
  "fctcode": "3",
  "sky": "87",
  "wspd": {
  "english": "9",
  "metric": "14"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "210"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "80",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "57",
  "metric": "14"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.81",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "3",
  "hour_padded": "03",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341396000",
  "pretty": "3:00 AM PDT on July 04, 2012",
  "civil": "3:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "56",
  "metric": "14"
  },
  "dewpoint": {
  "english": "51",
  "metric": "11"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_mostlycloudy.gif",
  "fctcode": "3",
  "sky": "87",
  "wspd": {
  "english": "9",
  "metric": "14"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "210"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "80",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "56",
  "metric": "14"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.81",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "4",
  "hour_padded": "04",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341399600",
  "pretty": "4:00 AM PDT on July 04, 2012",
  "civil": "4:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "56",
  "metric": "13"
  },
  "dewpoint": {
  "english": "51",
  "metric": "10"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_mostlycloudy.gif",
  "fctcode": "3",
  "sky": "87",
  "wspd": {
  "english": "8",
  "metric": "13"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "210"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "80",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "56",
  "metric": "13"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.81",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "5",
  "hour_padded": "05",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341403200",
  "pretty": "5:00 AM PDT on July 04, 2012",
  "civil": "5:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "55",
  "metric": "13"
  },
  "dewpoint": {
  "english": "50",
  "metric": "10"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_mostlycloudy.gif",
  "fctcode": "3",
  "sky": "85",
  "wspd": {
  "english": "8",
  "metric": "13"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "202"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "80",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "55",
  "metric": "13"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "0.00",
  "metric": "0.00"
  },
  "pop": "0",
  "mslp": {
  "english": "29.80",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "6",
  "hour_padded": "06",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341406800",
  "pretty": "6:00 AM PDT on July 04, 2012",
  "civil": "6:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "56",
  "metric": "14"
  },
  "dewpoint": {
  "english": "51",
  "metric": "10"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/mostlycloudy.gif",
  "fctcode": "3",
  "sky": "85",
  "wspd": {
  "english": "8",
  "metric": "13"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "202"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "80",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "56",
  "metric": "14"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.80",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "7",
  "hour_padded": "07",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341410400",
  "pretty": "7:00 AM PDT on July 04, 2012",
  "civil": "7:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "58",
  "metric": "14"
  },
  "dewpoint": {
  "english": "51",
  "metric": "11"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/mostlycloudy.gif",
  "fctcode": "3",
  "sky": "85",
  "wspd": {
  "english": "8",
  "metric": "13"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "202"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "80",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "58",
  "metric": "14"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.80",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "8",
  "hour_padded": "08",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341414000",
  "pretty": "8:00 AM PDT on July 04, 2012",
  "civil": "8:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "59",
  "metric": "15"
  },
  "dewpoint": {
  "english": "52",
  "metric": "11"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/mostlycloudy.gif",
  "fctcode": "3",
  "sky": "85",
  "wspd": {
  "english": "8",
  "metric": "13"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "203"
  },
  "wx": "",
  "uvi": "1",
  "humidity": "80",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "59",
  "metric": "15"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.82",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "9",
  "hour_padded": "09",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341417600",
  "pretty": "9:00 AM PDT on July 04, 2012",
  "civil": "9:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "63",
  "metric": "17"
  },
  "dewpoint": {
  "english": "53",
  "metric": "11"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/mostlycloudy.gif",
  "fctcode": "3",
  "sky": "85",
  "wspd": {
  "english": "9",
  "metric": "15"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "203"
  },
  "wx": "",
  "uvi": "1",
  "humidity": "71",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "63",
  "metric": "17"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.82",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "10",
  "hour_padded": "10",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341421200",
  "pretty": "10:00 AM PDT on July 04, 2012",
  "civil": "10:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "68",
  "metric": "20"
  },
  "dewpoint": {
  "english": "53",
  "metric": "12"
  },
  "condition": "Mostly Cloudy",
  "icon": "mostlycloudy",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/mostlycloudy.gif",
  "fctcode": "3",
  "sky": "85",
  "wspd": {
  "english": "11",
  "metric": "17"
  },
  "wdir": {
  "dir": "SSW",
  "degrees": "203"
  },
  "wx": "",
  "uvi": "1",
  "humidity": "63",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "68",
  "metric": "20"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.82",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "11",
  "hour_padded": "11",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341424800",
  "pretty": "11:00 AM PDT on July 04, 2012",
  "civil": "11:00 AM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "AM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "72",
  "metric": "22"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "20",
  "wspd": {
  "english": "12",
  "metric": "19"
  },
  "wdir": {
  "dir": "SW",
  "degrees": "231"
  },
  "wx": "",
  "uvi": "6",
  "humidity": "54",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "72",
  "metric": "22"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "0.00",
  "metric": "0.00"
  },
  "pop": "0",
  "mslp": {
  "english": "29.84",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "12",
  "hour_padded": "12",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341428400",
  "pretty": "12:00 PM PDT on July 04, 2012",
  "civil": "12:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "71",
  "metric": "21"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "20",
  "wspd": {
  "english": "13",
  "metric": "21"
  },
  "wdir": {
  "dir": "SW",
  "degrees": "231"
  },
  "wx": "",
  "uvi": "6",
  "humidity": "56",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "71",
  "metric": "21"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.84",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "13",
  "hour_padded": "13",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341432000",
  "pretty": "1:00 PM PDT on July 04, 2012",
  "civil": "1:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "69",
  "metric": "21"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "20",
  "wspd": {
  "english": "14",
  "metric": "22"
  },
  "wdir": {
  "dir": "SW",
  "degrees": "231"
  },
  "wx": "",
  "uvi": "6",
  "humidity": "59",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "69",
  "metric": "21"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.84",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "14",
  "hour_padded": "14",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341435600",
  "pretty": "2:00 PM PDT on July 04, 2012",
  "civil": "2:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "68",
  "metric": "20"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "20",
  "wspd": {
  "english": "15",
  "metric": "24"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "237"
  },
  "wx": "",
  "uvi": "7",
  "humidity": "61",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "68",
  "metric": "20"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.83",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "15",
  "hour_padded": "15",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341439200",
  "pretty": "3:00 PM PDT on July 04, 2012",
  "civil": "3:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "67",
  "metric": "20"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "20",
  "wspd": {
  "english": "14",
  "metric": "22"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "237"
  },
  "wx": "",
  "uvi": "7",
  "humidity": "62",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "67",
  "metric": "20"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.83",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "16",
  "hour_padded": "16",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341442800",
  "pretty": "4:00 PM PDT on July 04, 2012",
  "civil": "4:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "67",
  "metric": "19"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "20",
  "wspd": {
  "english": "13",
  "metric": "21"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "237"
  },
  "wx": "",
  "uvi": "7",
  "humidity": "64",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "67",
  "metric": "19"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.83",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "17",
  "hour_padded": "17",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341446400",
  "pretty": "5:00 PM PDT on July 04, 2012",
  "civil": "5:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "66",
  "metric": "19"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "16",
  "wspd": {
  "english": "12",
  "metric": "19"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "237"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "65",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "66",
  "metric": "19"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "0.00",
  "metric": "0.00"
  },
  "pop": "0",
  "mslp": {
  "english": "29.82",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "18",
  "hour_padded": "18",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341450000",
  "pretty": "6:00 PM PDT on July 04, 2012",
  "civil": "6:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "65",
  "metric": "18"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "16",
  "wspd": {
  "english": "12",
  "metric": "19"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "237"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "68",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "65",
  "metric": "18"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.82",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "19",
  "hour_padded": "19",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341453600",
  "pretty": "7:00 PM PDT on July 04, 2012",
  "civil": "7:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "64",
  "metric": "18"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "16",
  "wspd": {
  "english": "12",
  "metric": "19"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "237"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "72",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "64",
  "metric": "18"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.82",
  "metric": "1009"
  }
  }, {
  "FCTTIME": {
  "hour": "20",
  "hour_padded": "20",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341457200",
  "pretty": "8:00 PM PDT on July 04, 2012",
  "civil": "8:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "63",
  "metric": "17"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/clear.gif",
  "fctcode": "1",
  "sky": "16",
  "wspd": {
  "english": "12",
  "metric": "19"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "237"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "75",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "63",
  "metric": "17"
  },
  "qpf": {
  "english": "0.00",
  "metric": "0.00"
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.84",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "21",
  "hour_padded": "21",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341460800",
  "pretty": "9:00 PM PDT on July 04, 2012",
  "civil": "9:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "61",
  "metric": "16"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_clear.gif",
  "fctcode": "1",
  "sky": "16",
  "wspd": {
  "english": "12",
  "metric": "19"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "237"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "80",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "61",
  "metric": "16"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.84",
  "metric": "1010"
  }
  }, {
  "FCTTIME": {
  "hour": "22",
  "hour_padded": "22",
  "min": "00",
  "sec": "0",
  "year": "2012",
  "mon": "7",
  "mon_padded": "07",
  "mon_abbrev": "Jul",
  "mday": "4",
  "mday_padded": "04",
  "yday": "185",
  "isdst": "1",
  "epoch": "1341464400",
  "pretty": "10:00 PM PDT on July 04, 2012",
  "civil": "10:00 PM",
  "month_name": "July",
  "month_name_abbrev": "Jul",
  "weekday_name": "Wednesday",
  "weekday_name_night": "Wednesday Night",
  "weekday_name_abbrev": "Wed",
  "weekday_name_unlang": "Wednesday",
  "weekday_name_night_unlang": "Wednesday Night",
  "ampm": "PM",
  "tz": "",
  "age": ""
  },
  "temp": {
  "english": "59",
  "metric": "15"
  },
  "dewpoint": {
  "english": "54",
  "metric": "12"
  },
  "condition": "Clear",
  "icon": "clear",
  "icon_url": "http://icons-ak.wxug.com/i/c/k/nt_clear.gif",
  "fctcode": "1",
  "sky": "16",
  "wspd": {
  "english": "11",
  "metric": "18"
  },
  "wdir": {
  "dir": "WSW",
  "degrees": "237"
  },
  "wx": "",
  "uvi": "0",
  "humidity": "84",
  "windchill": {
  "english": "-9998",
  "metric": "-9998"
  },
  "heatindex": {
  "english": "-9998",
  "metric": "-9998"
  },
  "feelslike": {
  "english": "59",
  "metric": "15"
  },
  "qpf": {
  "english": "",
  "metric": ""
  },
  "snow": {
  "english": "",
  "metric": ""
  },
  "pop": "0",
  "mslp": {
  "english": "29.84",
  "metric": "1010"
  }
  }]
}
*/
