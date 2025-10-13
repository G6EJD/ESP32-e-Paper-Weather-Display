//

#include <Arduino.h>

typedef struct { // For current Day and Day 1, 2, 3, etc
  String Time;
  float  High;
  float  Low;
} HL_record_type;

HL_record_type  HLReadings[max_readings];

typedef struct { // For current Day and Day 1, 2, 3, etc
  int      Dt;
  String   Period;
  String   Icon;
  String   Trend;
  String   Main0;
  String   Forecast0;
  String   Forecast1;
  String   Forecast2;
  String   Description;
  String   Time;
  String   Country;
  float    lat;
  float    lon;
  float    Temperature;
  float    FeelsLike;
  float    Humidity;
  float    DewPoint;
  float    High;
  float    Low;
  float    Winddir;
  float    Windspeed;
  float    Rainfall;
  float    Snowfall;
  float    Pressure;
  int      Cloudcover;
  int      Visibility;
  int      Sunrise;
  int      Sunset;
  int      Moonrise;
  int      Moonset;
  int      Timezone;
  float    UVI;
  float    PoP;
} Forecast_record_type;

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];
Forecast_record_type  Daily[8];

bool ReceiveOneCallWeather(WiFiClient& client, bool print);
bool DecodeOneCallWeather(WiFiClient& json, bool print);
void Convert_Readings_to_Imperial();
String ConvertUnixTime(int unix_time);
float mm_to_inches(float value_mm);
float hPa_to_inHg(float value_hPa);
int JulianDate(int d, int m, int y);
float SumOfPrecip(float DataArray[], int readings);
String TitleCase(String text);
double NormalizedMoonPhase(int d, int m, int y);

//#########################################################################################
void Convert_Readings_to_Imperial() {
  WxConditions[0].Pressure = hPa_to_inHg(WxConditions[0].Pressure);
  WxForecast[1].Rainfall   = mm_to_inches(WxForecast[1].Rainfall);
  WxForecast[1].Snowfall   = mm_to_inches(WxForecast[1].Snowfall);
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
// Test call: http://api.openweathermap.org/data/3.0/onecall?lat=33&lon=-112&APPID=1a838280c1f7a40c3f8a5e5bc573e22d&mode=json&units=metric&lang=US&exclude=minutely
bool ReceiveOneCallWeather(WiFiClient& client, bool print) {
  Serial.println("Rx weather data...");
  const String units = (Units == "M" ? "metric" : "imperial");
  client.stop(); // close connection before sending a new request
  HTTPClient http;    
  // Update for API 3.0 June '24
  String uri = "/data/3.0/onecall?lat=" + LAT + "&lon=" + LON + "&appid=" + apikey + "&mode=json&units=" + units + "&lang=" + Language + "&exclude=minutely";
  http.begin(client, server, 80, uri);
  int httpCode = http.GET();
  if(httpCode == HTTP_CODE_OK) {
    if (!DecodeOneCallWeather(http.getStream(), print)) return false;
    client.stop();
    http.end();
    return true;
  }
  else
  {
    Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
  http.end();
  return true;
}
//#######################################################################################
bool DecodeOneCallWeather(WiFiClient& json, bool print) {
  if (print) Serial.println("Decoding Wx Data...");
  JsonDocument doc;                                        // allocate the JsonDocument
  DeserializationError error = deserializeJson(doc, json); // Deserialize the JSON document
  if (error) {                                             // Test if parsing succeeds.
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  const char* TempString;
  Serial.println("\nDecoding data...");
  if (print) Serial.println("Displaying CURRENT conditions..."); // Needed for the main display items
  WxConditions[0].Timezone    = doc["timezone_offset"];      if (print) Serial.println("TZon: " + String(WxConditions[0].Timezone));
  JsonObject current = doc["current"];
  JsonObject current_weather_0 = current["weather"][0];
  int weather_id = current_weather_0["id"]; // 800
  const char* main_weather = current_weather_0["main"]; // "Clear"
  const char* weather = current_weather_0["description"]; // "Clear Skies"
  WxConditions[0].Description = String(weather);              if (print) Serial.println("Fore: " + String(weather));
  const char* current_icon = current_weather_0["icon"];
  WxConditions[0].Icon        = current_icon;                 if (print) Serial.println("Icon: " + String(WxConditions[0].Icon));
  int sunriseL =  int(WxConditions[0].Timezone) + int(current["sunrise"]);
  WxConditions[0].Sunrise     = current["sunrise"];           if (print) Serial.println("SRis: " + String(WxConditions[0].Sunrise) + " " + ConvertUnixTime(sunriseL));
  int sunsetL  =  int(WxConditions[0].Timezone) + int(current["sunset"]);
  WxConditions[0].Sunset      = current["sunset"];            if (print) Serial.println("SSet: " + String(WxConditions[0].Sunset)  + " " + ConvertUnixTime(sunsetL));
  WxConditions[0].Temperature = current["temp"];              if (print) Serial.println("Temp: " + String(WxConditions[0].Temperature));
  WxConditions[0].FeelsLike   = current["feels_like"];        if (print) Serial.println("FLik: " + String(WxConditions[0].FeelsLike));
  WxConditions[0].Pressure    = current["pressure"];          if (print) Serial.println("Pres: " + String(WxConditions[0].Pressure));
  WxConditions[0].Humidity    = current["humidity"];          if (print) Serial.println("Humi: " + String(WxConditions[0].Humidity));
  WxConditions[0].DewPoint    = current["dew_point"];         if (print) Serial.println("DewP: " + String(WxConditions[0].DewPoint));
  WxConditions[0].UVI         = current["uvi"];               if (print) Serial.println("UVin: " + String(WxConditions[0].UVI));
  WxConditions[0].Cloudcover  = current["clouds"];            if (print) Serial.println("CCov: " + String(WxConditions[0].Cloudcover));
  WxConditions[0].Visibility  = current["visibility"];        if (print) Serial.println("Visi: " + String(WxConditions[0].Visibility));
  WxConditions[0].Windspeed   = current["wind_speed"];        if (print) Serial.println("WSpd: " + String(WxConditions[0].Windspeed));
  WxConditions[0].Winddir     = current["wind_deg"];          if (print) Serial.println("WDir: " + String(WxConditions[0].Winddir));

  JsonArray hourlyArray = doc["hourly"];

  if (print) Serial.println("\nDisplaying 48-hrs of HOURLY data..."); // Needed for the graphs
  for (int r = 0; r < max_readings; r++) {
    JsonObject hourly = hourlyArray[r];
    if (print) Serial.println("Day (Hour)-" + String(r) + " --------------");
    WxForecast[r].Dt          = hourly["dt"];                 if (print) Serial.println(ConvertUnixTime(WxForecast[r].Dt));
    WxForecast[r].Temperature = hourly["temp"];               if (print) Serial.println("Temp: " + String(WxForecast[r].Temperature));
    WxForecast[r].FeelsLike   = hourly["feels_like"];         if (print) Serial.println("FLik: " + String(WxForecast[r].FeelsLike));
    WxForecast[r].Pressure    = hourly["pressure"];           if (print) Serial.println("Pres: " + String(WxForecast[r].Pressure));
    WxForecast[r].Humidity    = hourly["humidity"];           if (print) Serial.println("Humi: " + String(WxForecast[r].Humidity));
    WxForecast[r].DewPoint    = hourly["dew_point"];          if (print) Serial.println("DewP: " + String(WxForecast[r].DewPoint));
    WxForecast[r].Rainfall    = hourly["rain"]["1h"];         if (print) Serial.println("Rain: " + String(WxForecast[r].Rainfall));
    WxForecast[r].Snowfall    = hourly["snow"]["1h"];         if (print) Serial.println("Snow: " + String(WxForecast[r].Snowfall));
    WxForecast[r].Rainfall    = hourly["rain"]["1h"];         if (print) Serial.println("Rain: " + String(WxForecast[r].Rainfall));
    WxForecast[r].Snowfall    = hourly["snow"]["1h"];         if (print) Serial.println("Snow: " + String(WxForecast[r].Snowfall));
    JsonObject hourly_weather = hourly["weather"][0];
    WxForecast[r].Icon        = hourly["weather"][0]["icon"].as<const char*>(); if (print) Serial.println("Icon: " + String(WxForecast[r].Icon));
  }

  JsonArray daily = doc["daily"];
  if (print) Serial.println("\nDisplaying DAILY Data --------------"); // Neded for the 7-day forecast section
  for (int r = 0; r < 8; r++) { // Maximum of 8-days!
    if (print) Serial.println("\nData for DAY - " + String(r) + " --------------");
    JsonObject daily_values = daily[r];
    Daily[r].Dt          = daily_values["dt"];                                   if (print) Serial.println(ConvertUnixTime(Daily[r].Dt));
    Daily[r].Description = daily_values["summary"].as<const char*>();            if (print) Serial.println("Summary: " + Daily[r].Description);
    Daily[r].Temperature = daily_values["temp"]["day"];                          if (print) Serial.println("Temp   : " + String(Daily[r].Temperature));
    Daily[r].High        = daily_values["temp"]["max"];                          if (print) Serial.println("High   : " + String(Daily[r].High));
    Daily[r].Low         = daily_values["temp"]["min"];                          if (print) Serial.println("Low    : " + String(Daily[r].Low));
    Daily[r].Humidity    = daily_values["humidity"];                             if (print) Serial.println("Humi   : " + String(Daily[r].Humidity));
    Daily[r].PoP         = daily_values["pop"];                                  if (print) Serial.println("PoP    : " + String(Daily[r].PoP*100, 0) + "%");
    Daily[r].UVI         = daily_values["uvi"];                                  if (print) Serial.println("UVI    : " + String(Daily[r].UVI, 1));
    Daily[r].Rainfall    = daily_values["rain"];                                 if (print) Serial.println("Rain   : " + String(Daily[r].Rainfall));
    Daily[r].Snowfall    = daily_values["snow"];                                 if (print) Serial.println("Snow   : " + String(Daily[r].Snowfall));
    Daily[r].Icon        = daily_values["weather"][0]["icon"].as<const char*>(); if (print) Serial.println("Icon   : " + String(Daily[r].Icon));
  }
  //------------------------------------------
  float pressure_trend = WxForecast[0].Pressure - WxForecast[2].Pressure; // Measure pressure slope between ~now and later
  pressure_trend = ((int)(pressure_trend * 10)) / 10.0; // Remove any small variations of less than 0.1
  WxConditions[0].Trend = "=";
  if (pressure_trend > 0)  WxConditions[0].Trend = "+";
  if (pressure_trend < 0)  WxConditions[0].Trend = "-";
  if (pressure_trend == 0) WxConditions[0].Trend = "0";
  if (Units == "I") Convert_Readings_to_Imperial();
  return true;
}

float mm_to_inches(float value_mm){
  return 0.0393701 * value_mm;
}

float hPa_to_inHg(float value_hPa){
  return 0.02953 * value_hPa;
}

int JulianDate(int d, int m, int y) {
  int mm, yy, k1, k2, k3, j;
  yy = y - (int)((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12) mm = mm - 12;
  k1 = (int)(365.25 * (yy + 4712));
  k2 = (int)(30.6001 * mm + 0.5);
  k3 = (int)((int)((yy / 100) + 49) * 0.75) - 38;
  // 'j' for dates in Julian calendar:
  j = k1 + k2 + d + 59 + 1;
  if (j > 2299160) j = j - k3; // 'j' is the Julian date at 12h UT (Universal Time) For Gregorian calendar:
  return j;
}

float SumOfPrecip(float DataArray[], int readings) {
  float sum = 0;
  for (int i = 0; i < readings; i++) {
    sum += DataArray[i];
  }
  return sum;
}

String TitleCase(String text){
  if (text.length() > 0) {
    String temp_text = text.substring(0,1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1); // Title-case the string
  }
  else return text;
}

double NormalizedMoonPhase(int d, int m, int y) {
  int j = JulianDate(d, m, y);
  //Calculate the approximate phase of the moon
  double Phase = (j + 4.867) / 29.53059;
  return (Phase - (int) Phase);
}

