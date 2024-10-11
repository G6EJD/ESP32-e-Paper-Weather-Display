///////////////////////////////////////////////////////////////////////////////
// MqttInterface.h
//
// MQTT Interface for ESP32-e-Paper-Weather-Display
//
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
// 20241010 Extracted from Waveshare_7_5_T7_Sensors.ino
// 20241011 Fixed sensor status flags, added secure MQTT
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _MQTT_INTERFACE
#define _MQTT_INTERFACE
#include <Arduino.h>
#include <string>
#include "config.h"

#include <WiFi.h>
#if defined(USE_SECUREWIFI)
#include <WiFiClientSecure.h>
#endif

#include <MQTT.h>        // https://github.com/256dpi/arduino-mqtt
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson needs version v6 or above

#include "secrets.h"
#include "LocalInterface.h"

// MQTT Sensor Data
struct MqttS
{
    bool valid;           //!<
    char received_at[32]; //!< MQTT message received date/time
    struct
    {
        unsigned int ws_batt_ok : 1;  //!< weather sensor battery o.k.
        unsigned int ws_dec_ok : 1;   //!< weather sensor decoding o.k.
        unsigned int s1_batt_ok : 1;  //!< soil moisture sensor battery o.k.
        unsigned int s1_dec_ok : 1;   //!< soil moisture sensor dencoding o.k.
        unsigned int ble_batt_ok : 1; //!< BLE sensor battery o.k.
        unsigned int ble_ok : 1;      //!< BLE T-/H-sensor data o.k.

    } status;
    float air_temp_c;          //!< temperature in degC
    uint8_t humidity;          //!< humidity in %
    float wind_direction_deg;  //!< wind direction in deg
    float wind_gust_meter_sec; //!< wind speed (gusts) in m/s
    float wind_avg_meter_sec;  //!< wind speed (avg)   in m/s
    float rain_mm;             //!< rain gauge level in mm
    uint16_t supply_v;         //!< supply voltage in mV
    uint16_t battery_v;        //!< battery voltage in mV
    float water_temp_c;        //!< water temperature in degC
    float indoor_temp_c;       //!< indoor temperature in degC
    uint8_t indoor_humidity;   //!< indoor humidity in %
    float soil_temp_c;         //!< soil temperature in degC
    uint8_t soil_moisture;     //!< soil moisture in %
    float rain_hr;             //!< hourly precipitation in mm
    bool rain_hr_valid;        //!< hourly precipitation valid
    float rain_day;            //!< daily precipitation in mm
    bool rain_day_valid;       //!< daily precipitation valid
    float rain_day_prev;       //!< daily precipitation in mm, previous value
    float rain_week;           //!< weekly precipitation in mm
    float rain_month;          //!< monthly precipitatiion in mm
};

typedef struct MqttS mqtt_sensors_t; //!< Shortcut for struct Sensor
struct MqttHistQData
{
    float temperature; //!< temperature in degC
    uint8_t humidity;  //!< humidity in %
    bool valid;        //!< data valid
};

typedef struct MqttHistQData mqtt_hist_t; //!< Shortcut for struct MqttHistQData

// TOPICS_NEW: BresserWeatherSensorLW
#define TOPICS_NEW

#ifdef TOPICS_NEW
#define WS_TEMP_C "ws_temp_c"
#define WS_HUMIDITY "ws_humidity"
#define BLE0_TEMP_C "ble0_temp_c"
#define BLE0_HUMIDITY "ble0_humidity"
#define A0_VOLTAGE_MV "a0_voltage_mv"
#define WS_RAIN_DAILY_MM "ws_rain_daily_mm"
#define WS_RAIN_HOURLY_MM "ws_rain_hourly_mm"
#define WS_RAIN_MM "ws_rain_mm"
#define WS_RAIN_MONTHLY_MM "ws_rain_monthly_mm"
#define WS_RAIN_WEEKLY_MM "ws_rain_weekly_mm"
#define SOIL1_MOISTURE "soil1_moisture"
#define SOIL1_TEMP_C "soil1_temp_c"
#define OW0_TEMP_C "ow0_temp_c"
#define WS_WIND_AVG_MS "ws_wind_avg_ms"
#define WS_WIND_DIR_DEG "ws_wind_dir_deg"
#define WS_WIND_GUST_MS "ws_wind_gust_ms"
#else
#define WS_TEMP_C "air_temp_c"
#define WS_HUMIDITY "humidity"
#define BLE0_TEMP_C "indoor_temp_c"
#define BLE0_HUMIDITY "indoor_humidity"
#define A0_VOLTAGE_MV "battery_v"
#define WS_RAIN_DAILY_MM "rain_day"
#define WS_RAIN_HOURLY_MM "rain_hr"
#define WS_RAIN_MM "rain_mm"
#define WS_RAIN_MONTHLY_MM "rain_mon"
#define WS_RAIN_WEEKLY_MM "rain_week"
#define SOIL1_MOISTURE "soil_moisture"
#define SOIL1_TEMP_C "soil_temp_c"
#define OW0_TEMP_C "water_temp_c"
#define WS_WIND_AVG_MS "wind_avg_meter_sec"
#define WS_WIND_DIR_DEG "wind_direction_deg"
#define WS_WIND_GUST_MS "wind_gust_meter_sec"
#endif

// Encoding of invalid values
// for floating point, see
// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/NaN
#define INV_FLOAT NAN
#define INV_UINT32 0xFFFFFFFF
#define INV_UINT16 0xFFFF
#define INV_UINT8 0xFF
#define INV_TEMP 327.67

class MqttInterface
{
private:
#if defined(USE_SECUREWIFI)
    NetworkClientSecure net;
    #else
    WiFiClient net;
#endif
    MQTTClient MqttClient;

public:
    /*!
     * \brief Constructor
     */
    MqttInterface(MQTTClient &_MqttClient);

    /**
     * \brief Connect to MQTT broker
     *
     * \return true if connection was succesful, otherwise false
     */
    bool mqttConnect();

    /**
     * \brief Get MQTT data from broker
     *
     * \param MqttClient  MQTT client object
     */
    void getMqttData(mqtt_sensors_t &MqttSensors);

    bool mqttUplink(MQTTClient &MqttClient, local_sensors_t &data);
};
#endif