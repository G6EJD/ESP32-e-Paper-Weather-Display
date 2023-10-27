#ifndef RIVER_RECORD_H_
#define RIVER_RECORD_H_

#include <Arduino.h>

typedef struct { // For measured river levels. EA gauges at first instance
  String   Timestamp;
  float    Waterlevel;
} River_record_type;

#endif /* ifndef RIVER_RECORD_H_ */
