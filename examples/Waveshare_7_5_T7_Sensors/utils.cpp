///////////////////////////////////////////////////////////////////////////////
// utils.cpp
//
// Utility functions for ESP32-e-Paper-Weather-Display
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

#include "utils.h"

extern const char *weekday_D[];
extern const char *month_M[];
extern const String Language;
extern const String Units;
extern const String TXT_UPDATED;
extern String Time_str;
extern String Date_str;
extern int CurrentDay;
extern int CurrentHour;
extern int CurrentMin;
extern int CurrentSec;
extern int WiFiSignal;

// WiFi connection to multiple alternative access points
static WiFiMulti wifiMulti;

// Start WiFi connection
uint8_t StartWiFi()
{
  // Add list of wifi networks
  wifiMulti.addAP(ssid0, password0);
  wifiMulti.addAP(ssid1, password1);
  wifiMulti.addAP(ssid2, password2);
  
  if (WiFi.status() == WL_CONNECTED)
  {
    return WL_CONNECTED;
  }

  IPAddress dns(MY_DNS);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP

  uint8_t connectionStatus = wifiMulti.run();

  if (connectionStatus == WL_CONNECTED)
  {
    String ssid = WiFi.SSID();
    WiFiSignal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    log_i("WiFi connected to '%s'", ssid.c_str());
    log_i("WiFi connected at: %s", WiFi.localIP().toString().c_str());
  }
  else
  {
    log_w("WiFi connection failed!");
  }

  return connectionStatus;
}

// Disconnects WiFi and switches off WiFi to save power.
void StopWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

// Check if history update is due
bool HistoryUpdateDue(void)
{
  int mins = (CurrentHour * 60 + CurrentMin) % HIST_UPDATE_RATE;
  bool rv = (mins <= HIST_UPDATE_TOL) || (mins >= (HIST_UPDATE_RATE - HIST_UPDATE_TOL));
  return rv;
}

void convertUtcTimestamp(String time_str_utc, struct tm *ti_local, int tz_offset)
{
  struct tm received_at_utc;
  memset(&received_at_utc, 0, sizeof(struct tm));
  memset(ti_local, 0, sizeof(struct tm));

  // Read time string
  strptime(time_str_utc.c_str(), "%Y-%m-%dT%H:%M:%S%z", &received_at_utc);
  log_d("UTC: %d-%02d-%02d %02d:%02d DST: %d\n", received_at_utc.tm_year + 1900, received_at_utc.tm_mon + 1, received_at_utc.tm_mday, received_at_utc.tm_hour, received_at_utc.tm_min, received_at_utc.tm_isdst);

  // Convert time struct and calculate offset
  time_t received_at = mktime(&received_at_utc) - tz_offset;

  // Convert time value to time struct (local time; with consideration of DST)
  localtime_r(&received_at, ti_local);
  char tbuf[26];
  strftime(tbuf, 25, "%Y-%m-%d %H:%M", ti_local);
  log_d("Message received at: %s local time, DST: %d\n", tbuf, ti_local->tm_isdst);
}

// Get time from NTP server and initialize/update RTC
boolean SetupTime()
{
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTPSERVER, "pool.ntp.org"); // (gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", TIMEZONE, 1);                                                  // setenv() adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                    // Set the TZ environment variable
  delay(100);
  return UpdateLocalTime();
}

// Get local time (from RTC) and update global variables
boolean UpdateLocalTime()
{
  struct tm timeinfo;
  char time_output[32], date_output[32];
  while (!getLocalTime(&timeinfo, 10000))
  { // Wait for 10-sec for time to synchronise
    log_w("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin = timeinfo.tm_min;
  CurrentSec = timeinfo.tm_sec;
  CurrentDay = timeinfo.tm_mday;
  printTime(timeinfo, time_output, date_output, 32, TXT_UPDATED);

  Date_str = date_output;
  Time_str = time_output;
  return true;
}

// Print localized time to variables
void printTime(struct tm &timeinfo, char *date_output, char *time_output, int max_size, const String label)
{
  char update_time[30];

  // See http://www.cplusplus.com/reference/ctime/strftime/
  // Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");      // Displays: Saturday, June 24 2017 14:05:49
  if (Units == "M")
  {
    if ((Language == "CZ") || (Language == "DE") || (Language == "PL") || (Language == "NL"))
    {
      sprintf(date_output, "%s, %02u. %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900); // day_output >> So., 23. Juni 2019 <<
    }
    else
    {
      sprintf(date_output, "%s %02u-%s-%04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    }
    strftime(update_time, max_size, "%H:%M:%S", &timeinfo); // Creates: '14:05:49'
  }
  else
  {
    strftime(date_output, max_size, "%a %b-%d-%Y", &timeinfo);   // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo); // Creates: '02:05:49pm'
    
  }
  sprintf(time_output, "%s %s", label, update_time);
}