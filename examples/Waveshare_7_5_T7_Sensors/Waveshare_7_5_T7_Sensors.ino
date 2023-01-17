/* ESP32 Weather Display using an EPD 7.5" 800x480 Display, obtains data from Open Weather Map, decodes and then displays it.
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
//#include <WiFiClientSecure.h>
#include <time.h>                     // Built-in
#include <SPI.h>                      // Built-in
#include <vector>
#include <string>
#include <Ticker.h>
#include <MQTT.h>
#include <cQueue.h>
#include "garten.h"
#include "wohnung.h"
#include "bitmaps.h"

#define START_SCREEN 0
#define LAST_SCREEN  2
//#define SIMULATE_MQTT
//#define FORCE_LOW_BATTERY
//#define FORCE_NO_SIGNAL

#define MQTT_PAYLOAD_SIZE 4096
#define MQTT_CONNECT_TIMEOUT 30
#define MQTT_DATA_TIMEOUT 600
#define MQTT_KEEPALIVE    60
#define MQTT_TIMEOUT      1800
#define MQTT_CLEAN_SESSION false

#define MQTT_HIST_SIZE  144
#define LOCAL_HIST_SIZE 144
#define HIST_UPDATE_RATE 30
#define HIST_UPDATE_TOL   6

#ifdef FLORA
#define MQTT_SUB_IN "ESPWeather-267B81/data/WeatherSensor"
#endif
#ifdef TTN
// #2
//#define MQTT_SUB_IN "v3/flora-lora@ttn/devices/eui-9876b6000011c87b/up"
// #1
#define MQTT_SUB_IN "v3/flora-lora@ttn/devices/eui-9876b6000011c941/up"
#endif

#define MITHERMOMETER_EN
#define MITHERMOMETER_BATTALERT 6 // Low battery alert threshold 
#define BME280_EN
#define I2C_SDA 21
#define I2C_SCL 22

#define  ENABLE_GxEPD2_display 1
#include <GxEPD2_BW.h>
//#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "src/epaper_fonts.h"
#include "src/forecast_record.h"
//#include "src/lang.h"                     // Localisation (English)
//#include "lang_cz.h"                  // Localisation (Czech)
//#include "lang_fr.h"                  // Localisation (French)
#include "src/lang_de.h"                  // Localisation (German)
//#include "lang_it.h"                  // Localisation (Italian)
//#include "lang_nl.h"
//#include "lang_pl.h"                  // Localisation (Polish)

#ifdef MITHERMOMETER_EN
    // BLE Temperature/Humidity Sensor
    #include <ATC_MiThermometer.h>
#endif

#ifdef BME280_EN
  #include <pocketBME280.h>
#endif

#define SCREEN_WIDTH  800             // Set for landscape mode
#define SCREEN_HEIGHT 480
bool DebugDisplayUpdate = true;

enum alignment {LEFT, RIGHT, CENTER};



// Connections for e.g. LOLIN D32
//static const uint8_t EPD_BUSY = 4;  // to EPD BUSY
//static const uint8_t EPD_CS   = 5;  // to EPD CS
//static const uint8_t EPD_RST  = 16; // to EPD RST
//static const uint8_t EPD_DC   = 17; // to EPD DC
//static const uint8_t EPD_SCK  = 18; // to EPD CLK
//static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
//static const uint8_t EPD_MOSI = 23; // to EPD DIN

// Connections for e.g. Waveshare ESP32 e-Paper Driver Board
static const uint8_t    EPD_BUSY   = 25;
static const uint8_t    EPD_CS     = 15;
static const uint8_t    EPD_RST    = 26; 
static const uint8_t    EPD_DC     = 27; 
static const uint8_t    EPD_SCK    = 13;
static const uint8_t    EPD_MISO   = 12; // Master-In Slave-Out not used, as no data from display
static const uint8_t    EPD_MOSI   = 14;
static const gpio_num_t TOUCH_WAKE = GPIO_NUM_32;
static const uint8_t    TOUCH_INT  = 32;

#ifdef SIMULATE_MQTT
const char * MqttBuf = "{\"end_device_ids\":{\"device_id\":\"eui-9876b6000011c87b\",\"application_ids\":{\"application_id\":\"flora-lora\"},\"dev_eui\":\"9876B6000011C87B\",\"join_eui\":\"0000000000000000\",\"dev_addr\":\"260BFFCA\"},\"correlation_ids\":[\"as:up:01GH0PHSCTGKZ51EB8XCBBGHQD\",\"gs:conn:01GFQX269DVXYK9W6XF8NNZWDD\",\"gs:up:host:01GFQX26AXQM4QHEAPW48E8EWH\",\"gs:uplink:01GH0PHS6A65GBAPZB92XNGYAP\",\"ns:uplink:01GH0PHS6BEPXS9Y7DMDRNK84Y\",\"rpc:/ttn.lorawan.v3.GsNs/HandleUplink:01GH0PHS6BY76SY2VPRSHNDDRH\",\"rpc:/ttn.lorawan.v3.NsAs/HandleUplink:01GH0PHSCS7D3V8ERSKF0DTJ8H\"],\"received_at\":\"2022-11-04T06:51:44.409936969Z\",\"uplink_message\":{\"session_key_id\":\"AYRBaM/qASfqUi+BQK75Gg==\",\"f_port\":1,\"frm_payload\":\"PwOOWAgACAAIBwAAYEKAC28LAw0D4U0DwAoAAAAAwMxMP8DMTD/AzEw/AAAAAAAAAAAA\",\"decoded_payload\":{\"bytes\":{\"air_temp_c\":\"9.1\",\"battery_v\":2927,\"humidity\":88,\"indoor_humidity\":77,\"indoor_temp_c\":\"9.9\",\"rain_day\":\"0.8\",\"rain_hr\":\"0.0\",\"rain_mm\":\"56.0\",\"rain_mon\":\"0.8\",\"rain_week\":\"0.8\",\"soil_moisture\":10,\"soil_temp_c\":\"9.6\",\"status\":{\"ble_ok\":true,\"res\":false,\"rtc_sync_req\":false,\"runtime_expired\":true,\"s1_batt_ok\":true,\"s1_dec_ok\":true,\"ws_batt_ok\":true,\"ws_dec_ok\":true},\"supply_v\":2944,\"water_temp_c\":\"7.8\",\"wind_avg_meter_sec\":\"0.8\",\"wind_direction_deg\":\"180.0\",\"wind_gust_meter_sec\":\"0.8\"}},\"rx_metadata\":[{\"gateway_ids\":{\"gateway_id\":\"lora-db0fc\",\"eui\":\"3135323538002400\"},\"time\":\"2022-11-04T06:51:44.027496Z\",\"timestamp\":1403655780,\"rssi\":-104,\"channel_rssi\":-104,\"snr\":8.25,\"location\":{\"latitude\":52.27640735,\"longitude\":10.54058183,\"altitude\":65,\"source\":\"SOURCE_REGISTRY\"},\"uplink_token\":\"ChgKFgoKbG9yYS1kYjBmYxIIMTUyNTgAJAAQ5KyonQUaCwiA7ZKbBhCw6tpgIKDtnYPt67cC\",\"channel_index\":4,\"received_at\":\"2022-11-04T06:51:44.182146570Z\"}],\"settings\":{\"data_rate\":{\"lora\":{\"bandwidth\":125000,\"spreading_factor\":8,\"coding_rate\":\"4/5\"}},\"frequency\":\"867300000\",\"timestamp\":1403655780,\"time\":\"2022-11-04T06:51:44.027496Z\"},\"received_at\":\"2022-11-04T06:51:44.203702153Z\",\"confirmed\":true,\"consumed_airtime\":\"0.215552s\",\"locations\":{\"user\":{\"latitude\":52.24619,\"longitude\":10.50106,\"source\":\"SOURCE_REGISTRY\"}},\"network_ids\":{\"net_id\":\"000013\",\"tenant_id\":\"ttn\",\"cluster_id\":\"eu1\",\"cluster_address\":\"eu1.cloud.thethings.network\"}}}";
#else
char        MqttBuf[MQTT_PAYLOAD_SIZE+1];
#endif


#ifdef MITHERMOMETER_EN
    // BLE scan time in seconds
    const int bleScanTime = 10;

    // List of known sensors' BLE addresses
    std::vector<std::string> knownBLEAddresses = {"a4:c1:38:b8:1f:7f"};
#endif


GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY));   // B/W display
//GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT / 4> display(GxEPD2_750c(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY)); // 3-colour displays
//GxEPD2_3C<GxEPD2_750c_Z90, GxEPD2_750c_Z90::HEIGHT / 4> display(GxEPD2_750c_Z90(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY)); // 3-colour displays
// use GxEPD_BLACK or GxEPD_WHITE or GxEPD_RED or GxEPD_YELLOW depending on display type

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;  // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
// Using fonts:
// u8g2_font_helvB08_tf
// u8g2_font_helvB10_tf
// u8g2_font_helvB12_tf
// u8g2_font_helvB14_tf
// u8g2_font_helvB18_tf
// u8g2_font_helvB24_tf
// u8g2_font_helvR24_tf

//################  VERSION  ###########################################
String version = "16.11";    // Programme version, see change log at end
//################ VARIABLES ###########################################

boolean LargeIcon = true, SmallIcon = false;
#define Large  17           // For icon drawing, needs to be odd number for best effect
#define Small  6            // For icon drawing, needs to be odd number for best effect
String  Time_str;                    // Curent time as string
String  Date_str;                    // Current date as stringstrings to hold time and received weather data
int     wifi_signal = 0;             //!< WiFi signal strength
int     CurrentHour = 0;             //!< Current time - hour
int     CurrentMin = 0;              //!< Current time - minutes
int     CurrentSec = 0;              //!< Current time - seconds
long    StartTime = 0;               //!< Start timestamp
RTC_DATA_ATTR bool    touchTrig = false;           //!< Flag: Touch sensor has been triggered
bool    mqttMessageReceived = false; //!< Flag: MQTT message has been received

//################ PROGRAM VARIABLES and OBJECTS ################

#define max_readings 24

RTC_DATA_ATTR Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];

#include "src/common.h"

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

// Screen definitions
#define ScreenOWM   0
#define ScreenLocal 1
#define ScreenMQTT  2
#define ScreenTest  3

String Locations[] = {"Braunschweig", "Wohnung", "Garten"};

// OWM Forecast Data
float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings]    = {0};
float rain_readings[max_readings]        = {0};
float snow_readings[max_readings]        = {0};

// MQTT Sensor Data
struct MqttS {
    bool     valid;                //!< 
    struct {
        unsigned int ws_batt_ok:1; //!< weather sensor battery o.k.
        unsigned int ws_dec_ok:1;  //!< weather sensor decoding o.k.
        unsigned int s1_batt_ok:1; //!< soil moisture sensor battery o.k.
        unsigned int s1_dec_ok:1;  //!< soil moisture sensor dencoding o.k.
        unsigned int ble_ok:1;     //!< BLE T-/H-sensor data o.k.
        
    } status;
    float    air_temp_c;           //!< temperature in degC
    uint8_t  humidity;             //!< humidity in %
    float    wind_direction_deg;   //!< wind direction in deg
    float    wind_gust_meter_sec;  //!< wind speed (gusts) in m/s
    float    wind_avg_meter_sec;   //!< wind speed (avg)   in m/s
    float    rain_mm;              //!< rain gauge level in mm
    uint16_t supply_v;             //!< supply voltage in mV
    uint16_t battery_v;            //!< battery voltage in mV
    float    water_temp_c;         //!< water temperature in degC
    float    indoor_temp_c;        //!< indoor temperature in degC
    uint8_t  indoor_humidity;      //!< indoor humidity in %
    float    soil_temp_c;          //!< soil temperature in degC
    uint8_t  soil_moisture;        //!< soil moisture in %
    float    rain_hr;              //!< hourly precipitation in mm 
    float    rain_day;             //!< daily precipitation in mm
    float    rain_week;            //!< weekly precipitation in mm
    float    rain_month;           //!< monthly precipitatiion in mm
};

typedef struct MqttS mqtt_sensors_t;      //!< Shortcut for struct Sensor
RTC_DATA_ATTR mqtt_sensors_t MqttSensors; //!< MQTT sensor data

RTC_DATA_ATTR Queue_t MqttHistQCtrl;      //!< MQTT Sensor Data History FIFO Control

struct MqttHistQData {
  float temperature;  //!< temperature in degC
  float humidity;     //!< humidity in %
  float rain;         //!< precipitation in mm
  bool  valid;        //!< data valid
};

typedef struct MqttHistQData mqtt_hist_t; //!< Shortcut for struct MqttHistQData

RTC_DATA_ATTR mqtt_hist_t MqttHist[MQTT_HIST_SIZE]; //<! MQTT Sensor Data History


// Local Sensor Data
struct LocalS {
  struct {
      bool     valid;              //!< data valid
      float    temperature;        //!< temperature in degC
      float    humidity;           //!< humidity in %
      uint16_t batt_voltage;       //!< battery voltage in mV
      uint8_t  batt_level;         //!< battery level in %
      int      rssi;               //!< RSSI in dBm
  } ble_thsensor[1];
  struct {
      bool     valid;              //!< data valid
      float    temperature;        //!< temperature in degC
      float    humidity;           //!< humidity in %
      float    pressure;           //! pressure in hPa
  } i2c_thpsensor[1];
};

typedef struct LocalS local_sensors_t; //!< Shortcut for struct LocalS
local_sensors_t LocalSensors;          //!< Local Sensor Data

RTC_DATA_ATTR Queue_t LocalHistQCtrl;  //!< Local Sensor Data History FIFO Control

struct LocalHistQData {
  float temperature;  //!< temperature in degC
  float humidity;     //!< humidity in %
  float pressure;     //!< pressure in hPa
};

typedef struct LocalHistQData local_hist_t;            //!< Shortcut for struct LocalHistQData

RTC_DATA_ATTR local_hist_t LocalHist[LOCAL_HIST_SIZE]; // Local Sensor Data History


long SleepDuration = 30; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int  WakeupTime    = 7;  // Don't wakeup until after 07:00 to save battery power
int  SleepTime     = 23; // Sleep after (23+1) 00:00 to save battery power

RTC_DATA_ATTR int   ScreenNo     = START_SCREEN; //!< Current Screen No.
RTC_DATA_ATTR int   PrevScreenNo = -1;           //!< Previous Screen No.

Ticker HistoryUpdater;

void UpdateHistory() {
  SaveMqttData();
}

//#########################################################################################
// Touch Interrupt Service Routine
void ARDUINO_ISR_ATTR touch_isr() {
    touchTrig = true;
}

//#########################################################################################
void setup() {
    WiFiClient net;

    StartTime = millis();
    Serial.begin(115200);
    bool mqtt_connected = false;
    bool wifi_ok = false;
    bool time_ok = false;
    
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0 || touchTrig) {
        PrevScreenNo = ScreenNo;
        ScreenNo = (ScreenNo == LAST_SCREEN) ? 0 : ScreenNo + 1;
        touchTrig = false;
    } else if (esp_sleep_get_wakeup_cause() == 0xC /* SW_CPU_RESET */) {
        ;
    } 
    //else if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        // FIXME Check if touch wakeup occurs at the same time as timer wakeup
        //SaveLocalData();
    //}
    Serial.println("sizeof(WxConditions): " +String(sizeof(WxConditions[1])));
    Serial.println("sizeof(WxForecast):   " +String(sizeof(WxForecast[max_readings])));

    // FIXME: calculate time to trigger
    //int tmr = 0;
    //HistoryUpdater.once(tmr, UpdateHistory);
    
    Serial.printf("Screen No: %d\n", ScreenNo);
    
    pinMode(TOUCH_INT, INPUT);
    
    attachInterrupt(TOUCH_INT, touch_isr, RISING);
        
    if (ScreenNo != PrevScreenNo) {
        InitialiseDisplay();
        //display.fillRect(0, 0, SCREEN_WIDTH, 18, GxEPD_WHITE);
        DisplayGeneralInfoSection();
        display.displayWindow(0, 0, SCREEN_WIDTH, 19);
        int x = SCREEN_WIDTH/2 - 24;
        int y = SCREEN_HEIGHT/2 -24;
        display.fillRect(x, y, 48, 48, GxEPD_WHITE);
        display.drawBitmap(x, y, epd_bitmap_hourglass_top, 48, 48, GxEPD_BLACK);
        display.displayWindow(x, y, 48, 48);

        display.setFullWindow();
    }
    
    wifi_ok = (StartWiFi() == WL_CONNECTED);
    time_ok = SetupTime();
    
    Serial.println("WiFi o.k.: " + String(wifi_ok));
    Serial.println("Time o.k.: " + String(time_ok));
    Serial.println(Date_str + " " + Time_str);

  
    if (ScreenNo == ScreenOWM && wifi_ok) {
        //WiFiClient client;
        if ((CurrentHour >= WakeupTime && CurrentHour <= SleepTime) || DebugDisplayUpdate) {
            //InitialiseDisplay(); // Give screen time to initialise by getting weather data!
            byte Attempts = 1;
            bool RxWeather = false, RxForecast = false;
            // WiFiClient client;   // wifi client object
            while ((RxWeather == false || RxForecast == false) && Attempts <= 2) { // Try up-to 2 time for Weather and Forecast data
                if (RxWeather  == false) RxWeather  = obtain_wx_data(net, "weather");
                if (RxForecast == false) RxForecast = obtain_wx_data(net, "forecast");
                Attempts++;
            }
            if (RxWeather && RxForecast) { // Only if received both Weather or Forecast proceed
                //StopWiFi(); // Reduces power consumption
                ClearHourglass();
                DisplayOWMWeather();
                display.display(false); // Full screen update mode
            }
        }
    }
    else {

    }
    
    GetLocalData();
    if (HistoryUpdateDue()) {
        SaveLocalData();
    }
    
    if (ScreenNo == ScreenLocal) {
        //InitialiseDisplay();
        ClearHourglass();
        DisplayLocalWeather();
        display.display(false);      
    }

    // Screen has changed to MQTT - immediately update screen with saved data 
    if (ScreenNo == ScreenMQTT && ScreenNo != PrevScreenNo) {
        //InitialiseDisplay();
        ClearHourglass();
        DisplayMQTTWeather();
        display.display(false);      
    }

    // Display WiFi-Off-Icon
    if (!wifi_ok) {
        // Screen not changed - show next to Date/Time of last update
        if (ScreenNo == PrevScreenNo) {
            int x = (ScreenNo == ScreenOWM) ? 595 :  88;
            int y = (ScreenNo == ScreenOWM) ? 190 : 272;
            
            display.drawBitmap(x, y, epd_bitmap_wifi_off, 48, 48, GxEPD_BLACK);
            display.displayWindow(x, y, 48, 48);
            display.setFullWindow();
        }
        else {
            // Screen has changed to OWM (no previous data available)
            // show on at the screen's center 
            if (ScreenNo == ScreenOWM) {
                int x = SCREEN_WIDTH/2 - 24;
                int y = SCREEN_HEIGHT/2 -24;
                display.fillRect(x, y, 48, 48, GxEPD_WHITE);
                display.drawBitmap(x, y, epd_bitmap_wifi_off, 48, 48, GxEPD_BLACK);
                display.displayWindow(x, y, 48, 48);
                display.setFullWindow();
            }
        }
    }

    
    // Fetch MQTT data
    MQTTClient MqttClient(MQTT_PAYLOAD_SIZE);
    mqtt_connected = MqttConnect(net, MqttClient); 
    if (mqtt_connected) {
//        int x = (ScreenNo == ScreenOWM) ? 595 :  88;
  //      int y = (ScreenNo == ScreenOWM) ? 190 : 272;
        int x = 88;
        int y = (ScreenNo == ScreenOWM) ? 300 : 272;
        display.drawBitmap(x, y, epd_bitmap_downloading, 48, 48, GxEPD_BLACK);
        display.displayWindow(x, y, 48, 48);
        GetMqttData(net, MqttClient);
        display.fillRect(x, y, 48, 48, GxEPD_WHITE);
        display.displayWindow(x, y, 48, 48);
        display.setFullWindow();
    }
    if (HistoryUpdateDue()) {
        SaveMqttData();
    }
    
    // Update MQTT screen if active and data is available
    if (ScreenNo == ScreenMQTT && MqttSensors.valid) {
        display.fillScreen(GxEPD_WHITE);
        DisplayMQTTWeather();
        display.display(false);
    }

  
  
    StopWiFi();
  
    BeginSleep();
}
//#########################################################################################
void loop() { // this will never run!
}
//#########################################################################################
void BeginSleep() {
  display.powerOff();
  long SleepTimer;
  if (touchTrig) {
      // wake up immediately
      SleepTimer = 1000;
  } else {
      // Wake up at fixed interval, synchronized to begin of full hour
      SleepTimer = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec)); //Some ESP32 are too fast to maintain accurate time
      SleepTimer = (SleepTimer + 20) * 1000000LL; // Added extra 20-secs of sleep to allow for slow ESP32 RTC timers
  }
  esp_sleep_enable_timer_wakeup(SleepTimer); 
  esp_sleep_enable_ext0_wakeup(TOUCH_WAKE, 1); // Wake up from touch sensor
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Entering " + String(SleepTimer) + "-secs of sleep time");
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();      // Sleep for e.g. 30 minutes
}
//#########################################################################################
void ClearHourglass() {

    int x = SCREEN_WIDTH/2 - 24;
    int y = SCREEN_HEIGHT/2 -24;
    display.fillRect(x, y, 48, 48, GxEPD_WHITE);
    display.displayWindow(x, y, 48, 48);

    display.setFullWindow();
}

//#########################################################################################
bool HistoryUpdateDue(void) {
    if (SetupTime()) {
      Serial.println("HistoryUpdateDue(): " + Date_str + " " + Time_str);
    }
    int mins = (CurrentHour * 60 + CurrentMin) % HIST_UPDATE_RATE;
    bool rv = (mins <= HIST_UPDATE_TOL) || (mins >= (HIST_UPDATE_RATE - HIST_UPDATE_TOL));
    Serial.println("HistoryUpdateDue(): " + String(rv));
    return rv;
    
}
//#########################################################################################
void mqttMessageCb(String &topic, String &payload) {
  mqttMessageReceived = true;
  Serial.printf("Payload size: %d\n", payload.length());
  #ifndef SIMULATE_MQTT
  strncpy(MqttBuf, payload.c_str(), payload.length());
  #endif
  
}
void mqttMessageAdvancedCb(MQTTClient *client, char topic[], char bytes[], int length) {
  mqttMessageReceived = true;
  Serial.printf("Payload size: %d\n", length);
  #ifndef SIMULATE_MQTT
  strncpy(MqttBuf, bytes, length);
  #endif 
}

//#########################################################################################
void DisplayOWMWeather() {
  DisplayGeneralInfoSection();                  // Top line of the display
  //DisplayDateTime(487, 194);
  DisplayDateTime(90, 255);
  DisplayDisplayWindSection(108, 146, WxConditions[0].Winddir, WxConditions[0].Windspeed, 81, true, TXT_WIND_SPEED_DIRECTION);
  DisplayMainWeatherSection(300, 100);          // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
  DisplayForecastSection(217, 245);             // 3hr forecast boxes
  DisplayAstronomySection(391, 165);            // Astronomy section Sun rise/set, Moon phase and Moon icon
  //DrawRSSI(695, 15, wifi_signal);
  DrawRSSI(705, 15, wifi_signal);
}

bool MqttConnect(WiFiClient& net, MQTTClient& MqttClient) {
  Serial.print("checking wifi...");
  if (StartWiFi() != WL_CONNECTED) {
    return false;
  }

  Serial.print("\nMQTT connecting...");
  unsigned long start = millis();

  MqttClient.begin(MQTT_HOST, MQTT_PORT, net);
  MqttClient.setOptions(MQTT_KEEPALIVE /* keepAlive [s] */, MQTT_CLEAN_SESSION /* cleanSession */, MQTT_TIMEOUT * 1000 /* timeout [ms] */);

  while (!MqttClient.connect(Hostname, MQTT_USER, MQTT_PASS)) {
    Serial.print(".");
    if (millis() > start + MQTT_CONNECT_TIMEOUT * 1000) {
      Serial.println("\nconnect timeout!");
      return false;
    }
    delay(1000);
  }
  Serial.println("\nconnected!");

  //mqtt_client.onMessageAdvanced(mqttMessageAdvancedCb);
  MqttClient.onMessage(mqttMessageCb);

  if (!MqttClient.subscribe(MQTT_SUB_IN)) {
    Serial.println("MQTT subscription failed!");
    return false;
  }
  return true;
}
//#########################################################################################
void findMqttMinMaxTemp(float * t_min, float * t_max) {
  // Find min/max temperature in local history data
  mqtt_hist_t mqtt_data;
  float outdoorTMin = 0;
  float outdoorTMax = 0;

  if (!q_isInitialized(&MqttHistQCtrl)) {
    return;
  }

  // Go back in FIFO at most 24 hrs
  int maxIdx = 24 * 60 / SleepDuration;
  if (q_getCount(&MqttHistQCtrl) < maxIdx) {
    maxIdx = q_getCount(&MqttHistQCtrl);
  }

  // Initialize min/max with last valid entry
  for (int i=1; i<maxIdx; i++) {
    if (q_peekIdx(&MqttHistQCtrl, &mqtt_data, i) && mqtt_data.valid) {
        outdoorTMin = mqtt_data.temperature;
        outdoorTMax = mqtt_data.temperature;
        break;
    }
  }
  
  // Peek into FIFO and get min/max
  for (int i=1; i<maxIdx; i++) {
    if (q_peekIdx(&MqttHistQCtrl, &mqtt_data, i)) {
      if (mqtt_data.valid) {
        if (mqtt_data.temperature < outdoorTMin) {
            outdoorTMin = mqtt_data.temperature;
        }
        if (mqtt_data.temperature > outdoorTMax) {
            outdoorTMax = mqtt_data.temperature;
        }
      }
    }
  }
  *t_min = outdoorTMin;
  *t_max = outdoorTMax;
}

//#########################################################################################
void GetMqttData(WiFiClient& net, MQTTClient& MqttClient) {
  /*
  MQTTClient mqtt_client(MQTT_PAYLOAD_SIZE);
  mqtt_client.begin(MQTT_HOST, MQTT_PORT, wifi_client);
  Serial.println(F("MQTT connecting... "));
  //mqtt_client.setCleanSession(false);
  mqtt_client.setOptions(MQTT_KEEPALIVE, MQTT_CLEAN_SESSION, MQTT_TIMEOUT);
  //mqtt_connect(mqtt_client);
  //mqtt_client.onMessageAdvanced(mqttMessageAdvancedCb);
  mqtt_client.onMessage(mqttMessageCb);
  while (!mqtt_client.connect(Hostname, MQTT_USER, MQTT_PASS)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");

  if (!mqtt_client.subscribe(MQTT_SUB_IN)) {
    Serial.println("Subscription failed!");
  }
  */
  //DisplayGeneralInfoSection();                 // Top line of the display
  
  MqttSensors.valid = false;
  Serial.println(F("Waiting for MQTT message..."));
  #ifndef SIMULATE_MQTT
    unsigned long start = millis();
    int count = 0;
    while (!mqttMessageReceived) {
      MqttClient.loop();
      delay(10);
      if (count++ == 1000) {
          Serial.print(".");
          count = 0;
      }
      if (mqttMessageReceived)
        break;
      if (!MqttClient.connected()) {
        MqttConnect(net, MqttClient);
      }
      if (touchTrig) {
          Serial.println(F("Touch interrupt!"));
          return;
      }
      if (millis() > start + MQTT_DATA_TIMEOUT * 1000) {
        Serial.println(F("Timeout!"));
        MqttClient.disconnect();
        return;
      }
    }
  #else
    Serial.println("(Simulated MQTT incoming message)");
    MqttSensors.valid = true;
  #endif
  Serial.println(F("done!"));
  MqttClient.disconnect();
  Serial.printf(MqttBuf);
  
  Serial.print(F("\nCreating JSON object..."));

  // allocate the JsonDocument
  //StaticJsonDocument<MQTT_PAYLOAD_SIZE> doc;
  DynamicJsonDocument doc(MQTT_PAYLOAD_SIZE);
  
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, MqttBuf, MQTT_PAYLOAD_SIZE);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  else {
    Serial.println(F("done!"));
  }
  MqttSensors.valid = true;
  
  JsonObject uplink_message = doc["uplink_message"];

  // uplink_message_decoded_payload_bytes -> payload
  JsonObject payload = uplink_message["decoded_payload"]["bytes"];

  MqttSensors.air_temp_c          = payload["air_temp_c"];
  MqttSensors.humidity            = payload["humidity"];
  MqttSensors.indoor_temp_c       = payload["indoor_temp_c"];
  MqttSensors.indoor_humidity     = payload["indoor_humidity"];
  MqttSensors.battery_v           = payload["battery_v"];
  MqttSensors.rain_day            = payload["rain_day"];
  MqttSensors.rain_hr             = payload["rain_hr"];
  MqttSensors.rain_mm             = payload["rain_mm"];
  MqttSensors.rain_month          = payload["rain_mon"];
  MqttSensors.rain_week           = payload["rain_week"];
  MqttSensors.soil_moisture       = payload["soil_moisture"];
  MqttSensors.soil_temp_c         = payload["soil_temp_c"];
  MqttSensors.water_temp_c        = payload["water_temp_c"];
  MqttSensors.wind_avg_meter_sec  = payload["wind_avg_meter_sec"];
  MqttSensors.wind_direction_deg  = payload["wind_direction_deg"];
  MqttSensors.wind_gust_meter_sec = payload["wind_gust_meter_sec"];

  JsonObject status = payload["status"];
  MqttSensors.status.ble_ok     = status["ble_ok"];
  MqttSensors.status.s1_batt_ok = status["s1_batt_ok"];
  MqttSensors.status.s1_dec_ok  = status["s1_dec_ok"];
  MqttSensors.status.ws_batt_ok = status["ws_batt_ok"];
  MqttSensors.status.ws_dec_ok  = status["ws_dec_ok"];  
}
//#########################################################################################
void SaveMqttData() {
    Serial.println(F("Saving MQTT data."));
    if (!q_isInitialized(&MqttHistQCtrl)) {
      q_init_static(&MqttHistQCtrl, sizeof(MqttHist[0]), MQTT_HIST_SIZE, FIFO, true, (uint8_t *)MqttHist, sizeof(MqttHist));
    }
    mqtt_hist_t mqtt_data = {0, 0, 0, 0};
    if (MqttSensors.valid) {
      mqtt_data.temperature = MqttSensors.air_temp_c;
      mqtt_data.humidity    = MqttSensors.humidity;
      mqtt_data.rain        = MqttSensors.rain_mm;
      mqtt_data.valid       = true;
    }
    /*
    else {
      // Store previous data
      mqtt_hist_t prev_data = {0, 0, 0};
      if (q_peekPrevious(&MqttHistQCtrl, &prev_data)) {
        mqtt_data.temperature = prev_data.temperature;
        mqtt_data.humidity    = prev_data.humidity;
        mqtt_data.rain        = prev_data.rain;
      }
    }
    */
    q_push(&MqttHistQCtrl, &mqtt_data);   
}

//#########################################################################################
void DisplayMQTTWeather(void) { 
  //Serial.println("DisplayMQTTWeather(): calling DisplayGeneralInfoSection()");
  DisplayGeneralInfoSection();
  //Serial.println("DisplayMQTTWeather(): DisplayGeneralInfoSection() o.k.");
  //Serial.println("DisplayMQTTWeather(): Date_str=" + Date_str + " - calling DisplayDateTime()");
  DisplayDateTime(90, 225);
  //Serial.println("DisplayMQTTWeather(): DisplayDateTime() o.k.");
  display.drawBitmap(  5,  25, epd_bitmap_garten_sw, 220, 165, GxEPD_BLACK);
  display.drawRect(    4,  24, 222, 167, GxEPD_BLACK);
  display.drawBitmap(240,  45, epd_bitmap_temperatur_aussen, 64, 48, GxEPD_BLACK);
  display.drawBitmap(438,  45, epd_bitmap_feuchte_aussen, 64, 48, GxEPD_BLACK);
  display.drawBitmap(240, 100, epd_bitmap_temperatur_innen, 64, 48, GxEPD_BLACK);
  display.drawBitmap(438, 100, epd_bitmap_feuchte_innen, 64, 48, GxEPD_BLACK);
  display.drawBitmap(240, 155, epd_bitmap_boden_temperatur, 64, 48, GxEPD_BLACK);
  display.drawBitmap(438, 155, epd_bitmap_boden_feuchte, 64, 48, GxEPD_BLACK);
  display.drawBitmap(240, 210, epd_bitmap_wasser_temperatur, 48, 48, GxEPD_BLACK);

  // Weather Sensor
  #ifdef FORCE_NO_SIGNAL
  MqttSensors.status.ws_dec_ok = false;
  #endif
  //float outdoorTMin;
  //float outdoorTMax;
  // Find min/max temperature in local history data
  float outdoorTMin = 0;
  float outdoorTMax = 0;
  findMqttMinMaxTemp(&outdoorTMin, &outdoorTMax);

  if (MqttSensors.status.ws_dec_ok) {
    DisplayLocalTemperatureSection(358,  22, 137, 100, "", true, MqttSensors.air_temp_c, true, outdoorTMin, outdoorTMax);
    u8g2Fonts.setFont(u8g2_font_helvR24_tf);
    drawString(524,  75, String(MqttSensors.humidity) + "%", CENTER);

    #ifdef FORCE_LOW_BATTERY
    MqttSensors.status.ws_batt_ok = false;
    #endif
    if (!MqttSensors.status.ws_batt_ok) {
      display.drawBitmap(577,  60, epd_bitmap_battery_alert, 24, 24, GxEPD_BLACK);
    }
  }
  else {
    // No outdoor temperature
    display.drawBitmap(320,  47, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
    // No outdoor humidity
    display.drawBitmap(518,  47, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
  }
  
  // Indoor Sensor (BLE)
  if (MqttSensors.status.ble_ok) {
    DisplayLocalTemperatureSection(358,  77, 137, 100, "", true, MqttSensors.indoor_temp_c, false, 0, 0);
    u8g2Fonts.setFont(u8g2_font_helvR24_tf);
    drawString(524, 130, String(MqttSensors.indoor_humidity) + "%", CENTER);

    #ifdef FORCE_LOW_BATTERY
    MqttSensors.status.ws_batt_ok = false;
    #endif
    if (!MqttSensors.status.ws_batt_ok) {
      display.drawBitmap(577, 115, epd_bitmap_battery_alert, 24, 24, GxEPD_BLACK);
    }
  }
  else {
    // No indoor temperature
    display.drawBitmap(320, 102, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
    // No indoor humidity
    display.drawBitmap(518, 102, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
  }
      
  // Soil Sensor
  #ifdef FORCE_NO_SIGNAL
  MqttSensors.status.s1_dec_ok = false;
  #endif
  if (MqttSensors.status.s1_dec_ok) {
    DisplayLocalTemperatureSection(358,  132, 137, 100, "", true, MqttSensors.soil_temp_c, false, 0, 0);
    u8g2Fonts.setFont(u8g2_font_helvR24_tf);
    drawString(524, 185, String(MqttSensors.soil_moisture) + "%", CENTER);
    #ifdef FORCE_LOW_BATTERY
    MqttSensors.status.s1_batt_ok = false;
    #endif
    if (!MqttSensors.status.s1_batt_ok) {
      display.drawBitmap(580, 167, epd_bitmap_battery_alert, 24, 24, GxEPD_BLACK);
    }
  }
  else {
    // No soil temperature
    display.drawBitmap(320,  157, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
    // No soil moisture
    display.drawBitmap(518,  157, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
  }
  DisplayLocalTemperatureSection(358, 187, 137, 100, "", true, MqttSensors.water_temp_c, false, 0, 0);
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);

  #ifdef FORCE_NO_SIGNAL
  MqttSensors.status.ws_dec_ok = false;
  #endif
  DisplayDisplayWindSection(700, 120, MqttSensors.wind_direction_deg, MqttSensors.wind_avg_meter_sec, 81, MqttSensors.status.ws_dec_ok, "");
  if (!MqttSensors.status.ws_dec_ok) {
    display.drawBitmap(680, 100, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
  }
  DisplayMqttHistory();
  //DrawRSSI(695, 15, wifi_signal); // Wi-Fi signal strength
  DrawRSSI(705, 15, wifi_signal); // Wi-Fi signal strength
  
}
//#########################################################################################
void GetLocalData() {
  LocalSensors.ble_thsensor[0].valid  = false;
  LocalSensors.i2c_thpsensor[0].valid = false; 

  #ifdef MITHERMOMETER_EN
    // Setup BLE Temperature/Humidity Sensors
    ATC_MiThermometer miThermometer(knownBLEAddresses); //!< Mijia Bluetooth Low Energy Thermo-/Hygrometer
    miThermometer.begin();
    
    // Set sensor data invalid
    miThermometer.resetData();
        
    // Get sensor data - run BLE scan for <bleScanTime>
    miThermometer.getData(bleScanTime);
    
    if (miThermometer.data[0].valid) {
        LocalSensors.ble_thsensor[0].valid       = true;
        LocalSensors.ble_thsensor[0].temperature = miThermometer.data[0].temperature/100.0;
        LocalSensors.ble_thsensor[0].humidity    = miThermometer.data[0].humidity/100.0;
        LocalSensors.ble_thsensor[0].batt_level  = miThermometer.data[0].batt_level;
        Serial.printf("Outdoor Air Temp.:   % 3.1f °C\n", LocalSensors.ble_thsensor[0].temperature);
        Serial.printf("Outdoor Humidity:     %3.1f %%\n", LocalSensors.ble_thsensor[0].humidity);
        Serial.printf("Outdoor Sensor Batt:    %d %%\n", LocalSensors.ble_thsensor[0].batt_level);
    } else {
        Serial.printf("Outdoor Air Temp.:    --.- °C\n");
        Serial.printf("Outdoor Humidity:     --   %%\n");
        Serial.printf("Outdoor Sensor Batt:  --   %%\n");
    }
    miThermometer.clearScanResults();
  #endif

  #ifdef BME280_EN
    TwoWire myWire = TwoWire(0);
    myWire.begin(I2C_SDA, I2C_SCL, 100000);

    pocketBME280 bme280;
    bme280.setAddress(0x76);
    Serial.println("BME280: start");
    if (bme280.begin(myWire)) {
        LocalSensors.i2c_thpsensor[0].valid = true;
        bme280.startMeasurement();
        while (!bme280.isMeasuring()) {
          Serial.println("BME280: Waiting for Measurement to start");
          delay(1);
        }
        while (bme280.isMeasuring()) {
          Serial.println("BME280: Measurement in progress");
          delay(1);
        }
        LocalSensors.i2c_thpsensor[0].temperature = bme280.getTemperature() / 100.0;
        LocalSensors.i2c_thpsensor[0].pressure    = bme280.getPressure() / 100.0;
        LocalSensors.i2c_thpsensor[0].humidity    = bme280.getHumidity() / 1024.0;
        Serial.printf("Indoor Temperature: %.1f °C\n", LocalSensors.i2c_thpsensor[0].temperature);
        Serial.printf("Indoor Pressure: %.0f hPa\n",   LocalSensors.i2c_thpsensor[0].pressure);
        Serial.printf("Indoor Humidity: %.0f %%rH\n",  LocalSensors.i2c_thpsensor[0].humidity);
    }
    else {
        Serial.printf("Indoor Temperature: --.- °C\n");
        Serial.printf("Indoor Pressure:    --   hPa\n");
        Serial.printf("Indoor Humidity:    --  %%rH\n");      
    }
  
  /*
  if (LocalSensors.ble_thsensor[0].valid) {
    OutdoorTmin = (LocalSensors.ble_thsensor[0].temperature < OutdoorTmin) ? LocalSensors.ble_thsensor[0].temperature : OutdoorTmin;
    OutdoorTmax = (LocalSensors.ble_thsensor[0].temperature > OutdoorTmax) ? LocalSensors.ble_thsensor[0].temperature : OutdoorTmax;
  }
  */
  #endif
}
//#########################################################################################
void SaveLocalData() {
    if (!q_isInitialized(&LocalHistQCtrl)) {
      q_init_static(&LocalHistQCtrl, sizeof(LocalHist[0]), LOCAL_HIST_SIZE, FIFO, true, (uint8_t *)LocalHist, sizeof(LocalHist));
    }
    local_hist_t local_data;
    if (LocalSensors.ble_thsensor[0].valid) {
      local_data.temperature = LocalSensors.ble_thsensor[0].temperature;
      local_data.humidity    = LocalSensors.ble_thsensor[0].humidity;
    }
    else {
      // Store previous data
      local_hist_t prev_data;
      if (q_peekPrevious(&LocalHistQCtrl, &prev_data)) {
        local_data.temperature = prev_data.temperature;
        local_data.humidity    = prev_data.humidity;
      }
    }
    if (LocalSensors.i2c_thpsensor[0].valid) {
      local_data.pressure = LocalSensors.i2c_thpsensor[0].pressure;
    }
    else {
      // Store previous data
      local_hist_t prev_data;
      if (q_peekPrevious(&LocalHistQCtrl, &prev_data)) {
        local_data.pressure = prev_data.pressure;
      }      
    }
    q_push(&LocalHistQCtrl, &local_data);   
}
//#########################################################################################
void findLocalMinMaxTemp(float * t_min, float * t_max) {
  // Find min/max temperature in local history data
  local_hist_t local_data;
  float outdoorTMin = 0;
  float outdoorTMax = 0;

  if (!q_isInitialized(&MqttHistQCtrl)) {
    return;
  }

  // Initialize min/max with last entry 
  if (q_peekPrevious(&LocalHistQCtrl, &local_data)) {
    outdoorTMin = local_data.temperature;
    outdoorTMax = local_data.temperature;
  }

  // Go back in FIFO at most 24 hrs
  int maxIdx = 24 * 60 / SleepDuration;
  if (q_getCount(&LocalHistQCtrl) < maxIdx) {
    maxIdx = q_getCount(&LocalHistQCtrl);
  }
  
  // Peek into FIFO and get min/max
  for (int i=1; i<maxIdx; i++) {
    if (q_peekIdx(&LocalHistQCtrl, &local_data, i)) {
      if (local_data.temperature < outdoorTMin) {
        outdoorTMin = local_data.temperature;
      }
      if (local_data.temperature > outdoorTMax) {
        outdoorTMax = local_data.temperature;
      }
    }
  }
  *t_min = outdoorTMin;
  *t_max = outdoorTMax;
}
//#########################################################################################
void DisplayLocalWeather() {                        // 7.5" e-paper display is 800x480 resolution
  
  DisplayGeneralInfoSection();
  DisplayDateTime(90, 225);
  display.drawBitmap(  5,  25, epd_bitmap_wohnung_sw, 220, 165, GxEPD_BLACK);
  display.drawRect(    4,  24, 222, 167, GxEPD_BLACK);
  display.drawBitmap(240,  45, epd_bitmap_temperatur_aussen, 64, 48, GxEPD_BLACK);
  display.drawBitmap(438,  45, epd_bitmap_feuchte_aussen, 64, 48, GxEPD_BLACK);
  display.drawBitmap(240, 178, epd_bitmap_temperatur_innen, 64, 48, GxEPD_BLACK);
  display.drawBitmap(438, 178, epd_bitmap_feuchte_innen, 64, 48, GxEPD_BLACK);
  display.drawBitmap(660,  20, epd_bitmap_barometer, 80, 64, GxEPD_BLACK);

  // Find min/max temperature in local history data
  float outdoorTMin = 0;
  float outdoorTMax = 0;
  findLocalMinMaxTemp(&outdoorTMin, &outdoorTMax);
  
  // Outdoor sensor
  #ifdef FORCE_NO_SIGNAL
    LocalSensors.ble_thsensor[0].valid = false;
  #endif
  if (LocalSensors.ble_thsensor[0].valid) {
    DisplayLocalTemperatureSection(358,  22, 137, 100, "", true, LocalSensors.ble_thsensor[0].temperature, true, outdoorTMin, outdoorTMax);
    //u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    //u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    u8g2Fonts.setFont(u8g2_font_helvR24_tf);
    drawString(524,  75, String(LocalSensors.ble_thsensor[0].humidity, 0) + "%", CENTER);
    #ifdef FORCE_LOW_BATTERY
      LocalSensors.ble_thsensor[0].batt_level = MITHERMOMETER_BATTALERT;
    #endif
    if (LocalSensors.ble_thsensor[0].batt_level <= MITHERMOMETER_BATTALERT) {
      display.drawBitmap(577,  60, epd_bitmap_battery_alert, 24, 24, GxEPD_BLACK);
    }
  }
  else {
    // No outdoor temperature
    display.drawBitmap(320,  47, epd_bitmap_bluetooth_disabled, 40, 40, GxEPD_BLACK);
    // No outdor humidity
    display.drawBitmap(528,  47, epd_bitmap_bluetooth_disabled, 40, 40, GxEPD_BLACK);
  }

  // Indoor sensor
  if (LocalSensors.i2c_thpsensor[0].valid) {
    DisplayLocalTemperatureSection(358, 156, 137, 100, "", true, LocalSensors.i2c_thpsensor[0].temperature, false, 0, 0);
    //u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    //u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    u8g2Fonts.setFont(u8g2_font_helvR24_tf);
    drawString(524, 208, String(LocalSensors.i2c_thpsensor[0].humidity, 0) + "%", CENTER);
    drawString(660, 110, String(LocalSensors.i2c_thpsensor[0].pressure, 0) + "hPa", CENTER);
  }
  else {
     // No indoor temperature 
     display.drawBitmap(315,  180, epd_bitmap_bolt, 40, 40, GxEPD_BLACK);
     // No indoor humidity
     display.drawBitmap(523,  180, epd_bitmap_bolt, 40, 40, GxEPD_BLACK);
     // No pressure
     display.drawBitmap(670,   90, epd_bitmap_bolt, 40, 40, GxEPD_BLACK);
  }
  DisplayLocalHistory();
  //DrawRSSI(695, 15, wifi_signal); // Wi-Fi signal strength
  DrawRSSI(705, 15, wifi_signal); // Wi-Fi signal strength
}
//#########################################################################################
void DisplayTestScreen(void) {
    //float outdoor_temp_c = 9.9;
    //Serial.println("Temperature: "+String(outdoor_temp_c));
    Serial.println("Air Temperature: "+String(MqttSensors.air_temp_c));
    display.drawRect(159, 239, 222, 167, GxEPD_BLACK);
    display.drawBitmap(160, 240, epd_bitmap_garten_sw, 220, 165, GxEPD_BLACK);
    display.drawRect(419, 239, 222, 167, GxEPD_BLACK);
    display.drawBitmap(420, 240, epd_bitmap_wohnung_sw, 220, 165, GxEPD_BLACK);
    display.drawBitmap(5, 420, epd_bitmap_temperatur_innen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(60, 420, epd_bitmap_temperatur_aussen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(130, 420, epd_bitmap_feuchte_innen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(200, 420, epd_bitmap_feuchte_aussen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(270, 420, epd_bitmap_boden_temperatur, 64, 48, GxEPD_BLACK);
    display.drawBitmap(340, 420, epd_bitmap_boden_feuchte, 64, 48, GxEPD_BLACK);
    display.drawBitmap(410, 420, epd_bitmap_wasser_temperatur, 48, 48, GxEPD_BLACK);
    display.drawBitmap(480, 404, epd_bitmap_barometer, 80, 64, GxEPD_BLACK);
    
    DisplayGeneralInfoSection();                 // Top line of the display
    //DisplayLocalTemperatureSection(300 + 154, 100 - 81, 137, 100, TXT_TEMPERATURES_OUT, mithermometer_valid, outdoor_temp_c, true, OutdoorTmin, OutdoorTmax);
    DisplayLocalTemperatureSection(300 + 154, 100 - 81, 137, 100, TXT_TEMPERATURES_OUT, true, MqttSensors.air_temp_c, false, 0, 0);
    DisplayDisplayWindSection(108, 146, MqttSensors.wind_direction_deg, MqttSensors.wind_avg_meter_sec, 81, true, "");
    //DisplayMainWeatherSection(300, 100);          // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
    //DisplayForecastSection(217, 245);            // 3hr forecast boxes
    //DisplayAstronomySection(0, 245);             // Astronomy section Sun rise/set, Moon phase and Moon icon
    DisplayStatusSection(690, 215, wifi_signal); // Wi-Fi signal strength and Battery voltage
}

//#########################################################################################
void DisplayGeneralInfoSection(void) {
  const int dist = 40;
  uint16_t offs;
  uint16_t w;

  Serial.printf("DisplayGeneralInfoSection(): ScreenNo=%d\n", ScreenNo);
  // Print page heading
  for (int i = 0; i <= 2; i++) {   
    if (i == 0) {
      // Current menu item, centered
      u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      w = u8g2Fonts.getUTF8Width(Locations[ScreenNo].c_str());
      drawString(SCREEN_WIDTH/2 - w/2, 6, Locations[ScreenNo], LEFT);
      offs = SCREEN_WIDTH/2 + w/2 + dist;
    }
    else if (ScreenNo + i <= LAST_SCREEN) {
      // Next menu items to the right, at fixed distance 
      u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      w = u8g2Fonts.getUTF8Width(Locations[ScreenNo + i].c_str());
      drawString(offs, 6, Locations[ScreenNo + i], LEFT);
      offs = offs + w + dist;
    } 
  }
  for (int i = 0; i >= -2; i--) {   
    if (i == 0) {
      // Current menu item, centered (already printed, just calculate offset for previous item)
      u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      w = u8g2Fonts.getUTF8Width(Locations[ScreenNo].c_str());
      offs = SCREEN_WIDTH/2 - w/2 - dist;
    }
    else if (ScreenNo + i > -1) {
      // Previous menu items to the left, at fixed distance
      u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      w = u8g2Fonts.getUTF8Width(Locations[ScreenNo + i].c_str());
      offs = offs - w;
      drawString(offs, 6, Locations[ScreenNo + i], LEFT);
      offs = offs - dist;
    }
  }

  //DrawRSSI(695, 15, wifi_signal); // Wi-Fi signal strength and Battery voltage
  display.drawLine(0, 18, SCREEN_WIDTH-1, 18, GxEPD_BLACK);
}
//#########################################################################################
void DisplayDateTime(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x, y, Date_str, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 13, y+31, Time_str, CENTER);
}

//#########################################################################################
void DisplayMainWeatherSection(int x, int y) {
  //  display.drawRect(x-67, y-65, 140, 182, GxEPD_BLACK);
  display.drawLine(0, 38, SCREEN_WIDTH - 3, 38,  GxEPD_BLACK);
  DisplayConditionsSection(x + 3, y + 49, WxConditions[0].Icon, LargeIcon);
  DisplayTemperatureSection(x + 154, y - 81, 137, 100);
  DisplayPressureSection(x + 281, y - 81, WxConditions[0].Pressure, WxConditions[0].Trend, 137, 100);
  DisplayPrecipitationSection(x + 411, y - 81, 137, 100);
  DisplayForecastTextSection(x + 97, y + 20, 409, 65);
}
//#########################################################################################
void DisplayDisplayWindSection(int x, int y, float angle, float windspeed, int Cradius, bool valid, String label) {
  arrow(x, y, Cradius - 22, angle, 18, 33); // Show wind direction on outer circle of width and length
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y - Cradius - 41, label, CENTER);
  int dxo, dyo, dxi, dyi;
  //display.drawLine(0, 18, 0, y + Cradius + 37, GxEPD_BLACK);
  display.drawCircle(x, y, Cradius, GxEPD_BLACK);     // Draw compass circle
  display.drawCircle(x, y, Cradius + 1, GxEPD_BLACK); // Draw compass circle
  display.drawCircle(x, y, Cradius * 0.7, GxEPD_BLACK); // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45)  drawString(dxo + x + 12, dyo + y - 12, TXT_NE, CENTER);
    if (a == 135) drawString(dxo + x + 7,  dyo + y + 6,  TXT_SE, CENTER);
    if (a == 225) drawString(dxo + x - 18, dyo + y,      TXT_SW, CENTER);
    if (a == 315) drawString(dxo + x - 18, dyo + y - 12, TXT_NW, CENTER);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
  }
  drawString(x, y - Cradius - 12,   TXT_N, CENTER);
  drawString(x, y + Cradius + 6,    TXT_S, CENTER);
  drawString(x - Cradius - 12, y - 3, TXT_W, CENTER);
  drawString(x + Cradius + 10,  y - 3, TXT_E, CENTER);
  if (valid) {
    drawString(x - 2, y - 43, WindDegToDirection(angle), CENTER);
    drawString(x + 6, y + 30, String(angle, 0) + "°", CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    drawString(x - 12, y - 3, String(windspeed, 1), CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(x, y + 12, (Units == "M" ? "m/s" : "mph"), CENTER);
  }
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
void DisplayTemperatureSection(int x, int y, int twidth, int tdepth) {
  display.drawRect(x - 63, y - 1, twidth, tdepth, GxEPD_BLACK); // temp outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y + 5, TXT_TEMPERATURES, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 10, y + 78, String(WxConditions[0].High, 0) + "° | " + String(WxConditions[0].Low, 0) + "°", CENTER); // Show forecast high and Low
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  drawString(x - 22, y + 53, String(WxConditions[0].Temperature, 1) + "°", CENTER); // Show current Temperature
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 43, y + 53, Units == "M" ? "C" : "F", LEFT);
}
//#########################################################################################
void DisplayForecastTextSection(int x, int y , int fwidth, int fdepth) {
  display.drawRect(x - 6, y - 3, fwidth, fdepth, GxEPD_BLACK); // forecast text outline
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  // MPr:
  // Main0 ist der Originaltext (englisch)
  // Forecast0, Forecast1 und Forcast2 ist der lokalisierte Text
  //String Wx_Description = WxConditions[0].Main0;
  String Wx_Description;
  if (WxConditions[0].Forecast0 != "") Wx_Description += WxConditions[0].Forecast0;
  //if (WxConditions[0].Forecast0 != "") Wx_Description += " (" + WxConditions[0].Forecast0;
  if (WxConditions[0].Forecast1 != "") Wx_Description += ", " + WxConditions[0].Forecast1;
  if (WxConditions[0].Forecast2 != "") Wx_Description += ", " + WxConditions[0].Forecast2;
  //if (Wx_Description.indexOf("(") > 0) Wx_Description += ")";
  int MsgWidth = 43; // Using proportional fonts, so be aware of making it too wide!
  if (Language == "DE") drawStringMaxWidth(x, y + 23, MsgWidth, Wx_Description, LEFT); // Leave German text in original format, 28 character screen width at this font size
  else                  drawStringMaxWidth(x, y + 23, MsgWidth, TitleCase(Wx_Description), LEFT); // 28 character screen width at this font size
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
}
//#########################################################################################
void DisplayForecastWeather(int x, int y, int index) {
  int fwidth = 73;
  x = x + fwidth * index;
  display.drawRect(x, y, fwidth - 1, 81, GxEPD_BLACK);
  display.drawLine(x, y + 16, x + fwidth - 3, y + 16, GxEPD_BLACK);
  DisplayConditionsSection(x + fwidth / 2, y + 43, WxForecast[index].Icon, SmallIcon);
  drawString(x + fwidth / 2, y + 4, String(ConvertUnixTime(WxForecast[index].Dt + WxConditions[0].Timezone).substring(0,5)), CENTER);
  drawString(x + fwidth / 2 + 12, y + 66, String(WxForecast[index].High, 0) + "°/" + String(WxForecast[index].Low, 0) + "°", CENTER);
}
//#########################################################################################
void DisplayPressureSection(int x, int y, float pressure, String slope, int pwidth, int pdepth) {
  display.drawRect(x - 56, y - 1, pwidth, pdepth, GxEPD_BLACK); // pressure outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 8, y + 5, TXT_PRESSURE, CENTER);
  String slope_direction = TXT_PRESSURE_STEADY;
  if (slope == "+") slope_direction = TXT_PRESSURE_RISING;
  if (slope == "-") slope_direction = TXT_PRESSURE_FALLING;
  //display.drawRect(x + 40, y + 78, 41, 21, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  if (Units == "I") drawString(x - 22, y + 55, String(pressure, 2), CENTER); // "Imperial"
  else              drawString(x - 22, y + 55, String(pressure, 0), CENTER); // "Metric"
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 55, y + 53, (Units == "M" ? "hPa" : "in"), CENTER);
  drawString(x - 3, y + 78, slope_direction, CENTER);
}
//#########################################################################################
void DisplayPrecipitationSection(int x, int y, int pwidth, int pdepth) {
  display.drawRect(x - 48, y - 1, pwidth, pdepth, GxEPD_BLACK); // precipitation outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 25, y + 5, TXT_PRECIPITATION_SOON, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  if (WxForecast[1].Rainfall >= 0.005) { // Ignore small amounts
    drawString(x - 25, y + 40, String(WxForecast[1].Rainfall, 2) + (Units == "M" ? "mm" : "in"), LEFT); // Only display rainfall total today if > 0
    addraindrop(x + 58, y + 40, 7);
  }
  if (WxForecast[1].Snowfall >= 0.005)  // Ignore small amounts
    drawString(x - 25, y + 71, String(WxForecast[1].Snowfall, 2) + (Units == "M" ? "mm" : "in") + " **", LEFT); // Only display snowfall total today if > 0
  if (WxForecast[1].Pop >= 0.005)       // Ignore small amounts
    drawString(x + 2, y + 81, String(WxForecast[1].Pop*100, 0) + "%", LEFT); // Only display pop if > 0
}
//#########################################################################################
void DisplayAstronomySection(int x, int y) {
  display.drawRect(x, y + 16, 216, 65, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 4, y + 24, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNRISE, LEFT);
  drawString(x + 4, y + 44, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNSET, LEFT);
  time_t now = time(NULL);
  struct tm * now_utc  = gmtime(&now);
  const int day_utc = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc = now_utc->tm_year + 1900;
  drawString(x + 4, y + 64, MoonPhase(day_utc, month_utc, year_utc, Hemisphere), LEFT);
  DrawMoon(x + 137, y, day_utc, month_utc, year_utc, Hemisphere);
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
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
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
  c   = 365.25 * y;
  e   = 30.6  * m;
  jd  = c + e + d - 694039.09;     /* jd is total days elapsed */
  jd /= 29.53059;                        /* divide by the moon cycle (29.53 days) */
  b   = jd;                              /* int(jd) -> b, take integer part of jd */
  jd -= b;                               /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;                /* scale fraction from 0-8 and round by adding 0.5 */
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
void DisplayForecastSection(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  int f = 0;
  do {
    DisplayForecastWeather(x, y, f);
    f++;
  } while (f <= 7);
  // Pre-load temporary arrays with with data - because C parses by reference
  int r = 1;
  do {
    if (Units == "I") pressure_readings[r] = WxForecast[r].Pressure * 0.02953;   else pressure_readings[r] = WxForecast[r].Pressure;
    if (Units == "I") rain_readings[r]     = WxForecast[r].Rainfall * 0.0393701; else rain_readings[r]     = WxForecast[r].Rainfall;
    if (Units == "I") snow_readings[r]     = WxForecast[r].Snowfall * 0.0393701; else snow_readings[r]     = WxForecast[r].Snowfall;
    temperature_readings[r] = WxForecast[r].Temperature;
    humidity_readings[r]    = WxForecast[r].Humidity;
    r++;
  } while (r <= max_readings);
  int gwidth = 150, gheight = 72;
  int gx = (SCREEN_WIDTH - gwidth * 4) / 5 + 5;
  int gy = 375;
  int gap = gwidth + gx;
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(SCREEN_WIDTH / 2, gy - 40, TXT_FORECAST_VALUES, CENTER); // Based on a graph height of 60
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  // (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  DrawGraph(gx + 0 * gap, gy, gwidth, gheight, 900, 1050, Units == "M" ? TXT_PRESSURE_HPA : TXT_PRESSURE_IN, pressure_readings, max_readings, autoscale_on, barchart_off, 0, 2, 1, NULL);
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 10, 30,    Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature_readings, max_readings, autoscale_on, barchart_off, 0, 2, 1, NULL);
  DrawGraph(gx + 2 * gap, gy, gwidth, gheight, 0, 100,   TXT_HUMIDITY_PERCENT, humidity_readings, max_readings, autoscale_off, barchart_off, 0, 2, 1, NULL);
  if (SumOfPrecip(rain_readings, max_readings) >= SumOfPrecip(snow_readings, max_readings))
    DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_readings, max_readings, autoscale_on, barchart_on, 0, 2, 1, NULL);
  else DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_SNOWFALL_MM : TXT_SNOWFALL_IN, snow_readings, max_readings, autoscale_on, barchart_on, 0, 2, 1, NULL);
}
//#########################################################################################
void DisplayLocalHistory() {
  /*
  local_data.temperature = LocalSensors.ble_thsensor[0].temperature;
  local_data.humidity    = LocalSensors.ble_thsensor[0].temperature;
  local_data.pressure    = LocalSensors.i2c_thpsensor[0].pressure;
  */

  local_hist_t local_data;
  float temperature[LOCAL_HIST_SIZE];
  float humidity[LOCAL_HIST_SIZE];
  float pressure[LOCAL_HIST_SIZE];
  
  for (int i = 0; i<LOCAL_HIST_SIZE; i++) {
    temperature[i] = 0;
    humidity[i] = 0;
    pressure[i] = 0; 
  }

  for (int i=q_getCount(&LocalHistQCtrl)-1, j=LOCAL_HIST_SIZE-1; i>=0; i--, j--) {
    q_peekIdx(&LocalHistQCtrl, &local_data, i);
    temperature[j] = local_data.temperature;
    humidity[j] = local_data.humidity;
    pressure[j] = local_data.pressure;
  }

  int gwidth = 150, gheight = 72;
  int gx = (SCREEN_WIDTH - gwidth * 3) / 4 + 5;
  int gy = 375;
  int gap = gwidth + gx;
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(SCREEN_WIDTH / 2, gy - 40, TXT_LOCAL_HISTORY_VALUES, CENTER); // Based on a graph height of 60
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);

  // (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  DrawGraph(gx + 0 * gap, gy, gwidth, gheight, 900, 1050, Units == "M" ? TXT_PRESSURE_HPA : TXT_PRESSURE_IN, pressure, LOCAL_HIST_SIZE, autoscale_off, barchart_off, -2, 0, 1, NULL);
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 10, 30,    Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature, LOCAL_HIST_SIZE, autoscale_on, barchart_off, -2, 0, 1, NULL);
  DrawGraph(gx + 2 * gap, gy, gwidth, gheight, 0, 100,    TXT_HUMIDITY_PERCENT, humidity, LOCAL_HIST_SIZE, autoscale_off, barchart_off, -2, 0, 1, NULL);
}
//#########################################################################################
void DisplayMqttHistory() {
/*
  int r = 1;
  do {
    if (Units == "I") pressure_readings[r] = WxForecast[r].Pressure * 0.02953;   else pressure_readings[r] = WxForecast[r].Pressure;
    if (Units == "I") rain_readings[r]     = WxForecast[r].Rainfall * 0.0393701; else rain_readings[r]     = WxForecast[r].Rainfall;
    if (Units == "I") snow_readings[r]     = WxForecast[r].Snowfall * 0.0393701; else snow_readings[r]     = WxForecast[r].Snowfall;
    temperature_readings[r] = WxForecast[r].Temperature;
    humidity_readings[r]    = WxForecast[r].Humidity;
    r++;
  } while (r <= max_readings);
  */
  
  mqtt_hist_t mqtt_data;
  float temperature[MQTT_HIST_SIZE];
  float humidity[MQTT_HIST_SIZE];
  float rain[MQTT_HIST_SIZE];
  bool  valid[MQTT_HIST_SIZE];
  for (int i = 0; i<MQTT_HIST_SIZE; i++) {
    temperature[i] = 0;
    humidity[i]    = 0;
    rain[i]        = 0;
    valid[i]       = false;
  }
  for (int i=q_getCount(&MqttHistQCtrl)-1, j=MQTT_HIST_SIZE-1; i>=0; i--, j--) {
    q_peekIdx(&MqttHistQCtrl, &mqtt_data, i);
    temperature[j] = mqtt_data.temperature;
    humidity[j]    = mqtt_data.humidity;
    rain[j]        = mqtt_data.rain;
    valid[j]       = mqtt_data.valid;
  }

  int gwidth = 150, gheight = 72;
  int gx = (SCREEN_WIDTH - gwidth * 4) / 5 + 5;
  int gy = 375;
  int gap = gwidth + gx;
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(SCREEN_WIDTH / 2, gy - 40, TXT_MQTT_HISTORY_VALUES, CENTER); // Based on a graph height of 60
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);

  // (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 10, 30,    Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature, max_readings, autoscale_on, barchart_off, -2, 0, 1, valid);
  DrawGraph(gx + 2 * gap, gy, gwidth, gheight, 0, 100,   TXT_HUMIDITY_PERCENT, humidity, max_readings, autoscale_off, barchart_off, -2, 0, 1, valid);
  DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain, max_readings, autoscale_on, barchart_on, -2, 0, 1, valid);
}

//#########################################################################################
void DisplayConditionsSection(int x, int y, String IconName, bool IconSize) {
  Serial.println("Icon name: " + IconName);
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
  else                                              Nodata(x, y, IconSize, IconName);
  if (IconSize == LargeIcon) {
    display.drawRect(x - 86, y - 131, 173, 228, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(x, y - 125, TXT_CONDITIONS, CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(x - 25, y + 70, String(WxConditions[0].Humidity, 0) + "%", CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(x + 35, y + 70, "RH", CENTER);
    if (WxConditions[0].Visibility > 0) Visibility(x - 62, y - 87, String(WxConditions[0].Visibility) + "M");
    if (WxConditions[0].Cloudcover > 0) CloudCover(x + 35, y - 87, WxConditions[0].Cloudcover);
  }
}
//#########################################################################################
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;         float y1 = plength;
  float x2 = pwidth / 2;  float y2 = pwidth / 2;
  float x3 = -pwidth / 2; float y3 = pwidth / 2;
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
  if (WiFi.status() == WL_CONNECTED) {
    return WL_CONNECTED;
  }
  Serial.print("\r\nConnecting to: "); Serial.println(String(ssid));
  //IPAddress dns(8, 8, 8, 8); // Google DNS
  IPAddress dns(192, 168, 0, 1); // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
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
    delay(50);
  }
  if (connectionStatus == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
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
void DisplayStatusSection(int x, int y, int rssi) {
  display.drawRect(x - 35, y - 32, 145, 61, GxEPD_BLACK);
  display.drawLine(x - 35, y - 17, x - 35 + 145, y - 17, GxEPD_BLACK);
  display.drawLine(x - 35 + 146 / 2, y - 18, x - 35 + 146 / 2, y - 32, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y - 29, TXT_WIFI, CENTER);
  drawString(x + 68, y - 30, TXT_POWER, CENTER);
  DrawRSSI(x - 10, y + 6, rssi);
  DrawBattery(x + 58, y + 6);;
}

//#########################################################################################
void DrawRSSI(int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  if (WiFi.status() == WL_CONNECTED) {
    for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
      if (_rssi <= -20)  WIFIsignal = 20; //            <-20dbm displays 5-bars
      if (_rssi <= -40)  WIFIsignal = 16; //  -40dbm to  -21dbm displays 4-bars
      if (_rssi <= -60)  WIFIsignal = 12; //  -60dbm to  -41dbm displays 3-bars
      if (_rssi <= -80)  WIFIsignal = 8;  //  -80dbm to  -61dbm displays 2-bars
      if (_rssi <= -100) WIFIsignal = 4;  // -100dbm to  -81dbm displays 1-bar
      display.fillRect(x + xpos * 6, y - WIFIsignal, 5, WIFIsignal, GxEPD_BLACK);
      xpos++;
    }
    //display.fillRect(x, y - 1, 5, 1, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(x + 60, y - 10, String(rssi) + "dBm", CENTER);
  }
  else {
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(x + 20, y - 10, TXT_WIFI_OFF, CENTER);
  }
}
//#########################################################################################
boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "pool.ntp.org"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset(); // Set the TZ environment variable
  delay(100);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//#########################################################################################
boolean UpdateLocalTime() {
  struct tm timeinfo;
  char   time_output[32], day_output[30], update_time[30];
  while (!getLocalTime(&timeinfo, 10000)) { // Wait for 10-sec for time to synchronise
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");      // Displays: Saturday, June 24 2017 14:05:49
  if (Units == "M") {
    if ((Language == "CZ") || (Language == "DE") || (Language == "PL") || (Language == "NL")){
      sprintf(day_output, "%s, %02u. %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900); // day_output >> So., 23. Juni 2019 <<
    }
    else
    {
      sprintf(day_output, "%s %02u-%s-%04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    }
    strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo);  // Creates: '14:05:49'
    sprintf(time_output, "%s %s", TXT_UPDATED, update_time);
  }
  else
  {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo); // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);        // Creates: '02:05:49pm'
    sprintf(time_output, "%s %s", TXT_UPDATED, update_time);
  }
  Date_str = day_output;
  Time_str = time_output;
  return true;
}
//#########################################################################################
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.46;
  if (voltage > 1 ) { // Only display if there is a valid reading
    Serial.println("Voltage = " + String(voltage));
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.50) percentage = 0;
    display.drawRect(x + 15, y - 12, 19, 10, GxEPD_BLACK);
    display.fillRect(x + 34, y - 10, 2, 5, GxEPD_BLACK);
    display.fillRect(x + 17, y - 10, 15 * percentage / 100.0, 6, GxEPD_BLACK);
    drawString(x + 10, y - 11, String(percentage) + "%", RIGHT);
    drawString(x + 13, y + 5,  String(voltage, 2) + "v", CENTER);
  }
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);    // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK); // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  //Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);            // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);            // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE);  // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE); // Right middle upper circle
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE); // Upper and lower lines
}
//#########################################################################################
void addraindrop(int x, int y, int scale) {
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
  x = x + scale * 1.6; y = y + scale / 3;
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) scale *= 1.34;
  for (int d = 0; d < 4; d++) {
    addraindrop(x + scale * (7.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale, bool IconSize) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.1;
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
  else y = y - 3; // Shift up small sun icon
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
  int scale = Small, linesize = 3;
  if (IconSize == LargeIcon) {
    scale = Large;
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
  }
  else {
    y += 10;
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    addcloud(x + 30, y - 45, 5, linesize); // Cloud top right
    addcloud(x - 20, y - 30, 7, linesize); // Cloud top left
    addcloud(x, y, scale, linesize);       // Main cloud
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
  addcloud(x - 9, y - 3, Small * 0.5, 2); // Cloud top left
  addcloud(x + 3, y - 3, Small * 0.5, 2); // Cloud top right
  addcloud(x, y,         Small * 0.5, 2); // Main cloud
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 15, y - 5, String(CCover) + "%", LEFT);
}
//#########################################################################################
void Visibility(int x, int y, String Visi) {
  y = y - 3; //
  float start_angle = 0.52, end_angle = 2.61;
  int r = 10;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    display.drawPixel(x + r * cos(i), y - r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i), GxEPD_BLACK);
  }
  start_angle = 3.61; end_angle = 5.78;
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
    display.fillCircle(x - 62, y - 68, scale, GxEPD_BLACK);
    display.fillCircle(x - 43, y - 68, scale * 1.6, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 25, y - 15, scale, GxEPD_BLACK);
    display.fillCircle(x - 18, y - 15, scale * 1.6, GxEPD_WHITE);
  }
}
//#########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) u8g2Fonts.setFont(u8g2_font_helvB24_tf); else u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 10, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}
//#########################################################################################
/* (C) D L BIRD
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
void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int readings, boolean auto_scale, boolean barchart_mode, int xmin, int xmax, int dx, bool ValidArray[]) {
#define auto_scale_margin 0 // Sets the autoscale increment, so axis steps up in units of e.g. 3
#define y_minor_axis 5      // 5 y-axis division markers
  float maxYscale = -10000;
  float minYscale =  10000;
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
  drawString(x_pos + gwidth / 2 + 6, y_pos - 16, title, CENTER);
  // Draw the data
  for (int gx = 1; gx < readings; gx++) {
    x2 = x_pos + gx * gwidth / (readings - 1) - 1 ; // max_readings is the global variable that sets the maximum data that can be plotted
    y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
    if (!ValidArray || ValidArray[gx]) {
        if (barchart_mode) {
        display.fillRect(x2, y2, (gwidth / readings) - 1, y_pos + gheight - y2 + 2, GxEPD_BLACK);
        } else {
        display.drawLine(last_x, last_y, x2, y2, GxEPD_BLACK);
        }
        last_x = x2;
        last_y = y2;
    }
  }
  //Draw the Y-axis scale
#define number_of_dashes 20
  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
    for (int j = 0; j < number_of_dashes; j++) { // Draw dashed graph grid lines
      if (spacing < y_minor_axis) display.drawFastHLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes), GxEPD_BLACK);
    }
    if ((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing) < 5 || title == TXT_PRESSURE_IN) {
      drawString(x_pos - 1, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
    }
    else
    {
      if (Y1Min < 1 && Y1Max < 10)
        drawString(x_pos - 1, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
      else
        drawString(x_pos - 2, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0), RIGHT);
    }
  }
  //
  float xdiv = xmax - xmin + 1;
  xdiv = (xdiv < 0) ? -xdiv/dx : xdiv/dx;
  
  // FIXME adjust x-offset for other number of labels than 3
  for (int n = xmin, i=0; n <= xmax; n = n + dx, i++) {
    drawString(15 + x_pos + gwidth / xdiv * i, y_pos + gheight + 3, String(n), LEFT);
    //i++;
  }
  drawString(x_pos + gwidth / 2, y_pos + gheight + 14, TXT_DAYS, CENTER);
}

//#########################################################################################
void drawString(int x, int y, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}
//#########################################################################################
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
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
    secondLine.trim(); // Remove any leading spaces
    u8g2Fonts.println(secondLine);
  }
}
//#########################################################################################
void DisplayLocalTemperatureSection(int x, int y, int twidth, int tdepth, String label, bool valid, float tcurrent, bool minmax, float tmin, float tmax) {
  //display.drawRect(x - 63, y - 1, twidth, tdepth, GxEPD_BLACK); // temp outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  if (label != ""){
    drawString(x, y + 5, label, CENTER);
  }
  String _unit = (Units == "M") ? "°C" : "°F"; 
  if (!valid) {
    if (minmax) {
      u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      drawString(x-5, y + 70, "? " + _unit + " | ? " + _unit, CENTER); // Show forecast high and Low
    }
    //u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    //u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    u8g2Fonts.setFont(u8g2_font_helvR24_tf);
    drawString(x - 22, y + 53, "?.? " + _unit, CENTER); // Show current Temperature
    //u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    //drawString(x + 43, y + 53, Units == "M" ? "C" : "F", LEFT);
  } else {
    if (minmax) {
      u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      drawString(x-5, y + 70, String(tmax, 0) + _unit + " | " + String(tmin, 0) + _unit, CENTER); // Show forecast high and Low
    }
    //u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    //u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    u8g2Fonts.setFont(u8g2_font_helvR24_tf);
    drawString(x - 22, y + 53, String(tcurrent, 1) + _unit, CENTER); // Show current Temperature
  }

  //u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  //drawString(x + 43, y + 53, Units == "M" ? "C" : "F", LEFT);
}

//#########################################################################################
void InitialiseDisplay() {
  display.init(115200, true, 2, false); // init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode)
  // display.init(); for older Waveshare HATs
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  u8g2Fonts.begin(display);                  // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}
//#########################################################################################
/*String Translate_EN_DE(String text) {
  if (text == "clear")            return "klar";
  if (text == "sunny")            return "sonnig";
  if (text == "mist")             return "Nebel";
  if (text == "fog")              return "Nebel";
  if (text == "rain")             return "Regen";
  if (text == "shower")           return "Regenschauer";
  if (text == "cloudy")           return "wolkig";
  if (text == "clouds")           return "Wolken";
  if (text == "drizzle")          return "Nieselregen";
  if (text == "snow")             return "Schnee";
  if (text == "thunderstorm")     return "Gewitter";
  if (text == "light")            return "leichter";
  if (text == "heavy")            return "schwer";
  if (text == "mostly cloudy")    return "größtenteils bewölkt";
  if (text == "overcast clouds")  return "überwiegend bewölkt";
  if (text == "scattered clouds") return "aufgelockerte Bewölkung";
  if (text == "few clouds")       return "ein paar Wolken";
  if (text == "clear sky")        return "klarer Himmel";
  if (text == "broken clouds")    return "aufgerissene Bewölkung";
  if (text == "light rain")       return "leichter Regen";
  return text;
  }
*/
/*
  Version 16.0 reformatted to use u8g2 fonts
   1.  Added ß to translations, eventually that conversion can move to the lang_xx.h file
   2.  Spaced temperature, pressure and precipitation equally, suggest in DE use 'niederschlag' for 'Rain/Snow'
   3.  No-longer displays Rain or Snow unless there has been any.
   4.  The nn-mm 'Rain suffix' has been replaced with two rain drops
   5.  Similarly for 'Snow' two snow flakes, no words and '=Rain' and '"=Snow' for none have gone.
   6.  Improved the Cloud Cover icon and only shows if reported, 0% cloud (clear sky) is no-report and no icon.
   7.  Added a Visibility icon and reported distance in Metres. Only shows if reported.
   8.  Fixed the occasional sleep time error resulting in constant restarts, occurred when updates took longer than expected.
   9.  Improved the smaller sun icon.
   10. Added more space for the Sunrise/Sunset and moon phases when translated.

  Version 16.1 Correct timing errors after sleep - persistent problem that is not deterministic
   1.  Removed Weather (Main) category e.g. previously 'Clear (Clear sky)', now only shows area category of 'Clear sky' and then ', caterory1' and ', category2'
   2.  Improved accented character displays

  Version 16.2 Correct comestic icon issues
   1.  At night the addition of a moon icon overwrote the Visibility report, so order of drawing was changed to prevent this.
   2.  RainDrop icon was too close to the reported value of rain, moved right. Same for Snow Icon.
   3.  Improved large sun icon sun rays and improved all icon drawing logic, rain drops now use common shape.
   5.  Moved MostlyCloudy Icon down to align with the rest, same for MostlySunny.
   6.  Improved graph axis alignment.

  Version 16.3 Correct comestic icon issues
   1.  Reverted some aspects of UpdateLocalTime() as locialisation changes were unecessary and can be achieved through lang_aa.h files
   2.  Correct configuration mistakes with moon calculations.

  Version 16.4 Corrected time server addresses and adjusted maximum time-out delay
   1.  Moved time-server address to the credentials file
   2.  Increased wait time for a valid time setup to 10-secs
   3.  Added a lowercase conversion of hemisphere to allow for 'North' or 'NORTH' or 'nOrth' entries for hemisphere
   4.  Adjusted graph y-axis alignment, redcued number of x dashes

  Version 16.5 Clarified connections for Waveshare ESP32 driver board
   1.  Added SPI.end(); and SPI.begin(CLK, MISO, MOSI, CS); to enable explicit definition of pins to be used.

  Version 16.6 changed GxEPD2 initialisation from 115200 to 0
   1.  Display.init(115200); becomes display.init(0); to stop blank screen following update to GxEPD2
   
  Version 16.7 changed u8g2 fonts selection
   1.  Omitted 'FONT(' and added _tf to font names either Regular (R) or Bold (B)
  
  Version 16.8
   1. Added extra 20-secs of sleep to allow for slow ESP32 RTC timers
   
  Version 16.9
   1. Added probability of precipitation display e.g. 17%

  Version 16.10
   1. Updated display inittialisation for 7.5" T7 display type, which iss now the standard 7.5" display type.
  
  Version 16.11
   1. Adjusted graph drawing for negative numbers
   2. Correct offset error for precipitation 
 
*/
