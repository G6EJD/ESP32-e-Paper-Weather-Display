// Change to your WiFi credentials
const char* ssid1     = "yourSSID";
const char* password1 = "your_PASSWORD";
const char* ssid2     = "ssid-2";  // e.g. of a network extender
const char* password2 = "password-2";

// Use your own API key by signing up for a free developer account at https://openweathermap.org/
String apikey       = "your-Open-Weather_API_key";            // See: https://openweathermap.org/
const char server[] = "api.openweathermap.org";
//http://api.openweathermap.org/data/2.5/forecast?q=Melksham,UK&APPID=your-OWM-api-key&mode=json&units=metric&cnt=2
// Set your location according to OWM locations
String City         = "Melksham"; // Your home city
String Country      = "UK";       // Your country  
String Hemisphere   = "north";    // or "south" 
String Units        = "M";        // M for Metric or I for Imperial

