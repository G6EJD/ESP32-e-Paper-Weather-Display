#ifndef COMMON_FUNCTIONS_H_
#define COMMON_FUNCTIONS_H_
#include <Arduino.h>

float mm_to_inches(float value_mm);
float hPa_to_inHg(float value_hPa);
int JulianDate(int d, int m, int y);
float SumOfPrecip(float DataArray[], int readings);
String TitleCase(String text);

#endif /* ifndef COMMON_FUNCTIONS_H_ */
