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

#include "jsvalue.h"
#include "jsstring.h"
#define ExponentBias 1023
#define ExponentShift 52

#define SignBit 0x8000000000000000ULL
#define SignificandBits 0x000fffffffffffffULL

using namespace maple;

double __js_toDecimal(uint16_t *start, uint16_t *end, int base, uint16_t **pos);
uint64_t __js_strtod(uint16_t *start, uint16_t *end, uint16_t **pos);
bool __js_chars2num(uint16_t *chars, int32_t length, uint64_t *result);
uint64_t __js_str2num(__jsstring *str);
double __js_str2num_base_x(__jsstring *str, int32_t base, bool& isNum);
__jsstring *__jsnum_integer_encode(int value, int base);
// ecma 15.7.4.2
TValue __jsnum_pt_toString(TValue &this_object, TValue &radix);

// ecma 15.7.4.3
TValue __jsnum_pt_toLocaleString(TValue &this_object, TValue *args, uint32_t num_args);

// ecma 15.7.4.4
TValue __jsnum_pt_valueOf(TValue &this_object);

// ecma 15.7.4.5
TValue __jsnum_pt_toFixed(TValue &this_number, TValue &fracdigit);

// ecma 15.7.4.6
TValue __jsnum_pt_toExponential(TValue &this_number, TValue &fractdigit);

// ecma 15.7.4.7
TValue __jsnum_pt_toPrecision(TValue &this_number, TValue &precision);

int32_t __js_str2num2(__jsstring *, bool &, bool);
TValue __js_str2double(__jsstring *, bool &);
TValue __js_str2double2(__jsstring *, bool &);
bool __is_double_no_decimal(double);
bool __is_double_to_int(double);
