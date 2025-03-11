///////////////////////////////////////////////////////////////////////////////
// MqttInterface.cpp
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
// 20250322 Added MQTT discovery for Home Assistant, added status message
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////

#include "MqttInterface.h"

extern uint8_t StartWiFi();
extern bool HistoryUpdateDue();
extern void SaveLocalData();
extern bool TouchTriggered();
extern RTC_DATA_ATTR time_t LocalHistTStamp;

static bool mqttMessageReceived = false; //!< Flag: MQTT message has been received

#if defined(USE_SECUREWIFI) && defined(CHECK_CA_ROOT)
static const char digicert[] PROGMEM = DIGICERT;
#endif

#ifdef SIMULATE_MQTT
static const char *MqttBuf = "{\"end_device_ids\":{\"device_id\":\"eui-9876b6000011c87b\",\"application_ids\":{\"application_id\":\"flora-lora\"},\"dev_eui\":\"9876B6000011C87B\",\"join_eui\":\"0000000000000000\",\"dev_addr\":\"260BFFCA\"},\"correlation_ids\":[\"as:up:01GH0PHSCTGKZ51EB8XCBBGHQD\",\"gs:conn:01GFQX269DVXYK9W6XF8NNZWDD\",\"gs:up:host:01GFQX26AXQM4QHEAPW48E8EWH\",\"gs:uplink:01GH0PHS6A65GBAPZB92XNGYAP\",\"ns:uplink:01GH0PHS6BEPXS9Y7DMDRNK84Y\",\"rpc:/ttn.lorawan.v3.GsNs/HandleUplink:01GH0PHS6BY76SY2VPRSHNDDRH\",\"rpc:/ttn.lorawan.v3.NsAs/HandleUplink:01GH0PHSCS7D3V8ERSKF0DTJ8H\"],\"received_at\":\"2022-11-04T06:51:44.409936969Z\",\"uplink_message\":{\"session_key_id\":\"AYRBaM/qASfqUi+BQK75Gg==\",\"f_port\":1,\"frm_payload\":\"PwOOWAgACAAIBwAAYEKAC28LAw0D4U0DwAoAAAAAwMxMP8DMTD/AzEw/AAAAAAAAAAAA\",\"decoded_payload\":{\"bytes\":{\"air_temp_c\":\"9.1\",\"battery_v\":2927,\"humidity\":88,\"indoor_humidity\":77,\"indoor_temp_c\":\"9.9\",\"rain_day\":\"0.8\",\"rain_hr\":\"0.0\",\"rain_mm\":\"56.0\",\"rain_mon\":\"0.8\",\"rain_week\":\"0.8\",\"soil_moisture\":10,\"soil_temp_c\":\"9.6\",\"status\":{\"ble_ok\":true,\"res\":false,\"rtc_sync_req\":false,\"runtime_expired\":true,\"s1_batt_ok\":true,\"s1_dec_ok\":true,\"ws_batt_ok\":true,\"ws_dec_ok\":true},\"supply_v\":2944,\"water_temp_c\":\"7.8\",\"wind_avg_meter_sec\":\"0.8\",\"wind_direction_deg\":\"180.0\",\"wind_gust_meter_sec\":\"0.8\"}},\"rx_metadata\":[{\"gateway_ids\":{\"gateway_id\":\"lora-db0fc\",\"eui\":\"3135323538002400\"},\"time\":\"2022-11-04T06:51:44.027496Z\",\"timestamp\":1403655780,\"rssi\":-104,\"channel_rssi\":-104,\"snr\":8.25,\"location\":{\"latitude\":52.27640735,\"longitude\":10.54058183,\"altitude\":65,\"source\":\"SOURCE_REGISTRY\"},\"uplink_token\":\"ChgKFgoKbG9yYS1kYjBmYxIIMTUyNTgAJAAQ5KyonQUaCwiA7ZKbBhCw6tpgIKDtnYPt67cC\",\"channel_index\":4,\"received_at\":\"2022-11-04T06:51:44.182146570Z\"}],\"settings\":{\"data_rate\":{\"lora\":{\"bandwidth\":125000,\"spreading_factor\":8,\"coding_rate\":\"4/5\"}},\"frequency\":\"867300000\",\"timestamp\":1403655780,\"time\":\"2022-11-04T06:51:44.027496Z\"},\"received_at\":\"2022-11-04T06:51:44.203702153Z\",\"confirmed\":true,\"consumed_airtime\":\"0.215552s\",\"locations\":{\"user\":{\"latitude\":52.24619,\"longitude\":10.50106,\"source\":\"SOURCE_REGISTRY\"}},\"network_ids\":{\"net_id\":\"000013\",\"tenant_id\":\"ttn\",\"cluster_id\":\"eu1\",\"cluster_address\":\"eu1.cloud.thethings.network\"}}}";
#else
static char MqttBuf[MQTT_PAYLOAD_SIZE + 1]; //!< MQTT Payload Buffer
#endif

// MQTT message reception callback function
static void mqttMessageCb(String &topic, String &payload)
{
  mqttMessageReceived = true;
  log_d("Payload size: %d", payload.length());
#ifndef SIMULATE_MQTT
  strncpy(MqttBuf, payload.c_str(), payload.length());
#endif
}

// Constructor
MqttInterface::MqttInterface(MQTTClient &_MqttClient)
{
#if defined(USE_SECUREWIFI)
#ifdef CHECK_CA_ROOT
  net.setCACert(digicert);
#endif
#ifdef CHECK_PUB_KEY
  error "CHECK_PUB_KEY: not implemented"
#endif
#ifdef CHECK_FINGERPRINT
      net.setFingerprint(fp);
#endif
#if (!defined(CHECK_PUB_KEY) and !defined(CHECK_CA_ROOT) and !defined(CHECK_FINGERPRINT))
  // do not verify tls certificate
  net.setInsecure();
#endif
#endif
  MqttClient = _MqttClient;

  uint32_t id_l = ESP.getEfuseMac() & 0xFFFFFFFF;
  uint32_t id_h = (ESP.getEfuseMac() >> 32) & 0xFFFF;

  sprintf(chip_id, "%04lX%08lX", id_h, id_l);
}

// Connect to MQTT broker
bool MqttInterface::mqttConnect()
{
  log_d("Checking wifi...");
  if (StartWiFi() != WL_CONNECTED)
  {
    return false;
  }

  log_i("MQTT connecting...");
  unsigned long start = millis();

  MqttClient.begin(MQTT_HOST, MQTT_PORT, net);
  MqttClient.setOptions(MQTT_KEEPALIVE /* keepAlive [s] */, MQTT_CLEAN_SESSION /* cleanSession */, MQTT_TIMEOUT * 1000 /* timeout [ms] */);

  while (!MqttClient.connect(HOSTNAME, MQTT_USER, MQTT_PASS))
  {
    log_d(".");
    if (millis() > start + MQTT_CONNECT_TIMEOUT * 1000)
    {
      log_i("Connect timeout!");
      return false;
    }
    delay(1000);
  }
  log_i("Connected!");

  MqttClient.onMessage(mqttMessageCb);

  if (!MqttClient.subscribe(MQTT_SUB_IN))
  {
    log_i("MQTT subscription failed!");
    return false;
  }
  return true;
}

// Get data from MQTT broker
void MqttInterface::getMqttData(mqtt_sensors_t &MqttSensors)
{
  MqttSensors.valid = false;

  log_i("Waiting for MQTT message...");

  // allocate the JsonDocument
  JsonDocument doc;

  // LoRaWAN fPort
  unsigned char f_port;

  do
  {
#ifndef SIMULATE_MQTT
    unsigned long start = millis();
    int count = 0;
    while (!mqttMessageReceived)
    {
      MqttClient.loop();
      delay(10);
      if (count++ == 1000)
      {
        log_d(".");
        count = 0;
      }
      if (mqttMessageReceived)
        break;
      if (!MqttClient.connected())
      {
        mqttConnect();
      }
      if (TouchTriggered())
      {
        log_i("Touch interrupt!");
        return;
      }
      if (millis() > start + MQTT_DATA_TIMEOUT * 1000)
      {
        log_i("Timeout!");
        MqttClient.disconnect();
        return;
      }
      // During this time-consuming loop, updating local history could be due
      if (HistoryUpdateDue())
      {
        time_t now = time(NULL);
        if (now - LocalHistTStamp >= (HIST_UPDATE_RATE - HIST_UPDATE_TOL) * 60)
        {
          LocalHistTStamp = now;
          SaveLocalData();
        }
      }
    }
#else
    log_i("(Simulated MQTT incoming message)");
    MqttSensors.valid = true;
#endif

    log_i("done!");
    log_d("%s", MqttBuf);

    log_d("Creating JSON object...");

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, MqttBuf, MQTT_PAYLOAD_SIZE);

    // Test if parsing succeeds.
    if (error)
    {
      log_i("deserializeJson() failed: %s", error.c_str());
      return;
    }
    else
    {
      log_d("Done!");
    }

    MqttClient.disconnect();
    MqttSensors.valid = true;

    const char *received_at = doc["received_at"];
    if (received_at)
    {
      strncpy(MqttSensors.received_at, received_at, 30);
    }
    f_port = doc["uplink_message"]["f_port"];
    if (f_port != 1)
    {
      // Wrong f_port, retrying...
      mqttMessageReceived = false;
    }
  } while (f_port != 1);
  JsonVariant payload = doc["uplink_message"]["decoded_payload"]["bytes"];

  MqttSensors.air_temp_c = payload[WS_TEMP_C].isNull() ? INV_TEMP : payload[WS_TEMP_C];
  MqttSensors.humidity = payload[WS_HUMIDITY].isNull() ? INV_UINT8 : payload[WS_HUMIDITY];
  MqttSensors.indoor_temp_c = payload[BLE0_TEMP_C].isNull() ? INV_TEMP : payload[BLE0_TEMP_C];
  MqttSensors.indoor_humidity = payload[BLE0_HUMIDITY].isNull() ? INV_UINT8 : payload[BLE0_HUMIDITY];
  MqttSensors.battery_v = payload[A0_VOLTAGE_MV].isNull() ? INV_UINT16 : payload[A0_VOLTAGE_MV];
  MqttSensors.rain_day = payload[WS_RAIN_DAILY_MM].isNull() ? INV_FLOAT : payload[WS_RAIN_DAILY_MM];
  MqttSensors.rain_hr = payload[WS_RAIN_HOURLY_MM].isNull() ? INV_FLOAT : payload[WS_RAIN_HOURLY_MM];
  MqttSensors.rain_mm = payload[WS_RAIN_MM].isNull() ? INV_FLOAT : payload[WS_RAIN_MM];
  MqttSensors.rain_month = payload[WS_RAIN_MONTHLY_MM].isNull() ? INV_FLOAT : payload[WS_RAIN_MONTHLY_MM];
  MqttSensors.rain_week = payload[WS_RAIN_WEEKLY_MM].isNull() ? INV_FLOAT : payload[WS_RAIN_WEEKLY_MM];
  MqttSensors.soil_moisture = payload[SOIL1_MOISTURE].isNull() ? INV_UINT8 : payload[SOIL1_MOISTURE];
  MqttSensors.soil_temp_c = payload[SOIL1_TEMP_C].isNull() ? INV_TEMP : payload[SOIL1_TEMP_C];
  MqttSensors.water_temp_c = payload[OW0_TEMP_C].isNull() ? INV_TEMP : payload[OW0_TEMP_C];
  MqttSensors.wind_avg_meter_sec = payload[WS_WIND_AVG_MS].isNull() ? INV_UINT16 : payload[WS_WIND_AVG_MS];
  MqttSensors.wind_direction_deg = payload[WS_WIND_DIR_DEG].isNull() ? INV_UINT16 : payload[WS_WIND_DIR_DEG];
  MqttSensors.wind_gust_meter_sec = payload[WS_WIND_GUST_MS].isNull() ? INV_UINT16 : payload[WS_WIND_GUST_MS];

  JsonObject status = payload["status"];
  bool ble_ok = MqttSensors.indoor_temp_c != INV_TEMP && MqttSensors.indoor_humidity != INV_UINT8;
  // MqttSensors.status.ble_ok = status["ble_ok"] | ble_ok;
  MqttSensors.status.ble_ok = ble_ok;
  MqttSensors.status.ble_batt_ok = 1; // No MQTT signal available
  bool s1_dec_ok = MqttSensors.soil_temp_c != INV_TEMP && MqttSensors.soil_moisture != INV_UINT8;
  // MqttSensors.status.s1_dec_ok = status["s1_dec_ok"] | s1_dec_ok;
  MqttSensors.status.s1_dec_ok = s1_dec_ok;
  bool ws_dec_ok = MqttSensors.air_temp_c != INV_TEMP && MqttSensors.humidity != INV_UINT8 && MqttSensors.rain_mm != INV_FLOAT;
  // MqttSensors.status.ws_dec_ok = status["ws_dec_ok"] | ws_dec_ok;
  MqttSensors.status.ws_dec_ok = ws_dec_ok;

  MqttSensors.status.s1_batt_ok = status["s1_batt_ok"];
  MqttSensors.status.ws_batt_ok = status["ws_batt_ok"];

  // Sanity checks
  if (MqttSensors.humidity == 0)
  {
    MqttSensors.status.ws_dec_ok = false;
  }
  MqttSensors.rain_hr_valid = (MqttSensors.rain_hr >= 0) && (MqttSensors.rain_hr < 300);
  MqttSensors.rain_day_valid = (MqttSensors.rain_day >= 0) && (MqttSensors.rain_day < 1800);

  // If not valid, set value to zero to avoid any problems with auto-scale etc.
  if (!MqttSensors.rain_hr_valid)
  {
    MqttSensors.rain_hr = 0;
  }
  if (!MqttSensors.rain_day_valid)
  {
    MqttSensors.rain_day = 0;
  }

  log_d("ws_dec_ok:  %d", MqttSensors.status.ws_dec_ok);
  log_d("s1_dec_ok:  %d", MqttSensors.status.s1_dec_ok);
  log_d("ble_ok:  %d", MqttSensors.status.ble_ok);
  log_d("ws_batt_ok: %d", MqttSensors.status.ws_batt_ok);
  log_d("s1_batt_ok: %d", MqttSensors.status.s1_batt_ok);
  log_i("MQTT data updated: %d", MqttSensors.valid ? 1 : 0);
}

// Publish sensor data to MQTT broker
bool MqttInterface::mqttUplink(MQTTClient &MqttClient, local_sensors_t &data)
{
  char payload[21];
  char topic[41];

  log_d("Checking wifi...");
  if (StartWiFi() != WL_CONNECTED)
  {
    return false;
  }

  log_i("MQTT (publishing) connecting...");
  unsigned long start = millis();
  String mqttPubStatus = String(HOSTNAME) + "/status";

  MqttClient.begin(MQTT_HOST_P, MQTT_PORT_P, net);
  MqttClient.setOptions(MQTT_KEEPALIVE /* keepAlive [s] */, MQTT_CLEAN_SESSION /* cleanSession */, MQTT_TIMEOUT * 1000 /* timeout [ms] */);
  MqttClient.setWill(mqttPubStatus.c_str(), "dead", true /* retained */, 1 /* qos */);

  while (!MqttClient.connect(HOSTNAME, MQTT_USER_P, MQTT_PASS_P))
  {
    log_d(".");
    if (millis() > start + MQTT_CONNECT_TIMEOUT * 1000)
    {
      log_i("Connect timeout!");
      return false;
    }
    delay(1000);
  }
  log_i("Connected!");
  MqttClient.publish(mqttPubStatus.c_str(), "online");

  log_d("Publishing...");
#if defined(SCD4X_EN)
#if defined(AUTO_DISCOVERY)
  publishAutoDiscovery(MqttClient, "CO2", "carbon_dioxide", "ppm", "sdc4x", "co2");
#endif
  if (data.i2c_co2sensor.valid)
  {
    snprintf(payload, 20, "%u", data.i2c_co2sensor.co2);
    snprintf(topic, 40, "%s/sdc4x/co2", HOSTNAME);
    MqttClient.publish(topic, payload);

    snprintf(payload, 20, "%3.1f", data.i2c_co2sensor.temperature);
    snprintf(topic, 40, "%s/sdc4x/temperature", HOSTNAME);
    MqttClient.publish(topic, payload);

    snprintf(payload, 20, "%3.0f", data.i2c_co2sensor.humidity);
    snprintf(topic, 40, "%s/sdc4x/humidity", HOSTNAME);
    MqttClient.publish(topic, payload);
  }
  MqttClient.loop();
#endif

#if defined(BME280_EN)
#if defined(AUTO_DISCOVERY)
  publishAutoDiscovery(MqttClient, "Indoor Temperature", "temperature", "°C", "bme280", "temperature");
  publishAutoDiscovery(MqttClient, "Indoor Humidity", "humidity", "%", "bme280", "humidity");
  publishAutoDiscovery(MqttClient, "Indoor Pressure", "atmospheric_pressure", "hPa", "bme280", "pressure");
#endif
  if (data.i2c_thpsensor[0].valid)
  {
    snprintf(payload, 20, "%3.1f", data.i2c_thpsensor[0].temperature);
    snprintf(topic, 40, "%s/bme280/temperature", HOSTNAME);
    MqttClient.publish(topic, payload);

    snprintf(payload, 20, "%3.0f", data.i2c_thpsensor[0].humidity);
    snprintf(topic, 40, "%s/bme280/humidity", HOSTNAME);
    MqttClient.publish(topic, payload);

    snprintf(payload, 20, "%4.0f", data.i2c_thpsensor[0].pressure);
    snprintf(topic, 40, "%s/bme280/pressure", HOSTNAME);
    MqttClient.publish(topic, payload);
  }
  MqttClient.loop();
#endif

#if defined(MITHERMOMETER_EN) || defined(THEENGSDECODER_EN)
#if defined(AUTO_DISCOVERY)
  publishAutoDiscovery(MqttClient, "Outdoor Temperature", "temperature", "°C", "ble", "temperature");
  publishAutoDiscovery(MqttClient, "Outdoor Humidity", "humidity", "%", "ble", "humidity");
  publishAutoDiscovery(MqttClient, "Outdoor Sensor Battery", "battery", "%", "ble", "batt_level");
#endif
  if (data.ble_thsensor[0].valid)
  {
    snprintf(payload, 20, "%3.1f", data.ble_thsensor[0].temperature);
    snprintf(topic, 40, "%s/ble/temperature", HOSTNAME);
    MqttClient.publish(topic, payload);

    snprintf(payload, 20, "%3.0f", data.ble_thsensor[0].humidity);
    snprintf(topic, 40, "%s/ble/humidity", HOSTNAME);
    MqttClient.publish(topic, payload);

    snprintf(payload, 20, "%u", data.ble_thsensor[0].batt_level);
    snprintf(topic, 40, "%s/ble/batt_level", HOSTNAME);
    MqttClient.publish(topic, payload);
  }
  MqttClient.loop();
#endif

  for (int i = 0; i < 10; i++)
  {
    MqttClient.loop();
    delay(500);
  }
  MqttClient.publish(mqttPubStatus, "offline", true /* retained */, 0 /* qos */);
  MqttClient.loop();

  log_i("MQTT (publishing) disconnect.");
  MqttClient.disconnect();

  return true;
}

#if defined(AUTO_DISCOVERY)
// Publish auto-discovery configuration for Home Assistant
void MqttInterface::publishAutoDiscovery(MQTTClient &MqttClient, const char *sensor_name, const char *device_class, const char *unit,
                                         const char *sensor_topic, const char *value_topic)
{
  JsonDocument doc;

  doc["name"] = sensor_name;
  doc["device_class"] = device_class;
  doc["unique_id"] = String(HOSTNAME) + "_" + String(chip_id) + String("_") + String(sensor_topic) + String("_") + String(value_topic);
  doc["state_topic"] = String(HOSTNAME) + "/" + sensor_topic + "/" + value_topic;
  doc["availability_topic"] = String(HOSTNAME) + "/status";
  doc["payload_not_available"] = "dead"; // default: "offline"
  doc["unit_of_measurement"] = unit;
  doc["value_template"] = "{{ value }}";
  JsonObject device = doc["device"].to<JsonObject>();
  device["name"] = String(HOSTNAME);
  device["identifiers"] = String(HOSTNAME) + "_" + chip_id;

  char buffer[512];
  serializeJson(doc, buffer);

  String tmp_topic = String(HOSTNAME) + "_" + sensor_topic + "_" + value_topic;
  String topic = String("homeassistant/sensor/") + tmp_topic + "/config";
  log_d("Publishing auto-discovery configuration: %s: %s", topic.c_str(), buffer);
  MqttClient.publish(topic.c_str(), buffer);
}
#endif
