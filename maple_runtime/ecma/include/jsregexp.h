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

#ifndef JSREGEXP_H
#define JSREGEXP_H

#include <pcre.h>
#include "jsvalue.h"

__jsobject *__js_ToRegExp(__jsstring *jsstr);

TValue __js_new_regexp_obj(TValue this_value, TValue *arg_list, uint32_t nargs);

TValue __jsregexp_Exec(TValue &this_value, TValue &value);
TValue __jsregexp_Test(TValue &this_value, TValue &value);
TValue __jsregexp_ToString(TValue &this_value);

void CheckAndSetFlagOptions(__jsstring *s, __jsstring *js_pattern,
                            bool& global, bool& ignorecase, bool& multiline);

dart::jscre::JSRegExp *RegExpCompile(__jsstring *js_pattern,
                                     bool ignorecase,
                                     bool multiline, 
                                     unsigned int *num_captures, 
                                     const char **error_message, 
                                     dart::jscre::malloc_t *alloc_func, 
                                     dart::jscre::free_t *free_func);

int RegExpExecute(const dart::jscre::JSRegExp *re, __jsstring *js_subject,
				  int start_offset, int *offsets, int offset_count);

void* RegExpAlloc(size_t size);
void RegExpFree(void *ptr);

TValue __jsregexp_Source(TValue &this_arg);
TValue __jsregexp_Global(TValue &this_arg);
TValue __jsregexp_Ignorecase(TValue &this_arg);
TValue __jsregexp_Multiline(TValue &this_arg);

#endif // JSREGEXP_H
