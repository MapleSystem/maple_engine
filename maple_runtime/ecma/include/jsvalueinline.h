/*
 * Copyright (C) [2021] Futurewei Technologies, Inc. All rights reserved.
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

/// Only small inline functions for jsvalue.
#ifndef JSVALUEINLINE_H
#define JSVALUEINLINE_H
#include "jsvalue.h"
#include "jsobject.h"
#include "jsnum.h"
static inline bool __is_null(TValue data) {
  return IS_NULL(data.x.u64);
}

static inline bool __is_undefined(TValue data) {
  return IS_UNDEFINED(data.x.u64);
}

static inline bool __is_nan(TValue data) {
  return IS_NAN(data.x.u64);
}

static inline bool __is_infinity(TValue data) {
  return IS_INFINITY(data.x.u64);
}

static inline bool __is_positive_infinity(TValue data) {
  return __is_infinity(data) && (data.x.u32 == 0);
}

static inline bool __is_neg_infinity(TValue data) {
  return __is_infinity(data) && (data.x.u32 == 1);
}

static inline bool __is_null_or_undefined(TValue data) {
  return IS_NULL(data.x.u64) || IS_UNDEFINED(data.x.u64);
}

static inline bool __is_boolean(TValue data) {
  return IS_BOOLEAN(data.x.u64);
}

static inline bool __is_string(TValue data) {
  return IS_STRING(data.x.u64);
}

static inline bool __is_number(TValue data) {
  return IS_NUMBER(data.x.u64);
}

static inline bool __is_double(TValue data) {
  return IS_DOUBLE(data.x.u64);
}

static inline bool __is_int32(TValue data) {
  return IS_NUMBER(data.x.u64);
}

static inline bool __is_primitive(uint32_t ptyp) {
  return ptyp == JSTYPE_NUMBER || ptyp == JSTYPE_STRING || ptyp == JSTYPE_BOOLEAN || ptyp == JSTYPE_UNDEFINED
         || ptyp == JSTYPE_DOUBLE || ptyp == JSTYPE_NAN || ptyp == JSTYPE_INFINITY || ptyp == JSTYPE_NULL;
}

static inline bool __is_primitive(TValue data) {
  return IS_NUMBER(data.x.u64) || IS_BOOLEAN(data.x.u64) || IS_DOUBLE(data.x.u64) || IS_STRING(data.x.u64) ||
         IS_NAN(data.x.u64) || IS_UNDEFINED(data.x.u64) || IS_NULL(data.x.u64) || IS_INFINITY(data.x.u64);
}

static inline bool __is_js_object(TValue data) {
  return IS_OBJECT(data.x.u64);
}

static inline bool __is_js_object_or_primitive(TValue data) {
  return __is_js_object(data) || __is_primitive(data);
}

static inline bool __is_positive_zero(TValue data) {
  return data.x.u64 == POS_ZERO;
}

static inline bool __is_negative_zero(TValue data) {
  return data.x.u64 == NEG_ZERO;
}

#if MACHINE64
#define IsNeedRc(v) (((uint8_t)v & 0x4) == 4)
static inline __jsstring *__jsval_to_string(TValue data) {
  return (__jsstring *)data.x.c.payload;
}
static inline __jsobject *__jsval_to_object(TValue data) {
  return (__jsobject *)data.x.c.payload;
}
static inline void __set_string(TValue &data, __jsstring *str) {
  data.x.u64 = (uint64_t)str | NAN_STRING;
}
static inline void __set_object(TValue &data, __jsobject *obj) {
  data.x.u64 = (uint64_t)obj | NAN_OBJECT;;
}
static inline bool __is_js_function(TValue data) {
  return IS_OBJECT(data.x.u64) && ((__jsobject*)data.x.c.payload)->object_class == JSFUNCTION;
}
static inline bool __is_js_array(TValue data) {
  return IS_OBJECT(data.x.u64) && ((__jsobject*)data.x.c.payload)->object_class == JSARRAY;
}
static inline double __jsval_to_double(TValue data) {
  if (IS_DOUBLE(data.x.u64) || IS_INFINITY(data.x.u64))
    return data.x.f64;
  return (double) data.x.i32;
}
static inline TValue __double_value(double f64) {
  TValue ret = {.x.f64 = f64};
  if (ret.x.u64 == 0)
    ret.x.u64 = POS_ZERO;
  return ret;
}
static inline TValue __function_value (void *addr) {
  return (TValue){.x.u64 = (uint64_t)addr | NAN_FUNCTION};
}
#else
static inline __jsstring *__jsval_to_string(TValue *data) {
  MAPLE_JS_ASSERT(__is_string(data));
  return data->x.payload.str;
}

static inline __jsobject *__jsval_to_object(TValue *data) {
  MAPLE_JS_ASSERT(__is_js_object(data));
  return data->x.payload.obj;
}
static inline void __set_string(TValue *data, __jsstring *str) {
  data->ptyp = JSTYPE_STRING;
  data->x.payload.str = str;
}
static inline void __set_object(TValue *data, __jsobject *obj) {
  data->ptyp = JSTYPE_OBJECT;
  data->x.payload.obj = obj;
}
static inline bool __is_js_function(TValue *data) {
  if (__is_js_object(data)) {
    return data->x.payload.obj->object_class == JSFUNCTION;
  }
  return false;
}
static inline bool __is_js_array(TValue *data) {
  if (__is_js_object(data)) {
    return data->x.payload.obj->object_class == JSARRAY;
  }
  return false;
}
#endif

static inline bool __is_none(TValue data) {
  return IS_NONE(data.x.u64);
}

static inline bool __jsval_to_boolean(TValue data) {
  return (bool)data.x.u8;
}

static inline int32_t __jsval_to_number(TValue data) {
  return data.x.i32;
}

static inline int32_t __jsval_to_int32(TValue data) {
  return data.x.i32;
}

static inline uint32_t __jsval_to_uint32(TValue data) {
  if (__is_number(data)) {
    return (uint32_t)__jsval_to_number(data);
  } else if (__is_double(data)) {
    return (uint32_t)__jsval_to_double(data);
  }
  //  MAPLE_JS_ASSERT(0 && "__jsval_to_uint32");
  return 0;
}

static inline TValue __string_value(__jsstring *str) {
  return (TValue){.x.u64 = (uint64_t)str | NAN_STRING};
}

static inline TValue __undefined_value() {
  return (TValue){.x.u64 = NAN_UNDEFINED};
}

static inline TValue __object_value(__jsobject *obj) {
  if (!obj) {
    return (TValue){.x.u64 = NAN_UNDEFINED};
  }
  return (TValue){.x.u64 = (uint64_t)obj | NAN_OBJECT};
}

static inline void __set_boolean(TValue &data, bool b) {
  data.x.u64 = NAN_BOOLEAN | (uint64_t)b;
}

static inline void __set_number(TValue &data, int32_t i) {
  data.x.u64 = NAN_NUMBER; data.x.i32 = i;
}

static inline TValue __null_value() {
  return (TValue){.x.u64 = NAN_NULL};
}

static inline TValue __boolean_value(bool b) {
  return (TValue){.x.u64 = NAN_BOOLEAN | (uint64_t)b};
}

static inline TValue __number_value(int32_t i) {
  TValue v = {.x.u64 = NAN_NUMBER};
  v.x.i32 = i;
  return v;
}

static inline TValue __number_infinity() {
  return (TValue){.x.u64 = NAN_INFINITY};
}

static inline TValue __number_neg_infinity() {
  return (TValue){.x.u64 = NAN_INFINITY | 1L};
}

static inline TValue __infinity_value(int32_t i) {
  return (TValue){.x.u64 = NAN_INFINITY | (uint64_t)i};
}

static inline TValue __env_value(void * addr) {
  return (TValue){.x.u64 = NAN_ENV | (uint64_t)addr};
}

static inline TValue __none_value() {
  return (TValue){.x.u64 = NAN_NONE};
}

static inline TValue __none_value(uint32_t v) {
  return (TValue){.x.u64 = NAN_NONE | (uint64_t)v};
}

static inline TValue __nan_value() {
  return (TValue){.x.u64 = NAN_NAN};
}

static inline TValue __positive_zero_value() {
  return (TValue){.x.u64 = POS_ZERO};
}

static inline TValue __negative_zero_value() {
  return (TValue){.x.u64 = NEG_ZERO};
}

static inline __jstype __jsval_typeof(TValue data) {
  //  MAPLE_JS_ASSERT((__is_js_object_or_primitive(data)
  //     || __is_none(data) || __is_nan(data) || __is_infinity(data)) && "internal error.");
  return (__jstype)GET_TYPE(data);
}

#endif
