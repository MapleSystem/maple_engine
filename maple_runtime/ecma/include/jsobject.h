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

#ifndef JSOBJECT_H
#define JSOBJECT_H
#include "jsvalue.h"
#include "jsstring.h"
#include "jsfunction.h"
#include "jscontext.h"
#include "jsdataview.h"
#include <map>
#include <string>

#define JSPROP_HAS_GET 0x01
#define JSPROP_HAS_SET 0x02
#define JSPROP_HAS_VALUE 0x04
#define JSPROP_UNDEFINED 0x40

#define MAX_ARRAY_INDEX 0xFFFFFFFF
#define MAX_LENGTH_PROPERTY_SIZE 9007199254740991 // 2^53 -1

#define JSPROP_DESC_HAS_ATTR 0x02
#define JSPROP_DESC_ATTR_TRUE 0x03
#define JSPROP_DESC_ATTR_FALSE 0x02
// HAS VALUE FALSE, WRITABLE TRUE,  ENUMERABLE TRUE, CONFIGURABLE FALSE.
#define JSPROP_DESC_HAS_UVWEUC \
  (uint32_t)(JSPROP_DESC_ATTR_FALSE << 16 | JSPROP_DESC_ATTR_TRUE << 8 | JSPROP_DESC_ATTR_TRUE)
// HAS VALUE, No attrs.
#define JSPROP_DESC_HAS_V (uint32_t)(JSPROP_HAS_VALUE << 24)
// HAS VALUE, WRITABLE TRUE,  ENUMERABLE TRUE, CONFIGURABLE TRUE.
#define JSPROP_DESC_HAS_VWEC \
  (uint32_t)(JSPROP_HAS_VALUE << 24 | JSPROP_DESC_ATTR_TRUE << 16 | JSPROP_DESC_ATTR_TRUE << 8 | JSPROP_DESC_ATTR_TRUE)
// HAS VALUE, WRITABLE FALSE,  ENUMERABLE FALSE, CONFIGURABLE FALSE.
#define JSPROP_DESC_HAS_VUWUEUC                                                                    \
  (uint32_t)(JSPROP_HAS_VALUE << 24 | JSPROP_DESC_ATTR_FALSE << 16 | JSPROP_DESC_ATTR_FALSE << 8 | \
             JSPROP_DESC_ATTR_FALSE)
// HAS VALUE, WRITABLE FALSE,  ENUMERABLE FALSE, CONFIGURABLE TRUE.
#define JSPROP_DESC_HAS_VUWUEC                                                                    \
  (uint32_t)(JSPROP_HAS_VALUE << 24 | JSPROP_DESC_ATTR_TRUE << 16 | JSPROP_DESC_ATTR_FALSE << 8 | \
             JSPROP_DESC_ATTR_FALSE)
// HAS VALUE, WRITABLE TRUE,  ENUMERABLE FALSE, CONFIGURABLE TRUE.
#define JSPROP_DESC_HAS_VWUEC \
  (uint32_t)(JSPROP_HAS_VALUE << 24 | JSPROP_DESC_ATTR_TRUE << 16 | JSPROP_DESC_ATTR_FALSE << 8 | JSPROP_DESC_ATTR_TRUE)
// HAS VALUE, WRITABLE TRUE,  ENUMERABLE FALSE, CONFIGURABLE FALSE.
#define JSPROP_DESC_HAS_VWUEUC                                                                     \
  (uint32_t)(JSPROP_HAS_VALUE << 24 | JSPROP_DESC_ATTR_FALSE << 16 | JSPROP_DESC_ATTR_FALSE << 8 | \
             JSPROP_DESC_ATTR_TRUE)
// NO VALUE, WRITABLE FALSE,  ENUMERRABLE FALSE, CONFIGURABLE TRUE
#define JSPROP_DESC_HAS_UVUWUEC  \
  (uint32_t)(JSPROP_DESC_ATTR_TRUE << 16 | JSPROP_DESC_ATTR_FALSE << 8)

// HAS GET, ENUMERABLE TRUE, CONFIGURABLE TRUE.
#define JSPROP_DESC_HAS_GEC (uint32_t)(JSPROP_HAS_GET << 24 | JSPROP_DESC_ATTR_TRUE << 16 | JSPROP_DESC_ATTR_TRUE << 8)
// HAS SET, ENUMERABLE TRUE, CONFIGURABLE FALSE.
#define JSPROP_DESC_HAS_SEC (uint32_t)(JSPROP_HAS_SET << 24 | JSPROP_DESC_ATTR_TRUE << 16 | JSPROP_DESC_ATTR_TRUE << 8)

// ecma 8.6.1
struct __attribute__((packed)) __jsprop_desc {
  union {
    struct {
      TValue value;
    } named_data_property;
    struct {
      __jsobject *get;
      __jsobject *set;
    } named_accessor_property;
  };
  union {
    struct {
      uint8_t attr_writable;
      uint8_t attr_enumerable;
      uint8_t attr_configurable;
      uint8_t fields;
    } s;
    uint32_t attrs;
  };
};

#define USE_PROP_MAP 1

struct __attribute__((packed)) __jsprop {
  union {
    uint32_t index; // the jsprop can be a integer
    __jsstring *name;
  }n;
#ifdef USE_PROP_MAP
  struct __jsprop *prev;
#endif
  struct __jsprop *next;
  __jsprop_desc desc;
  bool isIndex;
};

struct __attribute__((packed)) __jsfast_prop {

  __jsstring *name;
  struct __jsfast_prop *next;
  TValue v;
};

// when any new class is added, the table for class names defined in function
// __jsobj_helper_get_object_class_name() should be updated also
enum __jsobj_class : uint8_t {
  JSGLOBAL,
  JSOBJECT,
  JSFUNCTION,
  JSARRAY,
  JSSTRING,
  JSBOOLEAN,
  JSNUMBER,
  JSMATH,
  JSDATE,
  JSREGEXP,
  JSON,
  JSERROR,
  JSARGUMENTS,
  JSINTL_COLLATOR,
  JSINTL_NUMBERFORMAT,
  JSINTL_DATETIMEFORMAT,
  JSOBJ_CLASS_LAST,
  JSDOUBLE,
  JSARRAYBUFFER,
  JSDATAVIEW,
};

// Implementation-dependent.
// Not ecma standard.
// field: object_type in __jsobject.
enum __jsobj_type {
  // Generic object.
  JSGENERIC = 0,
  // Extern object, which created by plug-in script.
  JSEXTERN,
  // A regular object mean have no own accessor property or own unwritable data property,
  // and the EXTENSIBLE is true.
  JSREGULAR_OBJECT,
  // A regular array only have simple values for data properties with default
  // attributes and the length property.
  // Storage-mode of a regular array:
  //     Length property: obj.shared.array_props[0];
  //     Elem0: obj.shared.array_props[1];
  //     Elem1: obj.shared.array_props[2];
  //     ...
  JSREGULAR_ARRAY,
  // Special Number object for NaN and Infinity
  JSSPECIAL_NUMBER_OBJECT,
};

struct __jsobject {
  // General properties' list.
  // Includes named data or accessor properties as ecma defined.
  __jsprop *prop_list;
#ifdef USE_PROP_MAP
  std::map<uint32_t, __jsprop *> *prop_index_map;
  std::map<__jsstring *, __jsprop *> *prop_string_map;
#endif
  // The prototype of this object.
  // Use id iff proto_is_builtin is true.
  union {
    __jsobject *obj;
    __jsbuiltin_object_id id;
  } prototype;
  // If true, own properties may be added to the object.
  // Can be compressed to one bit.
  uint8_t extensible;
  // Indicate a specification defined classification of objects.
  // Can be compressed to three bits.
  __jsobj_class object_class;
  // Implementation-dependent.
  // This field indicate the storage-mode of Object, Array, Function and etc.
  // See __jsobj_type.
  uint8_t object_type : 4;
  uint8_t is_builtin : 2;
  uint8_t proto_is_builtin : 2;
  // Used iff this object is a ecma builtin object.
  __jsbuiltin_object_id builtin_id;
  // Implementation-dependent.
  // A shared field for each classification of objects.
  union {
    // Simple name-value pairs for named data properties with default
    // attributes.
    // Linear memory storage ???
    __jsfast_prop *fast_props;
    // Simple values for array-properties with an "Index" name and default
    // attributes.
    TValue *array_props;
    // For function objects.
    __jsfunction *fun;
    // Primitive Value for string-object.
    __jsstring *prim_string;
    // Primitive Value for number-object.
    int32_t prim_number;
    // Primitive Value for boolean-object.
    int32_t prim_bool;
    __jsprop_desc *arr;
    double primDouble;
    // for ArrayBuffer
    __jsarraybyte *arrayByte;
    __jsdataview *dataView;
  } shared;
};

static inline void InitProp(__jsprop *prop, __jsprop_desc propDesc, uint32_t index) {
  prop->isIndex = true;
#ifdef USE_PROP_MAP
  prop->prev = nullptr;
#endif
  prop->next = nullptr;
  prop->desc = propDesc;
  prop->n.index = index;
}

static inline void InitProp(__jsprop *prop, __jsprop_desc propDesc, __jsstring *name) {
  prop->isIndex = false;
#ifdef USE_PROP_MAP
  prop->prev = nullptr;
#endif
  prop->next = nullptr;
  prop->desc = propDesc;
  prop->n.name = name;
}

static inline void __jsobj_set_prototype(__jsobject *obj, __jsbuiltin_object_id proto_id) {
  obj->proto_is_builtin = true;
  obj->prototype.id = proto_id;
}

void __jsobj_set_prototype(__jsobject *obj, __jsobject *proto_obj);
__jsobject *__jsobj_get_or_create_builtin(__jsbuiltin_object_id id);
static inline __jsobject *__jsobj_get_prototype(__jsobject *obj) {
  if (obj->proto_is_builtin) {
    return __jsobj_get_or_create_builtin(obj->prototype.id);
  }
  return obj->prototype.obj;
}

void __jsobj_helper_reject(bool throw_p);
void __jsobj_helper_convert_to_generic(__jsobject *obj);
void __jsobj_helper_add_value_property(__jsobject *obj, TValue &name, TValue &v, uint32_t attrs, __jsprop *prop_cache = NULL);
void __jsobj_helper_add_value_property(__jsobject *obj, __jsstring *name, TValue &v, uint32_t attrs, __jsprop *prop_cache = NULL);
void __jsobj_helper_add_value_property(__jsobject *obj, __jsbuiltin_string_id id, TValue &v, uint32_t attrs, __jsprop *prop_cache = NULL);
__jsprop *__jsobj_helper_init_value_property(__jsobject *obj, __jsbuiltin_string_id id, TValue &v, uint32_t attrs);

uint32_t __jsobj_helper_get_length(__jsobject *obj);
uint64_t __jsobj_helper_get_lengthsize(__jsobject *obj);
TValue __jsobj_helper_get_length_value(__jsobject *obj);
void __jsobj_helper_set_length(__jsobject *obj, uint64_t length, bool throw_p);
__jsobject *__js_new_obj_obj_0();
// Helper function for object constructors.
__jsprop *__create_builtin_property(__jsobject *obj, __jsstring *name);
bool __jsobj_helper_HasPropertyAndGet(__jsobject *obj, __jsbuiltin_string_id id, TValue *result);
bool __jsobj_helper_HasPropertyAndGet(__jsobject *obj, uint32_t index, TValue *result);
bool __jsobj_helper_HasPropertyAndGet(__jsobject *obj, __jsstring *p, TValue *result);
bool __jsobj_helper_GetAndCall(__jsobject *obj, __jsbuiltin_string_id id, TValue *result);
// ecma 8.10.1
// bool __jsprop_desc_IsAccessorDescriptor(__jsprop_desc desc);
// ecma 8.10.2
// bool __jsprop_desc_IsDataDescriptor(__jsprop_desc desc);
// ecma 8.10.3
// bool __jsprop_desc_IsGenericDescriptor(__jsprop_desc desc);
// ecma 8.10.4
// TValue __jsprop_desc_FromPropertyDescriptor(__jsprop_desc desc);
// ecma 8.10.5
__jsprop_desc __jsprop_desc_ToPropertyDescriptor(TValue &o);
// ecma 8.12.1
__jsprop_desc __jsobj_internal_GetOwnProperty(__jsobject *o, __jsbuiltin_string_id id);
__jsprop_desc __jsobj_internal_GetOwnProperty(__jsobject *o, TValue &p);
// ecma 8.12.2
__jsprop_desc __jsobj_internal_GetProperty(__jsobject *o, TValue &p);
__jsprop_desc __jsobj_internal_GetProperty(__jsobject *o, __jsstring *p);
// ecma 8.12.3
TValue __jsobj_internal_Get(__jsobject *o, TValue &p);
TValue __jsobj_internal_Get(__jsobject *o, __jsstring *p);
TValue __jsobj_internal_Get(__jsobject *o, __jsbuiltin_string_id id);
TValue __jsobj_internal_Get(__jsobject *o, uint32_t index);
// ecma 8.12.4
bool __jsobj_internal_CanPut(__jsobject *o, TValue *p, bool isStrict = false, __jsprop **prop_cache = NULL);
// ecma 8.12.5
void __jsobj_internal_Put(__jsobject *o, __jsstring *p, TValue &v, bool throw_p, bool isStrict = false);
void __jsobj_internal_Put(__jsobject *o, uint32_t index, TValue &v, bool throw_p);
// ecma 8.12.6
// ecma 8.12.6
bool __jsobj_internal_HasProperty(__jsobject *o, __jsstring *p, __jsprop_desc *descp = NULL);
// ecma 8.12.7
bool __jsobj_internal_Delete(__jsobject *o, TValue &p, bool mark_as_deleted = false, bool throw_p = false);
bool __jsobj_internal_Delete(__jsobject *o, __jsstring *p, bool mark_as_deleted = false, bool throw_p = false);
bool __jsobj_internal_Delete(__jsobject *o, uint32_t index, bool mark_as_deleted = false, bool throw_p = false);
// ecma 8.12.8
TValue __object_internal_DefaultValue(__jsobject *o, __jstype hint);
void __jsobj_internal_DefineOwnPropertyByValue(__jsobject *o, uint32_t index, __jsprop_desc desc, bool throw_p);
// ecma 8.12.9
void __jsobj_internal_DefineOwnProperty(__jsobject *o, __jsstring *p, __jsprop_desc desc, bool throw_p, __jsprop *prop_cache = NULL);
void __jsobj_internal_DefineOwnProperty(__jsobject *o, __jsbuiltin_string_id id, __jsprop_desc desc, bool throw_p, __jsprop *prop_cache = NULL);
void __jsobj_internal_DefineOwnProperty(__jsobject *o, TValue &p, __jsprop_desc desc, bool throw_p, __jsprop *prop_cache = NULL);
// ecma 15.2.3.2
TValue __jsobj_getPrototypeOf(TValue &this_object, TValue &o);
// ecma 15.2.3.3
TValue __jsobj_getOwnPropertyDescriptor(TValue &this_object, TValue &o, TValue &p);
// ecma 15.2.3.4
TValue __jsobj_getOwnPropertyNames(TValue &this_object, TValue &o);
// ecma 15.2.3.5
TValue __jsobj_create(TValue &this_object, TValue &o, TValue &properties);
// ecma 15.2.3.6
TValue __jsobj_defineProperty(TValue &this_object, TValue &o, TValue &p, TValue &attributes);
// ecma 15.2.3.7
TValue __jsobj_defineProperties(TValue &this_object, TValue &o, TValue &properties);
// ecma 15.2.3.8
TValue __jsobj_seal(TValue &this_object, TValue &o);
// ecma 15.2.3.9
TValue __jsobj_freeze(TValue &this_object, TValue &o);
// ecma 15.2.3.10
TValue __jsobj_preventExtensions(TValue &this_object, TValue &o);
// ecma 15.2.3.11
TValue __jsobj_isSealed(TValue &this_object, TValue &o);
// ecma 15.2.3.12
TValue __jsobj_isFrozen(TValue &this_object, TValue &o);
// ecma 15.2.3.13
TValue __jsobj_isExtensible(TValue &this_object, TValue &o);
// ecma 15.2.3.14
TValue __jsobj_keys(TValue &this_object, TValue &o);
// ecma 15.2.4.2
TValue __jsobj_pt_toString(TValue &this_object);
// ecma 15.2.4.3
TValue __jsobj_pt_toLocaleString(TValue &this_object);
// ecma 15.2.4.4
TValue __jsobj_pt_valueOf(TValue &this_object);
// ecma 15.2.4.5
TValue __jsobj_pt_hasOwnProperty(TValue &this_object, TValue &v);
// ecma 15.2.4.6
TValue __jsobj_pt_isPrototypeOf(TValue &this_object, TValue &v);
// ecma 15.2.4.7
TValue __jsobj_pt_propertyIsEnumerable(TValue &this_object, TValue &v);
void __jsop_initprop_by_name(TValue &o, __jsstring *p, TValue &v);
void __jsop_initprop(TValue &o, TValue &p, TValue &v);
void __jsop_setprop(TValue &object, TValue &prop_name, TValue &v);
void __jsop_setprop(maple::TValue &object, maple::TValue &prop_name, maple::TValue &v);
void __jsop_initprop_getter(TValue &object, TValue &prop_name, TValue &v);
TValue __jsop_getprop(TValue *object, TValue *prop_name);
TValue __jsop_getprop(maple::TValue &object, maple::TValue &prop_name);
TValue __jsop_delprop(TValue &o, TValue &nameIndex, bool throw_p = false);
TValue __jserror_pt_toString(TValue &this_object);
TValue __js_rangeerror_pt_toString(TValue &this_object);
TValue __js_evalerror_pt_toString(TValue &this_object);
TValue __js_referenceerror_pt_toString(TValue &this_object);
TValue __js_typeerror_pt_toString(TValue &this_object);
TValue __js_urierror_pt_toString(TValue &this_object);
TValue __js_syntaxerror_pt_toString(TValue &this_object);
void __jsobj_initprop_fromString(__jsobject *obj, __jsstring *str);
__jsprop *__jsobj_helper_init_value_propertyByValue(__jsobject *, uint32_t, TValue &, uint32_t);
TValue __jsobj_GetValueFromPropertyByValue(__jsobject *, uint32_t);
bool __jsPropertyIsWritable(__jsobject *, uint32_t);
void __jsconsole_pt_log (TValue &, TValue &);
TValue __jsobj_internal_get_by_desc(__jsobject *obj, __jsprop_desc desc, TValue *orgVal = NULL);
__jsprop_desc __jsobj_internal_GetOwnPropertyByValue(__jsobject *o, uint32_t index);
#endif
