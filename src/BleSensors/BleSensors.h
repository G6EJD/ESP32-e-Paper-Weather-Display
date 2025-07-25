///////////////////////////////////////////////////////////////////////////////
// BleSensors.h
// 
// Wrapper class for Theeengs Decoder (https://github.com/theengs/decoder)
//
// Intended for compatibility to the ATC_MiThermometer library
// (https://github.com/matthias-bs/ATC_MiThermometer)
//
// created: 02/2023
//
//
// MIT License
//
// Copyright (c) 2023 Matthias Prinke
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
// 20230211 Created
// 20240417 Added additional constructor and method setAddresses()
// 20240427 Added paramter activeScan to getData()
// 20250121 Updated for NimBLE-Arduino v2.x
//
// ToDo:
// - 
//
///////////////////////////////////////////////////////////////////////////////

#if !defined(BLE_SENSORS) && !defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2) \
                          && !defined(ARDUINO_ARCH_RP2040)
#define BLE_SENSORS

#include <Arduino.h>
#include <NimBLEDevice.h>       //!< https://github.com/h2zero/NimBLE-Arduino
#include <decoder.h>            //!< https://github.com/theengs/decoder

/*!
 * \brief BLE sensor data
 */
struct BleDataS {
      bool     valid;              //!< data valid
      float    temperature;        //!< temperature in degC
      float    humidity;           //!< humidity in %
      uint8_t  batt_level;         //!< battery level in %
      int      rssi;               //!< RSSI in dBm
};

typedef struct BleDataS ble_sensors_t; //!< Shortcut for struct BleDataS


/*!
 * \brief BLE Sensor (e.g. thermometer/hygrometer) client
 */
class BleSensors {
    public:
        /*!
         * \brief Constructor.
         *
         * \param known_sensors    Vector of BLE MAC addresses of known sensors, e.g. {"11:22:33:44:55:66", "AA:BB:CC:DD:EE:FF"}
         */
        BleSensors(std::vector<std::string> known_sensors) {
            _known_sensors = known_sensors;
            data.resize(known_sensors.size());
        };

        /*!
         * \brief Constructor.
         */
        BleSensors(void) {
        };

        /*!
         * \brief Set BLE MAC addresses of known sensors
         * 
         * \param known_sensors vector of BLE MAC addresses (see constructor)
         */
        void setAddresses(std::vector<std::string> known_sensors) {
            _known_sensors = known_sensors;
            data.resize(known_sensors.size());
        };

        /*!
         * \brief Initialization.
         */
        void begin(void) {
        };
        
        /*!
         * \brief Delete results from BLEScan buffer to release memory.
         */
        void clearScanResults(void);
        
        /*!
         * \brief Get data from sensors by running a BLE scan.
         * 
         * \param duration     Scan duration in seconds
         * \param activeScan   0: passive scan / 1: active scan
         */                
        unsigned getData(uint32_t duration, bool activeScan = true);
        
        /*!
         * \brief Set sensor data invalid.
         */                        
        void resetData(void);
        
        /*!
         * \brief Sensor data.
         */
        std::vector<ble_sensors_t>  data;
        
    protected:
        std::vector<std::string> _known_sensors; /// MAC addresses of known sensors
        NimBLEScan*              _pBLEScan;      /// NimBLEScan object
};
#endif
