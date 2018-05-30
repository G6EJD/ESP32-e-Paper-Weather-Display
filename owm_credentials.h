// Change to your WiFi credentials
const char* ssid1     = "your-SSID";
const char* password1 = "your-PASSWORD";
const char* ssid2     = "ssid-2";  // e.g. of a network extender
const char* password2 = "password-2";

// Use your own API key by signing up for a free developer account at https://openweathermap.org/
String apikey       = "yoir_OWM-API-key";                   // See: https://openweathermap.org/
const char server[] = "api.openweathermap.org";
//http://api.openweathermap.org/data/2.5/forecast?q=Melksham,UK&APPID=yourAPIkey&mode=json&units=metric&cnt=40
//http://api.openweathermap.org/data/2.5/weather?q=Melksham,UK&APPID=yourAPIkey&mode=json&units=metric&cnt=1
// Set your location according to OWM locations
String City          = "Melksham";                      // Your home city See: http://bulk.openweathermap.org/sample/
String Country       = "UK";                            // Your country  
String Hemisphere    = "north";                         // or "south"  
String Units         = "M";                             // Use M for Metric or I for Imperial 
const char* Timezone = "GMT0BST,M3.5.0/01,M10.5.0/02";  // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
// Example time zones
//const char* Timezone = "MET-1METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
//const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";       // Central Europe
//const char* Timezone = "EST-1METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
//const char* Timezone = "EST5EDT,M3.2.0,M11.1.0";           // EST USA  
//const char* Timezone = "CST6CDT,M3.2.0,M11.1.0";           // CST USA
//const char* Timezone = "MST7MDT,M4.1.0,M10.5.0";           // MST USA
//const char* Timezone = "NZST-12NZDT,M9.5.0,M4.1.0/3";      // Auckland
//const char* Timezone = "EET-2EEST,M3.5.5/0,M10.5.5/0";     // Asia
//const char* Timezone = "ACST-9:30ACDT,M10.1.0,M4.1.0/3":   // Australia
