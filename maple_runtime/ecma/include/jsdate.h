/*
 * Copyright (c) [2021] Futurewei Technologies Co.,Ltd.All rights reserved.
 *
 * OpenArkCompiler is licensed under the Mulan Permissive Software License v2.
 * You can use this software according to the terms and conditions of the MulanPSL - 2.0.
 * You may obtain a copy of MulanPSL - 2.0 at:
 *
 *   https://opensource.org/licenses/MulanPSL-2.0
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR
 * FIT FOR A PARTICULAR PURPOSE.
 * See the MulanPSL - 2.0 for more details.
 */

#ifndef JSDATE_H
#define JSDATE_H
#include "jsvalue.h"

// ES5 15.9.1.2
const int64_t kMsPerDay = 86400000;

// ES5 15.9.1.10
const int64_t kHoursPerDay = 24;
const int64_t kMinutesPerHour = 60;
const int64_t kSecondsPerMinute = 60;
const int64_t kMsPerSecond = 1000;
const int64_t kMsPerMinute = kMsPerSecond * kSecondsPerMinute;
const int64_t kMsPerHour = kMsPerMinute * kMinutesPerHour;

TValue __js_ToDate(TValue &this_object, TValue *arg_list, uint32_t nargs);
TValue __js_new_date_obj(TValue &this_object, TValue *arg_list, uint32_t nargs);

// external
TValue __jsdate_GetDate(TValue &this_date);
TValue __jsdate_GetDay(TValue &this_date);
TValue __jsdate_GetFullYear(TValue &this_date);
TValue __jsdate_GetHours(TValue &this_date);
TValue __jsdate_GetMilliseconds(TValue &this_date);
TValue __jsdate_GetMinutes(TValue &this_date);
TValue __jsdate_GetMonth(TValue &this_date);
TValue __jsdate_GetSeconds(TValue &this_date);
TValue __jsdate_GetTime(TValue &this_date);
TValue __jsdate_GetTimezoneOffset(TValue &this_date);
TValue __jsdate_GetUTCDate(TValue &this_date);
TValue __jsdate_GetUTCDay(TValue &this_date);
TValue __jsdate_GetUTCFullYear(TValue &this_date);
TValue __jsdate_GetUTCHours(TValue &this_date);
TValue __jsdate_GetUTCMilliseconds(TValue &this_date);
TValue __jsdate_GetUTCMinutes(TValue &this_date);
TValue __jsdate_GetUTCMonth(TValue &this_date);
TValue __jsdate_GetUTCSeconds(TValue &this_date);
TValue __jsdate_SetDate(TValue &this_date, TValue &value);
TValue __jsdate_SetFullYear(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_SetHours(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_SetMilliseconds(TValue &this_date, TValue &value);
TValue __jsdate_SetMinutes(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_SetMonth(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_SetSeconds(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_SetTime(TValue &this_date, TValue &value);
TValue __jsdate_SetUTCDate(TValue &this_date, TValue &value);
TValue __jsdate_SetUTCFullYear(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_SetUTCHours(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_SetUTCMilliseconds(TValue &this_date, TValue &value);
TValue __jsdate_SetUTCMinutes(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_SetUTCMonth(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_SetUTCSeconds(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_ToDateString(TValue &this_date);
TValue __jsdate_ToLocaleDateString(TValue &this_date, TValue *arg_list, uint32_t nargs);
TValue __jsdate_ToLocaleString(TValue &this_date, TValue *arg_list, uint32_t nargs);
TValue __jsdate_ToLocaleTimeString(TValue &this_date, TValue *arg_list, uint32_t nargs);
TValue __jsdate_ToString(TValue &this_date);
TValue __jsdate_ToString_Obj(__jsobject *obj);
TValue __jsdate_ToTimeString(TValue &this_date);
TValue __jsdate_ToUTCString(TValue &this_date);
TValue __jsdate_ValueOf(TValue &this_date);
TValue __jsdate_ToISOString(TValue &this_date);
TValue __jsdate_UTC(TValue &this_date, TValue *args, uint32_t nargs);
TValue __jsdate_Now(void);
TValue __jsdate_ToJSON(TValue &this_date, TValue &value);
TValue __jsdate_Parse(TValue &this_date, TValue &value);
TValue __jsdate_GetYear(TValue &this_date);
TValue __jsdate_SetYear(TValue &this_date,  TValue &value);
TValue __jsdate_ToGMTString(TValue &this_date);

// internal
int64_t Day(int64_t t);
int64_t TimeWithinDday(int64_t t);
int64_t DaysInYear(int64_t y);
int64_t InLeapYear(int64_t t);
int64_t TimeFromYear(int64_t y);
int64_t YearFromTime(int64_t t);
int64_t DayFromYear(int64_t y);
int64_t DayWithinYear(int64_t t);
int64_t MonthFromTime(int64_t t);
int64_t DateFromTime(int64_t t);
int64_t HourFromTime(int64_t t);
int64_t MinFromTime(int64_t t);
int64_t SecFromTime(int64_t t);
int64_t MsFromTime(int64_t t);
int64_t DaylightSavingTA(int64_t t);
int64_t LocalTime(int64_t t);
int64_t UTC(int64_t t);
int64_t LocalTZA();
int64_t MakeDate(int64_t day, int64_t time);
int64_t MakeDay(int64_t year, int64_t month, int64_t date);
int64_t MakeTime(int64_t hour, int64_t min, int64_t sec, int64_t ms);
int64_t TimeClip(int64_t time);
int64_t WeekDay(int64_t t);

// 15.9.2 The Date Constructor called as a function
TValue __js_new_dateconstructor(TValue &, TValue *, uint32_t);
#endif // JSDATE_H
