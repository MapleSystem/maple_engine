//
// Copyright (C) [2020-2021] Futurewei Technologies, Inc. All rights reserved.
//
// OpenArkCompiler is licensed underthe Mulan Permissive Software License v2.
// You can use this software according to the terms and conditions of the MulanPSL - 2.0.
// You may obtain a copy of MulanPSL - 2.0 at:
//
//   https://opensource.org/licenses/MulanPSL-2.0
//
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR
// FIT FOR A PARTICULAR PURPOSE.
// See the MulanPSL - 2.0 for more details.
//

#include "cfg_primitive_types.h"
#include "vmutils.h"
#include "vmmemory.h"
#include "mval.h"
#include "mvalue.h"
#include "jsplugin.h"


namespace maple {

#define CHECK_REFERENCE  0x1
#define JSEHOP_jstry   0x1
#define JSEHOP_throw   0x2
#define JSEHOP_jscatch 0x3
#define JSEHOP_finally 0x4


struct JavaScriptGlobal {
  uint8_t flavor;
  uint8_t srcLang;
  uint16_t id;
  uint16_t globalmemsize;
  uint8_t globalwordstypetagged;
  uint8_t globalwordsrefcounted;
  uint8_t numfuncs;
  uint64_t entryfunc;
};

class JsEh;
// imported from class Interpreter
class InterSource {
public:
  void *memory;  // memory block for APP and VM
  uint32_t total_memory_size_;
  uint32_t heap_size_;
  uint32_t stack;
  uint32_t heap;
  uint32_t sp;  // stack pointer
  uint32_t fp;  // frame pointer
  uint8_t *gp;  // global data pointer
  uint8_t *topGp;
  // AddrMap *heapRefList;     // ref used in heap
  // AddrMap *globalRefList;   // ref used in global memory
  TValue retVal0;
  JsEh *currEH;
  JsPlugin *jsPlugin;
  uint32_t EHstackReuseSize;
  StackT<JsEh *> EHstackReuse;
  StackT<JsEh *> EHstack;
  static uint8_t ptypesizetable[kPtyDerived];
  DynMFunction *curDynFunction;


public:
  explicit InterSource();
  void SetRetval0(TValue &);
  void SetRetval0Object(void *, bool);
  void SetRetval0NoInc (uint64_t);
  void EmulateStore(uint8_t *, TValue &);
  TValue EmulateLoad(uint8 *, uint32, PrimType);
  int32_t PassArguments(TValue &, void *, TValue *, int32_t, int32_t);
  inline void *GetSPAddr() {return (void *) (sp + (uint8 *)memory);}
  inline void *GetFPAddr() {return (void *) (fp + (uint8 *)memory);}
  inline void *GetGPAddr() {return (void *) gp;}
  TValue VmJSopAdd(TValue &, TValue &);
  TValue PrimAdd(TValue &, TValue &, PrimType);
  TValue JSopArith(TValue &, TValue &, PrimType, Opcode);
  TValue JSopMul(TValue &, TValue &, PrimType, Opcode);
  TValue JSopSub(TValue &, TValue &, PrimType, Opcode);
  TValue JSopDiv(TValue &, TValue &, PrimType, Opcode);
  TValue JSopRem(TValue &, TValue &, PrimType, Opcode);
  TValue JSopBitOp(TValue &, TValue &, PrimType, Opcode);
  TValue JSopCmp(TValue, TValue, Opcode, PrimType);
  bool JSopSwitchCmp(TValue &, TValue &, TValue &);
  TValue JSopCVT(TValue &, PrimType, PrimType);
  TValue JSopNewArrLength(TValue &);
  void JSopSetProp(TValue &, TValue &, TValue &);
  void JSopInitProp(TValue &, TValue &, TValue &);
  TValue  JSopNew(TValue &size);
  TValue  JSopNewIterator(TValue &, TValue &);
  TValue JSopNextIterator(TValue &);
  TValue JSopMoreIterator(TValue &);
  TValue JSopBinary(MIRIntrinsicID, TValue &, TValue &);
  TValue JSBoolean(TValue &);
  TValue JSNumber(TValue &);
  TValue JSopConcat(TValue &, TValue &);
  TValue JSopNewObj0();
  TValue JSopNewObj1(TValue &);
  void JSopSetPropByName (TValue &, TValue &, TValue &, bool isStrict = false);
  TValue JSopGetProp (TValue &, TValue &);
  TValue JSopGetPropByName(TValue &, TValue &);
  TValue JSopDelProp(TValue &, TValue &, bool throw_p = false);
  void JSopInitPropByName(TValue &, TValue &, TValue &);
  void JSopInitPropGetter(TValue &, TValue &, TValue &);
  void JSopInitPropSetter(TValue &, TValue &, TValue &);
  TValue JSopNewArrElems(TValue &, TValue &);
  TValue JSopGetBuiltinString(TValue &);
  TValue JSopGetBuiltinObject(TValue &);
  TValue JSString(TValue &);
  TValue JSStringVal(TValue &);
  TValue JSopLength(TValue &);
  TValue JSopThis();
  TValue JSUnary(MIRIntrinsicID, TValue &);
  TValue JSopUnary(TValue &, Opcode, PrimType);
  TValue JSopUnaryNeg(TValue &);
  TValue JSopUnaryLnot(TValue &);
  TValue JSopUnaryBnot(TValue &);
  TValue JSopRequire(TValue &);
  void CreateJsPlugin(char *);
  void SwitchPluginContext(JsFileInforNode *);
  void RestorePluginContext(JsFileInforNode *);
  TValue IntrinCall(MIRIntrinsicID, TValue *, int);
  TValue NativeFuncCall(MIRIntrinsicID, TValue *, int);
  TValue BoundFuncCall(TValue *, int);
  TValue FuncCall(void *, bool, void *, TValue *, int, int, int, bool);
  TValue IntrinCCall(TValue *, int);
  TValue FuncCall_JS(__jsobject*, TValue &, void *, TValue *, int32_t);
  void JsTry(void *, void *, void *, DynMFunction *);
  void JSPrint(TValue);
  void IntrnError(TValue *, int);
  void InsertProlog(uint16);
  void InsertEplog();
  TValue JSopGetArgumentsObject(void *);
  void* CreateArgumentsObject(TValue *, uint32_t, TValue &);
  TValue GetOrCreateBuiltinObj(__jsbuiltin_object_id);
  void JSdoubleConst(uint64_t, TValue &);
  TValue JSIsNan(TValue &);
  TValue JSDate(uint32_t, TValue *);
  uint64_t GetIntFromJsstring(__jsstring* );
  TValue JSRegExp(TValue &);
  void JSopSetThisPropByName (TValue &, TValue &);
  void JSopInitThisPropByName(TValue &);
  TValue JSopGetThisPropByName(TValue &);
  void UpdateArguments(int32_t, TValue &);
  void SetCurFunc(DynMFunction *func) {
    curDynFunction = func;
  }
  DynMFunction* GetCurFunc() {
    return curDynFunction;
  }

private:
  bool JSopPrimCmp(Opcode, TValue &, TValue &, PrimType);
};

inline uint32_t GetTagFromPtyp (PrimType ptyp) {
  switch(ptyp) {
        case PTY_dynnone:     return JSTYPE_NONE;
        case PTY_dynnull:  return JSTYPE_NULL;
        case PTY_dynbool:
        case PTY_u1:       return JSTYPE_BOOLEAN;
        case PTY_i8:
        case PTY_i16:
        case PTY_i32:
        case PTY_i64:
        case PTY_u16:
        case PTY_u8:
        case PTY_u32:
        case PTY_a64:
        case PTY_u64:       return  JSTYPE_NUMBER;
        case PTY_f32:
        case PTY_f64:       return  JSTYPE_DOUBLE;
        case PTY_simplestr: return  JSTYPE_STRING;
        case PTY_simpleobj: return  JSTYPE_OBJECT;
        case kPtyInvalid:
        case PTY_dynany:
        case PTY_dynundef:
        case PTY_dynstr:
        case PTY_dynobj:
        case PTY_void:
        default:            assert(false&&"unexpected");
    };
}

inline uint64_t GetNaNCodeFromPtyp (PrimType ptyp) {
  switch(ptyp) {
        case PTY_dynnone:  return NAN_NONE;
        case PTY_dynnull:  return NAN_NULL;
        case PTY_dynbool:
        case PTY_u1:       return NAN_BOOLEAN;
        case PTY_i8:
        case PTY_i16:
        case PTY_i32:
        case PTY_i64:
        case PTY_u16:
        case PTY_u8:
        case PTY_u32:
        case PTY_a64:
        case PTY_u64:       return  NAN_NUMBER;
        case PTY_f32:
        case PTY_f64:       return  0;
        case PTY_simplestr: return  NAN_STRING;
        case PTY_simpleobj: return  NAN_OBJECT;
        case kPtyInvalid:
        case PTY_dynany:
        case PTY_dynundef:
        case PTY_dynstr:
        case PTY_dynobj:
        case PTY_void:
        default:            assert(false&&"unexpected");
    };
}

/*
inline bool IsPrimitiveDyn(PrimType ptyp) {
  // return ptyp >= PTY_simplestr && ptyp <= PTY_dynnone;
  return ptyp >= PTY_dynany && ptyp <= PTY_dynnone;
}
*/
#define IsPrimitiveDyn(ptyp) (ptyp >= PTY_dynany && ptyp <= PTY_dynnone)

inline uint32_t GetMvalueValue(MValue &mv) {
  return mv.x.u32;
}
inline uint32_t GetMValueTag(MValue &mv) {
  return (mv.ptyp);
}

inline void SetMValueValue (MValue &mv, uint64_t val) {
  mv.x.u64 = val;
}
inline void SetMValueTag (MValue &mv, uint32_t ptyp) {
  mv.ptyp = ptyp;
}
extern JavaScriptGlobal *jsGlobal;
extern uint32_t *jsGlobalMemmap;
extern InterSource *gInterSource;

}
