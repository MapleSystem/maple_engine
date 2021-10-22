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

#ifndef JSARRAY_H
#define JSARRAY_H
#include "jsvalue.h"
#include "jstycnv.h"
#include "jsobject.h"

#define SPACE_UNIT 5
// maximum index number use index mode
// otherwise use string to get/set property
#define ARRAY_MAXINDEXNUM_INTERNAL 0x10000

using namespace maple;

enum __jsarr_iter_type {
  JSARR_EVERY = 0,
  JSARR_SOME,
  JSARR_FOREACH,
  JSARR_MAP,
  JSARR_FILTER,
  JSARR_FIND,
};

// Helper function for internal use.
__jsobject *__js_new_arr_internal(uint32_t len);
void __jsarr_internal_DefineOwnProperty(__jsobject *a, TValue &p, __jsprop_desc desc, bool throw_p);
void __jsarr_internal_DefineOwnPropertyByValue(__jsobject *a, uint32_t, __jsprop_desc desc, bool throw_p);

void __jsarr_internal_DefineLengthProperty(__jsobject *a, __jsprop_desc desc, bool throw_p);

void __jsarr_internal_DefineElemProperty(__jsobject *a, uint32_t index, __jsprop_desc desc, bool throw_p);
// __jsarr_getIndex(p) returns MAX_ARRAY_INDEX if p is not an index
uint32_t __jsarr_getIndex(TValue &p);

void __jsarr_internal_MoveElem(__jsobject *arr, uint32_t to_idx, uint32_t from_idx);

void __jsarr_internal_MoveElem(__jsobject *o, TValue *arr, uint32_t to_idx, uint32_t from_idx);

void __set_generic_elem(__jsobject *arr, uint32_t index, TValue &v);
void __set_regular_elem(TValue *arr, uint32_t index, TValue &v);

__jsstring *__jsarr_ElemToString(TValue &elem);

__jsstring *__jsarr_JoinOnce(TValue &elem, __jsstring *r, __jsstring *sep);

// idx is guaranteed outside to be less than length
// equivalent to __jsobj_internal_Get, not __jsobj_internal_GetOwnProperty
TValue __jsarr_GetRegularElem(__jsobject *o, TValue *arr, uint32_t idx);

TValue __jsarr_GetElem(__jsobject *o, uint32_t idx);

TValue *__jsarr_RegularRealloc(TValue *arr, uint32_t old_len, uint32_t new_len);

// ecma 15.4.2.1
__jsobject *__js_new_arr_elems(TValue &items, uint32_t length);
__jsobject *__js_new_arr_elems_direct(TValue *items, uint32_t length);
// ecma 15.4.2.2
__jsobject *__js_new_arr_length(TValue l);
// ecma 15.4.3.2
TValue __jsarr_isArray(TValue &this_array, TValue &arg);
// ecma 15.4.4.2
TValue __jsarr_pt_toString(TValue &this_array);
// ecma 15.4.4.3
TValue __jsarr_pt_toLocaleString(TValue &this_array);
// ecma 15.4.4.4
TValue __jsarr_pt_concat(TValue &this_array, TValue *items, uint32_t size);
// ecma 15.4.4.5
TValue __jsarr_pt_join(TValue &this_array, TValue &separator);
// ecma 15.4.4.6
TValue __jsarr_pt_pop(TValue &this_array);
// ecma 15.4.4.7
TValue __jsarr_pt_push(TValue &this_array, TValue *items, uint32_t size);
// ecma 15.4.4.8
TValue __jsarr_pt_reverse(TValue &this_array);
// ecma 15.4.4.9
TValue __jsarr_pt_shift(TValue &this_array);
// ecma 15.4.4.10
TValue __jsarr_pt_slice(TValue &this_array, TValue &start, TValue &end);
// ecma 15.4.4.11
TValue __jsarr_pt_sort(TValue &this_array, TValue &comparefn);
// ecma 15.4.4.12
TValue __jsarr_pt_splice(TValue &this_array, TValue *items, uint32_t size);
// ecma 15.4.4.13
TValue __jsarr_pt_unshift(TValue &this_array, TValue *items, uint32_t size);
// ecma 15.4.4.14
TValue __jsarr_pt_indexOf(TValue &this_array, TValue *arg_list, uint32_t argNum);
// ecma 15.4.4.15
TValue __jsarr_pt_lastIndexOf(TValue &this_array, TValue *arg_list, uint32_t argNum);
// ecma 15.4.4.16
TValue __jsarr_pt_every(TValue &this_array, TValue *arg_list, uint32_t argNum);
// ecma 15.4.4.17
TValue __jsarr_pt_some(TValue &this_array, TValue *arg_list, uint32_t argNum);
// ecma 15.4.4.18
TValue __jsarr_pt_forEach(TValue &this_array, TValue *arg_list, uint32_t argNum);
// ecma 15.4.4.19
TValue __jsarr_pt_map(TValue &this_array, TValue *arg_list, uint32_t argNum);
// ecma 15.4.4.20
TValue __jsarr_pt_filter(TValue &this_array, TValue *arg_list, uint32_t argNum);
// ecma 15.4.4.21
TValue __jsarr_pt_reduce(TValue &this_array, TValue *arg_list, uint32_t argNum);
// ecma 15.4.4.22
TValue __jsarr_pt_reduceRight(TValue &this_array, TValue *arg_list, uint32_t argNum);
TValue __jsarr_internal_reduce(TValue &this_array, TValue *arg_list, uint32_t argNum, bool right_flag);
// ecma 23.1.2.3 Array.of(..items)
TValue __jsarr_pt_of(TValue &arr, TValue *items, uint32_t size);
// ecma 23.1.2.1 Array.from(items[,mapfn[,thisArg]])
TValue __jsarr_pt_from(TValue &arr, TValue *args, uint32_t count);
// ecma 22.1.3.8 Array.prototype.find (predicate[,thisArg])
TValue __jsarr_pt_find(TValue &this_array, TValue *arg_list, uint32_t argNum);
#endif
