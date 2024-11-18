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

/**
 * ESP32 Weather Display using an EPD 7.5" 800x480 Display
 *
 * Four virtual screens are provided:
 * 0. Start screen
 * 1. Current weather data and weather forecast by OpenWeatherMap
 * 2. Local weather sensor data (internal T/H/P-sensor with I2C interface, BLE sensor(s))
 * 3. Remote weather sensor data (received from MQTT broker)
 *
 * Initially the start screen is shown. After a delay, the default screen is shown.
 *
 * For each cycle:
 * - Connect to WiFi
 * - Draw screen title bar
 * - Retrieve local data (I2C bus and BLE sensors) -
 *   bleScanTime must be greater than BLE sensor's advertising cycle time;
 *   BLE scanning is aborted if all known sensors have been received or a touch sensor is triggered
 * - Draw current screen
 * - Retrieve MQTT data -
 *   wait for MQTT message until MQTT_DATA_TIMEOUT occurs or a touch sensor is triggered
 * - For MQTT screen: update if new (valid) data has been received
 * - go to deep sleep mode; wake-up is triggered after an integer multiple of SleepDuration past
 *   the full hour - minus SleepOffset (to be ready for receiving equally triggered MQTT messages)
 *
 * Switching between screens 1..3 is done by touch sensors.
 *
 * Local and remote sensor data (i.e. not OWM data) is stored in non-volatile (NV) RAM. Thus, at least
 * data received previously can be shown even in WiFi is not availably.
 *
 * NV RAM is also used to store data for plotting history graphs and for displaying min/max values
 * (during past 24 hrs).
 *
 * Based on G6EJD/ESP32-e-Paper-Weather-Display Version 16.11
 *
 * History:
 *
 * 20241008 Fixed B/W display (small artefacts remaining)
 * 20241009 Fixed B/W display of main screens
 * 20241011 Added secure MQTT
 * 20241109 Fixed several B/W display issues
 *
 */

#include "config.h"
#include "owm_credentials.h" // See 'owm_credentials' tab and enter your OWM API key and set the Wifi SSID and PASSWORD
#include <ArduinoJson.h>     // https://github.com/bblanchon/ArduinoJson needs version v6 or above

#include <time.h>                  // Built-in
#include <SPI.h>                   // Built-in
#include <string>                  // Built-in
#include <MQTT.h>                  // https://github.com/256dpi/arduino-mqtt
#include <cQueue.h>                // https://github.com/SMFSW/cQueue
#include "MqttInterface.h"         // MQTT sensor data interface
#include "LocalInterface.h"        // Local sensor data interface (BLE and I2C)
#include "RainHistory.h"           // Rainfall history data definitions
#include "WeatherSymbols.h"        // Functions for drawing weather symbols at runtime
#include "bitmap_icons.h"          // Icon bitmaps
#include "bitmap_weather_report.h" // Picture shown on ScreenStart
#include "bitmap_local.h"          // Picture shown on ScreenLocal - replace by your own
#include "bitmap_remote.h"         // Picture shown on ScreenMQTT  - replace by your own
#include "utils.h"
#include "secrets.h"

#define ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h> //!< https://github.com/ZinggJM/GxEPD2
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h> //!< https://github.com/olikraus/U8g2_for_Adafruit_GFX
#include "forecast_record.h"
// #include "src/lang.h"            // Localisation (English)
// #include "src/lang_cz.h"         // Localisation (Czech)
// #include "src/lang_fr.h"         // Localisation (French)
#include "lang_de.h" // Localisation (German)
// #include "src/lang_it.h"         // Localisation (Italian)
// #include "src/lang_nl.h"         // Localization (Dutch)
// #include "src/lang_pl.h"         // Localisation (Polish)

long SleepDuration = 30;        //!< Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour (+ SleepOffset)
long SleepOffset = -120;        //!< Offset in seconds from SleepDuration; -120 will trigger wakeup 2 minutes earlier
int WakeupTime = 7;             //!< Don't wakeup until after 07:00 to save battery power
int SleepTime = 23;             //!< Sleep after (23+1) 00:00 to save battery power (currently only used for OWM screen)
bool DebugDisplayUpdate = true; //!< If true, ignore SleepTime/WakeupTime

enum alignment
{
  LEFT,
  RIGHT,
  CENTER
};

// Connections for e.g. LOLIN D32
// static const uint8_t EPD_BUSY = 4;  // to EPD BUSY
// static const uint8_t EPD_CS   = 5;  // to EPD CS
// static const uint8_t EPD_RST  = 16; // to EPD RST
// static const uint8_t EPD_DC   = 17; // to EPD DC
// static const uint8_t EPD_SCK  = 18; // to EPD CLK
// static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
// static const uint8_t EPD_MOSI = 23; // to EPD DIN

// Connections for e.g. Waveshare ESP32 e-Paper Driver Board
static const uint8_t EPD_BUSY = 25;   //!< EPD Busy
static const uint8_t EPD_CS = 15;     //!< EPD Chip Select
static const uint8_t EPD_RST = 26;    //!< EPD Reset
static const uint8_t EPD_DC = 27;     //!< EPD Data / Command
static const uint8_t EPD_SCK = 13;    //!< EPD Serial Clock
static const uint8_t EPD_MISO = 12;   //!< EPD Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 14;   //!< EPD Master-Out Slave-In
static const uint8_t TOUCH_NEXT = 32; //!< Touch sensor right (next)
static const uint8_t TOUCH_PREV = 33; //!< Touch sensor left  (previous)
static const uint8_t TOUCH_MID = 35;  //!< Touch sensor middle

#if defined(DISPLAY_BW)
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(/*CS=*/EPD_CS, /*DC=*/EPD_DC, /*RST=*/EPD_RST, /*BUSY=*/EPD_BUSY)); // B/W display
#elif defined(DISPLAY_3C)
GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 2> display(GxEPD2_750c_Z08(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
#endif
// GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT / 4> display(GxEPD2_750c(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY)); // 3-colour displays
// GxEPD2_3C<GxEPD2_750c_Z90, GxEPD2_750c_Z90::HEIGHT / 4> display(GxEPD2_750c_Z90(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY)); // 3-colour displays
//  use GxEPD_BLACK or GxEPD_WHITE or GxEPD_RED or GxEPD_YELLOW depending on display type

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts; // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
// Using fonts:
// u8g2_font_helvB08_tf
// u8g2_font_helvB10_tf
// u8g2_font_helvB12_tf
// u8g2_font_helvB14_tf
// u8g2_font_helvB18_tf
// u8g2_font_helvB24_tf
// u8g2_font_helvR24_tf

// ################  VERSION  ###########################################
String version = "16.11"; // Programme version, see change log at end
// ################ VARIABLES ###########################################

boolean LargeIcon = true, SmallIcon = false;
#define Large 17       // For icon drawing, needs to be odd number for best effect
#define Small 6        // For icon drawing, needs to be odd number for best effect
String Time_str;       //!< Curent time as string
String Date_str;       //!< Current date as stringstrings to hold time and received weather data
extern long _timezone; //!< FIXME: Where defined? Needed?
int WiFiSignal = 0;    //!< WiFi signal strength
int CurrentHour = 0;   //!< Current time - hour
int CurrentMin = 0;    //!< Current time - minutes
int CurrentSec = 0;    //!< Current time - seconds
int CurrentDay = 0;    //!< Current date - day of month
long StartTime = 0;    //!< Start timestamp

RTC_DATA_ATTR bool touchPrevTrig = false; //!< Flag: Left   touch sensor has been triggered
RTC_DATA_ATTR bool touchNextTrig = false; //!< Flag: Right  touch sensor has been triggered
RTC_DATA_ATTR bool touchMidTrig = false;  //!< Flag: Middle touch sensor has been triggered

// ################ PROGRAM VARIABLES and OBJECTS ################

#define autoscale_on true
#define autoscale_off false
#define barchart_on true
#define barchart_off false

const String Locations[] = LOCATIONS_TXT; //!< /Screen Titles

// OWM Forecast Data
#define max_readings 24
RTC_DATA_ATTR Forecast_record_type WxConditions[1]; //!< OWM Weather Conditions
Forecast_record_type WxForecast[max_readings];      //!< OWM Weather Forecast
float pressure_readings[max_readings] = {0};        //!< OWM pressure readings
float temperature_readings[max_readings] = {0};     //!< OWM temperature readings
float humidity_readings[max_readings] = {0};        //!< OWM humidity readings
float rain_readings[max_readings] = {0};            //!< OWM rain readings
float snow_readings[max_readings] = {0};            //!< OWM snow readings

#include "common.h"

// MQTT Sensor Data
RTC_DATA_ATTR mqtt_sensors_t MqttSensors;           //!< MQTT sensor data
RTC_DATA_ATTR Queue_t MqttHistQCtrl;                //!< MQTT Sensor Data History FIFO Control
RTC_DATA_ATTR mqtt_hist_t MqttHist[MQTT_HIST_SIZE]; //<! MQTT Sensor Data History
RTC_DATA_ATTR time_t MqttHistTStamp = 0;            //!< Last MQTT History Update Timestamp

// Rainfall Data History
RTC_DATA_ATTR Queue_t RainHrHistQCtrl;                      //!< Hourly Rain Data History FIFO Control
RTC_DATA_ATTR rain_hr_hist_t RainHrHist[RAIN_HR_HIST_SIZE]; //<! Hourly Rain Data History
RTC_DATA_ATTR time_t RainHrHistTStamp = 0;                  //!< Last Hourly Rain Data History Update Timestamp

RTC_DATA_ATTR Queue_t RainDayHistQCtrl;                       //!< Daily Rain Data History FIFO Control
RTC_DATA_ATTR rain_hr_hist_t RainDayHist[RAIN_DAY_HIST_SIZE]; //<! Daily Rain Data History
RTC_DATA_ATTR uint8_t RainDayHistMDay = 0;                    //!< Last Daily Rain Data History Update, Day of Month

// Local Sensor Data
local_sensors_t LocalSensors;                          //!< Local Sensor Data
RTC_DATA_ATTR Queue_t LocalHistQCtrl;                  //!< Local Sensor Data History FIFO Control
RTC_DATA_ATTR local_hist_t LocalHist[LOCAL_HIST_SIZE]; //!< Local Sensor Data History
RTC_DATA_ATTR time_t LocalHistTStamp = 0;              //!< Last Local History Update Timestamp

RTC_DATA_ATTR int ScreenNo = START_SCREEN; //!< Current Screen No.
RTC_DATA_ATTR int PrevScreenNo = -1;       //!< Previous Screen No.

LocalInterface localInterface;

/**
 * \brief Touch Interrupt Service Routines
 */
void ARDUINO_ISR_ATTR touch_prev_isr()
{
  touchPrevTrig = true;
}

void ARDUINO_ISR_ATTR touch_next_isr()
{
  touchNextTrig = true;
}

void ARDUINO_ISR_ATTR touch_mid_isr()
{
  touchMidTrig = true;
}

/**
 * \brief Arduino setup()
 */
void setup()
{
  StartTime = millis();
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  bool wifi_ok = false;

  if ((esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) || TouchTriggered())
  {
    if (touchPrevTrig || (esp_sleep_get_ext1_wakeup_status() & (1ULL << TOUCH_PREV)))
    {
      ScreenNo = (ScreenNo == 0) ? LAST_SCREEN : ScreenNo - 1;
      touchPrevTrig = false;
    }
    if (touchNextTrig || (esp_sleep_get_ext1_wakeup_status() & (1ULL << TOUCH_NEXT)))
    {
      ScreenNo = (ScreenNo == LAST_SCREEN) ? 0 : ScreenNo + 1;
      touchNextTrig = false;
    }
    if (touchMidTrig || (esp_sleep_get_ext1_wakeup_status() & (1ULL << TOUCH_MID)))
    {
      touchMidTrig = false;
    }
  }
  log_i("Screen No: %d", ScreenNo);

  // Set GPIO pins for touch sensors to input
  pinMode(TOUCH_PREV, INPUT);
  pinMode(TOUCH_NEXT, INPUT);
  pinMode(TOUCH_MID, INPUT);

  // Attach interrupt service routines to inputs
  attachInterrupt(TOUCH_PREV, touch_prev_isr, RISING);
  attachInterrupt(TOUCH_NEXT, touch_next_isr, RISING);
  attachInterrupt(TOUCH_MID, touch_mid_isr, RISING);

  // Screen has changed
  if (ScreenNo != PrevScreenNo)
  {
    InitialiseDisplay();

    // Display start screen for 10 seconds
    if (ScreenNo == ScreenStart)
    {
#if defined(DISPLAY_3C)
      display.firstPage();
      do
      {
#endif
        DisplayStartScreen();
#if defined(DISPLAY_3C)
      } while (display.nextPage());
#endif
#if defined(DISPLAY_BW)
      display.display(false);
#endif
      delay(10000L);

      ScreenNo = ScreenOWM;
      // ScreenNo = ScreenLocal;
      // ScreenNo = ScreenMQTT;
    }

#if defined(DISPLAY_3C)
    display.firstPage();
    do
    {
#endif
      display.fillScreen(GxEPD_WHITE);
      DisplayGeneralInfoSection();
      int x = SCREEN_WIDTH / 2 - 24;
      int y = SCREEN_HEIGHT / 2 - 24;
      display.drawBitmap(x, y, epd_bitmap_hourglass_top, 48, 48, GxEPD_BLACK);
#if defined(DISPLAY_3C)
    } while (display.nextPage());
#endif
  } // if (ScreenNo != PrevScreenNo)
#if defined(DISPLAY_BW)
  display.display(true);
#endif

  wifi_ok = (StartWiFi() == WL_CONNECTED);

#if (CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
  int time_ok = SetupTime();
#else
  SetupTime();
#endif

  log_i("WiFi o.k.: %d", wifi_ok);
  log_i("Time o.k.: %d", time_ok);
  log_i("%s %s", Date_str.c_str(), Time_str.c_str());

  // Initialize history data queues if required (time has to be set already)
  if (!q_isInitialized(&MqttHistQCtrl))
  {
    q_init_static(&MqttHistQCtrl, sizeof(MqttHist[0]), MQTT_HIST_SIZE, FIFO, true, (uint8_t *)MqttHist, sizeof(MqttHist));
    MqttSensors.water_temp_c = WATER_TEMP_INVALID;
    MqttSensors.received_at[0] = '\0';
  }
  if (!q_isInitialized(&RainHrHistQCtrl))
  {
    q_init_static(&RainHrHistQCtrl, sizeof(RainHrHist[0]), RAIN_HR_HIST_SIZE, FIFO, true, (uint8_t *)RainHrHist, sizeof(RainHrHist));
  }
  if (!q_isInitialized(&RainDayHistQCtrl))
  {
    q_init_static(&RainDayHistQCtrl, sizeof(RainDayHist[0]), RAIN_DAY_HIST_SIZE, FIFO, true, (uint8_t *)RainDayHist, sizeof(RainDayHist));
    RainDayHistMDay = CurrentDay;
  }
  if (!q_isInitialized(&LocalHistQCtrl))
  {
    q_init_static(&LocalHistQCtrl, sizeof(LocalHist[0]), LOCAL_HIST_SIZE, FIFO, true, (uint8_t *)LocalHist, sizeof(LocalHist));
  }

  // Screen: Open Weather Map
  if (ScreenNo == ScreenOWM && wifi_ok)
  {
    if ((CurrentHour >= WakeupTime && CurrentHour <= SleepTime) || DebugDisplayUpdate)
    {
      byte Attempts = 1;
      bool RxWeather = false, RxForecast = false;

      while ((RxWeather == false || RxForecast == false) && Attempts <= 2)
      { // Try up-to 2 times for Weather and Forecast data
        if (RxWeather == false)
        {
          RxWeather = obtain_wx_data("weather");
        }
        if (RxForecast == false)
        {
          RxForecast = obtain_wx_data("forecast");
        }
        Attempts++;
      }
      if (RxWeather && RxForecast)
      { // Only if received both Weather or Forecast proceed
#if defined(MITHERMOMETER_EN) || defined(THEENGSDECODER_EN)
        DisplayOWMWeather(epd_bitmap_bluetooth_searching);
#else
        DisplayOWMWeather(NULL);
#endif
      }
    }
  }

  // Screen: MQTT (i.e. Remote)
  // if screen has changed: immediately update screen with saved data
  // otherwise            : skip update until new data is available
  if (ScreenNo == ScreenMQTT && ScreenNo != PrevScreenNo)
  {
#if defined(MITHERMOMETER_EN) || defined(THEENGSDECODER_EN)
    DisplayMQTTWeather(epd_bitmap_bluetooth_searching);
#else
    DisplayMQTTWeather(NULL);
#endif
  }

  // Get local sensor data (Bluetooth, I²C, SPI, ...)
  localInterface.GetLocalData();

  if (HistoryUpdateDue())
  {
    time_t now = time(NULL);
    if (now - LocalHistTStamp >= (HIST_UPDATE_RATE - HIST_UPDATE_TOL) * 60)
    {
      LocalHistTStamp = now;
      SaveLocalData();
    }
  }

  // Screen: Local Sensors
  // (already got Bluetooth data)
  if (ScreenNo == ScreenLocal)
  {
#if defined(MITHERMOMETER_EN) || defined(THEENGSDECODER_EN)
    DisplayLocalWeather(NULL);
#endif
  }
  // display.display(true);

  // WiFi is disconnected - display WiFi-Off-Icon
  if (!wifi_ok)
  {
    // Screen has changed to OWM (no previous data available in this case)
    // display Wifi-Off icon at the screen's center
    if (ScreenNo == ScreenOWM && ScreenNo != PrevScreenNo)
    {
#if defined(DISPLAY_3C)
      display.firstPage();
      do
      {
#endif
        display.fillScreen(GxEPD_WHITE);
        DisplayGeneralInfoSection();
        int x = SCREEN_WIDTH / 2 - 24;
        int y = SCREEN_HEIGHT / 2 - 24;
        display.drawBitmap(x, y, epd_bitmap_wifi_off, 48, 48, GxEPD_BLACK);
#if defined(DISPLAY_3C)
      } while (display.nextPage());
#endif
#if defined(DISPLAY_BW)
      display.display(true);
#endif
    }
    else
    {
      display.setFullWindow();
      switch (ScreenNo)
      {
      case ScreenOWM:
        DisplayOWMWeather(epd_bitmap_wifi_off);
        break;

      case ScreenLocal:
        DisplayLocalWeather(epd_bitmap_wifi_off);
        break;

      case ScreenMQTT:
        DisplayMQTTWeather(epd_bitmap_wifi_off);
      }
    } // previous data is available
  } // if (!wifi_ok)

  // Fetch MQTT data
  MQTTClient MqttClient(MQTT_PAYLOAD_SIZE);
  MqttInterface mqttInterface(MqttClient);
  if (mqttInterface.mqttConnect())
  {
    // Show download icon
    int x = 88;
    int y = (ScreenNo == ScreenOWM) ? 300 : 272;
    if (display.epd2.hasFastPartialUpdate)
    {
      //display.setPartialWindow(x, y, 48, 48);
#if defined(DISPLAY_3C)
      display.firstPage();
      do
      {
#endif
        display.fillRect(x, y, 48, 48, GxEPD_WHITE);
        display.drawBitmap(x, y, epd_bitmap_downloading, 48, 48, GxEPD_BLACK);
#if defined(DISPLAY_3C)
      } while (display.nextPage());
#endif
#if defined(DISPLAY_BW)
      display.displayWindow(x, y, 48, 48);
#endif
    }
    else
    {
      display.setFullWindow();
      switch (ScreenNo)
      {
      case ScreenOWM:
        DisplayOWMWeather(epd_bitmap_downloading);
        break;

      case ScreenLocal:
        DisplayLocalWeather(epd_bitmap_downloading);
        break;

      case ScreenMQTT:
        DisplayMQTTWeather(epd_bitmap_downloading);
      }
    } // no fast partial update

    // Get MQTT data (blocks until data is available)
    mqttInterface.getMqttData(MqttSensors);

    if (display.epd2.hasFastPartialUpdate && (ScreenNo != ScreenMQTT))
    {
      // Clear the download icon
      //display.setPartialWindow(x, y, 48, 48);
#if defined(DISPLAY_3C)
      display.firstPage();
      do
      {
#endif
        display.fillRect(x, y, 48, 48, GxEPD_WHITE);
#if defined(DISPLAY_3C)
      } while (display.nextPage());
#endif
#if defined(DISPLAY_BW)
      display.displayWindow(x, y, 48, 48);
#endif
    }
    else
    {
      // Update screen (with download icon cleared)
      display.setFullWindow();
      switch (ScreenNo)
      {
      case ScreenOWM:
        DisplayOWMWeather(NULL);
        break;

      case ScreenLocal:
        DisplayLocalWeather(NULL);
        break;

      case ScreenMQTT:
        DisplayMQTTWeather(NULL);
      }
    } // no fast partial update
  } // if MQTT connected

  time_t t_now = time(NULL);
  log_i("Time since last MqttHist update: %ld min", (t_now - MqttHistTStamp) / 60);
  log_i("                        minimum: %ld min", HIST_UPDATE_RATE - HIST_UPDATE_TOL);
  if (t_now - MqttHistTStamp >= (HIST_UPDATE_RATE - HIST_UPDATE_TOL) * 60)
  {
    MqttHistTStamp = t_now;
    SaveMqttData();
  }
  if (t_now - RainHrHistTStamp >= (60 - HIST_UPDATE_TOL) * 60)
  {
    RainHrHistTStamp = t_now;
    SaveRainHrData();
  }
  SaveRainDayData();
  
  if (!TouchTriggered()) {
    mqttInterface.mqttUplink(MqttClient, LocalSensors);
  }

  StopWiFi();
  BeginSleep();
}

/**
 * \brief Arduino loop()
 *
 * This will never run!
 */
void loop()
{
}

/**
 * \brief Prepare deep sleep mode
 *
 * The sleep time is adjusted dynamically to synchronize wake up to fixed integer multiple of
 * <code>SleepDuration</code> past full hour.
 */
void BeginSleep()
{
  display.powerOff();
  long SleepTimer;

  if (TouchTriggered())
  {
    // wake up immediately
    SleepTimer = 1000;
  }
  else
  {
    UpdateLocalTime();

    // Wake up at fixed interval, synchronized to begin of full hour
    SleepTimer = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec));
    // MPr: ???
    // Some ESP32 are too fast to maintain accurate time
    // SleepTimer = (SleepTimer + 20) * 1000000LL; // Added extra 20-secs of sleep to allow for slow ESP32 RTC timers
    SleepTimer = SleepTimer + SleepOffset;
    // Set minimum sleep time (20s)
    if (SleepTimer < 20)
    {
      SleepTimer = 20;
    }
    SleepTimer = SleepTimer * 1000000LL;
  }

  // Wake up from timer
  esp_sleep_enable_timer_wakeup(SleepTimer);

  // Wake up from touch sensors
  esp_sleep_enable_ext1_wakeup(1ULL << TOUCH_NEXT | 1ULL << TOUCH_PREV | 1ULL << TOUCH_MID,
                               ESP_EXT1_WAKEUP_ANY_HIGH);

#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif

  log_i("Awake for %.3f secs", (millis() - StartTime) / 1000.0);
  log_i("Entering %lld secs of sleep time", SleepTimer / 1000000LL);
  log_i("Starting deep-sleep period...");
  esp_deep_sleep_start(); // Sleep for e.g. 30 minutes
}

inline bool TouchTriggered(void)
{
  return (touchPrevTrig || touchMidTrig || touchNextTrig);
}

/**
 * \brief Display Open Weather Map (OWM) data
 *
 * \param bt      show "bluetooth searching" bitmap
 * \param dl      show "downloading" bitmap
 * \param nowifi  show "no wifi" bitmap
 */
void DisplayOWMWeather(const unsigned char *status_bitmap)
{
  // do {
  //   display.fillScreen(GxEPD_WHITE);
  // } while (display.nextPage());
  // display.display(true);
#if defined(DISPLAY_3C)
  display.firstPage();
  do
  {
#endif
    display.fillScreen(GxEPD_WHITE);
#if defined(DISPLAY_BW)
    display.display();
#endif
    DisplayGeneralInfoSection();
    DrawRSSI(705, 15, WiFiSignal);
    DisplayDisplayWindSection(108, 146, WxConditions[0].Winddir, WxConditions[0].Windspeed, 81, true, TXT_WIND_SPEED_DIRECTION);
    DisplayMainWeatherSection(300, 100); // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
    display.drawLine(0, 38, SCREEN_WIDTH - 3, 38, GxEPD_BLACK);
    DisplayDateTime(90, 255);
    DisplayAstronomySection(391, 165); // Astronomy section Sun rise/set, Moon phase and Moon icon
    DisplayOWMAttribution(581, 165);
    DisplayForecastSection(217, 245); // 3hr forecast boxes and 3d forecast graphs
    int x = 88;
    int y = 300;
    if (status_bitmap)
    {
      display.drawBitmap(x, y, status_bitmap, 48, 48, GxEPD_BLACK);
    }

#if defined(DISPLAY_3C)
  } while (display.nextPage());
#endif
#if defined(DISPLAY_BW)
  display.display(true);
#endif
}

/**
 * \brief Find min/max temperature in MQTT history data
 *
 * \param t_min   minimum temperature return value
 * \param t_max   maximum temperature return value
 */
void findMqttMinMaxTemp(float *t_min, float *t_max)
{
  mqtt_hist_t mqtt_data;
  float outdoorTMin = 0;
  float outdoorTMax = 0;

  // Go back in FIFO at most 24 hrs
  int maxIdx = 24 * 60 / SleepDuration;
  if (q_getCount(&MqttHistQCtrl) < maxIdx)
  {
    maxIdx = q_getCount(&MqttHistQCtrl);
  }

  // Initialize min/max with last valid entry
  for (int i = 1; i < maxIdx; i++)
  {
    if (q_peekIdx(&MqttHistQCtrl, &mqtt_data, i) && mqtt_data.valid)
    {
      outdoorTMin = mqtt_data.temperature;
      outdoorTMax = mqtt_data.temperature;
      break;
    }
  }

  // Peek into FIFO and get min/max
  for (int i = 1; i < maxIdx; i++)
  {
    if (q_peekIdx(&MqttHistQCtrl, &mqtt_data, i))
    {
      if (mqtt_data.valid)
      {
        if (mqtt_data.temperature < outdoorTMin)
        {
          outdoorTMin = mqtt_data.temperature;
        }
        if (mqtt_data.temperature > outdoorTMax)
        {
          outdoorTMax = mqtt_data.temperature;
        }
      }
    }
  }
  *t_min = outdoorTMin;
  *t_max = outdoorTMax;
}

/**
 * \brief Save MQTT data to history FIFO
 */
void SaveMqttData(void)
{
  mqtt_hist_t mqtt_data = {0, 0, 0};

  mqtt_data.temperature = MqttSensors.air_temp_c;
  mqtt_data.humidity = MqttSensors.humidity;
  mqtt_data.valid = MqttSensors.valid && MqttSensors.status.ws_dec_ok;

  log_i("MQTT data updated: %d / Weather sensor data valid: %d", MqttSensors.valid ? 1 : 0, MqttSensors.status.ws_dec_ok ? 1 : 0);
  log_i("#%d val: %d T: %.1f H: %d", q_getCount(&MqttHistQCtrl), mqtt_data.valid ? 1 : 0, MqttSensors.air_temp_c, MqttSensors.humidity);

  q_push(&MqttHistQCtrl, &mqtt_data);
}

/**
 * \brief Save Hourly Rain data to history FIFO
 */
void SaveRainHrData(void)
{
  rain_hr_hist_t rain_hr_data;

  rain_hr_data.rain = MqttSensors.rain_hr;
  rain_hr_data.valid = MqttSensors.valid && MqttSensors.rain_hr_valid;
  log_i("#%d val: %d, R: %.1f", q_getCount(&RainHrHistQCtrl), rain_hr_data.valid ? 1 : 0, MqttSensors.rain_hr);

  q_push(&RainHrHistQCtrl, &rain_hr_data);
}

/**
 * \brief Save Daily Rain data to history FIFO
 */
void SaveRainDayData(void)
{
  rain_day_hist_t rain_day_data;

  if (RainDayHistMDay != CurrentDay)
  {
    // Day has changed - store last valid accumulated daily value into FIFO
    rain_day_data.rain = MqttSensors.rain_day_prev;
    rain_day_data.valid = true;
    log_i("#%d R: %.1f", q_getCount(&RainDayHistQCtrl), rain_day_data.rain);
    q_push(&RainDayHistQCtrl, &rain_day_data);
  }

  if (MqttSensors.valid && MqttSensors.rain_day_valid)
  {
    // Save last valid accumulated daily value
    MqttSensors.rain_day_prev = MqttSensors.rain_day;
  }
  log_i("val: %d, R: %.1f", (MqttSensors.valid && MqttSensors.rain_day_valid) ? 1 : 0, MqttSensors.rain_day);

  // Save current day of month for detection of new day
  RainDayHistMDay = CurrentDay;
}

/**
 * \brief Display MQTT data
 *
 * \param bt      show "bluetooth searching" bitmap
 * \param dl      show "downloading" bitmap
 * \param nowifi  show "no wifi" bitmap
 */
void DisplayMQTTWeather(const unsigned char *status_bitmap)
{
#if defined(DIPLAY_3C)
  display.firstPage();
  do
  {
#endif
    display.fillScreen(GxEPD_WHITE);
#if defined(DISPLAY_BW)
    display.display();
#endif
    DisplayGeneralInfoSection();
    DisplayMQTTDateTime(90, 225);
    display.drawBitmap(5, 25, epd_bitmap_remote, 220, 165, GxEPD_BLACK);
    display.drawRect(4, 24, 222, 167, GxEPD_BLACK);
    display.drawBitmap(240, 45, epd_bitmap_temperatur_aussen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(438, 45, epd_bitmap_feuchte_aussen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(240, 100, epd_bitmap_temperatur_innen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(438, 100, epd_bitmap_feuchte_innen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(240, 155, epd_bitmap_boden_temperatur, 64, 48, GxEPD_BLACK);
    display.drawBitmap(438, 155, epd_bitmap_boden_feuchte, 64, 48, GxEPD_BLACK);
#if defined(WATERTEMP_EN)
    display.drawBitmap(240, 210, epd_bitmap_wasser_temperatur, 48, 48, GxEPD_BLACK);
#endif

// Weather Sensor
#ifdef FORCE_NO_SIGNAL
    MqttSensors.status.ws_dec_ok = false;
#endif

    // Find min/max temperature in local history data
    float outdoorTMin = 0;
    float outdoorTMax = 0;
    findMqttMinMaxTemp(&outdoorTMin, &outdoorTMax);

    if (MqttSensors.status.ws_dec_ok)
    {
      DisplayLocalTemperatureSection(358, 22, 137, 100, "", true, MqttSensors.air_temp_c, true, outdoorTMin, outdoorTMax);
      u8g2Fonts.setFont(u8g2_font_helvR24_tf);
      drawString(524, 75, String(MqttSensors.humidity) + "%", CENTER);

#ifdef FORCE_LOW_BATTERY
      MqttSensors.status.ws_batt_ok = false;
#endif
      if (!MqttSensors.status.ws_batt_ok)
      {
        display.drawBitmap(577, 60, epd_bitmap_battery_alert, 24, 24, GxEPD_BLACK);
      }
    }
    else
    {
      // No outdoor temperature
      display.drawBitmap(320, 47, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
      // No outdoor humidity
      display.drawBitmap(518, 47, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
    }

    // Indoor Sensor (BLE)
    if (MqttSensors.status.ble_ok)
    {
      DisplayLocalTemperatureSection(358, 77, 137, 100, "", true, MqttSensors.indoor_temp_c, false, 0, 0);
      u8g2Fonts.setFont(u8g2_font_helvR24_tf);
      drawString(524, 130, String(MqttSensors.indoor_humidity) + "%", CENTER);

#ifdef FORCE_LOW_BATTERY
      MqttSensors.status.ble_batt_ok = false;
#endif
      if (!MqttSensors.status.ble_batt_ok)
      {
        display.drawBitmap(577, 115, epd_bitmap_battery_alert, 24, 24, GxEPD_BLACK);
      }
    }
    else
    {
      // No indoor temperature
      display.drawBitmap(320, 102, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
      // No indoor humidity
      display.drawBitmap(518, 102, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
    }

    // Soil Sensor
#ifdef FORCE_NO_SIGNAL
    MqttSensors.status.s1_dec_ok = false;
#endif
    if (MqttSensors.status.s1_dec_ok)
    {
      DisplayLocalTemperatureSection(358, 132, 137, 100, "", true, MqttSensors.soil_temp_c, false, 0, 0);
      u8g2Fonts.setFont(u8g2_font_helvR24_tf);
      drawString(524, 185, String(MqttSensors.soil_moisture) + "%", CENTER);
#ifdef FORCE_LOW_BATTERY
      MqttSensors.status.s1_batt_ok = false;
#endif
      if (!MqttSensors.status.s1_batt_ok)
      {
        display.drawBitmap(580, 167, epd_bitmap_battery_alert, 24, 24, GxEPD_BLACK);
      }
    }
    else
    {
      // No soil temperature
      display.drawBitmap(320, 157, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
      // No soil moisture
      display.drawBitmap(518, 157, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
    }

#if defined(WATERTEMP_EN)
    // Water Temperature Sensor
    if ((MqttSensors.water_temp_c != WATER_TEMP_INVALID) && (MqttSensors.water_temp_c < INV_TEMP - 1))
    {
      DisplayLocalTemperatureSection(358, 187, 137, 100, "", true, MqttSensors.water_temp_c, false, 0, 0);
      u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    }
    else
    {
      // No water temperature
      display.drawBitmap(320, 217, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
    }
#endif

#ifdef FORCE_NO_SIGNAL
    MqttSensors.status.ws_dec_ok = false;
#endif
    DisplayDisplayWindSection(700, 120, MqttSensors.wind_direction_deg, MqttSensors.wind_avg_meter_sec, 81, MqttSensors.status.ws_dec_ok, "");
    if (!MqttSensors.status.ws_dec_ok)
    {
      display.drawBitmap(680, 100, epd_bitmap_signal_disconnected, 40, 40, GxEPD_BLACK);
    }
    DisplayMqttHistory();
    DrawRSSI(705, 15, WiFiSignal); // Wi-Fi signal strength

    const int x = 88;
    const int y = 272;
    if (status_bitmap)
    {
      display.drawBitmap(x, y, status_bitmap, 48, 48, GxEPD_BLACK);
    }
#if defined(DISPLAY_3C)
  } while (display.nextPage());
#endif
#if defined(DISPLAY_BW)
  display.display(true);
#endif
}

/**
 * \brief Save local sensor data to history FIFO
 */
void SaveLocalData(void)
{
  local_hist_t local_data = {0, 0, 0, 0, 0};

  local_data.th_valid = LocalSensors.ble_thsensor[0].valid;
  local_data.temperature = LocalSensors.ble_thsensor[0].temperature;
  local_data.humidity = LocalSensors.ble_thsensor[0].humidity;

  local_data.p_valid = LocalSensors.i2c_thpsensor[0].valid;
  local_data.pressure = LocalSensors.i2c_thpsensor[0].pressure;
  log_i("#%d th_val: %d, T: %.1f H: %.1f p_val: %d P: %.1f", q_getCount(&LocalHistQCtrl),
        local_data.th_valid ? 1 : 0,
        LocalSensors.ble_thsensor[0].temperature,
        LocalSensors.ble_thsensor[0].humidity,
        local_data.p_valid ? 1 : 0,
        LocalSensors.i2c_thpsensor[0].pressure);
  q_push(&LocalHistQCtrl, &local_data);
}

/**
 * \brief Find min/max temperature in local history data
 *
 * \param t_min   minimum temperature return value
 * \param t_max   maximum temperature return value
 */
void findLocalMinMaxTemp(float *t_min, float *t_max)
{
  // Find min/max temperature in local history data
  local_hist_t local_data;
  float outdoorTMin = 0;
  float outdoorTMax = 0;

  // Initialize min/max with last entry
  if (q_peekPrevious(&LocalHistQCtrl, &local_data))
  {
    outdoorTMin = local_data.temperature;
    outdoorTMax = local_data.temperature;
  }

  // Go back in FIFO at most 24 hrs
  int maxIdx = 24 * 60 / SleepDuration;
  if (q_getCount(&LocalHistQCtrl) < maxIdx)
  {
    maxIdx = q_getCount(&LocalHistQCtrl);
  }

  // Peek into FIFO and get min/max
  for (int i = 1; i < maxIdx; i++)
  {
    if (q_peekIdx(&LocalHistQCtrl, &local_data, i))
    {
      if (local_data.temperature < outdoorTMin)
      {
        outdoorTMin = local_data.temperature;
      }
      if (local_data.temperature > outdoorTMax)
      {
        outdoorTMax = local_data.temperature;
      }
    }
  }
  *t_min = outdoorTMin;
  *t_max = outdoorTMax;
}

/**
 * \brief Display local sensor data
 *
 * \param bt      show "bluetooth searching" bitmap
 * \param dl      show "downloading" bitmap
 * \param nowifi  show "no wifi" bitmap
 */
void DisplayLocalWeather(const unsigned char *status_bitmap)
{
#if defined(DISPLAY_3C)
  display.firstPage();
  do
  {
#endif
    display.fillScreen(GxEPD_WHITE);
#if defined(DISPLAY_BW)
    display.display();
#endif
    DisplayGeneralInfoSection();
    DisplayDateTime(90, 225);
#ifdef SCD4X_EN
    const int y1 = 125;
#else
  const int y1 = 178;
#endif
    const int y2 = 210;
    display.drawBitmap(5, 25, epd_bitmap_local, 220, 165, GxEPD_BLACK);
    display.drawRect(4, 24, 222, 167, GxEPD_BLACK);
    display.drawBitmap(240, 45, epd_bitmap_temperatur_aussen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(438, 45, epd_bitmap_feuchte_aussen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(240, y1, epd_bitmap_temperatur_innen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(438, y1, epd_bitmap_feuchte_innen, 64, 48, GxEPD_BLACK);
    display.drawBitmap(660, 20, epd_bitmap_barometer, 80, 64, GxEPD_BLACK);
#ifdef SCD4X_EN
    display.drawBitmap(240, y2, epd_bitmap_co2_innen, 48, 48, GxEPD_BLACK);
#endif

    // Find min/max temperature in local history data
    float outdoorTMin = 0;
    float outdoorTMax = 0;
    findLocalMinMaxTemp(&outdoorTMin, &outdoorTMax);

    // Outdoor sensor
#ifdef FORCE_NO_SIGNAL
    LocalSensors.ble_thsensor[0].valid = false;
#endif
    if (LocalSensors.ble_thsensor[0].valid)
    {
      DisplayLocalTemperatureSection(358, 22, 137, 100, "", true, LocalSensors.ble_thsensor[0].temperature, true, outdoorTMin, outdoorTMax);
      // u8g2Fonts.setFont(u8g2_font_helvB24_tf);
      // u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      u8g2Fonts.setFont(u8g2_font_helvR24_tf);
      drawString(524, 75, String(LocalSensors.ble_thsensor[0].humidity, 0) + "%", CENTER);
#ifdef FORCE_LOW_BATTERY
      LocalSensors.ble_thsensor[0].batt_level = MITHERMOMETER_BATTALERT;
#endif
      if (LocalSensors.ble_thsensor[0].batt_level <= MITHERMOMETER_BATTALERT)
      {
        display.drawBitmap(577, 60, epd_bitmap_battery_alert, 24, 24, GxEPD_BLACK);
      }
    }
    else
    {
      // No outdoor temperature
      display.drawBitmap(320, 47, epd_bitmap_bluetooth_disabled, 40, 40, GxEPD_BLACK);
      // No outdor humidity
      display.drawBitmap(528, 47, epd_bitmap_bluetooth_disabled, 40, 40, GxEPD_BLACK);
    }

    // Indoor sensor(s)
    // Temperature, humidity, pressure
    if (LocalSensors.i2c_thpsensor[0].valid)
    {
      DisplayLocalTemperatureSection(358, y1 - 22, 137, 100, "", true, LocalSensors.i2c_thpsensor[0].temperature, false, 0, 0);
      // u8g2Fonts.setFont(u8g2_font_helvB24_tf);
      // u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      u8g2Fonts.setFont(u8g2_font_helvR24_tf);
      drawString(524, y1 + 30, String(LocalSensors.i2c_thpsensor[0].humidity, 0) + "%", CENTER);
      drawString(660, 110, String(LocalSensors.i2c_thpsensor[0].pressure, 0) + "hPa", CENTER);
    }
    else
    {
      // No indoor temperature
      display.drawBitmap(315, y1 + 2, epd_bitmap_bolt, 40, 40, GxEPD_BLACK);
      // No indoor humidity
      display.drawBitmap(523, y1 + 2, epd_bitmap_bolt, 40, 40, GxEPD_BLACK);
      // No pressure
      display.drawBitmap(670, 90, epd_bitmap_bolt, 40, 40, GxEPD_BLACK);
    }

#if defined(SCD4X_EN)
    // CO2
    if (LocalSensors.i2c_co2sensor.valid)
    {
      u8g2Fonts.setFont(u8g2_font_helvR24_tf);
      drawString(323, y2 + 30, String(LocalSensors.i2c_co2sensor.co2) + "ppm", CENTER);
    }
    else
    {
      // No CO2
      display.drawBitmap(315, y2 + 2, epd_bitmap_bolt, 40, 40, GxEPD_BLACK);
    }
#endif

    DisplayLocalHistory();
    DrawRSSI(705, 15, WiFiSignal); // Wi-Fi signal strength

    const int x = 88;
    const int y = 272;
    if (status_bitmap)
    {
      display.drawBitmap(x, y, status_bitmap, 48, 48, GxEPD_BLACK);
    }
#if defined(DISPLAY_3C)
  } while (display.nextPage());
#endif
#if defined(DISPLAY_BW)
  display.display(true);
#endif
}

/**
 * \brief Display start screen
 *
 */
void DisplayStartScreen(void)
{
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  // u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  // u8g2Fonts.setFont(u8g2_font_helvR24_tf);
  drawString(240, 40, TXT_START, LEFT);

  display.drawBitmap(75, 50, epd_bitmap_weather_report, 650, 300, GxEPD_BLACK);
  display.drawBitmap(10, 380, epd_bitmap_OpenWeather_Logo, 228, 103, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  int x = 110;
  int y = 370;
  drawString(x + 8, y + 24, "Weather data provided by OpenWeather", LEFT);
  drawString(x + 8, y + 44, "https://openweathermap.org/", LEFT);
  x = 610;
  y = 420;
  drawString(x + 8, y + 24, "Material Design icons by Google -", LEFT);
  drawString(x + 8, y + 44, "Apache License 2.0", LEFT);
}

/**
 * \brief Display general info
 *
 * This shows the screen names at the top of the display. The name of the current screen
 * is centered and printed in a larger font.
 */
void DisplayGeneralInfoSection(void)
{
  const int dist = 40;
  uint16_t offs = 0;
  uint16_t w = 0;

  // Print page heading
  int i_max = sizeof(Locations) / sizeof(Locations[0]) - 2;
  for (int i = 0; i <= i_max; i++)
  {
    if (i == 0)
    {
      // Current menu item, centered
      u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      w = u8g2Fonts.getUTF8Width(Locations[ScreenNo].c_str());
      drawString(SCREEN_WIDTH / 2 - w / 2, 6, Locations[ScreenNo], LEFT);
      offs = SCREEN_WIDTH / 2 + w / 2 + dist;
    }
    else if (ScreenNo + i <= LAST_SCREEN)
    {
      // Next menu items to the right, at fixed distance
      u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      w = u8g2Fonts.getUTF8Width(Locations[ScreenNo + i].c_str());
      drawString(offs, 6, Locations[ScreenNo + i], LEFT);
      offs = offs + w + dist;
    }
  }
  for (int i = 0; i >= -2; i--)
  {
    if (i == 0)
    {
      // Current menu item, centered (already printed, just calculate offset for previous item)
      u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      w = u8g2Fonts.getUTF8Width(Locations[ScreenNo].c_str());
      offs = SCREEN_WIDTH / 2 - w / 2 - dist;
    }
    else if (ScreenNo + i > -1)
    {
      // Previous menu items to the left, at fixed distance
      u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      w = u8g2Fonts.getUTF8Width(Locations[ScreenNo + i].c_str());
      offs = offs - w;
      drawString(offs, 6, Locations[ScreenNo + i], LEFT);
      offs = offs - dist;
    }
  }

  // FIXME
  display.drawLine(0, 18, SCREEN_WIDTH - 1, 18, GxEPD_BLACK);
  display.drawLine(0, 19, SCREEN_WIDTH - 1, 19, GxEPD_BLACK);
}

/**
 * \brief Print date and time as localized string at the given position
 *
 * \param x   x-coordinate
 * \param y   y-coordinate
 */
void DisplayDateTime(int x, int y)
{
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x, y, Date_str, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 13, y + 31, Time_str, CENTER);
}

/**
 * \brief Print date and time as localized string at the given position
 *
 * \param x   x-coordinate
 * \param y   y-coordinate
 */
void DisplayMQTTDateTime(int x, int y)
{
  struct tm timeinfo;
  char mqtt_date[32];
  char mqtt_time[32];

  if (MqttSensors.received_at[0] == '\0')
    return;

  convertUtcTimestamp(MqttSensors.received_at, &timeinfo, _timezone);
  printTime(timeinfo, mqtt_date, mqtt_time, 32, TXT_UPDATED);

  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x, y, mqtt_date, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 13, y + 31, mqtt_time, CENTER);
}

/**
 * \brief Prints the OWM screen's main weather section at the given position
 *
 * \param x   x-coordinate
 * \param y   y-coordinate
 */
void DisplayMainWeatherSection(int x, int y)
{
  DisplayConditionsSection(x + 3, y + 49, WxConditions[0].Icon, LargeIcon);
  DisplayTemperatureSection(x + 154, y - 81, 137, 100);
  DisplayPressureSection(x + 281, y - 81, WxConditions[0].Pressure, WxConditions[0].Trend, 137, 100);
  DisplayPrecipitationSection(x + 411, y - 81, 137, 100);
  DisplayForecastTextSection(x + 97, y + 20, 409, 65);
}

/**
 * \brief Prints the wind section at the given position
 *
 * The wind section consists of the wind rose with direction and speed.
 *
 * \param x           x-coordinate
 * \param y           y-coordinate
 * \param angle       wind direction (angle in degrees)
 * \param windspeed   wind speed (m/s or mph)
 * \param Cradius     circle radius
 * \param valid       directions/speed are printed only if valid is true
 * \param label       widget label
 */
void DisplayDisplayWindSection(int x, int y, float angle, float windspeed, int Cradius, bool valid, String label)
{
  arrow(x, y, Cradius - 22, angle, 18, 33); // Show wind direction on outer circle of width and length
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y - Cradius - 41, label, CENTER);
  int dxo, dyo, dxi, dyi;
  // display.drawLine(0, 18, 0, y + Cradius + 37, GxEPD_BLACK);
  display.drawCircle(x, y, Cradius, GxEPD_BLACK);       // Draw compass circle
  display.drawCircle(x, y, Cradius + 1, GxEPD_BLACK);   // Draw compass circle
  display.drawCircle(x, y, Cradius * 0.7, GxEPD_BLACK); // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5)
  {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45)
      drawString(dxo + x + 12, dyo + y - 12, TXT_NE, CENTER);
    if (a == 135)
      drawString(dxo + x + 7, dyo + y + 6, TXT_SE, CENTER);
    if (a == 225)
      drawString(dxo + x - 18, dyo + y, TXT_SW, CENTER);
    if (a == 315)
      drawString(dxo + x - 18, dyo + y - 12, TXT_NW, CENTER);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
  }
  drawString(x, y - Cradius - 12, TXT_N, CENTER);
  drawString(x, y + Cradius + 6, TXT_S, CENTER);
  drawString(x - Cradius - 12, y - 3, TXT_W, CENTER);
  drawString(x + Cradius + 10, y - 3, TXT_E, CENTER);
  if (valid)
  {
    drawString(x - 2, y - 43, WindDegToDirection(angle), CENTER);
    drawString(x + 6, y + 30, String(angle, 0) + "°", CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    drawString(x - 12, y - 3, String(windspeed, 1), CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(x, y + 12, (Units == "M" ? "m/s" : "mph"), CENTER);
  }
}

/**
 * \brief Converts wind direction in degrees to cardinal direction as localized string
 *
 * \param winddirection   wind direction in degrees
 *
 * \return cardinal direction (localized, 1..3 letters)
 */
String WindDegToDirection(float winddirection)
{
  int dir = int((winddirection / 22.5) + 0.5);
  String Ord_direction[16] = {TXT_N, TXT_NNE, TXT_NE, TXT_ENE, TXT_E, TXT_ESE, TXT_SE, TXT_SSE, TXT_S, TXT_SSW, TXT_SW, TXT_WSW, TXT_W, TXT_WNW, TXT_NW, TXT_NNW};
  return Ord_direction[(dir % 16)];
}

/**
 * \brief Prints the OWM forecast's current temperature section at the given position
 *
 * The section consists of the current temperature and the min./max, temperatures
 * in °C or °F.
 *
 * \param x       x-coordinate
 * \param y       y-coordinate
 * \param twidth  section's width
 * \param tdepth  section's depth
 */
void DisplayTemperatureSection(int x, int y, int twidth, int tdepth)
{
  display.drawRect(x - 63, y - 1, twidth, tdepth, GxEPD_BLACK); // temp outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y + 5, TXT_TEMPERATURES, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 10, y + 78, String(WxConditions[0].Low, 0) + "° | " + String(WxConditions[0].High, 0) + "°", CENTER); // Show forecast low and high
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  drawString(x - 22, y + 53, String(WxConditions[0].Temperature, 1) + "°", CENTER); // Show current Temperature
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 43, y + 53, Units == "M" ? "C" : "F", LEFT);
}

/**
 * \brief Prints the OWM forecast's text
 *
 * The section prints the textual description of the current weather conditions
 *
 * \param x       x-coordinate
 * \param y       y-coordinate
 * \param fwidth  section's width
 * \param fdepth  section's depth
 */
void DisplayForecastTextSection(int x, int y, int fwidth, int fdepth)
{
  display.drawRect(x - 6, y - 3, fwidth, fdepth, GxEPD_BLACK); // forecast text outline
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  // MPr:
  // Main0 ist der Originaltext (englisch)
  // Forecast0, Forecast1 und Forcast2 ist der lokalisierte Text
  // String Wx_Description = WxConditions[0].Main0;
  String Wx_Description;
  if (WxConditions[0].Forecast0 != "")
    Wx_Description += WxConditions[0].Forecast0;
  // if (WxConditions[0].Forecast0 != "") Wx_Description += " (" + WxConditions[0].Forecast0;
  if (WxConditions[0].Forecast1 != "")
    Wx_Description += ", " + WxConditions[0].Forecast1;
  if (WxConditions[0].Forecast2 != "")
    Wx_Description += ", " + WxConditions[0].Forecast2;
  // if (Wx_Description.indexOf("(") > 0) Wx_Description += ")";
  int MsgWidth = 43; // Using proportional fonts, so be aware of making it too wide!
  if (Language == "DE")
    drawStringMaxWidth(x, y + 23, MsgWidth, Wx_Description, LEFT); // Leave German text in original format, 28 character screen width at this font size
  else
    drawStringMaxWidth(x, y + 23, MsgWidth, TitleCase(Wx_Description), LEFT); // 28 character screen width at this font size
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
}

/**
 * \brief Displays the OWM forecast with the specified index at the given position
 *
 * Each forecast item consist of local time, weather icon and min/max temperature.
 *
 * \param x       x-coordinate
 * \param y       y-coordinate
 * \param index   forecast data index
 */
void DisplayForecastWeather(int x, int y, int index)
{
  int fwidth = 73;
  x = x + fwidth * index;
  display.drawRect(x, y, fwidth - 1, 81, GxEPD_BLACK);
  display.drawLine(x, y + 16, x + fwidth - 3, y + 16, GxEPD_BLACK);
  DisplayConditionsSection(x + fwidth / 2, y + 43, WxForecast[index].Icon, SmallIcon);
  drawString(x + fwidth / 2, y + 4, String(ConvertUnixTime(WxForecast[index].Dt + WxConditions[0].Timezone).substring(0, 5)), CENTER);
  drawString(x + fwidth / 2 + 12, y + 66, String(WxForecast[index].High, 0) + "°/" + String(WxForecast[index].Low, 0) + "°", CENTER);
}

/**
 * \brief Displays the barometric pressure at the given position
 *
 * The pressure is printed in hPa or in, the trend (rising/falling) is printed as localized string.
 *
 * \param x         x-coordinate
 * \param y         y-coordinate
 * \param pressure  barometric pressure
 * \param slope     barometric pressure trend
 * \param pwith     section's width
 * \param pdepth    section's depth
 */
void DisplayPressureSection(int x, int y, float pressure, String slope, int pwidth, int pdepth)
{
  display.drawRect(x - 56, y - 1, pwidth, pdepth, GxEPD_BLACK); // pressure outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 8, y + 5, TXT_PRESSURE, CENTER);
  String slope_direction = TXT_PRESSURE_STEADY;
  if (slope == "+")
    slope_direction = TXT_PRESSURE_RISING;
  if (slope == "-")
    slope_direction = TXT_PRESSURE_FALLING;
  // display.drawRect(x + 40, y + 78, 41, 21, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  if (Units == "I")
    drawString(x - 22, y + 55, String(pressure, 2), CENTER); // "Imperial"
  else
    drawString(x - 22, y + 55, String(pressure, 0), CENTER); // "Metric"
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 55, y + 53, (Units == "M" ? "hPa" : "in"), CENTER);
  drawString(x - 3, y + 78, slope_direction, CENTER);
}

/**
 * \brief Displays the OWM precipitation forecast at the given position
 *
 * The precipitation forecast consists of the expected amount of rain/snow and
 * the forecast's probability.
 *
 * \param x         x-coordinate
 * \param y         y-coordinate
 * \param pwith     section's width
 * \param pdepth    section's depth
 */
void DisplayPrecipitationSection(int x, int y, int pwidth, int pdepth)
{
  display.drawRect(x - 48, y - 1, pwidth, pdepth, GxEPD_BLACK); // precipitation outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 25, y + 5, TXT_PRECIPITATION_SOON, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  if (WxForecast[1].Rainfall >= 0.005)
  {                                                                                                     // Ignore small amounts
    drawString(x - 25, y + 40, String(WxForecast[1].Rainfall, 2) + (Units == "M" ? "mm" : "in"), LEFT); // Only display rainfall total today if > 0
    addraindrop(x + 58, y + 40, 7);
  }
  if (WxForecast[1].Snowfall >= 0.005)                                                                          // Ignore small amounts
    drawString(x - 25, y + 60, String(WxForecast[1].Snowfall, 2) + (Units == "M" ? "mm" : "in") + " **", LEFT); // Only display snowfall total today if > 0
  if (WxForecast[1].Pop >= 0.005)                                                                               // Ignore small amounts
    drawString(x + 2, y + 81, String(WxForecast[1].Pop * 100, 0) + "%", LEFT);                                  // Only display pop if > 0
}

/**
 * \brief Displays the OWM astronomy section at the given position
 *
 * The astronony section consists of sunrise/sunset times and moon phase.
 * The moon phase is shown as localized text and as symbol.
 *
 * \param x         x-coordinate
 * \param y         y-coordinate
 */
void DisplayAstronomySection(int x, int y)
{
  display.drawRect(x, y + 16, 191, 65, GxEPD_BLACK);
  // display.drawLine(x, y + 16, x + 191, y + 16, GxEPD_BLACK);
  // display.drawLine(x, y + 16 + 64, x + 191, y + 16 + 64, GxEPD_BLACK);
  // display.drawLine(x, y + 16, x, y + 16 + 64, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 4, y + 24, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNRISE, LEFT);
  drawString(x + 4, y + 44, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNSET, LEFT);
  time_t now = time(NULL);
  struct tm *now_utc = gmtime(&now);
  const int day_utc = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc = now_utc->tm_year + 1900;
  drawString(x + 4, y + 64, MoonPhase(day_utc, month_utc, year_utc, Hemisphere), LEFT);
  DrawMoon(x + 110, y, day_utc, month_utc, year_utc, Hemisphere);
}

/**
 * \brief Displays the OWM attribution at the given position
 *
 * \param x         x-coordinate
 * \param y         y-coordinate
 */
void DisplayOWMAttribution(int x, int y)
{
  display.drawRect(x, y + 16, 219, 65, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 8, y + 24, "Weather data provided by OpenWeather", LEFT);
  drawString(x + 8, y + 44, "https://openweathermap.org/", LEFT);
}

/**
 * \brief Draws the moon phase symbol at the given position
 *
 * \param x           x-postition
 * \param y           y-position
 * \param dd          day
 * \param mm          month
 * \param yy          year
 * \param hemisphere  hemisphere (north/south)
 */
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere)
{
  const int diameter = 47;
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "south")
    Phase = 1 - Phase;
  // Draw dark part of moon
  display.fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++)
  {
    double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);
    // Determine the edges of the lighted part of the moon
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5)
    {
      Xpos1 = -Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    }
    else
    {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
    display.drawLine(pW1x, pW1y, pW2x, pW2y, GxEPD_WHITE);
    display.drawLine(pW3x, pW3y, pW4x, pW4y, GxEPD_WHITE);
  }
  display.drawCircle(x + diameter - 1, y + diameter, diameter / 2, GxEPD_BLACK);
}

/**
 * \brief Calculate moon phase
 *
 * \param d           day
 * \param m           month
 * \param y           year
 * \param hemisphere  hemisphere (north/south)
 */
String MoonPhase(int d, int m, int y, String hemisphere)
{
  int c, e;
  double jd;
  int b;
  if (m < 3)
  {
    y--;
    m += 12;
  }
  ++m;
  c = 365.25 * y;
  e = 30.6 * m;
  jd = c + e + d - 694039.09; /* jd is total days elapsed */
  jd /= 29.53059;             /* divide by the moon cycle (29.53 days) */
  b = jd;                     /* int(jd) -> b, take integer part of jd */
  jd -= b;                    /* subtract integer part to leave fractional part of original jd */
  b = jd * 8 + 0.5;           /* scale fraction from 0-8 and round by adding 0.5 */
  b = b & 7;                  /* 0 and 8 are the same phase so modulo 8 for 0 */
  if (hemisphere == "south")
    b = 7 - b;
  if (b == 0)
    return TXT_MOON_NEW; // New;              0%  illuminated
  if (b == 1)
    return TXT_MOON_WAXING_CRESCENT; // Waxing crescent; 25%  illuminated
  if (b == 2)
    return TXT_MOON_FIRST_QUARTER; // First quarter;   50%  illuminated
  if (b == 3)
    return TXT_MOON_WAXING_GIBBOUS; // Waxing gibbous;  75%  illuminated
  if (b == 4)
    return TXT_MOON_FULL; // Full;            100% illuminated
  if (b == 5)
    return TXT_MOON_WANING_GIBBOUS; // Waning gibbous;  75%  illuminated
  if (b == 6)
    return TXT_MOON_THIRD_QUARTER; // Third quarter;   50%  illuminated
  if (b == 7)
    return TXT_MOON_WANING_CRESCENT; // Waning crescent; 25%  illuminated
  return "";
}

// #########################################################################################
/* (C) D L BIRD
    This function will draw a graph on a ePaper/TFT/LCD display using data from an array containing data to be graphed.
    The variable 'max_readings' determines the maximum number of data elements for each array. Call it with the following parametric data:
    x_pos - the x axis top-left position of the graph
    y_pos - the y-axis top-left position of the graph, e.g. 100, 200 would draw the graph 100 pixels along and 200 pixels down from the top-left of the screen
    gwidth - the width of the graph in pixels
    gheight - height of the graph in pixels
    Y1Max - sets the scale of plotted data, for example 5000 would scale all data to a Y-axis of 5000 maximum
    DataArray is parsed by value, externally they can be called anything else, e.g. within the routine it is called DataArray, but externally could be temperature_readings
    auto_scale - a logical value (TRUE or FALSE) that switches the Y-axis autoscale On or Off
    barchart_mode - a logical value (TRUE or FALSE) that switches the drawing mode between barhcart and line graph
    If called with Y1Max value of 500 and the data never goes above 500, then autoscale will retain a 0-500 Y scale, if on, the scale increases/decreases to match the data.
    auto_scale_margin, e.g. if set to 1000 then autoscale increments the scale by 1000 steps.
*/

/**
 * This function will draw a graph on a ePaper/TFT/LCD display using data from an array containing data to be graphed.
 *
 * DataArray contains the elements to be plotted. Data is plotted from index data_offset to readings-1.
 * The same data range is also used for auto-scaling (if enabled).
 * If ValidArray is provided, only elements marked as valid are plotted. In linegraph mode, invalid elements lead to gaps.
 * In bargraph mode, invalid elements are drawn as bar outlines.
 * The x-axis is scaled from xmin to xmax with a step size of dx. Currently the x-axis is fixed to 3 steps.
 *
 * \param x_pos             x-coordinate
 * \param y_pos             y-coordinatre
 * \param gwidth            graph widget width
 * \param gheight           graph widget height
 * \param Y1Min             y-scale minimum value
 * \param Y1Max             y-scale maximum value
 * \param title             title, printed above graph
 * \param DataArray         array of data
 * \param readings          no. of elements in DataArray
 * \param auto_scale        if true, determine y-scale automatically, otherwise use fixed scale (Y1Min;Y1Max)
 * \param barchart_mode     if true, use barchart mode, otherwise use linechart mode
 * \param xmin              x-scale minimum value
 * \param xmax              x-scale maximum value
 * \param dx                x-scale step size (default: 1)
 * \param data_offset       offset in DataArray for start of plot (default: 1)
 * \param x_label           x-axis label, printed below graph (default: TXT_DAYS)
 * \param ValidArray        array of valid flags; one per each DataArray element (default: NULL)
 */
void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int readings, boolean auto_scale, boolean barchart_mode, int xmin, int xmax, int dx = 1,
               int data_offset = 1, const String x_label = TXT_DAYS, bool ValidArray[] = NULL);
void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int readings, boolean auto_scale, boolean barchart_mode, int xmin, int xmax, int dx,
               int data_offset, const String x_label, bool ValidArray[])
{
#define auto_scale_margin 0 // Sets the autoscale increment, so axis steps up in units of e.g. 3
#define y_minor_axis 5      // 5 y-axis division markers
  float maxYscale = -10000;
  float minYscale = 10000;
  int last_x, last_y;
  float x2, y2;

  if ((auto_scale == true) && (data_offset < readings))
  {
    // for (int i = 1; i < readings; i++ ) {
    for (int i = data_offset; i < readings; i++)
    {
      if (DataArray[i] >= maxYscale)
        maxYscale = DataArray[i];
      if (DataArray[i] <= minYscale)
        minYscale = DataArray[i];
    }
    maxYscale = round(maxYscale + auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Max
    Y1Max = round(maxYscale + 0.5);
    if (minYscale != 0)
      minYscale = round(minYscale - auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Min
    Y1Min = round(minYscale);
  }
  log_d("auto_scale: YMin=%f YMax=%f", Y1Min, Y1Max);
  // Draw the graph
  // last_x = x_pos + 1;
  last_x = -1;
  last_y = y_pos + (Y1Max - constrain(DataArray[data_offset], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;
  display.drawRect(x_pos, y_pos, gwidth + 3, gheight + 2, GxEPD_BLACK);
  drawString(x_pos + gwidth / 2 + 6, y_pos - 16, title, CENTER);
  // Draw the data
  for (int gx = data_offset; gx < readings; gx++)
  {
    x2 = x_pos + gx * gwidth / (readings - 1) - 1; // max_readings is the global variable that sets the maximum data that can be plotted
    y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
    if (ValidArray == NULL)
    {
      log_v("No array of valid-flags!");
    }
    if (ValidArray == NULL || ValidArray[gx])
    {
      if (ValidArray)
      {
        log_i("reading #%d val: %d data: %f", gx, ValidArray[gx] ? 1 : 0, DataArray[gx]);
      }
      if (barchart_mode)
      {
        display.fillRect(x2, y2, (gwidth / readings) - 1, y_pos + gheight - y2 + 2, GxEPD_BLACK);
      }
      else
      {
        if (last_x > -1)
        {
          display.drawLine(last_x, last_y, x2, y2, GxEPD_BLACK);
          last_y = y2;
        }
      }
      last_x = x2;
    }
    else
    {
      if (barchart_mode)
      {
        display.drawRect(x2, y_pos, (gwidth / readings) - 1, gheight + 2, GxEPD_BLACK);
      }
      else
      {
        last_x = -1;
        // display.drawLine(last_x, y2, x2, y2, GxEPD_BLACK);
      }
    }
  }
  // Draw the Y-axis scale
#define number_of_dashes 20
  for (int spacing = 0; spacing <= y_minor_axis; spacing++)
  {
    for (int j = 0; j < number_of_dashes; j++)
    { // Draw dashed graph grid lines
      if (spacing < y_minor_axis)
        display.drawFastHLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes), GxEPD_BLACK);
    }
    if ((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing) < 5 || title == TXT_PRESSURE_IN)
    {
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
  xdiv = (xdiv < 0) ? -xdiv / dx : xdiv / dx;

  // FIXME adjust x-offset for other number of labels than 3
  for (int n = xmin, i = 0; n <= xmax; n = n + dx, i++)
  {
    drawString(15 + x_pos + gwidth / 3 * i, y_pos + gheight + 3, String(n), LEFT);
    // i++;
  }
  drawString(x_pos + gwidth / 2, y_pos + gheight + 14, x_label, CENTER);
}

/**
 * \brief Display OWM 3-day forecast as graphs (pressure, temperature, humidity and precipitation)
 *
 * \param x   x-coordinate
 * \param y   y-coordinate
 */
void DisplayForecastSection(int x, int y)
{
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  int f = 0;
  do
  {
    DisplayForecastWeather(x, y, f);
    f++;
  } while (f <= 7);
  // Pre-load temporary arrays with with data - because C parses by reference
  int r = 0;
  do
  {
    if (Units == "I")
      pressure_readings[r] = WxForecast[r].Pressure * 0.02953;
    else
      pressure_readings[r] = WxForecast[r].Pressure;
    if (Units == "I")
      rain_readings[r] = WxForecast[r].Rainfall * 0.0393701;
    else
      rain_readings[r] = WxForecast[r].Rainfall;
    if (Units == "I")
      snow_readings[r] = WxForecast[r].Snowfall * 0.0393701;
    else
      snow_readings[r] = WxForecast[r].Snowfall;
    temperature_readings[r] = WxForecast[r].Temperature;
    humidity_readings[r] = WxForecast[r].Humidity;
    r++;
  } while (r < max_readings);
  int gwidth = 150, gheight = 72;
  int gx = (SCREEN_WIDTH - gwidth * 4) / 5 + 5;
  int gy = 375;
  int gap = gwidth + gx;
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(SCREEN_WIDTH / 2, gy - 40, TXT_FORECAST_VALUES, CENTER); // Based on a graph height of 60
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  // (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  DrawGraph(gx + 0 * gap, gy, gwidth, gheight, 900, 1050, Units == "M" ? TXT_PRESSURE_HPA : TXT_PRESSURE_IN, pressure_readings, max_readings, autoscale_on, barchart_off, 0, 2);
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 10, 30, Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature_readings, max_readings, autoscale_on, barchart_off, 0, 2);
  DrawGraph(gx + 2 * gap, gy, gwidth, gheight, 0, 100, TXT_HUMIDITY_PERCENT, humidity_readings, max_readings, autoscale_off, barchart_off, 0, 2, 1);
  if (SumOfPrecip(rain_readings, max_readings) >= SumOfPrecip(snow_readings, max_readings))
    DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_readings, max_readings, autoscale_on, barchart_on, 0, 2);
  else
    DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_SNOWFALL_MM : TXT_SNOWFALL_IN, snow_readings, max_readings, autoscale_on, barchart_on, 0, 2);
}

/**
 * \brief Display local sensor data history as graphs (pressure, temperature, humidity)
 */
void DisplayLocalHistory()
{
  local_hist_t local_data;
  float temperature[LOCAL_HIST_SIZE];
  float humidity[LOCAL_HIST_SIZE];
  float pressure[LOCAL_HIST_SIZE];
  bool th_valid[LOCAL_HIST_SIZE];
  bool p_valid[LOCAL_HIST_SIZE];

  for (int i = 0; i < LOCAL_HIST_SIZE; i++)
  {
    temperature[i] = 0;
    humidity[i] = 0;
    pressure[i] = 0;
    th_valid[i] = false;
    p_valid[i] = false;
  }
  /*
  for (int i=q_getCount(&LocalHistQCtrl)-1, j=LOCAL_HIST_SIZE-1; i>=0; i--, j--) {
    q_peekIdx(&LocalHistQCtrl, &local_data, i);
    temperature[j] = local_data.temperature;
    humidity[j] = local_data.humidity;
    pressure[j] = local_data.pressure;
    th_valid[j] = local_data.th_valid;
    p_valid[j]  = local_data.p_valid;
  }
  */
  int offs = LOCAL_HIST_SIZE - q_getCount(&LocalHistQCtrl);
  for (int i = 0; i < q_getCount(&LocalHistQCtrl); i++)
  {
    q_peekIdx(&LocalHistQCtrl, &local_data, i);
    temperature[i + offs] = local_data.temperature;
    humidity[i + offs] = local_data.humidity;
    pressure[i + offs] = local_data.pressure;
    th_valid[i + offs] = local_data.th_valid;
    p_valid[i + offs] = local_data.p_valid;
  }

  int gwidth = 150, gheight = 72;
  int gx = (SCREEN_WIDTH - gwidth * 3) / 4 + 5;
  int gy = 375;
  int gap = gwidth + gx;
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(SCREEN_WIDTH / 2, gy - 40, TXT_LOCAL_HISTORY_VALUES, CENTER); // Based on a graph height of 60
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);

  // (x, y, width, height, MinValue, MaxValue, Title, DataArray[], AutoScale, ChartMode, xmin, xmax, dx, data_offset, x_label, ValidArray[])
  DrawGraph(gx + 0 * gap, gy, gwidth, gheight, 900, 1050, Units == "M" ? TXT_PRESSURE_HPA : TXT_PRESSURE_IN, pressure, LOCAL_HIST_SIZE, autoscale_off, barchart_off, -2, 0, 1, offs, TXT_DAYS, p_valid);
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 10, 30, Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature, LOCAL_HIST_SIZE, autoscale_on, barchart_off, -2, 0, 1, offs, TXT_DAYS, th_valid);
  DrawGraph(gx + 2 * gap, gy, gwidth, gheight, 0, 100, TXT_HUMIDITY_PERCENT, humidity, LOCAL_HIST_SIZE, autoscale_off, barchart_off, -2, 0, 1, offs, TXT_DAYS, th_valid);
}

/**
 * \brief Display MQTT sensor data history as graphs (pressure, temperature, humidity)
 */
void DisplayMqttHistory()
{
  mqtt_hist_t mqtt_data;
  rain_hr_hist_t rain_hr_data;
  rain_day_hist_t rain_day_data;
  float temperature[MQTT_HIST_SIZE];
  float humidity[MQTT_HIST_SIZE];
  bool mqtt_valid[MQTT_HIST_SIZE];
  float rain_hr[RAIN_HR_HIST_SIZE];
  bool rain_hr_valid[RAIN_HR_HIST_SIZE];
  float rain_day[RAIN_DAY_HIST_SIZE + 1];
  bool rain_day_valid[RAIN_DAY_HIST_SIZE + 1];
  int offs;

  for (int i = 0; i < MQTT_HIST_SIZE; i++)
  {
    temperature[i] = 0;
    humidity[i] = 0;
  }

  offs = MQTT_HIST_SIZE - q_getCount(&MqttHistQCtrl);
  for (int i = 0; i < q_getCount(&MqttHistQCtrl); i++)
  {
    q_peekIdx(&MqttHistQCtrl, &mqtt_data, i);
    temperature[i + offs] = mqtt_data.temperature;
    humidity[i + offs] = (float)(mqtt_data.humidity);
    mqtt_valid[i + offs] = mqtt_data.valid;
  }

  int gwidth = 150, gheight = 72;
  int gx = (SCREEN_WIDTH - gwidth * 4) / 5 + 5;
  int gy = 375;
  int gap = gwidth + gx;

  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(SCREEN_WIDTH / 2, gy - 40, TXT_MQTT_HISTORY_VALUES, CENTER); // Based on a graph height of 60
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);

  // (x, y, width, height, MinValue, MaxValue, Title, DataArray[], AutoScale, ChartMode, xmin, xmax, dx, data_offset, x_label, ValidArray[])
  DrawGraph(gx, gy, gwidth, gheight, 10, 30, Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature, MQTT_HIST_SIZE, autoscale_on, barchart_off, -2, 0, 1, offs, TXT_DAYS, mqtt_valid);
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 0, 100, TXT_HUMIDITY_PERCENT, humidity, MQTT_HIST_SIZE, autoscale_off, barchart_off, -2, 0, 1, offs, TXT_DAYS, mqtt_valid);

  // Hourly Rain History
  for (int i = 0; i < RAIN_HR_HIST_SIZE; i++)
  {
    rain_hr[i] = 0;
  }

  offs = RAIN_HR_HIST_SIZE - q_getCount(&RainHrHistQCtrl);
  for (int i = 0; i < q_getCount(&RainHrHistQCtrl); i++)
  {
    q_peekIdx(&RainHrHistQCtrl, &rain_hr_data, i);
    rain_hr[i + offs] = rain_hr_data.rain;
    rain_hr_valid[i + offs] = rain_hr_data.valid;
  }

  // (x, y, width, height, MinValue, MaxValue, Title, DataArray[], AutoScale, ChartMode, xmin, xmax, dx, data_offset, x_label, ValidArray[])
  DrawGraph(gx + 2 * gap + 5, gy, gwidth, gheight, 0, 300, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_hr, RAIN_HR_HIST_SIZE + 1, autoscale_on, barchart_on, -20, -4, 8, offs, TXT_HOURS, rain_hr_valid);

  // Daily Rain History
  for (int i = 0; i < RAIN_DAY_HIST_SIZE; i++)
  {
    rain_day[i] = 0;
  }

  // Note: The queue size is RAIN_DAY_HIST_SIZE, but the array size is RAIN_DAY_HIST_SIZE + 1!!!
  // Write queue entries to the end of the array, but leave out last array entry
  offs = RAIN_DAY_HIST_SIZE - q_getCount(&RainDayHistQCtrl);
  for (int i = 0; i < q_getCount(&RainDayHistQCtrl); i++)
  {
    q_peekIdx(&RainDayHistQCtrl, &rain_day_data, i);
    rain_day[i + offs] = rain_day_data.rain;
    rain_day_valid[i + offs] = rain_day_data.valid;
  }

  // Write current value to the end of the array
  rain_day[RAIN_DAY_HIST_SIZE] = MqttSensors.rain_day_prev;
  rain_day_valid[RAIN_DAY_HIST_SIZE] = true;

  // (x, y, width, height, MinValue, MaxValue, Title, DataArray[], AutoScale, ChartMode, xmin, xmax, dx, data_offset, x_label, ValidArray[])
  DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 300, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_day, RAIN_DAY_HIST_SIZE, autoscale_on, barchart_on, -25, -5, 10, offs, TXT_DAYS, rain_day_valid);
}

/**
 * \brief Display current OWM forecast conditions at given position
 *
 * The widget consists of the weather condition icon, visibility in m,
 * cloud cover in % and rel. humidity in %.
 *
 * \param x         x-coordinate
 * \param y         y-coordinate
 * \param IconName  weather icon name
 * \param IconSize  icon size (SmallIcon/LargeIcon)
 */
void DisplayConditionsSection(int x, int y, String IconName, bool IconSize)
{
  log_d("Icon name: %s", IconName.c_str());
  if (IconName == "01d" || IconName == "01n")
    Sunny(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n")
    MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n")
    Cloudy(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n")
    MostlyCloudy(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n")
    ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n")
    Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n")
    Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n")
    Snow(x, y, IconSize, IconName);
  else if (IconName == "50d")
    Haze(x, y, IconSize, IconName);
  else if (IconName == "50n")
    Fog(x, y, IconSize, IconName);
  else
    Nodata(x, y, IconSize, IconName);
  if (IconSize == LargeIcon)
  {
    display.drawRect(x - 86, y - 131, 173, 228, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(x, y - 125, TXT_CONDITIONS, CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(x - 25, y + 70, String(WxConditions[0].Humidity, 0) + "% " + TXT_RELATIVE_HUMIDITY, CENTER);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    // drawString(x + 35, y + 70, "RH", CENTER);
    if (WxConditions[0].Visibility > 0)
      Visibility(x - 62, y - 87, String(WxConditions[0].Visibility) + "M");
    if (WxConditions[0].Cloudcover > 0)
      CloudCover(x + 35, y - 87, WxConditions[0].Cloudcover);
  }
}

/**
 * \brief Display arrow (triangle) for wind direction in wind rose
 *
 * \param x         x-coordinate
 * \param y         y-coordinate
 * \param aangle    direction (degrees)
 * \param asize     arrow size
 * \param pwidth    pointer(?) width
 * \param plength   pointer(?) length
 */
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength)
{
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;
  float y1 = plength;
  float x2 = pwidth / 2;
  float y2 = pwidth / 2;
  float x3 = -pwidth / 2;
  float y3 = pwidth / 2;
  float angle = aangle * PI / 180;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  display.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, GxEPD_BLACK);
}

#if 0
/**
 * \brief Display status section
 * 
 * FIXME: Not used, to be removed!
 */
void DisplayStatusSection(int x, int y, int rssi) {
  display.drawRect(x - 35, y - 32, 145, 61, GxEPD_BLACK);
  display.drawLine(x - 35, y - 17, x - 35 + 145, y - 17, GxEPD_BLACK);
  display.drawLine(x - 35 + 146 / 2, y - 18, x - 35 + 146 / 2, y - 32, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y - 29, TXT_WIFI, CENTER);
  drawString(x + 68, y - 30, TXT_POWER, CENTER);
  DrawRSSI(x - 10, y + 6, rssi);
  DrawBattery(x + 58, y + 6);
}
#endif

/**
 * \brief Draw WiFi RSSI as sequence of bars and numeric value
 *
 * If WiFi is not connected, the localized text TXT_WIFI_OFF is printed instead.
 *
 * \param x     x-position
 * \param y     y-position
 * \param rssi  RSSI (dBm)
 */
void DrawRSSI(int x, int y, int rssi)
{
  int WIFIsignal = 0;
  int xpos = 1;
  if (WiFi.status() == WL_CONNECTED)
  {
    for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20)
    {
      if (_rssi <= -20)
        WIFIsignal = 20; //            <-20dbm displays 5-bars
      if (_rssi <= -40)
        WIFIsignal = 16; //  -40dbm to  -21dbm displays 4-bars
      if (_rssi <= -60)
        WIFIsignal = 12; //  -60dbm to  -41dbm displays 3-bars
      if (_rssi <= -80)
        WIFIsignal = 8; //  -80dbm to  -61dbm displays 2-bars
      if (_rssi <= -100)
        WIFIsignal = 4; // -100dbm to  -81dbm displays 1-bar
      display.fillRect(x + xpos * 6, y - WIFIsignal, 5, WIFIsignal, GxEPD_BLACK);
      xpos++;
    }
    // display.fillRect(x, y - 1, 5, 1, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(x + 60, y - 10, String(rssi) + "dBm", CENTER);
  }
  else
  {
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(x + 20, y - 10, TXT_WIFI_OFF, CENTER);
  }
}

#if 0
/**
 * \brief Draw battery status
 * 
 * FIXME: Not used, to be removed! 
 * Or to be made more generic...
 * Or to be made optional...
 * And with calibrated ADC read...
 */
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.46;
  if (voltage > 1 ) { // Only display if there is a valid reading
    log_d("Voltage = %f", voltage);
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
#endif

/**
 * \brief Display question mark if weather icon name is unknown
 *
 * \param x         x-coordinate
 * \param y         y-coordinate
 * \param IconSize  size of weather icon (SmallIcin/Largeicon)
 * \param IconName  name of weather icon (not used)
 */
void Nodata(int x, int y, bool IconSize, String IconName)
{
  if (IconSize == LargeIcon)
    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  else
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 10, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}

/**
 * \brief Draw string at given position with specified alignment
 *
 * \param x           x-coordinate
 * \param y           y-coordinate
 * \param text        text
 * \param alignment   alignment (LEFT/RIGHT/CENTER)
 */
void drawString(int x, int y, String text, alignment align)
{
  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)
    x = x - w;
  if (align == CENTER)
    x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}

/**
 * \brief Draw string at given position with specified alignment and maximum width
 *
 * If the actual text is longer than the maximum width, it is wrapped into a second line.
 *
 * \param x           x-coordinate
 * \param y           y-coordinate
 * \param text_width  maximum text width (characters)
 * \param text        text
 * \param alignment   alignment (LEFT/RIGHT/CENTER)
 */
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align)
{
  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)
    x = x - w;
  if (align == CENTER)
    x = x - w / 2;
  u8g2Fonts.setCursor(x, y);
  if (text.length() > text_width * 2)
  {
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    text_width = 42;
    y = y - 3;
  }
  u8g2Fonts.println(text.substring(0, text_width));
  if (text.length() > text_width)
  {
    u8g2Fonts.setCursor(x, y + h + 15);
    String secondLine = text.substring(text_width);
    secondLine.trim(); // Remove any leading spaces
    u8g2Fonts.println(secondLine);
  }
}

/**
 * \brief Display temperature section
 *
 * Variant of DisplayTemperatureSection() with more parameters/features
 * - label
 * - display of invalid data as "?"
 * - current temperature
 * - optional min/max temperature
 *
 * \param x           x-coordinate
 * \param y           y-coordinate
 * \param twidth      section width
 * \param tdepth      section depth
 * \param label       label
 * \param valid       true if data is valid, false otherwise
 * \param tcurrent    current temperature
 * \param minmax      true if min/max values shall be displayed, false otherwise
 * \param tmin        min temperature
 * \param tmax        max temperature
 */
void DisplayLocalTemperatureSection(int x, int y, int twidth, int tdepth, String label, bool valid, float tcurrent, bool minmax, float tmin, float tmax)
{
  // display.drawRect(x - 63, y - 1, twidth, tdepth, GxEPD_BLACK); // temp outline
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  if (label != "")
  {
    drawString(x, y + 5, label, CENTER);
  }
  String _unit = (Units == "M") ? "°C" : "°F";
  if (!valid)
  {
    if (minmax)
    {
      u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      drawString(x - 5, y + 70, "? " + _unit + " | ? " + _unit, CENTER);
    }
    // u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    // u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    u8g2Fonts.setFont(u8g2_font_helvR24_tf);
    drawString(x - 22, y + 53, "?.? " + _unit, CENTER); // Show current Temperature
    // u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    // drawString(x + 43, y + 53, Units == "M" ? "C" : "F", LEFT);
  }
  else
  {
    if (minmax)
    {
      u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      drawString(x - 5, y + 70, String(tmin, 0) + _unit + " | " + String(tmax, 0) + _unit, CENTER); // Show temperature min and max
    }
    // u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    // u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    u8g2Fonts.setFont(u8g2_font_helvR24_tf);
    drawString(x - 22, y + 53, String(tcurrent, 1) + _unit, CENTER); // Show current Temperature
  }

  // u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  // drawString(x + 43, y + 53, Units == "M" ? "C" : "F", LEFT);
}

/**
 * \brief Initialize SPI controller and ePaper display
 *
 * Uses the global objects 'display' and 'u8g2Fonts' and the pin configuration values EPD_*
 */
void InitialiseDisplay()
{
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
// #########################################################################################

/*

  Version 16.11
   1. Adjusted graph drawing for negative numbers
   2. Correct offset error for precipitation

*/
