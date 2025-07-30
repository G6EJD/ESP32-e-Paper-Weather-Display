///////////////////////////////////////////////////////////////////////////////
// config.h
//
// General configuration
//
// Note: Provide secrets in secrets.h!
//
// created: 10/2024
//
//
// MIT License
//
// Copyright (c) 2024 Matthias Prinke
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//
// History:
//
// 20241010 Extracted from owm_credentials.h
// 20241011 Added secure WiFi
// 20250310 Added AUTO_DISCOVERY (Home Assistant)
// 20250725 Added define for selection of JSON payload variant (TTN/Helium)
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////

#ifndef CONFIG_H
#define CONFIG_H

// Bitmap images
#include "bitmap_local.h" // Picture shown on ScreenLocal - replace by your own
#include "bitmap_remote.h" // Picture shown on ScreenMQTT  - replace by your own

// Screen definitions
#define ScreenOWM   0
#define ScreenLocal 1
#define ScreenMQTT  2
#define ScreenStart 3

#define TXT_START "Your Weather Station"
#define START_SCREEN ScreenStart
#define LAST_SCREEN  ScreenMQTT

// Locations / Screen Titles
#define LOCATIONS_TXT {"Forecast", "Local", "Remote", "Start"}

// #define DISPLAY_3C
#define DISPLAY_BW
#define SCREEN_WIDTH 800  //!< EPD screen width
#define SCREEN_HEIGHT 480 //!< EPD screen height

// #define MITHERMOMETER_EN         //!< Enable MiThermometer   (BLE sensors)
#define THEENGSDECODER_EN //!< Enable Theengs Decoder (BLE sensors)
#define BME280_EN         //!< Enable BME280 T/H/p-sensor (I2C)
#define SCD4X_EN          //!< Enable SCD4x CO2-sensor (I2C)
// #define WATERTEMP_EN               //!< Enable Wather Temperature Display (MQTT)
#define MITHERMOMETER_BATTALERT 6 //!< Low battery alert threshold [%]
#define WATER_TEMP_INVALID -30.0  //!< Water temperature invalid marker [°C]
#define I2C_SDA 21                //!< I2C Serial Data Pin
#define I2C_SCL 22                //!< I2C Serial Clock Pin

//#define USE_SECUREWIFI

// enable only one of these below, disabling both is fine too.
//#define CHECK_CA_ROOT
//#define CHECK_PUB_KEY

//#define USE_HTTPS

// Domain Name Server - separate bytes by comma!
#define MY_DNS 192,168,0,1

// Weather display's hostname
#define HOSTNAME "your_hostname"

// Time zone and time server settings
#define TIMEZONE "GMT0BST,M3.5.0/01,M10.5.0/02"            // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
                                                           // See below for examples
#define NTPSERVER "0.uk.pool.ntp.org"                      // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
                                                           // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
                                                           // EU "0.europe.pool.ntp.org"
                                                           // US "0.north-america.pool.ntp.org"
                                                           // See: https://www.ntppool.org/en/                                                           
#define GMT_OFFSET_SEC 0                                   // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
#define DAYLIGHT_OFFSET_SEC 3600                           // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset

// Example time zones
// "MET-1METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
// "CET-1CEST,M3.5.0,M10.5.0/3";       // Central Europe
// "EST-2METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
// "EST5EDT,M3.2.0,M11.1.0";           // EST USA  
// "CST6CDT,M3.2.0,M11.1.0";           // CST USA
// "MST7MDT,M4.1.0,M10.5.0";           // MST USA
// "NZST-12NZDT,M9.5.0,M4.1.0/3";      // Auckland
// "EET-2EEST,M3.5.5/0,M10.5.5/0";     // Asia
// "ACST-9:30ACDT,M10.1.0,M4.1.0/3":   // Australia

//!< List of known BLE sensors' MAC addresses (separate multiple entries by comma)
 //#define KNOWN_BLE_ADDRESSES {"a4:c1:38:b8:1f:7f"}  
#define KNOWN_BLE_ADDRESSES {"49:22:05:17:0c:1f"}

// #define SIMULATE_MQTT
// #define FORCE_LOW_BATTERY
// #define FORCE_NO_SIGNAL

// MQTT connection for subscribing (remote sensor data)
#define MQTT_PORT 1883
#define MQTT_HOST "your_broker"
#define MQTT_SUB_IN "your/subscribe/topic"
#define MQTT_JSON_FMT_TTN
//#define MQTT_JSON_FMT_HELIUM
//#define MQTT_JSON_FMT_BWSR // Bresser Weather Sensor Receiver

// MQTT connection for publishing (local sensor data)
#define MQTT_PORT_P 1883
#define MQTT_HOST_P "your_broker_pub"

#define MQTT_PAYLOAD_SIZE 4096
#define MQTT_CONNECT_TIMEOUT 30
#define MQTT_DATA_TIMEOUT 600
#define MQTT_KEEPALIVE 60
#define MQTT_TIMEOUT 1800
#define MQTT_CLEAN_SESSION false

#define MQTT_HIST_SIZE 144
#define RAIN_HR_HIST_SIZE 24
#define RAIN_DAY_HIST_SIZE 29
#define LOCAL_HIST_SIZE 144
#define HIST_UPDATE_RATE 30
#define HIST_UPDATE_TOL 5

// Enable Home Assistant MQTT Auto-Discovery
#define AUTO_DISCOVERY

#endif
