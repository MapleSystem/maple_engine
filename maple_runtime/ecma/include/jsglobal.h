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

#ifndef JSGLOBAL_H
#define JSGLOBAL_H
#include "jsvalue.h"

using namespace maple;

// ecma 15.1.4.1 Object ( . . . ), See 15.2.1 and 15.2.2.
// ecma 15.2.1
// ecma 15.2.2
__jsobject *__js_new_obj_obj_0();
__jsobject *__js_new_obj_obj_1(TValue &);
TValue __js_new_obj(TValue &, TValue *, uint32_t);
TValue __js_new_arr(TValue &, TValue *, uint32_t);
// ecma 15.5.2
TValue __js_new_str_obj(TValue &, TValue *, uint32_t);
maple::TValue __js_new_str_obj(maple::TValue &);
// 15.1.4.5 Boolean ( . . . ), See 15.6.1 and 15.6.2.
// For 15.6.1 Use __js_ToBoolean instead.
// ecma 15.6.2
TValue __js_new_boo_obj(TValue &, TValue *, uint32_t );
maple::TValue __js_new_boo_obj(maple::TValue &);
// 15.1.4.6 Number ( . . . ), See 15.7.1 and 15.7.2.
// ecma 15.7.1  Use __js_ToNumer instead.
// ecma 15.7.2.
TValue __js_new_num_obj(TValue &, TValue *, uint32_t);
maple::TValue __js_new_num_obj(maple::TValue &);
// 15.3.4 Properties of the Function Prototype Object.
TValue __js_empty(TValue &, TValue *, uint32_t);
TValue __js_isnan(TValue &, TValue *, uint32_t);
TValue __js_new_reference_error_obj(TValue &, TValue *, uint32_t);
TValue __js_new_error_obj(TValue &, TValue *, uint32_t);
TValue __js_new_evalerror_obj(TValue &, TValue *, uint32_t);
TValue __js_new_rangeerror_obj(TValue &, TValue *, uint32_t);
TValue __js_new_syntaxerror_obj(TValue &, TValue *, uint32_t);
TValue __js_new_urierror_obj(TValue &, TValue *, uint32_t);
TValue __js_new_type_error_obj(TValue &, TValue *, uint32_t);

TValue __js_parseint(TValue &, TValue *, uint32_t);
TValue __js_decodeuri(TValue &, TValue *, uint32_t);
TValue __js_decodeuricomponent(TValue &, TValue *, uint32_t);
TValue __js_parsefloat(TValue &, TValue *, uint32_t);
TValue __js_isfinite(TValue &, TValue *, uint32_t);
TValue __js_encodeuri(TValue &, TValue *, uint32_t);
TValue __js_encodeuricomponent(TValue &, TValue *, uint32_t);
TValue __js_eval();
TValue __js_escape(TValue &, TValue *);

// 15.7.1 The Number Constructor Called as a Function
TValue __js_new_numberconstructor(TValue &, TValue *, uint32_t);
// 15.6.1 The Boolean Constructor Called as a Function
TValue __js_new_booleanconstructor(TValue &, TValue *, uint32_t);
// 15.5.1 The String Constructor Called as a Function
TValue __js_new_stringconstructor(TValue &, TValue *, uint32_t);
TValue __js_new_math_obj(TValue &);
TValue __js_new_json_obj(TValue &);
TValue __js_new_arraybufferconstructor(TValue &, TValue *, uint32_t);
TValue __js_new_dataviewconstructor(TValue &, TValue *, uint32_t);
#endif
