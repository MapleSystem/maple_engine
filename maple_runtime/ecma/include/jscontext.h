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

#ifndef JSCONTEXT_H
#define JSCONTEXT_H
#include "jsvalue.h"
#define UNCERTAIN_NARGS 0x7FFFFFFF

using namespace maple;

extern TValue __js_Global_ThisBinding;
extern TValue __js_ThisBinding;
extern bool __is_global_strict;

void __js_init_context(bool);
TValue __js_entry_function(TValue &this_arg, bool strict_p);
void __js_exit_function(TValue &this_arg, TValue old_this, bool strict_p);
__jsstring *__jsstr_get_builtin(__jsbuiltin_string_id id);
__jsobject *__jsobj_get_or_create_builtin(__jsbuiltin_object_id id);
#ifdef MEMORY_LEAK_CHECK
void __jsobj_release_builtin();
#endif
TValue __jsop_call(TValue &function, TValue &this_arg, TValue *arg_list, uint32_t arg_count);
void __jsop_print_item(TValue value);
TValue __jsop_add(TValue &x, TValue &y);
TValue __jsop_object_mul(TValue &x, TValue &y);
TValue __jsop_object_sub(TValue &x, TValue &y);
TValue __jsop_object_div(TValue &x, TValue &y);
TValue __jsop_object_rem(TValue &x, TValue &y);
bool __js_AbstractEquality(TValue &x, TValue &y, bool &);
bool __js_AbstractRelationalComparison(TValue &x, TValue &y, bool &, bool leftFirst = true);
bool __jsop_instanceof(TValue &x, TValue &y);
bool __jsop_in(TValue &x, TValue &y);
bool __jsop_stricteq(TValue &x, TValue &y);
bool __jsop_strictne(TValue &x, TValue &y);
// unary operations
TValue __jsop_typeof(TValue &v);
TValue __jsop_new(TValue &constructor, TValue this_arg, TValue *arg_list, uint32_t nargs);

bool __js_ToBoolean(TValue &v);
// Object.
__jsobject *__js_new_obj_obj_0();
__jsobject *__js_new_obj_obj_1(TValue &v);
void __jsop_setprop(TValue &o, TValue &p, TValue &v);
TValue __jsop_getprop(TValue &o, TValue &p);
void __jsop_setprop_by_name(TValue &o, __jsstring *p, TValue &v, bool isStrict);
void __jsop_set_this_prop_by_name(TValue &o, __jsstring *p, TValue &v, bool noThrowTE = false);
void __jsop_init_this_prop_by_name(TValue &o, __jsstring *name);
TValue __jsop_getprop_by_name(TValue &o, __jsstring *p);
TValue __jsop_get_this_prop_by_name(TValue &o, __jsstring *p);
TValue __jsop_delprop(TValue &o, TValue &p);
void __jsop_initprop_by_name(TValue &o, __jsstring *p, TValue &v);
void __jsop_initprop_getter(TValue &o, TValue &p, TValue &v);
void __jsop_initprop_setter(TValue &o, TValue &p, TValue &v);
// Array
__jsobject *__js_new_arr_elems(TValue &items, uint32_t len);
__jsobject *__js_new_arr_elems_direct(TValue *items, uint32_t length);
__jsobject *__js_new_arr_length(TValue len);
uint32_t __jsop_length(TValue &data);
// String
TValue __js_new_string(uint16_t *data);
#endif
