/* ESP32 Weather Display and an e-paper 1.54" Display, obtains data from Open Weather Map, decodes and then displays it.
  ####################################################################################################################################
  This software, the ideas and concepts is Copyright (c) David Bird 2019. All rights to this software are reserved.

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
#include "Micro_Wx_Icons.h"    // Weather Icons
#include <ArduinoJson.h>       // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>              // Built-in
#include "time.h"              // Built-in
#include <SPI.h>               // Built-in 
#include <GxEPD2_BW.h>         // GxEPD2 from Sketch, Include Library, Manage Libraries, search for GxEDP2
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include "epaper_fonts.h"
#include "forecast_record.h"
#include "M5CoreInk.h"
#include "esp_adc_cal.h"

#define SCREEN_WIDTH  200
#define SCREEN_HEIGHT 200

RTC_TimeTypeDef RTCtime;
RTC_DateTypeDef RTCDate;

char timeStrbuff[64];

enum alignment {LEFT, RIGHT, CENTER};

// Connections for M5-CoreInk
static const uint8_t EPD_BUSY = 4;
static const uint8_t EPD_CS   = 9;
static const uint8_t EPD_RST  = 0;
static const uint8_t EPD_DC   = 15;
static const uint8_t EPD_SCK  = 18;
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23;

GxEPD2_BW<GxEPD2_154_M09, GxEPD2_154_M09::HEIGHT> display(GxEPD2_154_M09(/*CS*/ EPD_CS, /*DC*/ EPD_DC, /*RST*/ EPD_RST, /*BUSY*/ EPD_BUSY)); // GDEW0154M09 200x200

//################  VERSION  ##########################
String version = "1.4";      // Version of this program
//################ VARIABLES ###########################

bool LargeIcon = true, SmallIcon = false, RxWeather = false, RxForecast = false;
#define Large  10
#define Small  4
String  Time_str, Date_str, rxtext; // strings to hold time and received weather data;
int     StartTime, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;

//################ PROGRAM VARIABLES and OBJECTS ################

#define max_readings 4

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];

#include "common.h"

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float rain_readings[max_readings]        = {0};

long SleepDuration = 30; // Sleep time in minutes, aligned to minute boundary, so if 30 will always update at 00 or 30 past the hour
int  WakeupTime    = 7;  // Don't wakeup until after 07:00 to save battery power
int  SleepTime     = 23; // Sleep after (23+1) 00:00 to save battery power

//#########################################################################################
void setup() {
  Serial.begin(115200);  
  M5.begin(false, false, true);
  StartTime = millis();

  UpdateLocalTimeFromRTC();  
  if (RTCDate.Year < 2021 || (CurrentHour >= WakeupTime && CurrentHour <= SleepTime)) {
    if (StartWiFi() == WL_CONNECTED && SetupTime() == true) {
      InitialiseDisplay(); // Give screen time to initialise by getting weather data!
      byte Attempts = 1;
      WiFiClient client;   // wifi client object
      while ((RxWeather == false || RxForecast == false) && Attempts <= 2) { // Try up-to twice for Weather and Forecast data
        if (RxWeather  == false) RxWeather  = obtain_wx_data(client, "weather");
        if (RxForecast == false) RxForecast = obtain_wx_data(client, "forecast");
        Attempts++;
      }
      if (RxWeather || RxForecast) { // If received either Weather or Forecast data then proceed, report later if either failed
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
  UpdateLocalTimeFromRTC();  
  long SleepTimer = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec)) + 20; //Some ESP32 are too fast to maintain accurate time
  esp_sleep_enable_timer_wakeup((SleepTimer+20) * 1000000LL); // Added +20 seconnds to cover ESP32 RTC timer source inaccuracies
  Serial.println("Entering " + String(SleepTimer) + "-secs of sleep time");
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  M5.shutdown((int)SleepTimer); // deep sleep if on battery
  delay(500);
  // low power if on usb
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();
}
//#########################################################################################
void DisplayWeather() {                                    // 1.54" e-paper display is 200x200 resolution
  DisplayHeadingSection();                                 // Top line of the display
  DisplayTempHumiSection(0, 12);                           // Current temperature with Max/Min
  DisplayWxPerson(114, 12, WxConditions[0].Icon);          // Weather person depiction of weather
  DisplayMainWeatherSection(0, 112);                       // Weather forecast text
  DisplayForecastSection(0, 135);                          // 3hr interval forecast boxes
}
//#########################################################################################
void DisplayTempHumiSection(int x, int y) {
  display.drawRect(x, y, 115, 97, GxEPD_BLACK);
  display.setFont(&DSEG7_Classic_Bold_21);
  display.setTextSize(2);
  drawString(x + 20, y + 5, String(WxConditions[0].Temperature, 0) + "'", LEFT);                                   // Show current Temperature
  display.setTextSize(1);
  drawString(x + 93, y + 30, (Units == "M" ? "C" : "F"), LEFT); // Add-in smaller Temperature unit
  display.setTextSize(2);
  display.setFont(&DejaVu_Sans_Bold_11);
  drawString(x + 57, y + 59, String(WxConditions[0].High, 0) + "'/" + String(WxConditions[0].Low, 0) + "'", CENTER); // Show forecast high and Low, in the font ' is a °
  display.setTextSize(1);
  drawString(x + 60,  y + 83, String(WxConditions[0].Humidity, 0) + "% RH", CENTER);                               // Show Humidity
}


float getBatVoltage()
{
    analogSetPinAttenuation(35,ADC_11db);
    esp_adc_cal_characteristics_t *adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 3600, adc_chars);
    uint16_t ADCValue = analogRead(35);
    
    uint32_t BatVolmV  = esp_adc_cal_raw_to_voltage(ADCValue,adc_chars);
    float BatVol = float(BatVolmV) * 25.1 / 5.1 / 1000;
    return BatVol;
}


//#########################################################################################
void DisplayHeadingSection() {
  drawString(2, 2, Time_str, LEFT);
  drawString(SCREEN_WIDTH - 2, 0, Date_str, RIGHT);

  char batteryStrBuff[64];
  int batPerc = (int)(123-(123/(powf(1+powf(getBatVoltage()/3.7f, 80), 0.165f))));
  sprintf(batteryStrBuff,"%d %%", batPerc);  
  Serial.println(batteryStrBuff);
  drawString(SCREEN_WIDTH / 2, 0, batteryStrBuff, CENTER);
  display.drawLine(0, 12, SCREEN_WIDTH, 12, GxEPD_BLACK);
}
//#########################################################################################
void DisplayMainWeatherSection(int x, int y) {
  display.drawRect(x, y - 4, SCREEN_WIDTH, 28, GxEPD_BLACK);
  String Wx_Description1 = WxConditions[0].Forecast0;
  display.setFont(&DejaVu_Sans_Bold_11);
  String Wx_Description2 = WindDegToDirection(WxConditions[0].Winddir) + " wind, " + String(WxConditions[0].Windspeed, 1) + (Units == "M" ? "m/s" : "mph");
  drawStringMaxWidth(x + 2, y - 2, 27, TitleCase(Wx_Description1), LEFT);
  drawStringMaxWidth(x + 2, y +10, 27, TitleCase(Wx_Description2), LEFT);  
}
//#########################################################################################
void DisplayForecastSection(int x, int y) {
  int offset = 50;
  DisplayForecastWeather(x + offset * 0, y, offset, 0);
  DisplayForecastWeather(x + offset * 1, y, offset, 1);
  DisplayForecastWeather(x + offset * 2, y, offset, 2);
  DisplayForecastWeather(x + offset * 3, y, offset, 3);
  int r = 0;
  do {
    if (Units == "I") pressure_readings[r] = WxForecast[r].Pressure * 0.02953;
    else              pressure_readings[r] = WxForecast[r].Pressure;
    temperature_readings[r]                = WxForecast[r].Temperature;
    if (Units == "I") rain_readings[r]     = WxForecast[r].Rainfall * 0.0393701;
    else              rain_readings[r]     = WxForecast[r].Rainfall;
    r++;
  } while (r < max_readings);
}
//#########################################################################################
void DisplayForecastWeather(int x, int y, int offset, int index) {
  display.drawRect(x, y, offset, 65, GxEPD_BLACK);
  display.drawLine(x, y + 13, x + offset, y + 13, GxEPD_BLACK);
  DisplayWxIcon(x + offset / 2 + 1, y + 35, WxForecast[index].Icon, SmallIcon);
  drawString(x + offset / 2, y  + 3, String(ConvertUnixTime(WxForecast[index].Dt + WxConditions[0].Timezone).substring(0,5)), CENTER);
  drawString(x + offset / 2, y + 50, String(WxForecast[index].High, 0) + "/" + String(WxForecast[index].Low, 0), CENTER);
}
//#########################################################################################
String WindDegToDirection(float winddirection) {
  int dir = int((winddirection / 22.5) + 0.5);
  String Ord_direction[16] = {TXT_N, TXT_NNE, TXT_NE, TXT_ENE, TXT_E, TXT_ESE, TXT_SE, TXT_SSE, TXT_S, TXT_SSW, TXT_SW, TXT_WSW, TXT_W, TXT_WNW, TXT_NW, TXT_NNW};
  return Ord_direction[(dir % 16)];
}
//#########################################################################################
void DisplayRain(int x, int y) {
  if (WxForecast[1].Rainfall > 0) drawString(x, y, String(WxForecast[1].Rainfall, 3) + (Units == "M" ? "mm" : "in") + " Rain", LEFT); // Only display rainfall if > 0
}
//#########################################################################################
void DisplayWxIcon(int x, int y, String IconName, bool LargeSize) {
  Serial.println(IconName);
  if      (IconName == "01d" || IconName == "01n") Sunny(x, y,       LargeSize, IconName);
  else if (IconName == "02d" || IconName == "02n") MostlySunny(x, y, LargeSize, IconName);
  else if (IconName == "03d" || IconName == "03n") Cloudy(x, y,      LargeSize, IconName);
  else if (IconName == "04d" || IconName == "04n") MostlyCloudy(x, y, LargeSize, IconName);
  else if (IconName == "09d" || IconName == "09n") ChanceRain(x, y,  LargeSize, IconName);
  else if (IconName == "10d" || IconName == "10n") Rain(x, y,        LargeSize, IconName);
  else if (IconName == "11d" || IconName == "11n") Tstorms(x, y,     LargeSize, IconName);
  else if (IconName == "13d" || IconName == "13n") Snow(x, y,        LargeSize, IconName);
  else if (IconName == "50d")                      Haze(x, y,        LargeSize, IconName);
  else if (IconName == "50n")                      Fog(x, y,         LargeSize, IconName);
  else                                             Nodata(x, y,      LargeSize);
}
//#########################################################################################
uint8_t StartWiFi() {
  Serial.print(F("\r\nConnecting to: ")); Serial.println(String(ssid));
  IPAddress dns(8, 8, 8, 8); // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  uint8_t connectionStatus;
  bool AttemptConnection = true;
  while (AttemptConnection) {
    connectionStatus = WiFi.status();
    if (millis() > start + 15000) { // Wait 15-secs maximum
      AttemptConnection = false;
    }
    if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED) {
      AttemptConnection = false;
    }
    delay(100);
  }
  if (connectionStatus == WL_CONNECTED) {
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else Serial.println("WiFi connection *** FAILED ***");
  return connectionStatus;
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
//#########################################################################################
boolean SetupTime() {
  configTime(0, 0, "0.uk.pool.ntp.org", "time.nist.gov");
  setenv("TZ", Timezone, 1);
  tzset(); // Set the TZ environment variable
  delay(100);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}

//#########################################################################################
boolean UpdateLocalTimeFromRTC() {
  M5.rtc.GetTime(&RTCtime);
  M5.rtc.GetDate(&RTCDate);
  CurrentHour = RTCtime.Hours;
  CurrentMin  = RTCtime.Minutes;
  CurrentSec  = RTCtime.Seconds;  

  
  sprintf(timeStrbuff,"M5 RTC %d/%02d/%02d %02d:%02d:%02d",
                      RTCDate.Year,RTCDate.Month,RTCDate.Date,
                      RTCtime.Hours,RTCtime.Minutes,RTCtime.Seconds);
                      
  Serial.printf(timeStrbuff);
                                 
}

boolean UpdateLocalTime() {
  struct tm timeinfo;
  char output[30], day_output[30];
  while (!getLocalTime(&timeinfo, 5000)) { // Wait for 5-sec for time to synchronise
    Serial.println(F("Failed to obtain time"));
    return false;
  }
  strftime(output, 30, "%H", &timeinfo);
  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  
  RTCtime.Hours = timeinfo.tm_hour;
  RTCtime.Minutes = timeinfo.tm_min;
  RTCtime.Seconds = timeinfo.tm_sec;
  M5.rtc.SetTime(&RTCtime);
  
  RTCDate.Year = timeinfo.tm_year + 1900;
  RTCDate.Month = timeinfo.tm_mon + 1;
  RTCDate.Date = timeinfo.tm_mday;
  M5.rtc.SetDate(&RTCDate);
  
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S"); // Displays: Saturday, June 24 2017 14:05:49
  Serial.println(&timeinfo, "%H:%M:%S");                 // Displays: 14:05:49
  if (Units == "M") {
    strftime(day_output, 30, "%d-%b-%y", &timeinfo);     // Displays: 24-Jun-17
    strftime(output, 30, "%H:%M", &timeinfo);            // Creates: '14:05'
  }
  else {
    strftime(day_output, 30, "%b-%d-%y", &timeinfo);     // Creates: Jun-24-17
    strftime(output, 30, "%I:%M%p", &timeinfo);          // Creates: '2:05pm'
  }
  Date_str = day_output;
  Time_str = output;
  return true;
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                      // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                      // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);            // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK); // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  //Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);           // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);           // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE); // left middle upper circle
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
  int linesize = 1;
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
  y -= 10;
  linesize = 1;
  for (int i = 0; i < 6; i++) {
    display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.7, scale * 6, linesize, GxEPD_BLACK);
  }
}
//#########################################################################################
void MostlyCloudy(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  int linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  addcloud(x, y + offset, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale);
  addcloud(x, y + offset, scale, linesize);
}
//#########################################################################################
void MostlySunny(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  int linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  addcloud(x, y + offset, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale);
}
//#########################################################################################
void Rain(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  if (LargeSize) {
    scale = Large;
    offset = 12;
  }
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  addcloud(x, y + offset, scale, linesize);
  addrain(x, y + offset, scale);
}
//#########################################################################################
void Cloudy(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  int linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  addcloud(x, y + offset, scale, linesize);
}
//#########################################################################################
void Sunny(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  scale = scale * 1.5;
  addsun(x, y + offset, scale);
}
//#########################################################################################
void ExpectRain(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  int linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale);
  addcloud(x, y + offset, scale, linesize);
  addrain(x, y + offset, scale);
}
//#########################################################################################
void ChanceRain(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  int linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale);
  addcloud(x, y + offset, scale, linesize);
  addrain(x, y + offset, scale);
}
//#########################################################################################
void Tstorms(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  int linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  addcloud(x, y + offset, scale, linesize);
  addtstorm(x, y + offset, scale);
}
//#########################################################################################
void Snow(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  int linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  addcloud(x, y + offset, scale, linesize);
  addsnow(x, y + offset, scale);
}
//#########################################################################################
void Fog(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  int linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  addcloud(x, y + offset, scale, linesize);
  addfog(x, y + offset, scale, linesize);
}
//#########################################################################################
void Haze(int x, int y, bool LargeSize, String IconName) {
  int scale = Small, offset = 0;
  int linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale);
  addsun(x, y + offset, scale * 1.4);
  addfog(x, y + offset, scale * 1.4, linesize);
}
//#########################################################################################
void addmoon (int x, int y, int scale) {
  display.fillCircle(x - 20, y - 15, scale, GxEPD_BLACK);
  display.fillCircle(x - 15, y - 15, scale * 1.6, GxEPD_WHITE);
}
//#########################################################################################
void Nodata(int x, int y, bool LargeSize) {
  int scale = Small, offset = 0;
  if (LargeSize) {
    scale = Large;
    offset = 7;
  }
  if (scale == Large)  display.setFont(&FreeMonoBold12pt7b); else display.setFont(&DejaVu_Sans_Bold_11);
  drawString(x - 20, y - 10 + offset, "N/A", LEFT);
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
  if (text.length() > text_width * 2) text = text.substring(0, text_width * 2); // Truncate if too long for 2 rows of text
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  display.setCursor(x, y + h);
  display.println(text.substring(0, text_width));
  if (text.length() > text_width) {
    display.setCursor(x, y + h * 2);
    display.println(text.substring(text_width));
  }
}
//#########################################################################################
void DisplayWxPerson(int x, int y, String IconName) {
  display.drawRect(x, y, 86, 97, GxEPD_BLACK);
  x = x + 3;
  y = y + 7;
  // NOTE: Using 'drawInvertedBitmap' and not 'drawBitmap' so that images are WYSIWYG, otherwise all images need to be inverted
  if      (IconName == "01d" || IconName == "01n")  display.drawInvertedBitmap(x, y, uWX_Sunny,       80, 80, GxEPD_BLACK);
  else if (IconName == "02d" || IconName == "02n")  display.drawInvertedBitmap(x, y, uWX_MostlySunny, 80, 80, GxEPD_BLACK);
  else if (IconName == "03d" || IconName == "03n")  display.drawInvertedBitmap(x, y, uWX_Cloudy,      80, 80, GxEPD_BLACK);
  else if (IconName == "04d" || IconName == "04n")  display.drawInvertedBitmap(x, y, uWX_MostlySunny, 80, 80, GxEPD_BLACK);
  else if (IconName == "09d" || IconName == "09n")  display.drawInvertedBitmap(x, y, uWX_ChanceRain,  80, 80, GxEPD_BLACK);
  else if (IconName == "10d" || IconName == "10n")  display.drawInvertedBitmap(x, y, uWX_Rain,        80, 80, GxEPD_BLACK);
  else if (IconName == "11d" || IconName == "11n")  display.drawInvertedBitmap(x, y, uWX_TStorms,     80, 80, GxEPD_BLACK);
  else if (IconName == "13d" || IconName == "13n")  display.drawInvertedBitmap(x, y, uWX_Snow,        80, 80, GxEPD_BLACK);
  else if (IconName == "50d")                       display.drawInvertedBitmap(x, y, uWX_Haze,        80, 80, GxEPD_BLACK);
  else if (IconName == "50n")                       display.drawInvertedBitmap(x, y, uWX_Fog,         80, 80, GxEPD_BLACK);
  else                                              display.drawInvertedBitmap(x, y, uWX_Nodata,      80, 80, GxEPD_BLACK);
}

void InitialiseDisplay() {
  display.init(115200, true, 2000, false);
  display.setRotation(0);
  display.setTextSize(0);
  display.setFont(&DejaVu_Sans_Bold_11);
  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
  display.clearScreen(0xFF);
  delay(500);
}

/*
  Version 1.0 Initial release

  Version 1.1 Added support for Waveshare ESP32 Driver board

  Version 1.2 Changed GxEPD2 initialisation from 115200 to 0
  1.  Display.init(115200); becomes display.init(0); to stop blank screen following update to GxEPD2
  
  Version 1.3 
  1.  Added extra 20-secs to sleep delay to allow for slower ESP32 RTC timers
  
  Version 1.4
  1. Modified for GxEPD2
  
*/
