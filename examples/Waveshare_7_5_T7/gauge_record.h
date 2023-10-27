#ifndef GAUGE_RECORD_H_
#define GAUGE_RECORD_H_

#include <Arduino.h>

typedef struct { // For measured river levels. EA gauges at first instance
  String   Timestamp;
  float    Waterlevel;
} Gauge_record_type;

#endif /* ifndef GAUGE_RECORD_H_ */
