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

#ifndef JSSTRING_H
#define JSSTRING_H
#include "jsvalue.h"
#include <string>

using namespace maple;

enum __jsbuiltin_string_id {
#define JSBUILTIN_STRING_DEF(id, length, builtin_string) id,
#include "jsbuiltinstrings.inc.h"
#undef JSBUILTIN_STRING_DEF
  JSBUILTIN_STRING_LAST
};

enum __jsstring_type : uint8_t {
    JSSTRING_UNICODE = 0x1,    // Bit is set for 16-bit code units
    JSSTRING_GEN = 0x2,        // Bit is set for non-const code units
    JSSTRING_BUILTIN = 0x4     // Bit is set for built-in strings
};

// Compress js-string's representation.
// |..8 bits..|..8 bits..|..8 bits..|..8 bits..|....actual chars's bytes....|
// | CLASS    |ID-bistring|LENGTH1  | LENGTH2  |      actual data           |

typedef struct {
    __jsstring_type        kind    : 8;
    __jsbuiltin_string_id  builtin : 8;
    uint16_t               length;
    union {
        char               ascii[0];
        char16_t           utf16[0];
    } x;
} __jsstring;

typedef char16_t __jschar;

inline uint32_t __jsstr_get_length(__jsstring *str) {
    return (uint32_t)str->length;
}

inline bool __jsstr_is_ascii(__jsstring *str) {
  return (str->kind & JSSTRING_UNICODE) == 0;
}

bool __is_UnicodeSpace(wchar_t c);

void __jsstr_dump(__jsstring *str);
void __jsstr_print(__jsstring *str, FILE *stream = stdout);
void __jsstr_set_char(__jsstring *str, uint32_t index, uint16_t ch);
void __jsstr_copy(__jsstring *to, uint32_t to_index, __jsstring *from);
uint32_t __jsstr_get_bytesize(__jsstring *str);
uint16_t __jsstr_get_char(__jsstring *str, uint32_t index);
__jsstring *__jsstr_get_builtin(__jsbuiltin_string_id id);
__jsstring *__jsstr_new_from_char(const char *ch);
bool __jsstr_equal_to_builtin(__jsstring *str, __jsbuiltin_string_id id);
bool __jsstr_equal(__jsstring *str1, __jsstring *str2, bool checknumber = false);
bool __jsstr_ne(__jsstring *str1, __jsstring *str2);
__jsstring *__jsstr_append_char(__jsstring *str, const __jschar ch);
__jsstring *__jsstr_concat_2(__jsstring *left, __jsstring *right);
__jsstring *__jsstr_concat_3(__jsstring *str1, __jsstring *str2, __jsstring *str3);
int32_t __jsstr_compare(__jsstring *lhs, __jsstring *rhs);
void __jsstr_copy_char2string(__jsstring *str, const char *src);
bool __js_isSpace(uint16_t ch);
uint16_t *__js_skipSpace(uint16_t *s, uint16_t *end);
__jsstring *__js_new_string_internal(uint32_t length, bool is_unicode);
TValue __jsstr_fromCharCode(TValue &this_string, TValue *array, uint32_t size);
TValue __jsstr_toString(TValue &this_string);
TValue __jsstr_valueOf(TValue &this_string);
TValue __jsstr_charAt(TValue &this_string, TValue &idx);
TValue __jsstr_charCodeAt(TValue &this_string, TValue &idx);
TValue __jsstr_concat(TValue &this_string, TValue *arr, uint32_t size);
TValue __jsstr_indexOf(TValue &this_string, TValue &search, TValue &pos);
TValue __jsstr_lastIndexOf(TValue &this_string, TValue &search, TValue &pos);
TValue __jsstr_localeCompare(TValue &this_string, TValue *args, uint32_t nargs);
TValue __jsstr_slice(TValue &this_string, TValue &start, TValue &end);
TValue __jsstr_substr(TValue &this_string, TValue &start, TValue &end);
TValue __jsstr_substring(TValue &this_string, TValue &start, TValue &end);
TValue __jsstr_toLowerCase(TValue &this_string);
TValue __jsstr_toLocaleLowerCase(TValue &this_string);
TValue __jsstr_toUpperCase(TValue &this_string);
TValue __jsstr_toLocaleUpperCase(TValue &this_string);
TValue __jsstr_trim(TValue &this_string);
TValue __jsstr_split(TValue &this_string, TValue &separator, TValue &limit);
TValue __jsstr_match(TValue &this_string, TValue &regexp);
TValue __jsstr_search(TValue &this_string, TValue &regexp);
TValue __jsstr_replace(TValue &this_string, TValue &search, TValue &replace);
__jsstring *__jsstr_extract(__jsstring *from, uint32_t from_index, uint32_t length);
std::wstring __jsstr_to_wstring(__jsstring *s, int offset = 0);
uint32_t __jsstr_is_number(__jsstring *);
uint32_t __jsstr_is_numidx(__jsstring *p, bool &isNum);
bool __jsstr_throw_typeerror(__jsstring *);
#endif
