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

#ifndef JSINTL_H
#define JSINTL_H

#include <vector>
#include <string>
#include <unordered_map>
#include "jsvalue.h"
#include "jsstring.h"

static const size_t INITIAL_STRING_BUFFER_SIZE = 32;

// Helper
TValue StrToVal(std::string str);
TValue StrVecToVal(std::vector<std::string> strs);
std::string ValToStr(TValue &value);
std::vector<std::string> VecValToVecStr(TValue &value);

// Constructor
TValue __js_IntlConstructor(TValue &this_arg, TValue *arg_list,
                               uint32_t nargs);
TValue __js_CollatorConstructor(TValue &this_arg, TValue *arg_list,
                                   uint32_t nargs);
TValue __js_NumberFormatConstructor(TValue &this_arg, TValue *arg_list,
                                       uint32_t nargs);
TValue __js_DateTimeFormatConstructor(TValue &this_arg,
                                         TValue *arg_list, uint32_t nargs);

// Common
TValue CanonicalizeLanguageTag(__jsstring *tag);
TValue CanonicalizeLocaleList(TValue &locales);
bool IsStructurallyValidLanguageTag(__jsstring *locale);
bool IsWellFormedCurrencyCode(TValue &currency);
TValue BestAvailableLocale(TValue &available_locales, TValue &locale);
TValue LookupMatcher(TValue &available_locales, TValue &requested_locales);
TValue BestFitMatcher(TValue &available_locales, TValue &requested_locales);
TValue ResolveLocale(TValue &available_locales, TValue &requested_locales,
                        TValue &options, TValue &relevant_extension_keys,
                        TValue &locale_data);
TValue LookupSupportedLocales(TValue &available_locales,
                                TValue &requested_locales);
TValue BestFitSupportedLocales(TValue &available_locales,
                                  TValue &requested_locales);
TValue SupportedLocales(TValue &available_locales,
                           TValue &requested_locales, TValue &options);
TValue GetOption(TValue &options, TValue &property, TValue &type,
                    TValue &values, TValue &fallback);
TValue GetNumberOption(TValue &options, TValue &property,
                          TValue &minimum, TValue &maximum,
                          TValue &fallback);
TValue DefaultLocale();
TValue GetAvailableLocales();
std::vector<std::string> GetNumberingSystems();
const char* ToICULocale(std::string& locale);


// NumberFormat
void InitializeNumberFormat(TValue &number_format, TValue &locales,
                            TValue &options);
void InitializeNumberFormatProperties(TValue &number_format, TValue &locales,
                                     std::vector<std::string> properties);
TValue CurrencyDigits(TValue &currency);
TValue __jsintl_NumberFormatSupportedLocalesOf(TValue &this_arg,
                                                  TValue *arg_list,
                                                  uint32_t nargs);
TValue __jsintl_NumberFormatFormat(TValue &number_format, TValue &value);
TValue __jsintl_NumberFormatResolvedOptions(TValue &number_format);
TValue ToDateTimeOptions(TValue &options, TValue &required, TValue &defaults);
TValue BasicFormatMatcher(TValue &options, TValue &formats);
TValue BestFitFormatMatcher(TValue &options, TValue &formats);
TValue FormatNumber(TValue &number_format, TValue &x);
TValue ToRawPrecision(TValue &x, TValue &min_sd, TValue &max_sd);
TValue ToRawFixed(TValue &x, TValue &min_integer,
                     TValue &min_fraction,TValue &max_fraction);

// Collator
void InitializeCollator(TValue &collator, TValue &locales, TValue &options);
void InitializeCollatorProperties(TValue &collator, TValue &locales, std::vector<std::string> propertie);
TValue __jsintl_CollatorSupportedLocalesOf(TValue &this_arg,
                                              TValue *arg_list,
                                              uint32_t nargs);
TValue __jsintl_CollatorCompare(TValue &this_collator, TValue &x, TValue &y);
TValue CompareStrings(TValue &collator, TValue &x, TValue &y);
TValue __jsintl_CollatorResolvedOptions(TValue &collator);
std::vector<std::string> GetCollations();

// DateTimeFormat
void InitializeDateTimeFormat(TValue &date_time_format, TValue &locales,
                              TValue &options);
void InitializeDateTimeFormatProperties(TValue &date_time_format, TValue &locales, TValue &options);
TValue __jsintl_DateTimeFormatSupportedLocalesOf(TValue &date_time_format,
                                                    TValue *arg_list,
                                                    uint32_t nargs);
TValue __jsintl_DateTimeFormatFormat(TValue &date_time_format, TValue &date);
TValue FormatDateTime(TValue &date_time_format, TValue &x);
TValue ToLocalTime(TValue &x, TValue &calendar, TValue &time_zone);
TValue __jsintl_DateTimeFormatResolvedOptions(TValue &date_time_format);
std::vector<std::string> GetAvailableCalendars();
std::string ToBCP47CalendarName(const char* name);
std::string GetDateTimeStringSkeleton(TValue &locale, TValue &options);
TValue GetICUPattern(std::string&, std::string& pattern);
void ResolveICUPattern(TValue &date_time_format, TValue &pattern);

#endif // JSINTL_H
