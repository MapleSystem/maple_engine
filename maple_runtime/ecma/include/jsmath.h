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
#include "jsvalueinline.h"
#include "jstycnv.h"
#include "vmmemory.h"
TValue __jsmath_pt_abs(TValue &this_math, TValue &value);
TValue __jsmath_pt_ceil(TValue &this_math, TValue &value);
TValue __jsmath_pt_floor(TValue &this_math, TValue &value);
TValue __jsmath_pt_max(TValue &this_math, TValue *items, uint32_t size);
TValue __jsmath_pt_min(TValue &this_math, TValue *items, uint32_t size);
TValue __jsmath_pt_pow(TValue &this_math, TValue &x, TValue &y);
TValue __jsmath_pt_round(TValue &this_math, TValue &value);
TValue __jsmath_pt_sqrt(TValue &this_math, TValue &value);
TValue __jsmath_pt_sin(TValue &this_math, TValue &value);
TValue __jsmath_pt_asin(TValue &this_math, TValue &value);
TValue __jsmath_pt_cos(TValue &this_math, TValue &value);
TValue __jsmath_pt_acos(TValue &this_math, TValue &value);
TValue __jsmath_pt_tan(TValue &this_math, TValue &value);
TValue __jsmath_pt_atan(TValue &this_math, TValue &value);
TValue __jsmath_pt_atan2(TValue &this_math, TValue &y, TValue &x);
TValue __jsmath_pt_log(TValue &this_math, TValue &value);
TValue __jsmath_pt_exp(TValue &this_math, TValue &value);
TValue __jsmath_pt_random(TValue &this_math);

inline bool __is_int32_range(double t) {
  return t >= (int32_t)0x80000000 && t <= (int32_t)0x7fffffff;
}
