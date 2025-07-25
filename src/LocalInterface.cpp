///////////////////////////////////////////////////////////////////////////////
// LocalInterface.cpp
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
// 20250308 Updated NimBLE-Arduino to v2.2.3
// 20250725 Replaced BLE code by src/BleSensors/BleSensors.h/.cpp
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////

#include "LocalInterface.h"

extern bool TouchTriggered();

extern local_sensors_t LocalSensors;

#if defined(MITHERMOMETER_EN) || defined(THEENGSDECODER_EN)
static const int bleScanTime = 31; //!< BLE scan time in seconds
static const int bleScanMode = 1;  //!< BLE scan mode: 0=passive, 1=active
static std::vector<std::string> knownBLEAddresses = KNOWN_BLE_ADDRESSES;
#endif

#ifdef MITHERMOMETER_EN
/// BLE Temperature/Humidity Sensors
ATC_MiThermometer bleSensors; //!< Mijia Bluetooth Low Energy Thermo-/Hygrometer
#endif
#ifdef THEENGSDECODER_EN
/// Bluetooth Low Energy sensors
BleSensors bleSensors;
#endif

// Get local sensor data
void LocalInterface::GetLocalData(void)
{
  LocalSensors.ble_thsensor[0].valid = false;
  LocalSensors.i2c_thpsensor[0].valid = false;
  LocalSensors.i2c_co2sensor.valid = false;

#if defined(SCD4X_EN) || defined(BME280_EN)
  TwoWire myWire = TwoWire(0);
  myWire.begin(I2C_SDA, I2C_SCL, 100000);
#endif

#ifdef SCD4X_EN
  SensirionI2cScd4x scd4x;

  uint16_t error;
  char errorMessage[256];

  scd4x.begin(myWire, SCD41_I2C_ADDR_62);

  // stop potential previously started measurement
  error = scd4x.stopPeriodicMeasurement();
  if (error)
  {
    errorToString(error, errorMessage, 256);
    log_e("Error trying to execute stopPeriodicMeasurement(): %s", errorMessage);
  }

  // Start Measurement
  error = scd4x.measureSingleShot();
  if (error)
  {
    errorToString(error, errorMessage, 256);
    log_e("Error trying to execute measureSingleShot(): %s", errorMessage);
  }

  log_d("First measurement takes ~5 sec...");
#endif

#ifdef MITHERMOMETER_EN
  // Setup BLE Temperature/Humidity Sensors
  ATC_MiThermometer miThermometer(knownBLEAddresses); //!< Mijia Bluetooth Low Energy Thermo-/Hygrometer
  miThermometer.begin();

  // Set sensor data invalid
  miThermometer.resetData();

  // Get sensor data - run BLE scan for <bleScanTime>
  miThermometer.getData(bleScanTime);

  if (miThermometer.data[0].valid)
  {
    LocalSensors.ble_thsensor[0].valid = true;
    LocalSensors.ble_thsensor[0].temperature = miThermometer.data[0].temperature / 100.0;
    LocalSensors.ble_thsensor[0].humidity = miThermometer.data[0].humidity / 100.0;
    LocalSensors.ble_thsensor[0].batt_level = miThermometer.data[0].batt_level;
  }
  miThermometer.clearScanResults();
#endif

 // BLE Temperature/Humidity Sensors
#if defined(MITHERMOMETER_EN)
  float div = 100.0;
#elif defined(THEENGSDECODER_EN)
  float div = 1.0;
#endif

#ifdef THEENGSDECODER_EN
  bleSensors = BleSensors(KNOWN_BLE_ADDRESSES);
  if (bleSensors.data.size() > 0)
  {
    bleSensors.getData(bleScanTime, bleScanMode);
  }

  if (bleSensors.data[0].valid)
  {
    LocalSensors.ble_thsensor[0].valid = true;
    LocalSensors.ble_thsensor[0].temperature = bleSensors.data[0].temperature / div;
    LocalSensors.ble_thsensor[0].humidity = bleSensors.data[0].humidity / div;
    LocalSensors.ble_thsensor[0].batt_level = bleSensors.data[0].batt_level;
    LocalSensors.ble_thsensor[0].rssi = bleSensors.data[0].rssi;
  }
  bleSensors.clearScanResults();

#endif

  if (LocalSensors.ble_thsensor[0].valid)
  {
    log_d("Outdoor Air Temp.:   % 3.1f °C", LocalSensors.ble_thsensor[0].temperature);
    log_d("Outdoor Humidity:     %3.1f %%", LocalSensors.ble_thsensor[0].humidity);
    log_d("Outdoor Sensor Batt:    %d %%", LocalSensors.ble_thsensor[0].batt_level);
  }
  else
  {
    log_d("Outdoor Air Temp.:    --.- °C");
    log_d("Outdoor Humidity:     --   %%");
    log_d("Outdoor Sensor Batt:  --   %%");
  }

#ifdef BME280_EN
  pocketBME280 bme280;
  bme280.setAddress(0x77);
  log_v("BME280: start");
  if (bme280.begin(myWire))
  {
    LocalSensors.i2c_thpsensor[0].valid = true;
    bme280.startMeasurement();
    while (!bme280.isMeasuring())
    {
      log_v("BME280: Waiting for Measurement to start");
      delay(1);
    }
    while (bme280.isMeasuring())
    {
      log_v("BME280: Measurement in progress");
      delay(1);
    }
    LocalSensors.i2c_thpsensor[0].temperature = bme280.getTemperature() / 100.0;
    LocalSensors.i2c_thpsensor[0].pressure = bme280.getPressure() / 100.0;
    LocalSensors.i2c_thpsensor[0].humidity = bme280.getHumidity() / 1024.0;
    log_d("Indoor Temperature: %.1f °C", LocalSensors.i2c_thpsensor[0].temperature);
    log_d("Indoor Pressure: %.0f hPa", LocalSensors.i2c_thpsensor[0].pressure);
    log_d("Indoor Humidity: %.0f %%rH", LocalSensors.i2c_thpsensor[0].humidity);
  }
  else
  {
    log_d("Indoor Temperature: --.- °C");
    log_d("Indoor Pressure:    --   hPa");
    log_d("Indoor Humidity:    --  %%rH");
  }
#endif

#ifdef SCD4X_EN
  if (LocalSensors.i2c_thpsensor[0].valid)
  {
    error = scd4x.setAmbientPressure((uint16_t)LocalSensors.i2c_thpsensor[0].pressure);
    if (error)
    {
      errorToString(error, errorMessage, 256);
      log_e("Error trying to execute setAmbientPressure(): %s", errorMessage);
    }
  }
  // Read Measurement
  bool isDataReady = false;

  for (int i = 0; i < 50; i++)
  {
    error = scd4x.getDataReadyStatus(isDataReady);
    if (error)
    {
      errorToString(error, errorMessage, 256);
      log_e("Error trying to execute getDataReadyFlag(): %s", errorMessage);
    }
    if (error || isDataReady)
    {
      break;
    }
    delay(100);
  }

  if (isDataReady && !error)
  {
    error = scd4x.readMeasurement(LocalSensors.i2c_co2sensor.co2, LocalSensors.i2c_co2sensor.temperature, LocalSensors.i2c_co2sensor.humidity);
    if (error)
    {
      errorToString(error, errorMessage, 256);
      log_e("Error trying to execute readMeasurement(): %s", errorMessage);
    }
    else if (LocalSensors.i2c_co2sensor.co2 == 0)
    {
      log_e("Invalid sample detected, skipping.");
    }
    else
    {
      log_d("SCD4x CO2: %d ppm", LocalSensors.i2c_co2sensor.co2);
      log_d("SCD4x Temperature: %4.1f °C", LocalSensors.i2c_co2sensor.temperature);
      log_d("SCD4x Humidity: %3.1f %%rH", LocalSensors.i2c_co2sensor.humidity);
      LocalSensors.i2c_co2sensor.valid = true;
    }
  }
  scd4x.powerDown();
#endif
}
