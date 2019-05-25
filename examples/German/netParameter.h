// Change to your WiFi credentials
const char* ssid    = "Your SSID";
const char* pwd 	= "Your password";

// *** Open Weather Maps **************************************************
// Use your own API key by signing up for a free developer account at https://openweathermap.org/
String apikey = "Your OWM Key "; 						// See: https://openweathermap.org/
const char server[] = "api.openweathermap.org";				// URL OpenWetherMap - siehe oben
String City          = "Yout home City";                    // Your home city See: http://bulk.openweathermap.org/sample/
String Country       = "DE";                                // Your country  
String Language      = "DE";                                // NOTE: Only the weather description (not used) is translated by OWM
String Hemisphere    = "north";                             // or "south"  
String Units         = "M";                                 // Use M for Metric or I for Imperial 
const char* Timezone = "WEST-1DWEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"; 	// Western European Time
   