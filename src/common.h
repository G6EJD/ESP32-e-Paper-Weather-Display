#ifndef COMMON_H_
#define COMMON_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "forecast_record.h"
#include "common_functions.h"
#include "config.h"

#if defined(USE_HTTPS)
static const char owm_certificate[] PROGMEM = OWM_CERTIFICATE;
#endif

//#########################################################################################
void Convert_Readings_to_Imperial() {
  WxConditions[0].Pressure = hPa_to_inHg(WxConditions[0].Pressure);
  WxForecast[1].Rainfall   = mm_to_inches(WxForecast[1].Rainfall);
  WxForecast[1].Snowfall   = mm_to_inches(WxForecast[1].Snowfall);
}

//#########################################################################################
// Problems with stucturing JSON decodes, see here: https://arduinojson.org/assistant/
bool DecodeWeather(WiFiClient& json, String Type) {
  log_d("Creating JSON object");
  // allocate the JsonDocument
  JsonDocument doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, json);
  // Test if parsing succeeds.
  if (error) {
    log_d("deserializeJson() failed: %s", error.f_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  log_d("Decoding %s data", Type.c_str());
  if (Type == "weather") {
    WxConditions[0].lon         = root["coord"]["lon"].as<float>();                    log_d(" Lon: %f", WxConditions[0].lon);
    WxConditions[0].lat         = root["coord"]["lat"].as<float>();                    log_d(" Lat: %f", WxConditions[0].lat);
    WxConditions[0].Main0       = root["weather"][0]["main"].as<const char*>();        log_d("Main: %s", WxConditions[0].Main0);
    WxConditions[0].Forecast0   = root["weather"][0]["description"].as<const char*>(); log_d("For0: %s", WxConditions[0].Forecast0);
    WxConditions[0].Forecast1   = root["weather"][1]["description"].as<const char*>(); log_d("For1: %s", WxConditions[0].Forecast1);
    WxConditions[0].Forecast2   = root["weather"][2]["description"].as<const char*>(); log_d("For2: %s", WxConditions[0].Forecast2);
    WxConditions[0].Icon        = root["weather"][0]["icon"].as<const char*>();        log_d("Icon: %s", WxConditions[0].Icon);
    WxConditions[0].Temperature = root["main"]["temp"].as<float>();                    log_d("Temp: %f", WxConditions[0].Temperature);
    WxConditions[0].Pressure    = root["main"]["pressure"].as<float>();                log_d("Pres: %f", WxConditions[0].Pressure);
    WxConditions[0].Humidity    = root["main"]["humidity"].as<float>();                log_d("Humi: %f", WxConditions[0].Humidity);
    WxConditions[0].Low         = root["main"]["temp_min"].as<float>();                log_d("TLow: %f", WxConditions[0].Low);
    WxConditions[0].High        = root["main"]["temp_max"].as<float>();                log_d("THig: %f", WxConditions[0].High);
    WxConditions[0].Windspeed   = root["wind"]["speed"].as<float>();                   log_d("WSpd: %f", WxConditions[0].Windspeed);
    WxConditions[0].Winddir     = root["wind"]["deg"].as<float>();                     log_d("WDir: %f", WxConditions[0].Winddir);
    WxConditions[0].Cloudcover  = root["clouds"]["all"].as<int>();                     log_d("CCov: %d", WxConditions[0].Cloudcover); // in % of cloud cover
    WxConditions[0].Visibility  = root["visibility"].as<int>();                        log_d("Visi: %d", WxConditions[0].Visibility); // in metres
    WxConditions[0].Rainfall    = root["rain"]["1h"].as<float>();                      log_d("Rain: %f", WxConditions[0].Rainfall);
    WxConditions[0].Snowfall    = root["snow"]["1h"].as<float>();                      log_d("Snow: %f", WxConditions[0].Snowfall);
    WxConditions[0].Country     = root["sys"]["country"].as<const char*>();            log_d("Ctry: %s", WxConditions[0].Country);
    WxConditions[0].Sunrise     = root["sys"]["sunrise"].as<int>();                    log_d("SRis: %d", WxConditions[0].Sunrise);
    WxConditions[0].Sunset      = root["sys"]["sunset"].as<int>();                     log_d("SSet: %d", WxConditions[0].Sunset);
    WxConditions[0].Timezone    = root["timezone"].as<int>();                          log_d("TZon: %d", WxConditions[0].Timezone); 
  }
  if (Type == "forecast") {
    //log_d(json);
    log_d("Receiving forecast - "); //------------------------------------------------
    JsonArray list                    = root["list"];
    for (byte r = 0; r < max_readings; r++) {
      log_d("Period-%d--------------", r);
      WxForecast[r].Dt                = list[r]["dt"].as<int>();                                log_d("DTim: %d", WxForecast[r].Dt);
      WxForecast[r].Temperature       = list[r]["main"]["temp"].as<float>();                    log_d("Temp: %f", WxForecast[r].Temperature);
      WxForecast[r].Low               = list[r]["main"]["temp_min"].as<float>();                log_d("TLow: %f", WxForecast[r].Low);
      WxForecast[r].High              = list[r]["main"]["temp_max"].as<float>();                log_d("THig: %f", WxForecast[r].High);
      WxForecast[r].Pressure          = list[r]["main"]["pressure"].as<float>();                log_d("Pres: %f", WxForecast[r].Pressure);
      WxForecast[r].Humidity          = list[r]["main"]["humidity"].as<float>();                log_d("Humi: %f", WxForecast[r].Humidity);
      WxForecast[r].Forecast0         = list[r]["weather"][0]["main"].as<const char*>();        log_d("For0: %s", WxForecast[r].Forecast0);
      WxForecast[r].Forecast1         = list[r]["weather"][1]["main"].as<const char*>();        log_d("For1: %s", WxForecast[r].Forecast1);
      WxForecast[r].Forecast2         = list[r]["weather"][2]["main"].as<const char*>();        log_d("For2: %s", WxForecast[r].Forecast2);
      WxForecast[r].Icon              = list[r]["weather"][0]["icon"].as<const char*>();        log_d("Icon: %s", WxForecast[r].Icon);
      WxForecast[r].Description       = list[r]["weather"][0]["description"].as<const char*>(); log_d("Desc: %s", WxForecast[r].Description);
      WxForecast[r].Cloudcover        = list[r]["clouds"]["all"].as<int>();                     log_d("CCov: %d", WxForecast[r].Cloudcover); // in % of cloud cover
      WxForecast[r].Windspeed         = list[r]["wind"]["speed"].as<float>();                   log_d("WSpd: %f", WxForecast[r].Windspeed);
      WxForecast[r].Winddir           = list[r]["wind"]["deg"].as<float>();                     log_d("WDir: %f", WxForecast[r].Winddir);
      WxForecast[r].Rainfall          = list[r]["rain"]["3h"].as<float>();                      log_d("Rain: %f", WxForecast[r].Rainfall);
      WxForecast[r].Snowfall          = list[r]["snow"]["3h"].as<float>();                      log_d("Snow: %f", WxForecast[r].Snowfall);
      WxForecast[r].Pop               = list[r]["pop"].as<float>();                             log_d("Pop:  %f", WxForecast[r].Pop);
      WxForecast[r].Period            = list[r]["dt_txt"].as<const char*>();                    log_d("Peri: %s", WxForecast[r].Period);
    }
    //------------------------------------------
    float pressure_trend = WxForecast[2].Pressure - WxForecast[0].Pressure; // Measure pressure slope between ~now and later
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0; // Remove any small variations less than 0.1
    WxConditions[0].Trend = "0";
    if (pressure_trend > 0)  WxConditions[0].Trend = "+";
    if (pressure_trend < 0)  WxConditions[0].Trend = "-";
    if (pressure_trend == 0) WxConditions[0].Trend = "0";

    if (Units == "I") Convert_Readings_to_Imperial();
  }
  return true;
}
//#########################################################################################
String ConvertUnixTime(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  time_t tm = unix_time;
  struct tm *now_tm = gmtime(&tm);
  char output[40];
  if (Units == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
  }
  else {
    strftime(output, sizeof(output), "%I:%M%P %m/%d/%y", now_tm);
  }
  return output;
}
//#########################################################################################
bool obtain_wx_data(const String& RequestType) {
#if defined(USE_HTTPS)
  NetworkClientSecure client;
  client.setCACert(owm_certificate);
#else
  WiFiClient client;
#endif
  
  const String units = (Units == "M" ? "metric" : "imperial");
  client.stop(); // close connection before sending a new request
  HTTPClient http;
  String uri = "/data/2.5/" + RequestType + "?q=" + City + "," + Country + "&APPID=" + apikey + "&mode=json&units=" + units + "&lang=" + Language;
  if(RequestType != "weather")
  {
    uri += "&cnt=" + String(max_readings);
  }
#if defined(USE_HTTPS)
  http.begin(client, String("https://") + server + uri);
#else
  http.begin(client, server, 80, uri);
#endif
  int httpCode = http.GET();
  if(httpCode == HTTP_CODE_OK) {
    if (!DecodeWeather(http.getStream(), RequestType)) return false;
    client.stop();
    http.end();
    return true;
  }
  else
  {
    log_i("HTTP connection failed, error: %s\n", http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
  http.end();
  return true;
}
#endif /* ifndef COMMON_H_ */
