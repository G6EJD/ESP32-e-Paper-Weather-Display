// Define if you want a instant update on startup
const bool DebugDisplayUpdate = false;

// Change to your WiFi credentials
const char* ssid     = "your_SSID";     // WiFi SSID to connect to
const char* password = "your_PASSWORD"; // WiFi password needed for the SSID

// Use your own API key by signing up for a free developer account at https://openweathermap.org/
String apikey       = "your_API_key";                      // See: https://openweathermap.org/  // It's free to get an API key, but don't take more than 60 readings/minute!
const char server[] = "api.openweathermap.org";
const char server_ea[] = "environment.data.gov.uk";
//http://api.openweathermap.org/data/2.5/weather?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=1   // Example API call for weather data
//http://api.openweathermap.org/data/2.5/forecast?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=40 // Example API call for forecast data
//Set your location according to OWM locations

String City             = "MELKSHAM";                      // Your home city See: http://bulk.openweathermap.org/sample/
String Country          = "GB";                            // Your _ISO-3166-1_two-letter_country_code country code, on OWM find your nearest city and the country code is displayed
                                                           // https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes
String Language         = "EN";                            // NOTE: Only the weather description is translated by OWM
                                                           // Examples: Arabic (AR) Czech (CZ) English (EN) Greek (EL) Persian(Farsi) (FA) Galician (GL) Hungarian (HU) Japanese (JA)
                                                           // Korean (KR) Latvian (LA) Lithuanian (LT) Macedonian (MK) Slovak (SK) Slovenian (SL) Vietnamese (VI)
String Hemisphere       = "north";                         // or "south"  
String Units            = "M";                             // Use 'M' for Metric or I for Imperial 
const char* Timezone    = "GMT0BST,M3.5.0/01,M10.5.0/02";  // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
                                                           // See below for examples
const char* ntpServer   = "0.uk.pool.ntp.org";             // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
                                                           // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
                                                           // EU "0.europe.pool.ntp.org"
                                                           // US "0.north-america.pool.ntp.org"
                                                           // See: https://www.ntppool.org/en/                                                           
int   gmtOffset_sec     = 0;    // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int  daylightOffset_sec = 3600; // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset

// Set Environment Agency gauge locations
String gauge_id_lhs = "E70124";                              // EA gauge for Liverpool: https://environment.data.gov.uk/flood-monitoring/id/stations/E70124
String gauge_label_lhs = "Liverpool";
//String gauge_id_rhs = "3319SE";                            // EA gauge for Southampton: https://environment.data.gov.uk/flood-monitoring/id/stations/3319SE
//String gauge_label_rhs = "Southampton";

// Example time zones
//const char* Timezone = "MET-1METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
//const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";       // Central Europe
//const char* Timezone = "EST-2METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
//const char* Timezone = "EST5EDT,M3.2.0,M11.1.0";           // EST USA  
//const char* Timezone = "CST6CDT,M3.2.0,M11.1.0";           // CST USA
//const char* Timezone = "MST7MDT,M4.1.0,M10.5.0";           // MST USA
//const char* Timezone = "NZST-12NZDT,M9.5.0,M4.1.0/3";      // Auckland
//const char* Timezone = "EET-2EEST,M3.5.5/0,M10.5.5/0";     // Asia
//const char* Timezone = "ACST-9:30ACDT,M10.1.0,M4.1.0/3":   // Australia
