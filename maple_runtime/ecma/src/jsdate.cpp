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

#include <stdio.h>
#include <sys/time.h>
#include <unicode/smpdtfmt.h>
#include "jsglobal.h"
#include "jsvalue.h"
#include "jsvalueinline.h"
#include "jsobject.h"
#include "jsobjectinline.h"
#include "jstycnv.h"
#include "jscontext.h"
#include "jsdate.h"
#include "jsfunction.h"
#include "vmmemory.h"
#include "jsintl.h"

TValue  __js_ToDate(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  return __js_new_date_obj(this_object, arg_list, nargs);
}

// check arglist are all zero
static bool __js_argsZero(TValue *arg_list, uint32_t nargs) {
  for (uint32_t i = 0; i < nargs; i++) {
    TValue arg = arg_list[i];
    if (!__is_positive_zero(arg)) {
      return false;
    }
  }
  return true;
}

TValue __js_new_date_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  __jsobject *obj = __create_object();
  __jsobj_set_prototype(obj, JSBUILTIN_DATEPROTOTYPE);
  obj->object_class = JSDATE;
  obj->extensible = true;

  double time;
  if (nargs == 0) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    int64_t time_val = tv.tv_sec * 1000L + tv.tv_usec / 1000L; // UTC in ms
    time = (double) time_val;
  } else if (nargs == 1) {
    if (__js_argsZero(arg_list, nargs)) {
        time = 0.0;
    } else {
        TValue time_val = arg_list[0];

        if (__is_string(time_val)) {
          time = 0.0;
          // TODO: not implemented yet
        } else {
            if (__is_nan(time_val) || __is_infinity(time_val)) {
                __jsstring* invalid_date = __jsstr_new_from_char("Invalid Date");
                return __string_value(invalid_date);
            } else {
                int64_t v = __js_ToNumber64(time_val);
                time = TimeClip(v);
            }
        }
    }
  } else {
    if (__is_nan(arg_list[0])
        || __is_infinity(arg_list[0])
        || __is_neg_infinity(arg_list[0])) {
      time = NAN;
    } else if (__is_nan(arg_list[1])
        || __is_infinity(arg_list[1])
        || __is_neg_infinity(arg_list[1])) {
      time = NAN;
    } else {
      int64_t y = (int64_t) __js_ToNumber(arg_list[0]);
      int64_t m = (int64_t) __js_ToNumber(arg_list[1]);

      for (int i = 2; i < nargs; i++) {
        if (__is_undefined(arg_list[i]) || __is_nan(arg_list[i])) {
          return __nan_value();
        }
      }
      int64_t dt = nargs >= 3 ? (int64_t) __js_ToNumber(arg_list[2]) : 1;
      int64_t h = nargs >= 4 ? (int64_t) __js_ToNumber(arg_list[3]) : 0;
      int64_t min = nargs >= 5 ? (int64_t) __js_ToNumber(arg_list[4]) : 0;
      int64_t s = nargs >= 6 ? (int64_t) __js_ToNumber(arg_list[5]) : 0;
      int64_t milli = nargs == 7 ? (int64_t) __js_ToNumber(arg_list[6]) : 0;

      int64_t yr = (!std::isnan(y) && y >= 0 && y <= 99) ? 1900 + y : y;
      int64_t final_date = UTC(MakeDate(MakeDay(yr, m, dt), MakeTime(h, min, s, milli)));

      // 15.9.1.1 Time Range
      if (final_date <= -100000000 * kMsPerDay || final_date >= 100000000 * kMsPerDay)
        MAPLE_JS_RANGEERROR_EXCEPTION();

      time = (double) TimeClip(final_date);
    }
  }
  obj->shared.primDouble = time;

  return __object_value(obj);
}

// ES5 15.9.1.13 MakeDate(day, time)
int64_t MakeDate(int64_t day, int64_t time) {
  if (!std::isfinite(day) || !std::isfinite(time))
    return NAN;

  return day * kMsPerDay + time;
}

// ES5 15.9.1.12 MakeDay(year, month, date)
int64_t MakeDay(int64_t year, int64_t month, int64_t date) {
  if (!std::isfinite(year) || !std::isfinite(month) || !std::isfinite(date))
    return NAN;

  int64_t y = year;
  int64_t m = month;
  int64_t dt = date;
  int64_t ym = y + (int64_t) floor(m / 12);
  int64_t mm = m % 12;

  int64_t t = TimeFromYear(ym);
  if (!std::isfinite(t) || std::isnan(t))
    return NAN;

  int64_t yd = 0;
  if (mm > 0)
    yd = 31;
  if (mm > 1) {
    if (InLeapYear(t))
      yd += 29;
    else
      yd += 28;
  }
  if (mm > 2)
      yd += 31;
  if (mm > 3)
      yd += 30;
  if (mm > 4)
      yd += 31;
  if (mm > 5)
      yd += 30;
  if (mm > 6)
      yd += 31;
  if (mm > 7)
      yd += 31;
  if (mm > 8)
      yd += 30;
  if (mm > 9)
      yd += 31;
  if (mm > 10)
      yd += 30;

  t += yd * kMsPerDay;

  return Day(t) + dt - 1;
}

// ES5 15.9.1.11 MakeTime(hour, min, sec, ms)
int64_t MakeTime(int64_t hour, int64_t min, int64_t sec, int64_t ms) {
  // TODO: not implemented yet
  if (!std::isfinite(hour) || !std::isfinite(min) || !std::isfinite(sec) || !std::isfinite(ms))
    return NAN;

  int64_t h = hour;
  int64_t m = min;
  int64_t s = sec;
  int64_t milli = ms;
  int64_t t = h * kMsPerHour + m * kMsPerMinute + s * kMsPerSecond + milli;

  return t;
}

// ES5 15.9.1.14 TimeClip(time)
int64_t TimeClip(int64_t time) {
  // TODO: not implemented yet
  return time;
}

static inline __jsobject *__jsdata_value_to_obj(TValue &this_date) {
  if (!__is_js_object(this_date))
    MAPLE_JS_TYPEERROR_EXCEPTION();

  __jsobject *obj = __jsval_to_object(this_date);
  if (obj->object_class != JSDATE)
    MAPLE_JS_TYPEERROR_EXCEPTION();
  return obj;
}

// ES5 15.9.5.14 Date.prototype.getDate()
TValue __jsdate_GetDate(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t date = DateFromTime(LocalTime(t));
  return __double_value((double) date);
}

// ES5 15.9.5.16 Date.prototype.getDay()
TValue __jsdate_GetDay(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t day = WeekDay(LocalTime(t));
  return __double_value((double) day);
}

// ES5 15.9.5.10 Date.prototype.getFullYear()
TValue __jsdate_GetFullYear(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t year = YearFromTime(LocalTime(t));
  return __double_value((double) year);
}

// ES5 15.9.5.18 Date.prototype.getHours()
TValue __jsdate_GetHours(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t hours = HourFromTime(LocalTime(t));
  return __double_value((double) hours);
}

// ES5 15.9.5.24 Date.prototype.getMilliseconds()
TValue __jsdate_GetMilliseconds(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t ms = MsFromTime(LocalTime(t));
  return __double_value((double) ms);
}

// ES5 15.9.5.20 Date.prototype.getMinutes()
TValue __jsdate_GetMinutes(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t mins = MinFromTime(LocalTime(t));
  return __double_value((double) mins);
}

// ES5 15.9.5.12 Date.prototype.getMonth()
TValue __jsdate_GetMonth(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t month = MonthFromTime(LocalTime(t));
  return __double_value((double) month);
}

// ES5 15.9.5.22 Date.prototype.getSeconds()
TValue __jsdate_GetSeconds(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t secs = SecFromTime(LocalTime(t));
  return __double_value((double) secs);
}

// ES5 15.9.5.9 Date.prototype.getTime()
TValue __jsdate_GetTime(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  double t = obj->shared.primDouble;

  return __double_value(t);
}

// ES5 15.9.5.26 Date.prototype.getTimezoneOffset()
TValue __jsdate_GetTimezoneOffset(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  double offset = (t - LocalTime(t)) / (double)kMsPerMinute;
  return __double_value(offset);
}

// ES5 15.9.5.15 Date.prototype.getUTCDate()
TValue __jsdate_GetUTCDate(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t date = DateFromTime(t);
  return __double_value(date);
}

// ES5 15.9.5.17 Date.prototype.getUTCDay()
TValue __jsdate_GetUTCDay(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t day = WeekDay(t);
  return __double_value(day);
}

// ES5 15.9.5.11 Date.prototype.getUTCFullYear()
TValue __jsdate_GetUTCFullYear(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t year = YearFromTime(t);
  return __double_value(year);
}

// ES5 15.9.5.19 Date.prototype.getUTCHours()
TValue __jsdate_GetUTCHours(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t hours = HourFromTime(t);
  return __double_value((double) hours);
}

// ES5 15.9.5.25 Date.prototype.getUTCMilliseconds()
TValue __jsdate_GetUTCMilliseconds(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t ms = MsFromTime(t);
  return __double_value((double) ms);
}

// ES5 15.9.5.21 Date.prototype.getUTCMinutes()
TValue __jsdate_GetUTCMinutes(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t mins = MinFromTime(t);
  return __double_value(mins);
}

// ES5 15.9.5.13 Date.prototype.getUTCMonth()
TValue __jsdate_GetUTCMonth(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t month = MonthFromTime(t);
  return __double_value((double) month);
}

// ES5 15.9.5.23 Date.prototype.getUTCSeconds()
TValue __jsdate_GetUTCSeconds(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  if (std::isnan(t))
    return __nan_value();

  int64_t secs = SecFromTime(t);
  return __double_value((double) secs);
}

// ES5 15.9.5.36 Date.prototype.setDate(date)
TValue __jsdate_SetDate(TValue &this_date, TValue &value) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.40 Date.prototype.setFullYear(year [, month [, date ]])
TValue __jsdate_SetFullYear(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.34 Date.prototype.setHours(hour [, min [, sec [, ms ]]])
TValue __jsdate_SetHours(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.28 Date.prototype.setMilliseconds(ms)
TValue __jsdate_SetMilliseconds(TValue &this_date, TValue &value) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.32 Date.prototype.setMinutes(min [, sec [, ms ]])
TValue __jsdate_SetMinutes(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.38 Date.prototype.setMonth(month [, date ])
TValue __jsdate_SetMonth(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.30 Date.prototype.setSeconds(sec [, ms ])
TValue __jsdate_SetSeconds(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.27 Dte.prototype.setTime(time)
TValue __jsdate_SetTime(TValue &this_date, TValue &value) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.37 Date.prototype.setUTCDate(date)
TValue __jsdate_SetUTCDate(TValue &this_date, TValue &value) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.41 Date.prototype.setUTCFullYear(year [, month [, date ]])
TValue __jsdate_SetUTCFullYear(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.35 Date.prototype.setUTCHours(hour [, min [, sec [, ms ]]])
TValue __jsdate_SetUTCHours(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.29 Date.prototype.setUTCMilliseconds(ms)
TValue __jsdate_SetUTCMilliseconds(TValue &this_date, TValue &value) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.33 Date.prototype.setUTCMinutes(min [, sec [, ms ]])
TValue __jsdate_SetUTCMinutes(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.39 Date.prototype.setUTCMonth(month [, date ])
TValue __jsdate_SetUTCMonth(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.31 Date.prototype.setUTCSeconds(sec [, ms ])
TValue __jsdate_SetUTCSeconds(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.3 Date.prototype.toDateString()
TValue __jsdate_ToDateString(TValue &this_date) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.6 Date.prototype.toLocaleDateString()
TValue __jsdate_ToLocaleDateString(TValue &this_date, TValue *arg_list, uint32_t nargs) {

  // Check this_date.
  if (__is_undefined(this_date) || __is_null(this_date) || !__is_js_object(this_date)) {
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }
  // Check if 'this_date' is 'Invalid Date'.
  if (__jsval_to_object(this_date)->object_class == JSSTRING) {
    if (__jsstr_equal(__js_ToString(this_date), __jsstr_new_from_char("Invalid Date"))) {
      return StrToVal("Invalid Date");
    }
  }
  if (__jsval_to_object(this_date)->object_class != JSDATE) {
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }

  TValue locales, options;

  if (nargs == 1) {
    locales = arg_list[0];
  } else if (nargs == 2) {
    locales = arg_list[0];
    options = arg_list[1];
  }
  // Step 1.
  TValue x = __double_value(__jsval_to_object(this_date)->shared.primDouble);
  // Step 2.
  if (__is_nan(x)) {
    return StrToVal("Invalid Date");
  }
  // Step 3.
  if (nargs < 1) {
    locales = __undefined_value();
  }
  // Step 4.
  if (nargs < 2) {
    options = __undefined_value();
  }

  // Check if 'locales' is valid (JSI9502).
  locales = CanonicalizeLocaleList(locales);

  // Check if 'options' are undefined.
  if (!__is_undefined(options)) {
    TValue p = StrToVal("localeMatcher");
    TValue v = __jsop_getprop(options, p);
    // Check if 'localeMatcher' in 'options' is null (JSI9502).
    if (__is_null(v)) {
      MAPLE_JS_RANGEERROR_EXCEPTION();
    }
  }

  // Step 5.
  TValue required = StrToVal("date");
  TValue defaults = StrToVal("date");
  options = ToDateTimeOptions(options, required, defaults);

  // Step 6.
  TValue undefined_val = __undefined_value();
  TValue args[] = {locales, options};
  TValue date_time_format = __js_DateTimeFormatConstructor(undefined_val, args, 2);

  // Step 7.
  return FormatDateTime(date_time_format, x);
}

// ES5 15.9.5.5 Date.prototype.toLocaleString()
// ECMA-402 13.3.1 Date.prototype.toLocaleString([locales [, options]])
TValue __jsdate_ToLocaleString(TValue &this_date, TValue *arg_list, uint32_t nargs) {

  // Check this_date.
  if (__is_undefined(this_date) || __is_null(this_date) || !__is_js_object(this_date)) {
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }
  // Check if 'this_date' is 'Invalid Date'.
  if (__jsval_to_object(this_date)->object_class == JSSTRING) {
    if (__jsstr_equal(__js_ToString(this_date), __jsstr_new_from_char("Invalid Date"))) {
      return StrToVal("Invalid Date");
    }
  }
  if (__jsval_to_object(this_date)->object_class != JSDATE) {
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }

  TValue locales, options;

  if (nargs == 1) {
    locales = arg_list[0];
  } else if (nargs == 2) {
    locales = arg_list[0];
    options = arg_list[1];
  }
  // Step 1.
  TValue x = __double_value(__jsval_to_object(this_date)->shared.primDouble);
  // Step 2.
  if (__is_nan(x)) {
    return StrToVal("Invalid Date");
  }
  // Step 3.
  if (nargs < 1) {
    locales = __undefined_value();
  }
  // Step 4.
  if (nargs < 2) {
    options = __undefined_value();
  }

  // Check if 'locales' is valid (JSI9502).
  locales = CanonicalizeLocaleList(locales);

  // Check if 'options' is undefined (JSI9416).
  if (!__is_undefined(options)) {
    TValue p = StrToVal("localeMatcher");
    TValue v = __jsop_getprop(options, p);
    // Check if 'localeMatcher' in 'options' is null (JSI9502).
    if (__is_null(v)) {
      MAPLE_JS_RANGEERROR_EXCEPTION();
    }
  }

  // Step 5.
  TValue required = StrToVal("any");
  TValue defaults = StrToVal("all");
  options = ToDateTimeOptions(options, required, defaults);
  // Step 6.
  TValue undefined_val = __undefined_value();
  TValue args[] = {locales, options};
  TValue date_time_format = __js_DateTimeFormatConstructor(undefined_val, args, 2);
  // Step 7.
  return FormatDateTime(date_time_format, x);
}

// ES5 15.9.5.7 Date.prototype.toLocaleTimeString()
TValue __jsdate_ToLocaleTimeString(TValue &this_date, TValue *arg_list, uint32_t nargs) {

  // Check this_date.
  if (__is_undefined(this_date) || __is_null(this_date) || !__is_js_object(this_date)) {
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }
  // Check if 'this_date' is 'Invalid Date'.
  if (__jsval_to_object(this_date)->object_class == JSSTRING) {
    if (__jsstr_equal(__js_ToString(this_date), __jsstr_new_from_char("Invalid Date"))) {
      return StrToVal("Invalid Date");
    }
  }
  if (__jsval_to_object(this_date)->object_class != JSDATE) {
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }

  TValue locales, options;

  if (nargs == 1) {
    locales = arg_list[0];
  } else if (nargs == 2) {
    locales = arg_list[0];
    options = arg_list[1];
  }
  // Step 1.
  TValue x = __double_value(__jsval_to_object(this_date)->shared.primDouble);
  // Step 2.
  if (__is_nan(x)) {
    return StrToVal("Invalid Date");
  }
  // Step 3.
  if (nargs < 1) {
    locales = __undefined_value();
  }
  // Step 4.
  if (nargs < 2) {
    options = __undefined_value();
  }
 
  // Check if 'locales' is valid (JSI9502).
  locales = CanonicalizeLocaleList(locales);

  // Check if 'options' is undefined (JSI9416).
  if (!__is_undefined(options)) {
    TValue p = StrToVal("localeMatcher");
    TValue v = __jsop_getprop(options, p);
    // Check if 'localeMatcher' in 'options' is null (JSI9502).
    if (__is_null(v)) {
      MAPLE_JS_RANGEERROR_EXCEPTION();
    }
  }

  // Step 5.
  TValue required = StrToVal("time");
  TValue defaults = StrToVal("time");
  options = ToDateTimeOptions(options, required, defaults);

  // Step 6.
  TValue undefined_val = __undefined_value();
  TValue args[] = {locales, options};
  TValue date_time_format = __js_DateTimeFormatConstructor(undefined_val, args, 2);

  // Step 7.
  return FormatDateTime(date_time_format, x);
}

// ES5 B.2.4 Date.prototype.getYea()
TValue __jsdate_GetYear(TValue &this_date) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 B.2.5 Date.prototype.setYear()
TValue __jsdate_SetYear(TValue &this_date,  TValue &value) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 B.2.6 Date.prototype.toGMTString()
TValue __jsdate_ToGMTString(TValue &this_date) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

const char *WeekDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *Months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


TValue __jsdate_ToString_Obj(__jsobject *obj) {
  int64_t time = LocalTime((int64_t) obj->shared.primDouble);
  if (time < -9007199254740992 || time > 9007199254740992)
    MAPLE_JS_RANGEERROR_EXCEPTION();

  if (!std::isfinite(time))
    MAPLE_JS_RANGEERROR_EXCEPTION();

  int time_zone_minutes = (int)LocalTZA() / kMsPerMinute;
  char buf[100];
  snprintf(buf, sizeof(buf), "%s %s %.2d %.4d %.2d:%.2d:%.2d GMT%+.2d%.2d",
           WeekDays[WeekDay(time)],
           Months[(int)MonthFromTime(time)],
           (int)DateFromTime(time),
           (int)YearFromTime(time),
           (int)HourFromTime(time),
           (int)MinFromTime(time),
           (int)SecFromTime(time),
           time_zone_minutes / 60,
           time_zone_minutes > 0 ? time_zone_minutes % 60 : -time_zone_minutes % 60);

  return __string_value(__jsstr_new_from_char(buf));
}
// ES5 15.9.5.2 Date.prototype.toString()
TValue __jsdate_ToString(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __jsdate_ToString_Obj(obj);
}

// ES5 15.9.5.4 Date.prototype.toTimeString()
TValue __jsdate_ToTimeString(TValue &this_date) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.42 Date.prototype.toUTCString()
TValue __jsdate_ToUTCString(TValue &this_date) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.5.8 Date.prototype.valueOf()
TValue __jsdate_ValueOf(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  int64_t t = (int64_t) obj->shared.primDouble;

  return __double_value((double) t);
}

// ES5 15.9.5.43 Date.prototype.toISOString()
TValue __jsdate_ToISOString(TValue &this_date) {
  __jsobject *obj = __jsdata_value_to_obj(this_date);

  int64_t time = (int64_t) obj->shared.primDouble;
  if (time < -9007199254740992 || time > 9007199254740992)
    MAPLE_JS_RANGEERROR_EXCEPTION();

  if (!std::isfinite(time))
    MAPLE_JS_RANGEERROR_EXCEPTION();

  char buf[100];
  snprintf(buf, sizeof(buf), "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d.%.3dZ",
           (int)YearFromTime(time),
           (int)MonthFromTime(time) + 1,
           (int)DateFromTime(time),
           (int)HourFromTime(time),
           (int)MinFromTime(time),
           (int)SecFromTime(time),
           (int)MsFromTime(time));

  return __string_value(__jsstr_new_from_char(buf));
}

// ES5 15.9.4.3
// Date.UTC(year, month [, date [, hours [, minutes [, seconds [, ms ]]]]])
TValue __jsdate_UTC(TValue &this_date, TValue *args, uint32_t nargs) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __double_value(0);
}

// ES5 15.9.5.44 Date.prototype.toJSON(key)
TValue __jsdate_ToJSON(TValue &this_date, TValue &value) {
  // TODO: not implemented yet
  __jsobject *obj = __jsdata_value_to_obj(this_date);
  return __object_value(obj);
}

// ES5 15.9.4.4 Date.now()
TValue __jsdate_Now(void) {
  // Basic Date implementation may need more scrutiny
  struct timeval current_time;
  gettimeofday(&current_time, NULL);
  int64_t t = current_time.tv_sec * 1000L + current_time.tv_usec / 1000L; // in ms.
  return __double_value(t);
}

// ES5 15.9.4.2 Date.parse(string)
TValue __jsdate_Parse(TValue &this_date, TValue &value) {
  __jsstring *date_str = __js_ToString(value);
  uint16_t len = date_str->length;

  // Try ISO string type first.
  UnicodeString pattern("Y-M-d'T'H:m:sZZZZZ");
  UErrorCode status = U_ZERO_ERROR;
  SimpleDateFormat *parser = new SimpleDateFormat(pattern, status);
  if (U_FAILURE(status)) {
    MAPLE_JS_ASSERT(false && "Error in SimpleDateFormat()");
  }
  UnicodeString source;
  if (date_str->kind == JSSTRING_UNICODE) {
    for (int i = 0; i < len; i++) {
      source.append(date_str->x.utf16[i]);
    }
  } else {
    for (int i = 0; i < len; i++) {
      source.append(date_str->x.ascii[i]);
    }
  }
  status = U_ZERO_ERROR;
  UDate date = parser->parse(source, status);
  if (! U_FAILURE(status)) {
    return __double_value((uint64_t) date);
  }
  // Try string parsing then.
  struct tm tm;
  char buf[255];
  char *d;
  memset(&tm, 0, sizeof(struct tm));
  if (date_str->kind == JSSTRING_UNICODE) {
    MAPLE_JS_ASSERT(false && "NYI");
  } else {
    d = date_str->x.ascii;
  }
  strptime(d, "%a, %d %b %Y %H:%M:%S %z", &tm);
  int64_t t = mktime(&tm) * 1000L;
  return __double_value((double) t);
}

// ES5 15.9.1.2
int64_t Day(int64_t t) {
  return (int64_t) floor(t / kMsPerDay);
}

// ES5 15.9.1.2
int64_t TimeWithinDay(int64_t t) {
  return t % kMsPerDay;
}

// ES5 15.9.1.3
int64_t DaysInYear(int64_t y) {
  int64_t result;

  if (fmod(y, 4) != 0) {
    result = 365;
  } else if ((fmod(y, 4) == 0) && (fmod(y, 100) != 0)) {
    result = 366;
  } else if ((fmod(y, 100) == 0) && (fmod(y, 400) != 0)) {
    result = 365;
  } else if (fmod(y, 400) == 0) {
    result = 366;
  }

  return result;
}

// ES5 15.9.1.3
int64_t InLeapYear(int64_t t) {
  int64_t result;

  int64_t y = YearFromTime(t);
  int64_t d = DaysInYear(y);

  if (d == 365)
    result = 0;
  else if (d == 366)
    result = 1;

  return result;
}

// ES5 15.9.1.3
int64_t TimeFromYear(int64_t y) {
  return kMsPerDay * DayFromYear(y);
}

// ES5 15.9.1.3
int64_t YearFromTime(int64_t t) {
  if (!std::isfinite(t))
    return NAN;

  int64_t y = (int64_t) floor(t / (kMsPerDay * 365.2425)) + 1970;
  int64_t t2 = TimeFromYear(y);

  if (t2 > t) {
    y--;
  } else {
    if (t2 + kMsPerDay * DaysInYear(y) <= t)
      y++;
  }

  return y;
}

// ES5 15.9.1.3
int64_t DayFromYear(int64_t y) {
  int64_t day = 365 * (y - 1970)
         + (int64_t) floor((y - 1969) / 4.)
         - (int64_t) floor((y - 1901) / 100.)
         + (int64_t) floor((y - 1601) / 400.);
  return day;
}

// ES5 15.9.1.4
int64_t DayWithinYear(int64_t t) {
  return Day(t) - DayFromYear(YearFromTime(t));
}

// ES5 15.9.1.4
int64_t MonthFromTime(int64_t t) {
  if (!std::isfinite(t))
    return NAN;

  int64_t result;
  int64_t d = DayWithinYear(t);
  int64_t leap_year = InLeapYear(t);

  if (d >= 0 && d < 31)
    result = 0;
  else if ((d >= 31) && d < (59 + leap_year))
    result = 1;
  else if ((d >= 59 + leap_year) && (d < 90 + leap_year))
    result = 2;
  else if ((d >= 90 + leap_year) && (d < 120 + leap_year))
    result = 3;
  else if ((d >= 120 + leap_year) && (d < 151 + leap_year))
    result = 4;
  else if ((d >= 151 + leap_year) && (d < 181 + leap_year))
    result = 5;
  else if ((d >= 181 + leap_year) && (d < 212 + leap_year))
    result = 6;
  else if ((d >= 212 + leap_year) && (d < 243 + leap_year))
    result = 7;
  else if ((d >= 243 + leap_year) && (d < 273 + leap_year))
    result = 8;
  else if ((d >= 273 + leap_year) && (d < 304 + leap_year))
    result = 9;
  else if ((d >= 304 + leap_year) && (d < 334 + leap_year))
    result = 10;
  else if ((d >= 334 + leap_year) && (d <= 365 + leap_year))
    result = 11;
  else
    assert(false);

  return result;
}

// ES5 15.9.1.5
int64_t DateFromTime(int64_t t) {
  if (!std::isfinite(t))
    return NAN;

  if (t < 0)
    t -= (kMsPerDay - 1);

  int64_t d = DayWithinYear(t);
  int64_t leap_year = InLeapYear(t);
  int64_t m = MonthFromTime(t);
  int64_t result;

  if (m == 0)
    result = d + 1;
  else if (m == 1)
    result = d - 30;
  else if (m == 2)
    result = d - 58 - leap_year;
  else if (m == 3)
    result = d - 89 - leap_year;
  else if (m == 4)
    result = d - 119 - leap_year;
  else if (m == 5)
    result = d - 150 - leap_year;
  else if (m == 6)
    result = d - 180 - leap_year;
  else if (m == 7)
    result = d - 211 - leap_year;
  else if (m == 8)
    result = d - 242 - leap_year;
  else if (m == 9)
    result = d - 272 - leap_year;
  else if (m == 10)
    result = d - 303 - leap_year;
  else if (m == 11)
    result = d - 333 - leap_year;
  else
    MAPLE_JS_ASSERT(false);

  return result;
}

// ES5 15.9.1.10
int64_t HourFromTime(int64_t t) {
  if (t < 0)
    return (int64_t) floor((kMsPerDay + (t % kMsPerDay)) / kMsPerHour) % kHoursPerDay;
  else
    return (int64_t) floor(t / kMsPerHour) % kHoursPerDay;
}

// ES5 15.9.1.10
int64_t MinFromTime(int64_t t) {
  if (t < 0)
    return (int64_t) floor((kMsPerDay + (t % kMsPerDay)) / kMsPerMinute) % kMinutesPerHour;
  else
    return (int64_t) floor(t / kMsPerMinute) % kMinutesPerHour;
}

// ES5 15.9.1.10
int64_t SecFromTime(int64_t t) {
  if (t < 0)
    return (int64_t) floor((kMsPerDay + (t % kMsPerDay)) / kMsPerSecond) % kSecondsPerMinute;
  else
    return (int64_t) floor(t / kMsPerSecond) % kSecondsPerMinute;
}

// ES5 15.9.1.10
int64_t MsFromTime(int64_t t) {
  if (t < 0)
    return (kMsPerDay + (t % kMsPerDay)) % kMsPerSecond;
  else
    return t % kMsPerSecond;
}

// ES5 15.9.1.8
int64_t DaylightSavingTA(int64_t t) {
  // TODO: not implemented yet
  return 0;
}

// ES5 15.9.1.9
int64_t LocalTime(int64_t t) {
  return t + LocalTZA() + DaylightSavingTA(t);
}

// ES5 15.9.1.9
int64_t UTC(int64_t t) {
  int64_t localTZA = LocalTZA();
  return t - localTZA - DaylightSavingTA(t - localTZA);
}

// ES5 15.9.1.7
// returns a value localTZA measured in milliseconds
int64_t LocalTZA() {
  tzset();
  return -timezone * 1000;
}

// ES5 15.9.1.6
int64_t WeekDay(int64_t t) {
  if (t < 0) {
    t -= (kMsPerDay - 1);
    return (((Day(t) % 7) + 11) % 7);
  }
  return (Day(t) + 4) % 7;
}

// 15.9.2 The Date Constructor called as a function, return a string
TValue __js_new_dateconstructor(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  TValue dateVal = __js_new_date_obj(this_object, arg_list, nargs);
  return __jsdate_ToString(dateVal);
}
