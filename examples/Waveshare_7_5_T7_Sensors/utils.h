///////////////////////////////////////////////////////////////////////////////
// utils.h
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

#ifndef _UTILS_H
#define _UTILS_H
#include <Arduino.h>
#include <time.h>
#include "config.h"

/**
 * \brief Check if history update is due
 *
 * History is updated at an interval of <code>HIST_UPDATE_RATE</code> synchronized
 * to the past full hour with a tolerance of <code>HIST_UPDATE_TOL</code>.
 *
 * \return true if update is due, otherwise false
 */
bool HistoryUpdateDue(void);

void convertUtcTimestamp(String time_str_utc, struct tm *ti_local, int tz_offset);

/**
 * \brief Get time from NTP server and initialize/update RTC
 *
 * The global constants gmtOffset_sec, daylightOffset_sec and ntpServer are used.
 *
 * \return true if RTC initialization was successfully, false otherwise
 */
boolean SetupTime();

/**
 * \brief Get local time (from RTC) and update global variables
 *
 * The global variables CurrentHour, CurrentMin, CurrentSec, CurrentDay
 * and day_output/time_output are updated.
 * date_output contains the localized date string,
 * time_output contains the localized text TXT_UPDATED with the time string.
 *
 * \return true if RTC time was valid, false otherwise
 */
boolean UpdateLocalTime();

/**
 * \brief Print localized time to variables
 *
 * \param timeinfo      date and time data structure
 * \param date_output   test output buffer for localized date
 * \param time_output   text output buffer for localized time
 * \param max_size      maximum text buffer size
 *
 *
 */
void printTime(struct tm &timeinfo, char *date_output, char *time_output, int max_size, const String label);

#endif