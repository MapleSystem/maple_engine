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

#include "jsglobal.h"
#include "jsvalue.h"
#include "jsvalueinline.h"
#include "jsobject.h"
#include "jsobjectinline.h"
#include "jsencode.h"
#include "jstycnv.h"
#include "jscontext.h"
#include "vmmemory.h"
#include "jsdataview.h"
// ecma 15.1.4.1 Object ( . . . ), See 15.2.1 and 15.2.2.
// ecma 15.2.1 Object ( . . . )
// If value not supplied, return __js_new_obj_obj_0(), else return __js_new_obj_obj_1.
// ecma 15.2.2 new Object ( . . . )
// Create an empty regular object without args.
__jsobject *__js_new_obj_obj_0() {
  // ecma 15.2.2 step 3~6.
  __jsobject *obj = __create_object();
  __jsobj_set_prototype(obj, JSBUILTIN_OBJECTPROTOTYPE);
  obj->object_class = JSOBJECT;
  obj->extensible = (uint8_t) true;
  obj->object_type = (uint8_t)JSREGULAR_OBJECT;
  return obj;
}

// ecma 15.2.2
// Convert V to an object or create an empty regular object.
__jsobject *__js_new_obj_obj_1(TValue &v) {
  if (__is_js_object(v)) {
    return __jsval_to_object(v);
  }
  if (!__is_null_or_undefined(v)) {
    return __js_ToObject(v);
  }
  return __js_new_obj_obj_0();
}

// A universal function to support ecma 15.2.2.
TValue __js_new_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  if (nargs == 0) {
    return __object_value(__js_new_obj_obj_0());
  } else {
    return __object_value(__js_new_obj_obj_1(arg_list[0]));
  }
}

TValue __js_new_arr(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  if (nargs == 1) {
      if(__is_number(arg_list[0]) || __is_double(arg_list[0])) {
          return __object_value(__js_new_arr_length(arg_list[0]));
      }
      if(__is_nan(arg_list[0]) || __is_infinity(arg_list[0]) || __is_neg_infinity(arg_list[0])) {
          MAPLE_JS_RANGEERROR_EXCEPTION();
      }
  }
  return __object_value(__js_new_arr_elems_direct(arg_list, nargs));
}

TValue __js_new_str_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 0 || nargs == 1);
  __jsobject *obj = __create_object();
  __jsobj_set_prototype(obj, JSBUILTIN_STRINGPROTOTYPE);
  obj->object_class = JSSTRING;
  obj->extensible = true;

  __jsstring *primstring;
  if (nargs == 0) {
    primstring = __jsstr_get_builtin(JSBUILTIN_STRING_EMPTY);
  } else {
    primstring = __js_ToString(arg_list[0]);
  }
  obj->shared.prim_string = primstring;
  GCIncRf(primstring);
  TValue length_value = __number_value(__jsstr_get_length(primstring));
  __jsobj_helper_init_value_property(obj, JSBUILTIN_STRING_LENGTH, length_value, JSPROP_DESC_HAS_VUWUEUC);
  // call __jsobj_initprop_fromString only when needed for performance
  //  __jsobj_initprop_fromString(obj, obj->shared.prim_string);
  return __object_value(obj);
}

TValue __js_new_str_obj(TValue &arg) {
  __jsobject *obj = __create_object();
  __jsobj_set_prototype(obj, JSBUILTIN_STRINGPROTOTYPE);
  obj->object_class = JSSTRING;
  obj->extensible = true;

  __jsstring *primstring;
  primstring = __js_ToString(arg);
  obj->shared.prim_string = primstring;
  GCIncRf(primstring);
  TValue length_value = __number_value(__jsstr_get_length(primstring));
  __jsobj_helper_init_value_property(obj, JSBUILTIN_STRING_LENGTH, length_value, JSPROP_DESC_HAS_VUWUEUC);
  // call __jsobj_initprop_fromString only when needed for performance
  //  __jsobj_initprop_fromString(obj, obj->shared.prim_string);
  return __object_value(obj);
}

// 15.1.4.5 Boolean ( . . . ), See 15.6.1 and 15.6.2.
// ecma 15.6.2
TValue __js_new_boo_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 0 || nargs == 1);
  __jsobject *obj = __create_object();
  __jsobj_set_prototype(obj, JSBUILTIN_BOOLEANPROTOTYPE);
  obj->object_class = JSBOOLEAN;
  obj->extensible = true;
  if (nargs == 0) {
    obj->shared.prim_bool = 0;
  } else {
    obj->shared.prim_bool = __js_ToBoolean(arg_list[0]);
  }
  return __object_value(obj);
}
TValue __js_new_boo_obj(TValue &arg) {
  __jsobject *obj = __create_object();
  __jsobj_set_prototype(obj, JSBUILTIN_BOOLEANPROTOTYPE);
  obj->object_class = JSBOOLEAN;
  obj->extensible = true;
  obj->shared.prim_bool = __js_ToBoolean(arg);
  return __object_value(obj);
}

// 15.1.4.6 Number ( . . . ), See 15.7.1 and 15.7.2.
// ecma 15.7.2
TValue __js_new_num_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 0 || nargs == 1);

  if (nargs == 1 && ((__is_nan(arg_list[0])) || (__is_infinity(arg_list[0])))) {
    __jsobject *obj = __create_object();
    __jsobj_set_prototype(obj, JSBUILTIN_NUMBERPROTOTYPE);
    obj->object_class = JSNUMBER;
    obj->object_type = JSSPECIAL_NUMBER_OBJECT;
    obj->extensible = true;
    __jsstring *primstring;
    primstring = __js_ToString(arg_list[0]);
    obj->shared.prim_string = primstring;
    GCIncRf(primstring);
    TValue length_value = __number_value(__jsstr_get_length(primstring));
    __jsobj_helper_init_value_property(obj, JSBUILTIN_STRING_LENGTH, length_value, JSPROP_DESC_HAS_VUWUEUC);
    return __object_value(obj);
  }

  bool isDouble = (nargs == 1 && __is_double(arg_list[0]));
  __jsobject *obj = __create_object();
  __jsobj_set_prototype(obj, JSBUILTIN_NUMBERPROTOTYPE);
  obj->object_class = isDouble ? JSDOUBLE : JSNUMBER;
  obj->extensible = true;
  if (nargs == 0) {
    obj->shared.prim_number = 0;
  } else {
    if (!isDouble)
      obj->shared.prim_number = __js_ToNumber(arg_list[0]);
    else
      obj->shared.primDouble = __jsval_to_double(arg_list[0]);
  }
  return __object_value(obj);
}

TValue __js_new_num_obj(TValue &arg) {
  if ((__is_nan(arg)) || (__is_infinity(arg))) {
    __jsobject *obj = __create_object();
    __jsobj_set_prototype(obj, JSBUILTIN_NUMBERPROTOTYPE);
    obj->object_class = JSNUMBER;
    obj->object_type = JSSPECIAL_NUMBER_OBJECT;
    obj->extensible = true;
    __jsstring *primstring;
    primstring = __js_ToString(arg);
    obj->shared.prim_string = primstring;
    GCIncRf(primstring);
    TValue length_value = __number_value(__jsstr_get_length(primstring));
    __jsobj_helper_init_value_property(obj, JSBUILTIN_STRING_LENGTH, length_value, JSPROP_DESC_HAS_VUWUEUC);
    return __object_value(obj);
  }

  bool isDouble = __is_double(arg);
  __jsobject *obj = __create_object();
  __jsobj_set_prototype(obj, JSBUILTIN_NUMBERPROTOTYPE);
  obj->object_class = isDouble ? JSDOUBLE : JSNUMBER;
  obj->extensible = true;
  if (!isDouble)
    obj->shared.prim_number = __js_ToNumber(arg);
  else
    obj->shared.primDouble = __jsval_to_double(arg);
  return __object_value(obj);
}


// 15.3.4 Properties of the Function Prototype Object
// The Function prototype object is itself a Function object
// (its [[Class]] is "Function") that, when invoked,
// accepts any arguments and returns undefined.
TValue __js_empty(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  return __undefined_value();
}

TValue __js_new_error_obj_base(TValue &this_object, TValue *arg_list, uint32_t nargs, __jsbuiltin_object_id protoid) {
  __jsobject *obj = __create_object();
  __jsobj_set_prototype(obj, protoid);
  obj->object_class = (JSBUILTIN_ERROR_PROTOTYPE == protoid) ? JSERROR: JSOBJECT;
  obj->object_type = JSREGULAR_OBJECT;
  obj->extensible = true;
  if (nargs >= 1) {
    TValue length_value = __number_value(__jsstr_get_length(__js_ToString(arg_list[0])));
    __jsobj_helper_init_value_property(obj, JSBUILTIN_STRING_LENGTH, length_value, JSPROP_DESC_HAS_VUWUEUC);
    __jsobj_helper_init_value_property(obj, JSBUILTIN_STRING_MESSAGE, arg_list[0], JSPROP_DESC_HAS_VUWUEUC);
  }
  obj->shared.prim_number = 0;
  return __object_value(obj);
}

TValue __js_new_reference_error_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  return __js_new_error_obj_base(this_object, arg_list, nargs, JSBUILTIN_REFERENCEERRORPROTOTYPE);
}

TValue __js_new_error_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  return __js_new_error_obj_base(this_object, arg_list, nargs, JSBUILTIN_ERROR_PROTOTYPE);
}

TValue __js_new_evalerror_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  return __js_new_error_obj_base(this_object, arg_list, nargs, JSBUILTIN_EVALERROR_PROTOTYPE);
}

TValue __js_new_rangeerror_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  return __js_new_error_obj_base(this_object, arg_list, nargs, JSBUILTIN_RANGEERROR_PROTOTYPE);
}

TValue __js_new_syntaxerror_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  return __js_new_error_obj_base(this_object, arg_list, nargs, JSBUILTIN_SYNTAXERROR_PROTOTYPE);
}

TValue __js_new_urierror_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  return __js_new_error_obj_base(this_object, arg_list, nargs, JSBUILTIN_URIERROR_PROTOTYPE);
}

TValue __js_new_type_error_obj(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  return __js_new_error_obj_base(this_object, arg_list, nargs, JSBUILTIN_TYPEERROR_PROTOTYPE);
}

TValue __js_isnan(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 1);
  //TValue resvalue;
  //resvalue.ptyp = JSTYPE_BOOLEAN;
  if(__is_string(arg_list[0])) {
    bool conv = true;
    TValue n = __js_str2double(__jsval_to_string(arg_list[0]), conv);
    return __boolean_value(__is_nan(n) ? 1 : 0);
      //resvalue.x.asbits = __is_nan(n) ? 1 : 0;
  } else if (__is_js_object(arg_list[0])) {
    bool isYConvertible = false;
    TValue res = __js_ToNumberSlow2(arg_list[0], isYConvertible);
    return __boolean_value(isYConvertible ? __is_nan(res) : 1);
    //resvalue.x.asbits = isYConvertible ? __is_nan(res) : 1;
  } else {
    return __boolean_value(__is_nan(arg_list[0]) || __is_undefined(arg_list[0]) ? 1 : 0);
    // resvalue.x.asbits = __is_nan(arg_list) || __is_undefined(arg_list[0]) ? 1 : 0;

  }
  //return(resvalue);
}

// 19.2.5
TValue __js_parseint(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs ==1 || nargs == 2);
  if (__is_nan(arg_list[0]) ||
      __is_undefined(arg_list[0])) {
    return(__nan_value());
  }

  __jsstring *jsstr;
  int32_t base = 10;

  // convert 2nd arg to radix
  if (nargs == 2) {
    if (__is_number(arg_list[1])) {
      base = __jsval_to_number(arg_list[1]);
    } else if (__is_double(arg_list[1])) { // ecma 15.1.2.2.6
      double d = __jsval_to_double(arg_list[1]);
      double f = floor(d);
      base = (long)f % 0x100000000;
    } else {
      base = __js_ToNumberSlow(arg_list[1]);
    }
  }
  if (base == 0) {
    base = 10;
  }
  if (base < 2 || base > 36) {
    return(__nan_value());
  }

  // convert 1st arg to string
  if (__is_string(arg_list[0])) {
    jsstr = __jsval_to_string(arg_list[0]);
  } else if ((__is_number(arg_list[0]) &&
               (__is_positive_zero(arg_list[0]) || __is_negative_zero(arg_list[0]))) ||
             (__is_double(arg_list[0]) &&
               (__is_positive_zero(arg_list[0])|| __is_negative_zero(arg_list[0])))) {
    // parseInt return pos 0 regardless of sign of zero
    return(__positive_zero_value());
  } else {
    jsstr = __js_ToStringSlow(arg_list[0]);
  }

  bool isNum;
  double val = __js_str2num_base_x(jsstr, base, isNum);
  if (!isNum) {
    return(__nan_value());
  }
  return __double_value(val);
}

// an empty function
TValue __js_eval() {
  MAPLE_JS_ASSERT(false);
}

TValue __js_parsefloat(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 1);
  MAPLE_JS_ASSERT(nargs == 1);
  bool isNum;

  if (__is_undefined(arg_list[0]) || __is_null(arg_list[0]) || __is_nan(arg_list[0])) {
    return(__nan_value());
  }
  if ((__is_number(arg_list[0]) &&
        (__is_positive_zero(arg_list[0]) || __is_negative_zero(arg_list[0]))) ||
      (__is_double(arg_list[0]) &&
        (__is_positive_zero(arg_list[0])|| __is_negative_zero(arg_list[0])))) {
      // parseFloat returns pos 0 regardless of sign of zero
      return(__positive_zero_value());
  } else if(__is_double(arg_list[0])) {
      return(*arg_list);
  } else if (__is_string(arg_list[0])) {
      return(__js_str2double2(__jsval_to_string(arg_list[0]), isNum));
  } else if (__is_number(arg_list[0])) {
      return(__js_str2double2(__js_NumberToString(arg_list[0].x.i32), isNum));
  } else if (__is_js_object(arg_list[0])) { //ecma 19.2.4.1 and 7.1.17
      __jsobject *obj = __jsval_to_object(arg_list[0]);
      if (obj->object_class == JSDOUBLE) {
        return __double_value(obj->shared.primDouble);
      }
      TValue prim = __js_ToPrimitive(arg_list[0], JSTYPE_STRING);
      if (__is_string(prim)) {
        return(__js_str2double2(__jsval_to_string(prim), isNum));
      } else {
        __jsstring *jsstr = __js_ToStringSlow(prim);
        return(__js_str2double2(jsstr, isNum));
      }
  } else if (__is_boolean(arg_list[0])) {
      return(__nan_value());
  } else if (__is_infinity(arg_list[0])) {
      return(__number_infinity());
  } else if (__is_nan(arg_list[0])) {
      return(__nan_value());
  }
  return __undefined_value();
}

// ecma 15.1.2.5
TValue __js_isfinite(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 1);
  //TValue resvalue;
  //resvalue.ptyp = JSTYPE_BOOLEAN;
  if(__is_string(arg_list[0])) {
    bool conv = true;
    TValue n = __js_str2double(__jsval_to_string(arg_list[0]), conv);
    //resvalue.x.asbits = __is_nan(n) || __is_infinity(n) ? 0 : 1;
    return __boolean_value(__is_nan(n) || __is_infinity(n) ? 0 : 1);
  } else if (__is_js_object(arg_list[0])) {
    bool isYConvertible = false;
    TValue res = __js_ToNumberSlow2(arg_list[0], isYConvertible);
    return __boolean_value(isYConvertible ? !__is_nan(res) : 0);
  } else {
    return __boolean_value(__is_nan(arg_list[0]) || __is_undefined(arg_list[0]) || __is_infinity(arg_list[0]) ? 0 : 1);

  }
  //return(resvalue);
}

// See jsencode.cpp for implementation
TValue __js_encodeuri(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 1);
  if(__is_string(arg_list[0]) || __is_js_object(arg_list[0])) {
      return __string_value(__jsop_encode_item(__js_ToString(arg_list[0]),false));
  }
  // just a test
  MAPLE_JS_URIERROR_EXCEPTION();
  return *arg_list;
}

// See jsencode.cpp for implementation
TValue __js_encodeuricomponent(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 1);
  if(__is_string(arg_list[0]) || __is_js_object(arg_list[0])) {
      return __string_value(__jsop_encode_item(__js_ToString(arg_list[0]),true));
  }
  return *arg_list;
}

// See jsencode.cpp for implementation
TValue __js_decodeuri(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 1);
  if(__is_string(arg_list[0]) || __is_js_object(arg_list[0])) {
      return __string_value(__jsop_decode_item(__js_ToString(arg_list[0]),false));
  }
  return *arg_list;
}

// See jsencode.cpp for implementation
TValue __js_decodeuricomponent(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 1);
  if(__is_string(arg_list[0]) || __is_js_object(arg_list[0])) {
      return __string_value(__jsop_decode_item(__js_ToString(arg_list[0]),true));
  }
  return *arg_list;
}

// 15.7.1 The Number Constructor Called as a Function
// When Number is called as a function rather than as a constructor, it performs a type conversion.
// return a Number value if input is a valid Number, otherwise return a Number Ojbect
TValue __js_new_numberconstructor(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 0 || nargs == 1);

  TValue num;
  if (nargs == 0) {
    __set_number(num, 0);
    return num;
  }
  if (nargs != 1) {
    MAPLE_JS_ASSERT(0 && "__js_new_numval NIY");
    return __undefined_value();
  }
  TValue arg = arg_list[0];
  TValue v;
  if (__is_string(arg)) {
    bool isNum = false;
    TValue v = __js_ToNumberSlow2(arg, isNum);
    arg = v;
  }

  if (__is_number(arg)) {
    __set_number(num, __js_ToNumber(arg));
    return num;
  }
  // return object if not a NUMBER
  __jsobject *obj = __create_object();
  __jsobj_set_prototype(obj, JSBUILTIN_NUMBERPROTOTYPE);
  obj->object_class = JSNUMBER;
  obj->extensible = true;

  if (__is_nan(arg) || __is_infinity(arg) || __is_undefined(arg)) {
    obj->object_type = JSSPECIAL_NUMBER_OBJECT;
    __jsstring *primstring;
    if (__is_undefined(arg)) {
        TValue nan = __nan_value();
        primstring = __js_ToString(nan);
    } else
        primstring = __js_ToString(arg);
    obj->shared.prim_string = primstring;
    GCIncRf(primstring);
  } else {
    if (__is_double(arg)) {
      // create object with double type
      obj->object_class = JSDOUBLE;
      obj->shared.primDouble = __jsval_to_double(arg);
    } else {
      obj->object_class = JSNUMBER;
      obj->shared.prim_number = __js_ToNumber(arg);
    }
  }
  return __object_value(obj);
}


// 15.6.1 The Boolean Constructor Called as a Function
TValue __js_new_booleanconstructor(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 0 || nargs == 1);
  //bool val = (nargs == 0) ? false : __js_ToBoolean(arg_list[0]);
  //TValue ret;
  //ret.x.asbits = 0;
  //__set_boolean(ret, val);
  //return ret;
  return __boolean_value((nargs == 0) ? false : __js_ToBoolean(arg_list[0]));
}

// 15.5.1 The String Constructor Called as a Function
TValue __js_new_stringconstructor(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  MAPLE_JS_ASSERT(nargs == 0 || nargs == 1);
  TValue ret;
  __jsstring *primstring = (nargs == 0) ? __jsstr_get_builtin(JSBUILTIN_STRING_EMPTY) : __js_ToString(arg_list[0]);
  __set_string(ret, primstring);
  return ret;
}

// 15.8 Math Object
TValue __js_new_math_obj(TValue &this_object) {
  __jsobject *obj = __jsobj_get_or_create_builtin(JSBUILTIN_MATH);
  return __object_value(obj);
}

// 15.12 Json Object
TValue __js_new_json_obj(TValue &this_object) {
  __jsobject *obj = __jsobj_get_or_create_builtin(JSBUILTIN_JSON);
  return __object_value(obj);
}

// B.2.1
TValue __js_escape(TValue &this_object, TValue *str) {
  // TODO
  return *str;
}

TValue __js_new_arraybufferconstructor(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  uint32 length = 0;
  if (nargs == 1) {
    TValue lenVal = arg_list[0];
    if (!__is_number(arg_list[0])) {
      MAPLE_JS_ASSERT(false);
    } else {
      length = __js_ToUint32(lenVal);
    }
  }
  __jsobject *arr = __create_object();
  arr->object_class = JSARRAYBUFFER;
  arr->extensible = true;
  __jsobj_set_prototype(arr, JSBUILTIN_ARRAYBUFFER_PROTOTYPE);
  arr->object_type = JSREGULAR_ARRAY;
  __jsarraybyte *arrayByte = (__jsarraybyte *)VMMallocGC(sizeof(__jsarraybyte));
  arrayByte->length = (length > INT32_MAX) ? __double_value((double)length) : __number_value(length);
  arrayByte->arrayRaw = (uint8_t *)VMMallocGC(sizeof(uint8_t) * length);
  arr->shared.arrayByte = arrayByte;
  return __object_value(arr);
}

TValue __js_new_dataviewconstructor(TValue &this_object, TValue *arg_list, uint32_t nargs) {
  __jsobject *arr = __create_object();
  arr->object_class = JSDATAVIEW;
  arr->extensible = true;
  __jsobj_set_prototype(arr, JSBUILTIN_DATAVIEW_PROTOTYPE);
  arr->object_type = JSREGULAR_OBJECT;
  MAPLE_JS_ASSERT(nargs >= 1 && nargs <= 3);
  TValue arg0 = arg_list[0];
  __jsobject *bfObj = __jsval_to_object(arg0);
  MAPLE_JS_ASSERT(bfObj->object_class == JSARRAYBUFFER);
  TValue *lengthVal = (TValue *)bfObj->shared.arrayByte;
  uint32_t startIdx = 0;
  uint32_t endIdx = __jsval_to_uint32(*lengthVal);
  if(nargs == 2){
    startIdx = __jsval_to_uint32(arg_list[1]);
  } else {
    startIdx = __jsval_to_uint32(arg_list[1]);
    endIdx = __jsval_to_uint32(arg_list[2]);
  }
  __jsdataview *props =  (__jsdataview *)VMMallocGC(sizeof(__jsdataview));
  arr->shared.dataView = props;
  props->arrayByte = bfObj->shared.arrayByte;
  props->startIndex = startIdx;
  props->endIndex = endIdx;
  return __object_value(arr);
}
