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
#include "credentials.h"       // See 'credentials' tab
#include <ArduinoJson.h>       // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>              // Built-in
#include "time.h"              // Built-in
#include <SPI.h>               // Built-in 
#include <FS.h>                // Built-in
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
static const uint8_t EPD_MISO  = 19; // Master In not used, no data from display
static const uint8_t EPD_MOSI  = 23;

EPD_WaveShare42 epd(EPD_SS, EPD_RST, EPD_DC, EPD_BUSY); 
MiniGrafx gfx = MiniGrafx(&epd, BITS_PER_PIXEL, palette); 

//################  VERSION  ##########################
String version = "3";       // Version of this program
//################ VARIABLES ###########################

unsigned long       lastConnectionTime = 0;             // Last time you connected to the server, in milliseconds
const unsigned long postingInterval = 30L*60L*1000000L; // Delay between updates, in microseconds, WU allows 500 requests per-day maximum, set to every 15-mins or more
String Units     =  "M"; // M for Metric or I for Imperial
bool LargeIcon   =  true;
bool SmallIcon   =  false;
#define Large  11
#define Small  4
String time_str, currCondString; // strings to hold time and received weather data;

// Now define all the Current weather parameters
String Ctemp, Chumidity, Cweather, Cicon, Cwinddir, Cwinddegrees, Cpressure, Cptrend, Cprecip, Cforecast;

//################ PROGRAM VARIABLES and OBJECTS ################
typedef struct { // For current Day and Day 1, 2, 3
   String WDay;
   String Day;
   String Icon;
   String High;
   String Low;
   String Conditions;
   String Pop;
   String winddir;
   String windangle;
   String windspeed;
   String forecast;
} Conditions_record_type;  
 
Conditions_record_type WxConditions[5];      

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

#define max_readings 48
float pressure_readings[max_readings+1] = {0};
float temperature_readings[max_readings+1] = {0};
int   pressure_readings_cnt, temperature_readings_cnt;

// Astronomy
String  Dhemisphere, DphaseofMoon, Sunrise, Sunset, Moonrise, Moonset, Moonlight;

WiFiClient client; // wifi client object

void setup() { 
  Serial.begin(115200);
  StartWiFi();
  SetupTime();
  lastConnectionTime = millis();
  bool Received_Forecast_OK = obtain_forecast("conditions","forecast","astronomy");
  // Now only refresh the screen if all the data was received OK, otherwise wait until the next timed check
  if (Received_Forecast_OK && (WiFi.status() == WL_CONNECTED)) { 
    //Received data OK at this point so turn off the WiFi to save power
    WiFi.mode(WIFI_OFF);
    gfx.init();
    gfx.setRotation(0);
    gfx.setColor(EPD_BLACK);
    gfx.fillBuffer(EPD_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);  
    RecoverData();
    Display_Weather();
    Update_readings();
    DrawGraph(35,205,150,85,900,1050,"Pressure", pressure_readings, pressure_readings_cnt, autoscale_on, barchart_off); // (x,y,width,height,MinValue, MaxValue, Title, Array, AutoScale, ChartMode
    DrawGraph(225,205,150,85,10,30,"Temperature", temperature_readings, temperature_readings_cnt, autoscale_on, barchart_off);
    SaveData();
    gfx.commit();
    delay(5000); //To enable drawing to complete before CPU goes to sleep, I don't know why? It appears there is no check of the display busy state...before commit exits.
  }
  else { 
    WiFi.mode(WIFI_OFF); // To reduce power consumption
    gfx.fillBuffer(EPD_WHITE); 
    gfx.setColor(EPD_BLACK); 
    gfx.setTextAlignment(TEXT_ALIGN_CENTER); 
    gfx.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, "Could not connect to WiFi...Check Network"); 
    gfx.commit(); 
    delay(5000); //To enable drawing to complete before CPU goes to sleep, I don't know why? It appears there is no check of the display busy state...before commit exits.
  } 
  esp_sleep_enable_timer_wakeup(postingInterval);
  esp_deep_sleep_start(); // Sleep for usualy 30 minutes
}
//###########################################################################
void loop() { // Retained for diagnostics when sleep is disabled, normally this will never run!
  if (millis() - lastConnectionTime > postingInterval) {
    obtain_forecast("conditions","forecast","astronomy");
    lastConnectionTime = millis();
    Display_Weather();
  }
}
//###########################################################################
void Display_Weather(){ // 2.9" e-paper display is 296x122 resolution
  gfx.setFont(ArialMT_Plain_10);
  Draw_HeadingLine();
  Draw_Current_Conditions(165,70);
  Draw_Forecast(230,18);
}
//###########################################################################
void Draw_Forecast(int x, int y){
  gfx.setFont(ArialMT_Plain_10);
  Draw_Forecast_Weather(x,y,2);
  Draw_Forecast_Weather(x+56,y,3);
  Draw_Forecast_Weather(x+112,y,4);
}
//###########################################################################
void Draw_Current_Conditions(int x, int y){
  DisplayWXicon(x,y-10,Cicon,LargeIcon); // or //DisplayWXicon(203,55,WxConditions[1].Icon,LargeIcon); // If you want to use the Forecast icon
  gfx.setFont(ArialRoundedMTBold_14);
  Draw_Pressure(x,y+57,Cptrend);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);  
  Draw_Rain(x-105,y+35);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x,y+33,Cweather); // or //gfx.drawString(x,y+30,WxConditions[1].Conditions); // If you want the forecast conditions
  Draw_Main_Wx(60,62);
  gfx.drawLine(0,y+68,SCREEN_WIDTH,y+68);
  gfx.drawStringMaxWidth(2,y+68,SCREEN_WIDTH,Cforecast);
  gfx.drawLine(0,y+118,SCREEN_WIDTH,y+118);
}
//###########################################################################
void Draw_Main_Wx(int x, int y){
  #define cradius 42
  gfx.drawCircle(x,y,cradius);
  gfx.drawCircle(x,y,cradius-1);
  arrow(x,y,cradius-7,WxConditions[1].windangle,20,22); // Show wind direction on outer circle
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);  
  gfx.drawString(x,y-28,WxConditions[1].High+"° | "+WxConditions[1].Low+"°"); // Show forecast high and Low
  gfx.drawString(x,y+16,WxConditions[1].winddir);
  gfx.setFont(ArialMT_Plain_24);
  gfx.drawString(x-5,y-10,Ctemp+"°"); // Show current Temperature
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);  
  gfx.drawString(x+22,y-9,Units=="I"?"F":"C"); // Add in smaller Temperature unit
}
//###########################################################################
void Draw_HeadingLine(){
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(SCREEN_WIDTH/2,-2,City);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(5,0,time_str);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(SCREEN_WIDTH-55,0,WxConditions[1].WDay+",");
  gfx.drawString(SCREEN_WIDTH-5,0,WxConditions[1].Day);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawLine(0,15,SCREEN_WIDTH,15);  
}
//###########################################################################
void Draw_Rain(int x, int y){
  gfx.setFont(ArialRoundedMTBold_14);
  DisplayWXicon(x-40,y-5,"probrain",Small);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x+15,y+2,WxConditions[1].Pop+"%");
  if (Cprecip.toFloat() > 0) gfx.drawString(x,y+14,Cprecip+(Units=="M"?"mm":"in")+" Rainfall"); // Only display rainfall total today if > 0
  gfx.setFont(ArialMT_Plain_10);
}
//###########################################################################
void Draw_Forecast_Weather(int x, int y, int index){
  gfx.setFont(ArialMT_Plain_10);
  gfx.setColor(EPD_BLACK); // Sometimes get set to WHITE, so change back
  gfx.drawRect(x,y,55,65);
  gfx.drawLine(x+1,y+13,x+55,y+13);
  DisplayWXicon(x+28,y+35,WxConditions[index].Icon,SmallIcon);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x+28,y,WxConditions[index].WDay);
  gfx.drawString(x+28,y+50,WxConditions[index].High+"° "+WxConditions[index].Pop+"%");
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  if (index == 2) {
    gfx.drawRect(x,y+64,167,52);
    gfx.drawString(x+4,y+68,"Sun:   "+Sunrise+"-"+Sunset);
    gfx.drawString(x+4,y+83,"Moon: "+Moonrise+"-"+Moonset);
    gfx.drawString(x+4,y+99,"Phase:"+DphaseofMoon);
    // DrawMoon(x,y,radius,hemisphere,%illumination,Moon Phase)
    DrawMoon(x+145,y+90,18,Dhemisphere, Moonlight, DphaseofMoon); 
  }
}
//###########################################################################
void Draw_Pressure(int x, int y, String slope){
  #define Asize 8
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x,y-10,Cpressure+(Units=="M"?"mb":"in"));
  x = x + 30;
  if (slope == "+") { // Rising
    gfx.drawLine(x,y,x+Asize,y-Asize);
    gfx.drawLine(x,y-Asize,x+Asize,y-Asize);
    gfx.drawLine(x+Asize,y-Asize,x+Asize,y);
  }
  if (slope == "-") { // Falling
    gfx.drawLine(x,y,x+Asize,y+Asize);
    gfx.drawLine(x,y+Asize,x+Asize,y+Asize);
    gfx.drawLine(x+Asize,y+Asize,x+Asize,y);
  }
  if (slope == "0") { // No change
    gfx.drawLine(x,y,x+Asize,y);
    gfx.drawLine(x+Asize,y-Asize,x+Asize*2,y);
    gfx.drawLine(x+Asize,y+Asize,x+Asize*2,y);
  }
}
//###########################################################################
void DrawMoon(int16_t x0, int16_t y0, int16_t r, String hemisphere, String illumination, String Phase) {
  Phase.toLowerCase();
  if (Phase != "new") if (Phase.indexOf("waxing")==0 || Phase.indexOf("first")==0) Phase = "First"; else Phase = "Second"; 
  int h = 2 * r;
  int m_illumination = 2*r-2*illumination.toInt()*r/50;
  x0 = x0 - m_illumination/2; 
  y0 = y0 - h/2;
  int16_t x1 = x0 + m_illumination; 
  int16_t y1 = y0 + h;
  //~~~~~~~~~~~~~~~~~~~~~~ based on R=50 
  gfx.setColor(EPD_BLACK);
  if (Phase == "new") {
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
//###########################################################################
void DrawCircle(int x, int y, int Cstart, int Cend, int Cradius, int Coffset_radius, int Coffset){
  gfx.setColor(EPD_BLACK);
  float dx,dy;
  for (int i=Cstart;i<Cend;i++) {
    dx = (Cradius+Coffset_radius) * cos((i-90)*PI/180) + x + Coffset/2; // calculate X position  
    dy = (Cradius+Coffset_radius) * sin((i-90)*PI/180) + y; // calculate Y position  
    gfx.setPixel(dx,dy);
  }
}
//###########################################################################
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
//###########################################################################
void DisplayWXicon(int x, int y, String IconName, bool LargeIcon){
  Serial.println(IconName);
  // Avaiable icons are Rain(x,y,size), ChanceRain(,,), Snow(,,), Sunny(,,), MostlySunny(,,),  Cloudy(,,), MostlyCloudy(,,), Tstorms(,,), Fog(,,)
  IconName.toLowerCase(); // In-case it gets called with e.g. 'Fog' and not 'fog'
  if      (IconName == "rain"            || IconName == "nt_rain")
           if (LargeIcon) Rain(x,y,Large); else Rain(x,y,Small);
  else if (IconName == "chancerain"      || IconName == "nt_chancerain")
           if (LargeIcon) ChanceRain(x,y,Large); else ChanceRain(x,y,Small);
  else if (IconName == "snow"            || IconName == "nt_snow"         ||
           IconName == "flurries"        || IconName == "nt_flurries"     ||
           IconName == "chancesnow"      || IconName == "nt_chancesnow"   ||
           IconName == "chanceflurries"  || IconName == "nt_chanceflurries")
           if (LargeIcon) Snow(x,y,Large); else Snow(x,y,Small);
  else if (IconName == "sleet"           || IconName == "nt_sleet"        ||
           IconName == "chancesleet"     || IconName == "nt_chancesleet")
           if (LargeIcon) Snow(x,y,Large); else Snow(x,y,Small);
  else if (IconName == "sunny"           || IconName == "nt_sunny"        ||
           IconName == "clear"           || IconName == "nt_clear")
           if (LargeIcon) Sunny(x,y,Large); else Sunny(x,y,Small);
  else if (IconName == "partlysunny"     || IconName == "nt_partlysunny"  ||
           IconName == "mostlysunny"     || IconName == "nt_mostlysunny")
           if (LargeIcon) MostlySunny(x,y,Large); else MostlySunny(x,y,Small);
  else if (IconName == "cloudy"          || IconName == "nt_cloudy")  
           if (LargeIcon) Cloudy(x,y,Large); else Cloudy(x,y,Small);
  else if (IconName == "mostlycloudy"    || IconName == "nt_mostlycloudy" ||
           IconName == "partlycloudy"    || IconName == "nt_partlycloudy")  
           if (LargeIcon) MostlyCloudy(x,y,Large); else MostlyCloudy(x,y,Small);
  else if (IconName == "tstorms"         || IconName == "nt_tstorms"      ||
           IconName == "chancetstorms"   || IconName == "nt_chancetstorms")
           if (LargeIcon) Tstorms(x,y,Large); else Tstorms(x,y,Small);
  else if (IconName == "fog"             || IconName == "nt_fog"          ||
           IconName == "hazy"            || IconName == "nt_hazy")
           if (LargeIcon) Fog(x,y,Large); else Fog(x,y,Small); // Fog and Hazy images are identical
  else if (IconName == "probrain")
           ProbRain(x,y,3);
  else     Nodata(x,y,Large);
}
//###########################################################################
bool obtain_forecast (String forecast_type1, String forecast_type2, String forecast_type3) {
  client.stop();  // Clear any current connections
  Serial.println("Connecting to "+String(host)); // start a new connection
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
   Serial.println("Connection failed");
   return false;
  }
  // Weather Underground API address http://api.wunderground.com/api/YOUR_API_KEY/conditions/q/YOUR_STATE/YOUR_CITY.json
  String url = "http://api.wunderground.com/api/"+apikey+"/"+forecast_type1+"/"+forecast_type2+"/"+forecast_type3+"/q/"+Country+"/"+City+".json";
  Serial.println("Requesting URL: "+String(url));
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
  "Host: " + host + "\r\n" +
  "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Connection Timeout...Stopping");
      client.stop();
      return false;
    }
  }
  Serial.print("Receiving API weather data");
  while(client.available()) {
    currCondString = client.readStringUntil('\r');
    Serial.print(".");
  }
  Serial.println("\r\nClosing connection");
  //Serial.println(currCondString); // Uncomment to see all of the received data
  Serial.println("Bytes received: "+String(currCondString.length()));
  if (Decode_Weather_Data(&currCondString));
    else 
    {
      Serial.println("Failed to get Weather Data");
      return false;
    }
  return true;
}
//###########################################################################
bool Decode_Weather_Data(String* currCondString) {
  // http://jsonviewer.stack.hu/
  Serial.println("Creating object...");
  DynamicJsonBuffer jsonBuffer(11*1024);
  // Create root object and parse the json file returned from the api. The API returns errors and these need to be checked to ensure successful decoding
  JsonObject& root = jsonBuffer.parseObject(*(currCondString));
  if (!root.success()) {
    Serial.println(F("jsonBuffer.parseObject() failed"));
  }
  JsonObject& current = root["current_observation"];
  String ctemp        = current[(Units=="M"?"temp_c":"temp_f")];
  String chumidity    = current["relative_humidity"];
  String cweather     = current["weather"];
  String cicon        = current["icon"];
  String cwinddir     = current["wind_dir"];
  String cwinddegrees = current["wind_degrees"];
  String cpressure    = current[(Units=="M"?"pressure_mb":"pressure_in")];
  String cptrend      = current["pressure_trend"];
  String cprecip      = current[(Units=="M"?"precip_today_metric":"precip_today_in")];
  String windspeed0   = current[(Units=="M"?"wind_mph":"wind_kph")];    
  Ctemp        = ctemp;
  Chumidity    = chumidity;
  Cweather     = cweather;
  Cicon        = cicon;
  Cwinddir     = cwinddir;
  Cwinddegrees = cwinddegrees;
  Cpressure    = cpressure;
  Cptrend      = cptrend;
  Cprecip      = cprecip;
  JsonObject& Cwforecast = root["forecast"]["txt_forecast"]["forecastday"][0];
  String forecast0 = Cwforecast[(Units=="M"?"fcttext_metric":"fcttext")];
  Cforecast    = forecast0;
  Serial.println("Forecast: "+Cforecast);
  JsonObject& forecast = root["forecast"]["simpleforecast"];
  String wday0       = forecast["forecastday"][0]["date"]["weekday_short"];                     WxConditions[1].WDay = wday0;
  String icon0       = forecast["forecastday"][0]["icon"];                                      WxConditions[1].Icon = icon0;
  String high0       = forecast["forecastday"][0]["high"][(Units=="M"?"celsius":"fahrenheit")]; WxConditions[1].High = high0;
  String low0        = forecast["forecastday"][0]["low"][(Units=="M"?"celsius":"fahrenheit")];  WxConditions[1].Low  = low0;
  String conditions0 = forecast["forecastday"][0]["conditions"];                                WxConditions[1].Conditions = conditions0;
  String pop0        = forecast["forecastday"][0]["pop"];                                       WxConditions[1].Pop      = pop0;
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
  WxConditions[1].windspeed = windspeed0;
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
  String phaseofMoon = moon["phaseofMoon"];
  String hemisphere  = moon["hemisphere"];
  int SRhour         = moon["sunrise"]["hour"];
  int SRminute       = moon["sunrise"]["minute"];
  int SShour         = moon["sunset"]["hour"];
  int SSminute       = moon["sunset"]["minute"];
  int MRhour         = moon["moonrise"]["hour"];
  int MRminute       = moon["moonrise"]["minute"];
  int MShour         = moon["moonset"]["hour"];
  int MSminute       = moon["moonset"]["minute"];
  String SRampm = (SRhour<13?"a":"p");
  String SSampm = (SShour<13?"a":"p");
  String MRampm = (MRhour<13?"a":"p");
  String MSampm = (MShour<13?"a":"p");
  if (Units == "I") {
    SRhour = SRhour%12; if (SRhour==0) SRhour=12;
    SShour = SShour%12; if (SShour==0) SRhour=12;
    MRhour = MRhour%12; if (MRhour==0) SRhour=12;
    MShour = MShour%12; if (MShour==0) SRhour=12;
  }
  Sunrise   = (SRhour<10?"0":"")+String(SRhour)+":"+(SRminute<10?"0":"")+String(SRminute)+(Units=="I"?SRampm:"");
  Sunset    = (SShour<10?"0":"")+String(SShour)+":"+(SSminute<10?"0":"")+String(SSminute)+(Units=="I"?SSampm:"");
  Moonrise  = (MRhour<10?"0":"")+String(MRhour)+":"+(MRminute<10?"0":"")+String(MRminute)+(Units=="I"?MRampm:"");
  Moonset   = (MShour<10?"0":"")+String(MShour)+":"+(MSminute<10?"0":"")+String(MSminute)+(Units=="I"?MSampm:"");
  Moonlight = percentIlluminated;
  DphaseofMoon = phaseofMoon;
  Dhemisphere  = hemisphere;
  return true;
}
//###########################################################################
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
//###########################################################################
void SetupTime(){
  configTime(0, 0, "0.uk.pool.ntp.org", "time.nist.gov");
  setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02",1);
  // https://github.com/nayarsystems/posix_tz_db 
  delay(200);
  UpdateLocalTime();
}
//###########################################################################
void UpdateLocalTime(){
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S"); // Displays: Saturday, June 24 2017 14:05:49
  Serial.println(&timeinfo, "%H:%M:%S"); // Displays: 14:05:49
  char output[30];
  if (Units == "M")
    strftime(output, 30, "( %H:%M:%S )", &timeinfo); // Displays: Sat 24-Jun-17 14:05:49
  else
    strftime(output, 30, "( %r )", &timeinfo); // Displays: Sat Jun-24-17 2:05:49pm
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
//###########################################################################
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
//###########################################################################
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
//###########################################################################
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
//###########################################################################
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
//###########################################################################
void addfog(int x, int y, int scale, int linesize){
  if (scale == Large) y -= 15; else y -= 10;
  if (scale == Small) linesize = 1;
  for (int i = 0; i < 5; i++){
    gfx.fillRect(x-scale*3, y+scale*1, scale*6, linesize); 
    gfx.fillRect(x-scale*3, y+scale*2, scale*6, linesize); 
    gfx.fillRect(x-scale*3, y+scale*3, scale*6, linesize); 
  }
}
//###########################################################################
void MostlyCloudy(int x, int y, int scale){ 
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize); 
  addsun(x-scale*1.8,y-scale*1.8,scale); 
  addcloud(x,y,scale,linesize); 
}
//###########################################################################
void MostlySunny(int x, int y, int scale){ 
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize); 
  addsun(x-scale*1.8,y-scale*1.8,scale); 
}
//###########################################################################
void Rain(int x, int y, int scale){ 
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize); 
  addrain(x,y,scale); 
} 
//###########################################################################
void ProbRain(int x, int y, int scale){
  x = x + 20;
  y = y + 15;
  addcloud(x,y,scale,1);
  y = y + scale/2;
  for (int i = 0; i < 6; i++){
    gfx.drawLine(x-scale*4+scale*i*1.3+0, y+scale*1.9, x-scale*3.5+scale*i*1.3+0, y+scale);
  }
}
//###########################################################################
void Cloudy(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
}
//###########################################################################
void Sunny(int x, int y, int scale){
  scale = scale * 1.5;
  addsun(x,y,scale);
}
//###########################################################################
void ExpectRain(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addsun(x-scale*1.8,y-scale*1.8,scale); 
  addcloud(x,y,scale,linesize);
  addrain(x,y,scale);
}
//###########################################################################
void ChanceRain(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addsun(x-scale*1.8,y-scale*1.8,scale); 
  addcloud(x,y,scale,linesize);
  addrain(x,y,scale);
}
//###########################################################################
void Tstorms(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
  addtstorm(x,y,scale);
}
//###########################################################################
void Snow(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addcloud(x,y,scale,linesize);
  addsnow(x,y,scale);
}
//###########################################################################
void Fog(int x, int y, int scale){
  int linesize = 3;  
  if (scale == Small) linesize = 1;
  addfog(x,y,scale,linesize);
}
//###########################################################################
void Nodata(int x, int y, int scale){
  if (scale > 1) gfx.setFont(ArialMT_Plain_24); else gfx.setFont(ArialMT_Plain_10);
   gfx.drawString(x-10,y-30,"?");
}

//###########################################################################
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
 void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, int Y1Min, int Y1Max, String title, float DataArray[max_readings], int readings, boolean auto_scale, boolean barchart_mode) {
  #define auto_scale_major_tick 3 // Sets the autoscale increment, so axis steps up in units of e.g. 3
  #define yticks 5                // 5 y-axis division markers
  int maxYscale = -10000;
  int minYscale =  10000;
  int last_x, last_y;
  if (auto_scale==true) {
    for (int i=1; i <= max_readings; i++ ) {
      if (DataArray[i] >= maxYscale) maxYscale = DataArray[i];
      if (DataArray[i] <= minYscale) minYscale = DataArray[i];
    }
    maxYscale = maxYscale+auto_scale_major_tick; // Auto scale the graph and round to the nearest value defined, default was Y1Max
    Y1Max = maxYscale; 
    minYscale = minYscale-auto_scale_major_tick; // Auto scale the graph and round to the nearest value defined, default was Y1Max
    Y1Min = minYscale;
  }
  // Draw the graph
  last_x = x_pos+1; 
  last_y = y_pos+(Y1Max-constrain(DataArray[1],Y1Min,Y1Max))/(Y1Max-Y1Min)*gheight;
  gfx.setColor(EPD_BLACK);
  gfx.drawRect(x_pos,y_pos,gwidth,gheight+3);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x_pos + gwidth/2,y_pos-18,title);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  // Draw the data
  int x1,y1,x2,y2;
  for(int gx = 1; gx < readings; gx++){
    x1 = last_x;
    y1 = last_y;
    x2 = x_pos+gx*gwidth/max_readings; // max_readings is the global variable that sets the maximum data that can be plotted 
    y2 = y_pos+(Y1Max-constrain(DataArray[gx],Y1Min,Y1Max)) / (Y1Max-Y1Min) * gheight;
    if (barchart_mode) { gfx.drawLine(x1,y1,x2,y2); } else { gfx.drawLine(last_x,last_y,x2,y2); }
    last_x = x2;
    last_y = y2;
  }
  //Draw the Y-axis scale
  for (int spacing = 0; spacing <= yticks; spacing++) {
    #define number_of_dashes 20
    for (int j=0; j < number_of_dashes; j++){ // Draw dashed graph grid lines
      if (spacing < yticks) gfx.drawHorizontalLine((x_pos+1+j*gwidth/number_of_dashes),y_pos+(gheight*spacing/yticks),gwidth/(2*number_of_dashes));
    }
    gfx.drawString(x_pos-2,y_pos+gheight*spacing/yticks-5, String((Y1Max-(float)(Y1Max-Y1Min)/yticks*spacing),0));
  }
}
//----------------------------------------------------------------------------------------------------
void RecoverData(){
  boolean mounted = SPIFFS.begin(true); 
  if (!mounted) { 
    SPIFFS.format(); 
    Serial.println("Formating SPIFFS");
    SPIFFS.begin();
    File file = SPIFFS.open("/gdata.txt", "w+");
    if (!file) {
      Serial.println("Failed to open data file");
      return;
    }
    file.println(1);
    for (int i = 1; i <= max_readings; i++){
       file.println(0);
    }
    file.println(1);
    for (int i = 1; i <= max_readings; i++){
       file.println(0);
    }
  } 
  if (LoadData()) Serial.println("Data successfully recovered"); 
  else Serial.println("Data not successfully recovered"); 
}

boolean LoadData() {
  File file = SPIFFS.open("/gdata.txt", "r");
  if (!file) {
    Serial.println("Failed to open data file");
    return false;
  }
  Serial.println("Recovering data...");
  if (file.available()) {
    pressure_readings_cnt = file.parseInt();
    for (int i = 1; i <= max_readings; i++){
      pressure_readings[i] = file.parseFloat();
    }
  }
  if (file.available()) {
    temperature_readings_cnt = file.parseInt();
    for (int i = 1; i <= max_readings; i++){
      temperature_readings[i] = file.parseFloat();
    }
  }
  file.close();
  Serial.println("Recovered data");
  return true;
}

boolean SaveData() { 
  File file = SPIFFS.open("/gdata.txt", "w+"); 
  if (!file) { 
    Serial.println("Failed to open config file"); 
    return false; 
  }
  file.println(pressure_readings_cnt);
  for (int i = 1; i <= max_readings; i++){
    file.println(pressure_readings[i]);
  }
  file.println(temperature_readings_cnt);
  for (int i = 1; i <= max_readings; i++){
    file.println(temperature_readings[i]);
  }
  file.close(); 
  Serial.println("Data saved"); 
  return true; 
} 

void Update_readings(){
  pressure_readings_cnt++;
  temperature_readings_cnt++;
  int noise_differential = 2;
  float new_pressure    = Cpressure.toFloat();
  float new_temperature = Ctemp.toFloat();
  if (new_pressure > (pressure_readings[pressure_readings_cnt-1])+noise_differential) pressure_readings[pressure_readings_cnt] = pressure_readings[pressure_readings_cnt-1];
  if (new_pressure < (pressure_readings[pressure_readings_cnt-1])-noise_differential) pressure_readings[pressure_readings_cnt] = pressure_readings[pressure_readings_cnt-1];
  if (pressure_readings_cnt > max_readings){
    for (int i = 1; i <= max_readings; i++) {
      pressure_readings[i] = pressure_readings[i+1]; //move all data to the left;
    }
    pressure_readings[max_readings] = new_pressure;
    pressure_readings_cnt = max_readings;
  } else pressure_readings[pressure_readings_cnt] = new_pressure;
  
  if (temperature_readings_cnt > max_readings){
    for (int i = 1; i <= max_readings; i++) {
      temperature_readings[i] = temperature_readings[i+1]; //move all data to the left;
    }
    temperature_readings[max_readings] = new_temperature;
    temperature_readings_cnt = max_readings;
  } else temperature_readings[temperature_readings_cnt] = new_temperature;
  for (int i = 1; i <= 48; i++){
    Serial.print(i);
    Serial.print("  ");
    Serial.print(pressure_readings[i]);
    Serial.println(temperature_readings[i]);
  }
}

