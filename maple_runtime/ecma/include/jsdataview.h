#ifndef JSDATAVIEW_H
#define JSDATAVIEW_H
#include "jsvalue.h"

using namespace maple;

struct __jsarraybyte {
  TValue length;
  uint8_t *arrayRaw;
};
struct __jsdataview {
  __jsarraybyte *arrayByte;
  uint32_t startIndex;
  uint32_t endIndex;
};

void __jsdataview_pt_setInt8(TValue &thisArg, TValue *argList, uint32_t nargs);
TValue __jsdataview_pt_getInt8(TValue &thisArg, TValue *argList, uint32_t nargs);
void __jsdataview_pt_setUint8(TValue &thisArg, TValue *argList, uint32_t nargs);
TValue __jsdataview_pt_getUint8(TValue &thisArg, TValue *argList, uint32_t nargs);
void __jsdataview_pt_setInt16(TValue &thisArg, TValue *argList, uint32_t nargs);
TValue __jsdataview_pt_getInt16(TValue &thisArg, TValue *argList, uint32_t nargs);
void __jsdataview_pt_setUint16(TValue &thisArg, TValue *argList, uint32_t nargs);
TValue __jsdataview_pt_getUint16(TValue &thisArg, TValue *argList, uint32_t nargs);
void __jsdataview_pt_setInt32(TValue &thisArg, TValue *argList, uint32_t nargs);
TValue __jsdataview_pt_getInt32(TValue &thisArg, TValue *argList, uint32_t nargs);
void __jsdataview_pt_setUint32(TValue &thisArg, TValue *argList, uint32_t nargs);
TValue __jsdataview_pt_getUint32(TValue &hisArg, TValue *argList, uint32_t nargs);
#endif // JSDATAVIEW_H
