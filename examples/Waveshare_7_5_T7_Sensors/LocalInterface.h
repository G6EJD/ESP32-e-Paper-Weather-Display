///////////////////////////////////////////////////////////////////////////////
// LocalInterface.h
//
// Local Sensor Data Interface for ESP32-e-Paper-Weather-Display
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
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _LOCAL_INTERFACE
#define _LOCAL_INTERFACE
#include <Arduino.h>
#include "config.h"


#ifdef MITHERMOMETER_EN
// BLE Temperature/Humidity Sensor
#include <ATC_MiThermometer.h> //!< https://github.com/matthias-bs/ATC_MiThermometer
#endif

#ifdef THEENGSDECODER_EN
#include "NimBLEDevice.h" //!< https://github.com/h2zero/NimBLE-Arduino
#include "decoder.h"      //!< https://github.com/theengs/decoder
#endif

#ifdef BME280_EN
#include <pocketBME280.h> // https://github.com/angrest/pocketBME280
#endif

#ifdef SCD4X_EN
#include <SensirionI2cScd4x.h> // https://github.com/Sensirion/arduino-i2c-scd4x
#endif



// Local Sensor Data
struct LocalS
{
    struct
    {
        bool valid;         //!< data valid
        float temperature;  //!< temperature in degC
        float humidity;     //!< humidity in %
        uint8_t batt_level; //!< battery level in %
        int rssi;           //!< RSSI in dBm
    } ble_thsensor[1];
    struct
    {
        bool valid;        //!< data valid
        float temperature; //!< temperature in degC
        float humidity;    //!< humidity in %
        float pressure;    //!< pressure in hPa
    } i2c_thpsensor[1];
    struct
    {
        bool valid;        //!< data valid
        float temperature; //!< temperature in degC
        float humidity;    //!< humidity in %
        uint16_t co2;      //!< CO2 in ppm
    } i2c_co2sensor;
};

typedef struct LocalS local_sensors_t; //!< Shortcut for struct LocalS

struct LocalHistQData
{
  float temperature; //!< temperature in degC
  float humidity;    //!< humidity in %
  float pressure;    //!< pressure in hPa
  bool th_valid;     //!< temperature/humidity valid
  bool p_valid;      //!< pressure valid
};

typedef struct LocalHistQData local_hist_t; //!< Shortcut for struct LocalHistQData

class LocalInterface
{
public:
    LocalInterface() {
    };

    /**
     * \brief Get local sensor data
     */
    void GetLocalData(void);
};

#endif