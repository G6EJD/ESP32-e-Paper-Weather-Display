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

#include "moon_icons.h"
#include "credentials.h"
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
String version = "5a";       // Version of this program
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
int wifisection, displaysection;

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

WiFiClient client; // wifi client object

// Wifi on section takes : 7.37 secs to complete at 119mA using a Lolin 32 (wifi on)
// Display section takes : 10.16 secs to complete at 40mA using a Lolin 32 (wifi off)
// Sleep current is 175uA using a Lolin 32
// Total power consumption per-hour based on 2 updates at 30-min intervals = (2x(7.37x119/3600 + 10.16x40/3600) + 0.15 x (3600-2x(7.37+10.16))/3600)/3600 = 0.887mAHr 
// A 2600mAhr battery will last for 2600/0.88 = 122 days

void setup() {
  wifisection = millis();
  Serial.begin(115200);
  StartWiFi();
  SetupTime();
  lastConnectionTime = millis();
  bool Received_Forecast_OK = (obtain_wx_data("conditions") && obtain_wx_data("forecast/astronomy"));
  // Now only refresh the screen if all the data was received OK, otherwise wait until the next timed check
  if (Received_Forecast_OK) { 
    //Received data OK at this point so turn off the WiFi to save power
    StopWiFi(); // Reduces power consumption
    displaysection = millis();
    gfx.init();
    gfx.setRotation(3);
    gfx.setFont(ArialMT_Plain_10);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.fillBuffer(EPD_BLACK);
    gfx.setColor(EPD_WHITE);
    gfx.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, "Getting data from Weather Underground...");
    gfx.commit();
    WaitForDisplay();
    gfx.setColor(EPD_BLACK);
    gfx.fillBuffer(EPD_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);  
    DisplayForecast();
  }
  Serial.println("   Wifi section took: "+ String(wifisection/1000.0)+" secs");
  Serial.println("Display section took: "+ String((millis()-displaysection)/1000.0)+" secs");
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
  gfx.drawLine(0,14,296,14);  
  //Period-0 (Main Icon/Report)
  DisplayWXicon(205,45,Cicon,Largesize); 
  // IDisplayWXicon(205,45,WxConditions[1].Icon,Largesize); if you want the forecast icon
  gfx.setFont(ArialMT_Plain_24);
  gfx.drawString(5,32,Ctemperature+"°");
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(Ctemperature.length()*13+8,33,(Units=="I"?"F":"C"));
  gfx.setFont(ArialMT_Plain_10);
  DisplayWXicon(100,32, "probrain",Smallsize);
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
  DrawMoon(245,82,Dhemisphere, DphaseofMoon); 
  gfx.commit();
  WaitForDisplay();
}

void DrawDayWeather(int x, int y, int index){
  DisplayWXicon(x+2,y,WxConditions[index].Icon,Smallsize);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x+3,y-25,WxConditions[index].WDay);
  gfx.drawString(x+2,y+11,WxConditions[index].High+"° "+WxConditions[index].Pop+"%");
  gfx.drawLine(x+28,77,x+28,129);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}

void DrawMoon(int16_t x, int16_t y, String hemisphere, String Phase) {
  Phase.toLowerCase();
  if (hemisphere == "North")
  {
   if (Phase == "new moon")        gfx.drawPalettedBitmapFromPgm(x, y, New_Moon);
   if (Phase == "waxing crescent") gfx.drawPalettedBitmapFromPgm(x, y, Waxing_Crescent);
   if (Phase == "first quarter")   gfx.drawPalettedBitmapFromPgm(x, y, First_Quarter);
   if (Phase == "waxing gibbous")  gfx.drawPalettedBitmapFromPgm(x, y, Waxing_Gibbous);
   if (Phase == "full moon")       gfx.drawPalettedBitmapFromPgm(x, y, Full_Moon);
   if (Phase == "waning gibbous")  gfx.drawPalettedBitmapFromPgm(x, y, Waning_Gibbous);
   if (Phase == "last quarter")    gfx.drawPalettedBitmapFromPgm(x, y, Last_Quarter);
   if (Phase == "waning crescent") gfx.drawPalettedBitmapFromPgm(x, y, Waning_Crescent);
  }
  else
  {
   if (Phase == "new moon")        gfx.drawPalettedBitmapFromPgm(x, y, New_Moon);
   if (Phase == "waxing crescent") gfx.drawPalettedBitmapFromPgm(x, y, Waning_Crescent);
   if (Phase == "first quarter")   gfx.drawPalettedBitmapFromPgm(x, y, Last_Quarter);
   if (Phase == "waxing gibbous")  gfx.drawPalettedBitmapFromPgm(x, y, Waning_Gibbous);
   if (Phase == "full moon")       gfx.drawPalettedBitmapFromPgm(x, y, Full_Moon);
   if (Phase == "waning gibbous")  gfx.drawPalettedBitmapFromPgm(x, y, Waxing_Gibbous);
   if (Phase == "last quarter")    gfx.drawPalettedBitmapFromPgm(x, y, First_Quarter);
   if (Phase == "waning crescent") gfx.drawPalettedBitmapFromPgm(x, y, Waxing_Crescent);
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
  gfx.drawString(x,y-Cradius-14,Cwindspeed+(Units=="M"?" kph":" mph"));
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}

void arrow(int x, int y, int asize, String aangle, int pwidth, int plength){
  // x,y is the centre poistion of the arrow and asize is the radius out from the x,y position
  // aangle is angle to draw the pointer at e.g. at 45° for NW
  // pwidth is the pointer width in pixels
  // plength is the pointer length in pixels
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
  IconName.toLowerCase(); IconName.trim();
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
  else if (IconName == "tstorms"         || IconName == "nt_tstorms"      ||
           IconName == "chancetstorms"   || IconName == "nt_chancetstorms")
           if (LargeSize) Tstorms(x,y,Large); else Tstorms(x,y,Small);
  else if (IconName == "fog"             || IconName == "nt_fog"          ||
           IconName == "hazy"            || IconName == "nt_hazy")
           if (LargeSize) Fog(x,y,Large); else Fog(x,y,Small); // Fog and Hazy images are identical
  else if (IconName == "probrain")
           if (LargeSize) ProbRain(x,y,Large); else ProbRain(x,y,Small);
  else     if (LargeSize) Nodata(x,y,Large); else Nodata(x,y,Small);
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

void WaitForDisplay(){
  while (digitalRead(4) == true); // Wait while screen is busy
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

void StopWiFi(){
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  wifisection    = millis() - wifisection;
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
  scale = scale * 1.45;
  addsun(x,y-4,scale);
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

//###########################################################################

