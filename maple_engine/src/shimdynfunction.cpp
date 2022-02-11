/*
 * Copyright (C) [2021] Futurewei Technologies, Inc. All rights reserved.
 *
 * OpenArkCompiler is licensed underthe Mulan Permissive Software License v2.
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

#include <cstdarg>
#include <cstring>

#include "mfunction.h"
#include "massert.h" // for MASSERT
#include "mshimdyn.h"
#include "vmmemory.h"
#include "jsvalueinline.h"
#include "jstycnv.h"
#include "jsiter.h"
#include "ccall.h"
#include <string>
#include "jsarray.h"
#include "jsregexp.h"
#include "jsdate.h"
#include "jseh.h"
#include "jsobjectinline.h"

namespace maple {

JavaScriptGlobal *jsGlobal = NULL;
uint32_t *jsGlobalMemmap = NULL;
InterSource *gInterSource = NULL;


uint8 InterSource::ptypesizetable[kPtyDerived] = {
  0, // PTY_invalid
  0, // PTY_(void)
  1, // PTY_i8
  2, // PTY_i16
  4, // PTY_i32
  8, // PTY_i64
  1, // PTY_u8
  2, // PTY_u16
  4, // PTY_u32
  8, // PTY_u64
  1, // PTY_u1
  4, // PTY_ptr
  4, // PTY_ref
  4, // PTY_a32
  8, // PTY_a64
  4, // PTY_f32
  8, // PTY_f64
  16, // PTY_f128
  16, // PTY_c64
  32, // PTY_c128
  8, // PTY_simplestr
  8, // PTY_simpleobj
  8, // PTY_dynany
  8, // PTY_dynundef
  8, // PTY_dynnull
  8, // PTY_dynbool
  8, // PTY_dyni32
  8, // PTY_dynstr
  8, // PTY_dynobj
  8, // PTY_dynnone
  8, // PTY_dynf64
  8, // PTY_dynf32
  0 // PTY_agg
};

InterSource::InterSource() {
  const char* heap_size_env = std::getenv("MAPLE_HEAP_SIZE");
  if (heap_size_env != nullptr) {
    heap_size_ = atoi(heap_size_env) * 1024 * 1024;
    if (heap_size_ < HEAP_SIZE)
      heap_size_ = HEAP_SIZE;
    else if (heap_size_ > 1024 * 1024 *1024)
      heap_size_ = 1024 * 1024 *1024;

    total_memory_size_ = heap_size_ + VM_MEMORY_SIZE;
  } else {
    total_memory_size_ = HEAP_SIZE + VM_MEMORY_SIZE;
    heap_size_ = HEAP_SIZE;
  }
  memory = (void *)malloc(heap_size_ + VM_MEMORY_SIZE);
#ifdef COULD_BE_ADDRESS
  // to test if COULD_BE_ADDRESS(v) stands
  assert(COULD_BE_ADDRESS(memory));
#endif
  void * internalMemory = (void *)((char *)memory + heap_size_);
  stack = heap_size_/*STACKOFFSET*/;
  sp = stack;
  fp = stack;
  heap = 0;
  retVal0 = __null_value();
  currEH = nullptr;
  EHstackReuseSize = 0;

  // retVal0.payload.asbits = 0;
  memory_manager = new MemoryManager();
  memory_manager->Init(memory, heap_size_, internalMemory, VM_MEMORY_SIZE);
  gInterSource = this;
  // currEH = NULL;
  // EHstackReuseSize = 0;
}

void InterSource::SetRetval0 (TValue &mval) {
  if (IS_NEEDRC(mval.x.u64))
    memory_manager->GCIncRf((void*)mval.x.c.payload);
  if (IS_NEEDRC(retVal0.x.u64))
    memory_manager->GCDecRf((void*)retVal0.x.c.payload);
  retVal0 = mval;
}

void InterSource::SetRetval0Object (void *obj, bool isdyntype) {
  memory_manager->GCIncRf(obj);
  if (IS_NEEDRC(retVal0.x.u64))
    memory_manager->GCDecRf((void*)retVal0.x.c.payload);
  retVal0 = __object_value((__jsobject *)obj);
}

void InterSource::SetRetval0NoInc (uint64_t val) {
  if (IS_NEEDRC(retVal0.x.u64))
    memory_manager->GCDecRf((void*)retVal0.x.c.payload);
  retVal0.x.u64 = val;
}

inline void InterSource::EmulateStore(uint8_t *memory, TValue &mval) {
  *(uint64_t *)memory = mval.x.u64;
}

int32_t InterSource::PassArguments(TValue &this_arg, void *env, TValue *arg_list,
                              int32_t narg, int32_t func_narg) {
  // The arguments' number of a function must be [0, 255].
  // If func_narg = -1, means func_narg equal to narg.
  //assert(func_narg <= 255 && func_narg >= -1 && "error");
  //assert(narg <= 255 && narg >= 0 && "error");

  uint8 *spaddr = (uint8 *)GetSPAddr();
  int32_t offset = 0;

  // If actual arguments is less than formal arguments, push undefined value,
  // else ignore the redundant actual arguments.
  // It's a C style call, formal arguments' number must be equal the actuals'.
  if (func_narg == -1) {
    // Do nothing.
  } else if (func_narg - narg > 0) {
    TValue undefined = __undefined_value();
    for (int32_t i = func_narg - narg; i > 0; i--) {
      offset -= MVALSIZE;
      ALIGNMENTNEGOFFSET(offset, MVALSIZE);
      EmulateStore(spaddr + offset, undefined);
    }
  } else /* Set the actual argument-number to the function acceptant. */ {
    narg = func_narg;
  }
  // Pass actual arguments.
  for (int32_t i = narg - 1; i >= 0; i--) {
    TValue actual = arg_list[i];
    offset -= MVALSIZE;
    ALIGNMENTNEGOFFSET(offset, MVALSIZE);
    EmulateStore(spaddr + offset, actual);
#ifndef RC_OPT_FUNC_ARGS
    // RC is increased for args and decreased after the func call, therefore, the pair of RC ops can be eliminated.
    if (IS_NEEDRC(actual.x.u64)) {
      memory_manager->GCIncRf((void*)actual.x.c.payload);
    }
#endif
  }
  if (env) {
    offset -= PTRSIZE + MVALSIZE;
  } else {
    offset -= MVALSIZE;
  }
  ALIGNMENTNEGOFFSET(offset, MVALSIZE);
  // Pass env pointer.
  if (env) {
    TValue envMal = __env_value(env);
    EmulateStore(spaddr + offset + MVALSIZE, envMal);
#ifndef RC_OPT_FUNC_ARGS
    memory_manager->GCIncRf((void*)envMal.x.c.payload);
#endif
  }
  // Pass the 'this'.
  EmulateStore(spaddr + offset, this_arg);
#ifndef RC_OPT_FUNC_ARGS
 // RC is increased for args and decreased after the func call, therefore, the pair of RC ops can be eliminated.
  if (IS_NEEDRC(this_arg.x.u64))
    memory_manager->GCIncRf((void*)this_arg.x.c.payload);
#endif
  return offset;
}

TValue InterSource::VmJSopAdd(TValue &mv0, TValue &mv1) {
  return (__jsop_add(mv0, mv1));
}

TValue InterSource::PrimAdd(TValue &mv0, TValue &mv1, PrimType ptyp) {
  TValue res;
  //uint32_t typ = JSTYPE_NUMBER;
  switch (ptyp) {
    case PTY_i8:  res.x.i8  = mv0.x.i8  + mv1.x.i8;  break;
    case PTY_i16: res.x.i16 = mv0.x.i16 + mv1.x.i16; break;
    case PTY_i32: res.x.i32 = mv0.x.i32 + mv1.x.i32; break;
    case PTY_i64: res.x.i64 = mv0.x.i64 + mv1.x.i64; break;
    case PTY_u16: res.x.u16 = mv0.x.u16 + mv1.x.u16; break;
    case PTY_u1:  res.x.u1  = mv0.x.u1  + mv1.x.u1;  break;
    case PTY_a32:
    case PTY_a64:  {
      if (IS_SPBASE(mv0.x.u64) || IS_FPBASE(mv0.x.u64) || IS_GPBASE(mv0.x.u64)) {
        //assert(GET_TYPE(mv1) == JSTYPE_NUMBER || GET_TYPE(mv1) == JSTYPE_NONE);
        res.x.u64 = mv0.x.u64 + mv1.x.i32;
      }
      else
        res.x.a64 = mv0.x.a64 + mv1.x.i64;
      break;
    }
    case PTY_simplestr: {
      //typ = JSTYPE_STRING;
      res.x.a64 = mv0.x.a64 + mv1.x.i32;
      break;
    }
    case PTY_f32: res.x.f32 = mv0.x.f32 + mv1.x.f32; break;
    case PTY_f64: assert(false && "nyi");
    default: assert(false && "error");
  }
  return res;
}

TValue InterSource::JSopDiv(TValue &mv0, TValue &mv1, PrimType ptyp, Opcode op) {
  uint32_t ptyp0 = GET_TYPE(mv0);
  uint32_t ptyp1 = GET_TYPE(mv1);
  if (ptyp0 == JSTYPE_NAN || ptyp1 == JSTYPE_NAN
    || ptyp0 == JSTYPE_UNDEFINED || ptyp1 == JSTYPE_UNDEFINED
    || (ptyp0 == JSTYPE_NULL && ptyp1 == JSTYPE_NULL)) {
    return (__nan_value());
  }
  switch (ptyp0) {
    case JSTYPE_BOOLEAN: {
      int32_t x0 = mv0.x.i32;
      switch (ptyp1) {
        case JSTYPE_BOOLEAN: {
          int32_t x1 = mv1.x.i32;
          if (x1 == 0) {
            return (x0 >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          return __number_value(x0);
        }
        case JSTYPE_NUMBER: {
          int32_t x1 = mv1.x.i32;
          if (x1 == 0) {
            return (x0 >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          return __number_value(x0 / x1);
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_div(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return (__number_infinity());
        }
        case JSTYPE_UNDEFINED: {
          return (__nan_value());
        }
        default:
          assert(false && "unexpected div op1");
      }
      break;
    }
    case JSTYPE_NUMBER: {
      int32_t x0 = mv0.x.i32;
      switch (ptyp1) {
        case JSTYPE_BOOLEAN: {
          int32_t x1 = mv1.x.i32;
          if (x1 == 0) {
            return (x0 >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          return __number_value(mv0.x.i32 / mv1.x.i32);
        }
        case JSTYPE_NUMBER: {
          int32_t x1 = mv1.x.i32;
          if (x1 == 0) {
            return (x0 >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          double db = (double)x0 / (double)x1;
          if (__is_double_to_int(db)) {
            return __number_value(x0 / x1);
          } else {
            return (__double_value(db));
          }
          break;
        }
        case JSTYPE_DOUBLE: {
          if (mv1.x.f64 == 0.0f) {
            // negative_zero
            return (x0 < 0 ? __number_infinity() : __number_neg_infinity());
          }
          double x1 = (mv1.x.f64);
          if (fabs(x1) < NumberMinValue) {
            return (x0 < 0 ? __number_infinity() : __number_neg_infinity());
          }
          double rtx = (double)mv0.x.i32 / x1;
          if (std::isinf(rtx)) {
            return __infinity_value(rtx == (-1.0/0.0) ? 1 : 0);
          } else {
            return __double_value(rtx);
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_div(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return (mv0.x.i32 > 0 ? __number_infinity() : __number_neg_infinity());
        }
        case JSTYPE_INFINITY: {
          bool isPos = __is_neg_infinity(mv1) ? x0 < 0 : x0 >= 0;
          return (isPos ? __positive_zero_value() :  __negative_zero_value());
        }
        default:
          assert(false && "unexpected div op1");
      }
      break;
    }
    case JSTYPE_DOUBLE: {
      double ldb = mv0.x.f64;
      switch (ptyp1) {
        case JSTYPE_NUMBER:
        case JSTYPE_DOUBLE: {
          double rhs = (ptyp1 == JSTYPE_NUMBER ? (double)mv1.x.i32 : (mv1.x.f64));
          if (fabs(rhs) < NumberMinValue) {
            return (ldb >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          double rtx = ldb / rhs;
          if (std::isinf(rtx)) {
            return __infinity_value(rtx == (-1.0/0.0) ? 1 : 0);
          } else {
            return (__double_value(rtx));
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_div(mv0, mv1));
        }
        case JSTYPE_NULL: {
          double rhs = (mv0.x.f64);
          return (rhs > 0 ? __number_infinity() : __number_neg_infinity());
        }
        case JSTYPE_INFINITY: {
          return (__positive_zero_value());
        }
        default:
          assert(false && "unexpected div op1");
      }
      break;
    }
    case JSTYPE_OBJECT: {
      return (__jsop_object_div(mv0, mv1));
    }
    case JSTYPE_NULL: {
      return __number_value(0);
      break;
    }
    case JSTYPE_INFINITY: {
      bool isPos0 = mv0.x.i32 == 0;
      bool isPos1 = false;
      switch (ptyp1) {
        case JSTYPE_NUMBER: {
          int32_t val1 = mv1.x.i32;
          if (val1 == 0) {
            return mv0;
          } else {
           isPos1 = val1 > 0;
          }
          break;
        }
        case JSTYPE_DOUBLE: {
          double rhs = (mv1.x.f64);
          if (fabs(rhs) < NumberMinValue) {
            return (isPos0 ? __number_neg_infinity() : __number_infinity());
          } else {
            isPos1 = rhs > 0;
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_div(mv0, mv1));
        }
        case JSTYPE_NULL: {
          assert(false && "unexpected sub op1");
          return __number_value(0);
          break;
        }
        case JSTYPE_INFINITY: {
          return (__nan_value());
        }
        default:
          assert(false && "unexpected div op1");
      }
      return (isPos0 == isPos1) ? (__number_infinity()) : (__number_neg_infinity());
    }
    case JSTYPE_UNDEFINED: {
      return (__nan_value());
    }
    case JSTYPE_STRING: {
      bool isConvertible = false;
      TValue leftV = __js_ToNumberSlow2(mv0, isConvertible);
      if (!isConvertible) {
        return (__nan_value());
      } else {
        return JSopDiv(leftV, mv1, ptyp, op);
      }
      break;
    }
    default:
      assert(false && "unexpected mul op0");
  }
}

TValue InterSource::JSopRem(TValue &mv0, TValue &mv1, PrimType ptyp, Opcode op) {
  uint32_t ptyp0 = GET_TYPE(mv0);
  uint32_t ptyp1 = GET_TYPE(mv1);
  if (ptyp0 == JSTYPE_NAN || ptyp1 == JSTYPE_NAN
    || ptyp0 == JSTYPE_UNDEFINED || ptyp1 == JSTYPE_UNDEFINED || ptyp1 == JSTYPE_NULL) {
    return (__nan_value());
  }
  switch (ptyp0) {
    case JSTYPE_STRING: {

      bool isNum;
      int32_t x0 = __js_str2num2(__jsval_to_string(mv0), isNum, true);
      if (!isNum) {
        return (__nan_value());
      }

      switch (ptyp1) {
        case JSTYPE_STRING: {
          int32_t x1 = __js_str2num2(__jsval_to_string(mv1), isNum, true);
          if (!isNum) {
            return (__nan_value());
          }
          return __number_value(x0 % x1);
          break;
        }
        case JSTYPE_BOOLEAN: {
          int32_t x1 = mv1.x.i32;
          if (x1 == 0) {
            return (x0 >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          return __number_value(x0 % mv1.x.i32);
          break;
        }
        case JSTYPE_NUMBER: {
          int32_t x1 = mv1.x.i32;
          if (x1 == 0) {
            return (x0 >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          return __number_value(x0 % x1);
          break;
        }
        case JSTYPE_DOUBLE: {
          double x1 = (mv1.x.f64);
          if (fabs(x1) < NumberMinValue) {
            return (x0 < 0 ? __number_infinity() : __number_neg_infinity());
          }
          double rtx = fmod((double)x0,x1);
          if (std::isinf(rtx)) {
            return __infinity_value(rtx == (-1.0/0.0) ? 1 : 0);
          } else {
            return (__double_value(rtx));
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_rem(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return (x0 > 0 ? __number_infinity() : __number_neg_infinity());
        }
        case JSTYPE_INFINITY: {
          TValue v1 = (mv1);
          bool isPos = __is_neg_infinity(mv1) ? x0 < 0 : x0 >= 0;
          return (isPos ? __positive_zero_value() :  __negative_zero_value());
        }
        default:
          assert(false && "unexpected rem op1");
      }
      break;
    }

    case JSTYPE_BOOLEAN: {
      int32_t x0 = mv0.x.i32;
      switch (ptyp1) {
        case JSTYPE_BOOLEAN: {
          int32_t x1 = mv1.x.i32;
          if (x1 == 0) {
            return (x0 >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          return __number_value(0);
        }
        case JSTYPE_NUMBER: {
          int32_t x1 = mv1.x.i32;
          if (x1 == 0) {
            return (x0 >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          return __number_value(x0 % x1);
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_rem(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return (__number_infinity());
        }
        case JSTYPE_UNDEFINED: {
          return (__nan_value());
        }
        default:
          assert(false && "unexpected rem op1");
      }
      break;
    }
    case JSTYPE_NUMBER: {
      int32_t x0 = mv0.x.i32;
      switch (ptyp1) {
        case JSTYPE_BOOLEAN: {
          int32_t x1 = mv1.x.i32;
          if (x1 == 0) {
            return (x0 >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          return __number_value(mv0.x.i32 % mv1.x.i32);
        }
        case JSTYPE_NUMBER: {
          int32_t x1 = mv1.x.i32;
          if (x1 == 0) {
            return (x0 >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          return __number_value(x0 % x1);
          break;
        }
        case JSTYPE_DOUBLE: {
          double x1 = (mv1.x.f64);
          if (fabs(x1) < NumberMinValue) {
            return (x0 < 0 ? __number_infinity() : __number_neg_infinity());
          }
          double rtx = fmod((double)mv0.x.i32,x1);
          if (std::isinf(rtx)) {
            return __infinity_value(rtx == (-1.0/0.0) ? 1 : 0);
          } else {
            if (rtx == 0) {
              return __number_value(0);
            } else {
              return (__double_value(rtx));
            }
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_rem(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return (mv0.x.i32 > 0 ? __number_infinity() : __number_neg_infinity());
        }
        case JSTYPE_INFINITY: {
          TValue v1 = (mv1);
          bool isPos = __is_neg_infinity(mv1) ? x0 < 0 : x0 >= 0;
          return (isPos ? __positive_zero_value() :  __negative_zero_value());
        }
        default:
          assert(false && "unexpected rem op1");
      }
      break;
    }
    case JSTYPE_DOUBLE: {
      double ldb = (mv0.x.f64);
      switch (ptyp1) {
        case JSTYPE_NUMBER:
        case JSTYPE_DOUBLE: {
          double rhs = (ptyp1 == JSTYPE_NUMBER ? (double)mv1.x.i32 : (mv1.x.f64));
          if (fabs(rhs) < NumberMinValue) {
            return (ldb >= 0 ? __number_infinity() : __number_neg_infinity());
          }
          double rtx = fmod(ldb, rhs);
          if (std::isinf(rtx)) {
            return __infinity_value(rtx == (-1.0/0.0) ? 1 : 0);
          } else {
            return (__double_value(rtx));
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_rem(mv0, mv1));
        }
        case JSTYPE_NULL: {
          double rhs = mv0.x.f64;
          return (rhs > 0 ? __number_infinity() : __number_neg_infinity());
        }
        case JSTYPE_INFINITY: {
          return (__positive_zero_value());
        }
        default:
          assert(false && "unexpected rem op1");
      }
      break;
    }
    case JSTYPE_OBJECT: {
      return (__jsop_object_rem(mv0, mv1));
    }
    case JSTYPE_NULL: {
      return __number_value(0);
      break;
    }
    case JSTYPE_INFINITY: {
      bool isPos0 = mv0.x.i32 == 0;
      bool isPos1 = false;
      switch (ptyp1) {
        case JSTYPE_NUMBER: {
          int32_t val1 = mv1.x.i32;
          if (val1 == 0) {
            return mv0;
          } else {
           isPos1 = val1 > 0;
          }
          break;
        }
        case JSTYPE_DOUBLE: {
          double rhs = mv1.x.f64;
          if (fabs(rhs) < NumberMinValue) {
            return (isPos0 ? __number_neg_infinity() : __number_infinity());
          } else {
            isPos1 = rhs > 0;
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_rem(mv0, mv1));
        }
        case JSTYPE_NULL: {
          assert(false && "unexpected rem op1");
          return __number_value(0);
          break;
        }
        case JSTYPE_INFINITY: {
          return (__nan_value());
        }
        default:
          assert(false && "unexpected rem op1");
      }
      return (isPos0 == isPos1) ? (__number_infinity()) : (__number_neg_infinity());
    }
    case JSTYPE_UNDEFINED: {
      return (__nan_value());
    }
    default:
      assert(false && "unexpected rem op0");
  }
}

TValue InterSource::JSopBitOp(TValue &mv0, TValue &mv1, PrimType ptyp, Opcode op) {
  uint32_t ptyp0 = GET_TYPE(mv0);
  uint32_t ptyp1 = GET_TYPE(mv1);
  TValue res;
  int32_t lVal;
  int32_t rVal;
  if (ptyp0 == JSTYPE_NAN || ptyp0 == JSTYPE_NULL
    || ptyp0 == JSTYPE_UNDEFINED) {
    lVal = 0;
  } else {
    lVal = __js_ToNumber(mv0);
  }

  if (ptyp1 == JSTYPE_NAN || ptyp1 == JSTYPE_NULL
    || ptyp1 == JSTYPE_UNDEFINED) {
    rVal = 0;
  } else {
    rVal = __js_ToNumber(mv1);
  }
  int32_t resVal = 0;
  switch (op) {
    case OP_ashr: {
      resVal = lVal >> rVal;
      break;
    }
    case OP_band: {
      resVal = lVal & rVal;
      break;
    }
    case OP_bior: {
      resVal = lVal | rVal;
      break;
    }
    case OP_bxor: {
      resVal = lVal ^ rVal;
      break;
    }
    case OP_lshr: {
      uint32_t ulVal = lVal;
      uint32_t urVal = rVal;
      resVal = ulVal >> urVal;
      break;
    }
    case OP_shl: {
      resVal = lVal << rVal;
      break;
    }
    case OP_land: {
      resVal = lVal && rVal;
      break;
    }
    case OP_lior: {
      resVal = lVal || rVal;
      break;
    }
    default:
      MIR_FATAL("unknown arithmetic op");
  }
  res = __number_value(resVal);
  return res;
}


TValue InterSource::JSopSub(TValue &mv0, TValue &mv1, PrimType ptyp, Opcode op) {
  uint32_t ptyp0 = GET_TYPE(mv0);
  uint32_t ptyp1 = GET_TYPE(mv1);
  switch (ptyp0) {
    case JSTYPE_BOOLEAN: {
      switch (ptyp1) {
        case JSTYPE_BOOLEAN: {
          return __number_value(mv0.x.i32 - mv1.x.i32);
        }
        case JSTYPE_NUMBER: {
          return __number_value(mv0.x.i32 - mv1.x.i32);
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_sub(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return __number_value(mv0.x.i32);
        }
        case JSTYPE_UNDEFINED: {
          return (__nan_value());
        }
        case JSTYPE_NAN: {
          return (__nan_value());
        }
        default:
          assert(false && "unexpected mul op1");
      }
      break;
    }
    case JSTYPE_NUMBER: {
      switch (ptyp1) {
        case JSTYPE_BOOLEAN: {
          return __number_value(mv0.x.i32 - mv1.x.i32);
        }
        case JSTYPE_NUMBER: {
          return __number_value(mv0.x.i32 - mv1.x.i32);
        }
        case JSTYPE_DOUBLE: {
          double rtx = (double)mv0.x.i32 - mv1.x.f64;
          if (std::isinf(rtx)) {
            return __infinity_value(rtx == (-1.0/0.0) ? 1 : 0);
          } else {
            return (__double_value(rtx));
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_sub(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return __number_value(mv0.x.i32);
        }
        case JSTYPE_UNDEFINED: {
          return (__nan_value());
        }
        case JSTYPE_NAN: {
          return (__nan_value());
        }
        case JSTYPE_INFINITY: {
          return __infinity_value(mv1.x.i32 == 0 ? 1 : 0);
        }
        default:
          assert(false && "unexpected mul op1");
      }
      break;
    }
    case JSTYPE_DOUBLE: {
      switch (ptyp1) {
        case JSTYPE_NUMBER:
        case JSTYPE_DOUBLE: {
          double rhs = (ptyp1 == JSTYPE_NUMBER ? (double)mv1.x.i32 : (mv1.x.f64));
          double rtx = (mv0.x.f64) - rhs;
          if (std::isinf(rtx)) {
            return __infinity_value(rtx == (-1.0/0.0) ? 1 : 0);
          } else {
            if (fabs(rtx) < NumberMinValue) {
              return __number_value(0);
            } else {
              return (__double_value(rtx));
            }
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_sub(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return mv0;
        }
        case JSTYPE_UNDEFINED: {
          return (__nan_value());
        }
        case JSTYPE_NAN: {
          return (__nan_value());
        }
        case JSTYPE_INFINITY: {
          return __infinity_value(mv1.x.i32 == 0 ? 1 : 0);
        }
        default:
          assert(false && "unexpected mul op1");
      }
      break;
    }
    case JSTYPE_OBJECT: {
      if (ptyp1 == JSTYPE_NAN) {
        return (__nan_value());
      }
      return (__jsop_object_sub(mv0, mv1));
    }
    case JSTYPE_NULL: {
      switch (ptyp1) {
        case JSTYPE_BOOLEAN: {
          return __number_value(-mv1.x.i32);
        }
        case JSTYPE_NUMBER:
        case JSTYPE_DOUBLE: {
          double rhs = (ptyp1 == JSTYPE_NUMBER ? (double)mv1.x.i32 : (mv1.x.f64));
          return (__double_value(-rhs));
        }
        case JSTYPE_OBJECT: {
          TValue v0 = __positive_zero_value();
          return (__jsop_object_sub(v0, mv1));
        }
        case JSTYPE_NULL: {
          break;
        }
        case JSTYPE_UNDEFINED: {
          return (__nan_value());
        }
        case JSTYPE_NAN: {
          return (__nan_value());
        }
        case JSTYPE_INFINITY: {
          return __infinity_value(mv1.x.i32 == 0 ? 1 : 0);
        }
        default:
          assert(false && "unexpected sub op1");
      }
      return __number_value(0);
    }
    case JSTYPE_UNDEFINED: {
      return (__nan_value());
    }
    case JSTYPE_INFINITY: {
      if (ptyp1 == JSTYPE_INFINITY) {
        bool isPos0 = mv0.x.i32 == 0;
        bool isPos1 = mv1.x.i32 == 0;
        if ((isPos0 && isPos1) || (!isPos0 && !isPos1)) {
          return (__nan_value());
        } else {
          return mv0;
        }
      } else if (ptyp1 == JSTYPE_NAN) {
        return (__nan_value());
      } else {
        return mv0;// infinity - something = infinity
      }
    }
    case JSTYPE_NAN: {
      return (__nan_value());
    }
    case JSTYPE_STRING: {
      bool isConvertible = false;
      TValue leftV = __js_ToNumberSlow2(mv0, isConvertible);
      if (isConvertible) {
        return JSopSub(leftV, mv1, ptyp, op);
      } else {
        return (__nan_value());
      }
    }
    default:
      assert(false && "unexpected sub op0");
  }
}

TValue InterSource::JSopMul(TValue &mv0, TValue &mv1, PrimType ptyp, Opcode op) {
  uint32_t ptyp0 = GET_TYPE(mv0);
  uint32_t ptyp1 = GET_TYPE(mv1);
  if (ptyp0 == JSTYPE_NAN || ptyp1 == JSTYPE_NAN ||
       ptyp0 == JSTYPE_UNDEFINED || ptyp1 == JSTYPE_UNDEFINED) {
    return (__nan_value());
  }
  switch (ptyp0) {
    case JSTYPE_BOOLEAN: {
      switch (ptyp1) {
        case JSTYPE_BOOLEAN: {
          return __number_value(mv0.x.i32 * mv1.x.i32);
        }
        case JSTYPE_NUMBER: {
          return __number_value(mv0.x.i32 * mv1.x.i32);
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_mul(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return __number_value(0);
        }
        default:
          assert(false && "unexpected mul op1");
      }
      break;
    }
    case JSTYPE_NUMBER: {
      switch (ptyp1) {
        case JSTYPE_BOOLEAN: {
          return __number_value(mv0.x.i32 * mv1.x.i32);
        }
        case JSTYPE_NUMBER: {
          double rtx = (double)mv0.x.i32 * (double)mv1.x.i32;
          if (fabs(rtx) < (double)INT32_MAX) {
            return __number_value(mv0.x.i32 * mv1.x.i32);
          } else if (std::isinf(rtx)) {
            return __infinity_value(rtx == (-1.0/0.0) ? 1 : 0);
          } else {
            return __double_value(rtx);
          }
          break;
        }
        case JSTYPE_DOUBLE: {
          double rtx = (double)mv0.x.i32 * (mv1.x.f64);
          if (std::isinf(rtx)) {
            return __infinity_value(rtx == (-1.0/0.0) ? 1 : 0);
          } else {
            return (__double_value(rtx));
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_mul(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return __number_value(0);
        }
        case JSTYPE_INFINITY: {
          return JSopMul(mv1, mv0, ptyp, op);
        }
        default:
          assert(false && "unexpected mul op1");
      }
      break;
    }
    case JSTYPE_DOUBLE: {
      switch (ptyp1) {
        case JSTYPE_NUMBER:
        case JSTYPE_DOUBLE: {
          double rhs = (ptyp1 == JSTYPE_NUMBER ? (double)mv1.x.i32 : (mv1.x.f64));
          double rtx = (mv0.x.f64) * rhs;
          if (std::isinf(rtx)) {
            return __infinity_value(rtx == (-1.0/0.0) ? 1 : 0);
          } else {
            return (__double_value(rtx));
          }
          break;
        }
        case JSTYPE_OBJECT: {
          return (__jsop_object_mul(mv0, mv1));
        }
        case JSTYPE_NULL: {
          return __number_value(0);
        }
        case JSTYPE_INFINITY: {
          return JSopMul(mv1, mv0, ptyp, op);
        }
        default:
          assert(false && "unexpected mul op1");
      }
      break;
    }
    case JSTYPE_OBJECT: {
      return (__jsop_object_mul(mv0, mv1));
    }
    case JSTYPE_NULL: {
      return __number_value(0);
    }
    case JSTYPE_INFINITY: {
      bool isPos0 = mv0.x.i32 == 0;
      bool isPos1 = false;
      switch (ptyp1) {
        case JSTYPE_INFINITY: {
          isPos1 = mv1.x.i32 == 0;
          break;
        }
        case JSTYPE_NUMBER: {
          if (mv1.x.i32 == 0)
            return (__nan_value());
          isPos1 = mv1.x.i32 > 0;
          break;
        }
        case JSTYPE_DOUBLE: {
          double x = (mv1.x.f64);
          if (fabs(x) < NumberMinValue)
            return (__nan_value());
          isPos1 = x > 0;
          break;
        }
        default:
          assert(false && "unexpected mul op0");
      }
      if ((isPos0 && isPos1) || (!isPos0 && !isPos1)) {
        return (__number_infinity());
      } else {
        return (__number_neg_infinity());
      }
    }
    case JSTYPE_STRING: {
      bool isConvertible = false;
      TValue leftV = __js_ToNumberSlow2(mv0, isConvertible);
      if (isConvertible) {
        return JSopMul(leftV, mv1, ptyp, op);
      } else {
        return (__nan_value());
      }
    }
    default:
      assert(false && "unexpected mul op0");
  }
}

TValue InterSource::JSopArith(TValue &mv0, TValue &mv1, PrimType ptyp, Opcode op) {
  switch (ptyp) {
    case PTY_i8:
    case PTY_u8:
    case PTY_i16:
    case PTY_u16:
      MIR_FATAL("InterpreteArith: small integer types not supported");
      break;
    case PTY_i32: {
      int32 val;
      int32 v0 = mv0.x.i32;
      int32 v1 = mv1.x.i32;
      switch (op) {
        case OP_ashr:
          val = v0 >> v1;
          break;
        case OP_band:
          val = v0 & v1;
          break;
        case OP_bior:
          val = v0 | v1;
          break;
        case OP_bxor:
          val = v0 ^ v1;
          break;
        case OP_lshr:
          val = v0 >> v1;
          break;
        case OP_max:
          val = v0 > v1 ? v0 : v1;
          break;
        case OP_min:
          val = v0 < v1 ? v0 : v1;
          break;
        case OP_rem:
          val = v0 % v1;
          break;
        case OP_shl:
          val = v0 << v1;
          break;
        case OP_div:
          val = v0 / v1;
          break;
        case OP_sub:
          val = v0 - v1;
          break;
        case OP_mul:
          val = v0 * v1;
          break;
        default:
          MIR_FATAL("unknown i32 arithmetic operations");
      }
      return __number_value(val);
    }
    case PTY_u32:
    case PTY_a32: {
      uint32 val;
      uint32 v0 = mv0.x.u32;
      uint32 v1 = mv1.x.u32;
      switch (op) {
        case OP_ashr:
          val = v0 >> v1;
          break;
        case OP_band:
          val = v0 & v1;
          break;
        case OP_bior:
          val = v0 | v1;
          break;
        case OP_bxor:
          val = v0 ^ v1;
          break;
        case OP_lshr:
          val = v0 >> v1;
          break;
        case OP_max:
          val = v0 > v1 ? v0 : v1;
          break;
        case OP_min:
          val = v0 < v1 ? v0 : v1;
          break;
        case OP_rem:
          val = v0 % v1;
          break;
        case OP_shl:
          val = v0 << v1;
          break;
        case OP_div:
          val = v0 / v1;
          break;
        case OP_sub:
          val = v0 - v1;
          break;
        case OP_mul:
          val = v0 * v1;
          break;
        default:
          MIR_FATAL("unknown u32/a32 arithmetic operations");
          val = 0;  // to suppress warning
      }
      return __number_value(val);
    }
    case PTY_a64: {
      switch (op) {
        case OP_mul: {
         mv0.x.c.payload = GET_PAYLOAD(mv0) * GET_PAYLOAD(mv1);
         return mv0;
        }
        default:
          MIR_FATAL("unknown a64 arithmetic operations");
      }
      break;
    }
    default:
      MIR_FATAL("unknown data types for arithmetic operations");
  }
}

void InterSource::JSPrint(TValue mv0) {
  __jsop_print_item((mv0));
}


bool InterSource::JSopPrimCmp(Opcode op, TValue &mv0, TValue &mv1, PrimType rpty0) {
  bool resBool;
  switch (rpty0) {
    case PTY_i8:
    case PTY_u8:
    case PTY_i16:
    case PTY_u16:
      MIR_FATAL("InterpreteCmp: small integer types not supported");
      break;
    case PTY_i32: {
      int32 val;
      int32 v0 = mv0.x.i32;
      int32 v1 = mv1.x.i32;
      switch(op) {
        case OP_eq: val = v0 == v1; break;
        case OP_ne: val = v0 != v1; break;
        case OP_ge: val = v0 >= v1; break;
        case OP_gt: val = v0 > v1; break;
        case OP_le: val = v0 <= v1; break;
        case OP_lt: val = v0 < v1; break;
        default: assert(false && "error");
      }
      resBool = val;
      break;
    }
    case PTY_u32: {
      uint32 val;
      uint32 v0 = mv0.x.u32;
      uint32 v1 = mv1.x.u32;
      switch(op) {
        case OP_eq: val = v0 == v1; break;
        case OP_ne: val = v0 != v1; break;
        case OP_ge: val = v0 >= v1; break;
        case OP_gt: val = v0 > v1; break;
        case OP_le: val = v0 <= v1; break;
        case OP_lt: val = v0 < v1; break;
        default: assert(false && "error");
      }
      resBool = val;
      break;
    }
    default: assert(false && "error");
  }
  return resBool;
}

TValue InterSource::JSopCmp(TValue mv0, TValue mv1, Opcode op, PrimType ptyp) {
  uint32_t rpty0 = GET_TYPE(mv0);
  uint32_t rpty1 = GET_TYPE(mv1);
  if ((rpty0!= JSTYPE_NONE) || (rpty1 != JSTYPE_NONE)) {
    if (__is_nan(mv0) || __is_nan(mv1)) {
      return __boolean_value(op == OP_ne);
    } else {
      bool isAlwaysFalse = false; // there are some weird comparison to be alwasy false for JS
      bool resCmp = false;
      switch (op) {
        case OP_eq: {
            resCmp = __js_AbstractEquality(mv0, mv1, isAlwaysFalse);
          }
          break;
        case OP_ne: {
            resCmp = !__js_AbstractEquality(mv0, mv1, isAlwaysFalse);
          }
          break;
        case OP_ge: {
            resCmp = !__js_AbstractRelationalComparison(mv0, mv1, isAlwaysFalse);
          }
          break;
        case OP_gt: {
            resCmp = __js_AbstractRelationalComparison(mv1, mv0, isAlwaysFalse, false);
          }
          break;
        case OP_le: {
            resCmp = !__js_AbstractRelationalComparison(mv1, mv0, isAlwaysFalse, false);
          }
          break;
        case OP_lt: {
            resCmp = __js_AbstractRelationalComparison(mv0, mv1, isAlwaysFalse);
          }
          break;
        default:
          MIR_FATAL("unknown comparison ops");
      }
      return __boolean_value(isAlwaysFalse ? false : resCmp);
    }
  }
  assert(rpty0 == rpty1 && "illegal comparison");
  return __boolean_value(JSopPrimCmp(op, mv0, mv1, ptyp));
}

TValue InterSource::JSopCVT(TValue &mv, PrimType toPtyp, PrimType fromPtyp) {
  TValue retMv;
   if (IsPrimitiveDyn(toPtyp)) {
    switch (fromPtyp) {
      case PTY_u1: {
        return __boolean_value(mv.x.u32);
      }
      case PTY_a32:
      case PTY_i32: {
        return __number_value(mv.x.u32);
      }
      case PTY_u32: {
        uint32_t u32Val = mv.x.u32;
        if (u32Val <= INT32_MAX) {
          return __number_value(u32Val);
        } else {
          return __double_value((double)(u32Val));
        }
      }
      case PTY_simpleobj: {
        return __object_value((__jsobject *)GET_PAYLOAD(mv));
      }
      case PTY_simplestr: {
        return __string_value((__jsstring *)GET_PAYLOAD(mv));
      }
      default:
        MIR_FATAL("interpreteCvt: NYI");
    }
  } else if (IsPrimitiveDyn(fromPtyp)) {
    switch (toPtyp) {
      case PTY_u1: {
        return __boolean_value(__js_ToBoolean(mv));
      }
      case PTY_a32:
      case PTY_u32:
      case PTY_i32: {
        return __number_value(__js_ToInteger(mv));
      }
      case PTY_simpleobj: {
        assert(false&&"NYI");
        break;
      }
      default:
        MIR_FATAL("interpreteCvt: NYI");
    }
  }
  return __undefined_value();
}

TValue InterSource::JSopNew(TValue &size) {
  return __env_value(VMMallocGC(size.x.i32, MemHeadEnv));
}

TValue InterSource::JSopNewArrLength(TValue &conVal) {
  return __object_value((__jsobject*)__js_new_arr_length(conVal));
}


void InterSource::JSopSetProp(TValue &mv0, TValue &mv1, TValue &mv2) {
  __jsop_setprop(mv0, mv1, mv2);
  // check if it's set prop for arguments
  DynMFunction *curFunc = GetCurFunc();
  if (!curFunc->is_strict() && __is_js_object(mv0)) {
    __jsobject *obj = __jsval_to_object(mv0);
    if (obj == (__jsobject *)curFunc->argumentsObj) {
      //uint32_t jstp = mv1.ptyp;
      if (IS_NUMBER(mv1.x.u64)) {
        uint32_t index = mv1.x.u32;
        uint32_t numArgs = curFunc->header->upFormalSize/sizeof(void *);
        if (index < numArgs - 1 && !curFunc->IsIndexDeleted(index)) {
          void **addrFp = (void **)((uint8 *)GetFPAddr() + (index + 1)*sizeof(void *));
          if (IS_NEEDRC(*addrFp))
            memory_manager->GCDecRf(*addrFp);
          if (IS_NEEDRC(mv2.x.u64))
            memory_manager->GCIncRf((void*)mv2.x.c.payload);
          EmulateStore((uint8_t *)addrFp, mv2);
        }
      }
    }
  }
}

void InterSource::JSopInitProp(TValue &mv0, TValue &mv1, TValue &mv2) {
  __jsop_initprop(mv0, mv1, mv2);
  // check if it's set prop for arguments
  DynMFunction *curFunc = GetCurFunc();
  if (!curFunc->is_strict() && __is_js_object(mv0)) {
    __jsobject *obj = __jsval_to_object(mv0);
    if (obj == (__jsobject *)curFunc->argumentsObj) {
      //uint32_t jstp = mv1.ptyp;
      if (IS_NUMBER(mv1.x.u64)) {
        uint32_t index = mv1.x.u32;
        uint32_t numArgs = curFunc->header->upFormalSize/sizeof(void *);
        if (index < numArgs - 1 && !curFunc->IsIndexDeleted(index)) {
          void **addrFp = (void **)((uint8 *)GetFPAddr() + (index + 1)*sizeof(void *));
          if (IS_NEEDRC(*addrFp))
            memory_manager->GCDecRf(*addrFp);
          if (IS_NEEDRC(mv2.x.u64))
            memory_manager->GCIncRf((void*)mv2.x.c.payload);
          EmulateStore((uint8_t *)addrFp, mv2);
        }
      }
    }
  }
}

TValue InterSource::JSopNewIterator(TValue &mv0, TValue &mv1) {
  return __string_value((__jsstring*)__jsop_valueto_iterator(mv0, mv1.x.u32));
}

TValue InterSource::JSopNextIterator(TValue &mv0) {
  return __jsop_iterator_next((void *)GET_PAYLOAD(mv0));
}

TValue InterSource::JSopMoreIterator(TValue &mv0) {
  return __boolean_value(__jsop_more_iterator((void *)GET_PAYLOAD(mv0)));
}

void InterSource::InsertProlog(uint16_t frameSize) {
  uint8 *addrs = (uint8 *)memory + sp;
  *(uint32_t *)(addrs - 4) = fp;
  memset(addrs - frameSize,0,frameSize - 4);
  fp = sp;
  sp -= frameSize;
}

TValue InterSource::JSopBinary(MIRIntrinsicID id, TValue &mv0, TValue &mv1) {
  uint64_t u64Ret = 0;
  switch (id) {
    case INTRN_JSOP_STRICTEQ:
      u64Ret = __jsop_stricteq(mv0, mv1);
      break;
    case INTRN_JSOP_STRICTNE:
      u64Ret = __jsop_strictne(mv0, mv1);
      break;
    case INTRN_JSOP_INSTANCEOF:
      u64Ret = __jsop_instanceof(mv0, mv1);
      break;
    case INTRN_JSOP_IN:
      u64Ret = __jsop_in(mv0, mv1);
      break;
    default:
      MIR_FATAL("unsupported binary operators");
  }
  return __boolean_value(u64Ret);
}



TValue InterSource::JSopConcat(TValue &mv0, TValue &mv1) {
  __jsstring *v0 = (__jsstring *)GET_PAYLOAD(mv0);
  __jsstring *v1 = (__jsstring *)GET_PAYLOAD(mv1);
  return __string_value(__jsstr_concat_2(v0, v1));
}

TValue InterSource::JSopNewObj0() {
  return __object_value(__js_new_obj_obj_0());
}

TValue InterSource::JSopNewObj1(TValue &mv0) {
  return __object_value(__js_new_obj_obj_1(mv0));
}

void InterSource::JSopInitThisPropByName (TValue &mv0) {
  TValue &v0 = __js_Global_ThisBinding;
  __jsstring *v1 = (__jsstring *) GET_PAYLOAD(mv0);
  __jsop_init_this_prop_by_name(v0, v1);
}

void InterSource::JSopSetThisPropByName (TValue &mv1, TValue &mv2) {
  TValue &v0 = __js_Global_ThisBinding;
  __jsstring *v1 = (__jsstring *) GET_PAYLOAD(mv1);
  if(GetCurFunc()->is_strict() && (__is_undefined(__js_ThisBinding) || \
              __js_SameValue(__js_Global_ThisBinding, __js_ThisBinding)) && \
          __jsstr_throw_typeerror(v1)) {
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }
  __jsop_set_this_prop_by_name(v0, v1, mv2, true);
}

void InterSource::JSopSetPropByName (TValue &mv0, TValue &mv1, TValue &mv2, bool isStrict) {
  __jsstring *v1 = (__jsstring *) GET_PAYLOAD(mv1);
  if (mv0.x.u64 == __js_Global_ThisBinding.x.u64 &&
    __is_global_strict && __jsstr_throw_typeerror(v1)) {
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }
  __jsop_setprop_by_name(mv0, v1, mv2, isStrict);
}


TValue InterSource::JSopGetProp (TValue &mv0, TValue &mv1) {
  return (__jsop_getprop(mv0, mv1));
}

TValue InterSource::JSopGetThisPropByName(TValue &mv1) {
  __jsstring *v1 = (__jsstring *) GET_PAYLOAD(mv1);
  return (__jsop_get_this_prop_by_name(__js_Global_ThisBinding, v1));
}

TValue InterSource::JSopGetPropByName(TValue &mv0, TValue &mv1) {
  if (__is_undefined(mv0)) {
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }
  __jsstring *v1 = (__jsstring *) GET_PAYLOAD(mv1);
  return (__jsop_getprop_by_name(mv0, v1));
}


TValue InterSource::JSopDelProp (TValue &mv0, TValue &mv1, bool throw_p) {
  TValue retMv = (__jsop_delprop(mv0, mv1, throw_p));
  DynMFunction *curFunc = GetCurFunc();
  if (!curFunc->is_strict() && __is_js_object(mv0)) {
    __jsobject *obj = __jsval_to_object(mv0);
    if (obj == (__jsobject *)curFunc->argumentsObj) {
      if (IS_NUMBER(mv1.x.u64)) {
        uint32_t index = mv1.x.u32;
        curFunc->MarkArgumentsDeleted(index);
      }
    }
  }
  return retMv;
}

void InterSource::JSopInitPropByName(TValue &mv0, TValue &mv1, TValue &mv2) {
  __jsstring *v1 = (__jsstring *) GET_PAYLOAD(mv1);
  __jsop_initprop_by_name(mv0, v1, mv2);
}


void InterSource::JSopInitPropGetter(TValue &mv0, TValue &mv1, TValue &mv2) {
  __jsop_initprop_getter(mv0, mv1, mv2);
}

void InterSource::JSopInitPropSetter(TValue &mv0, TValue &mv1, TValue &mv2) {
  __jsop_initprop_setter(mv0, mv1, mv2);
}

TValue InterSource::JSopNewArrElems(TValue &mv0, TValue &mv1) {
  return __object_value(__js_new_arr_elems(mv0, mv1.x.u32));
}


TValue InterSource::JSopGetBuiltinString(TValue &mv) {
  return __string_value(__jsstr_get_builtin((__jsbuiltin_string_id)mv.x.u32));
}

TValue InterSource::JSopGetBuiltinObject(TValue &mv) {
  return __object_value(__jsobj_get_or_create_builtin((__jsbuiltin_object_id)mv.x.u32));
}

TValue InterSource::JSBoolean(TValue &mv) {
  return __boolean_value(__js_ToBoolean(mv));
}

TValue InterSource::JSNumber(TValue &mv) {
  if (IS_NUMBER(mv.x.u64)) // fast path
    return mv;

  if (__is_undefined(mv)) {
    return (__nan_value());
  }
  if (__is_double(mv)) {
    return mv;
  }
  bool ok2Convert = false;
  TValue i32v = __js_ToNumber2(mv, ok2Convert);
  if (!ok2Convert) {
    return (__nan_value());
  } else {
    return i32v;
  }
}

const char *WeekDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *Months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

TValue InterSource::JSDate(uint32_t nargs, TValue *args) {
  //TODO: Handle return string correctly.
  char charray[64] = "";
  //__jsstring *jsstr = __jsstr_new_from_char(charray);
  TValue res;

  TValue now = __jsdate_Now();
  double time = __jsval_to_double(now);
  int time_zone_minutes = (int)LocalTZA() / kMsPerMinute;
  char buf[100];
  snprintf(buf, sizeof(buf), "%s %s %.2d %.4d %.2d:%.2d:%.2d GMT%+.2d%.2d",
           WeekDays[WeekDay(time)],
           Months[(int)MonthFromTime(time)],
           (int)DateFromTime(time),
           (int)YearFromTime(time),
           (int)HourFromTime(time),
           (int)MinFromTime(time),
           (int)SecFromTime(time),
           time_zone_minutes / 60,
           time_zone_minutes > 0 ? time_zone_minutes % 60 : -time_zone_minutes % 60);

  res = __string_value(__jsstr_new_from_char(buf));
  return res;
}

TValue InterSource::JSRegExp(TValue &mv) {
  // Note that, different from other functions, mv contains JS string as simplestr instead of dynstr.
  __jsstring *jsstr = (__jsstring *) GET_PAYLOAD(mv);
  assert(jsstr && "shouldn't be null");

  return __object_value(__js_ToRegExp(jsstr));
}

TValue InterSource::JSString(TValue &mv) {
  TValue v0 = mv;
  if (__is_none(v0)) {
    if (GET_TYPE(mv) == PTY_dynany)
      v0 = __undefined_value();
  }
  return __string_value(__js_ToString(v0));
}

TValue InterSource::JSStringVal(TValue &mv) {
  TValue v0 = mv;
  if (__is_none(v0)) {
    if (GET_TYPE(mv) == PTY_dynany)
      v0 = __undefined_value();
  } else if (__is_string(v0)) {
    return __string_value(__jsval_to_string(v0));
  }
  __jsstring *jsStr = NULL;
  switch (__jsval_typeof(v0)) {
    case JSTYPE_UNDEFINED:
      jsStr = __jsstr_get_builtin(JSBUILTIN_STRING_UNDEFINED);
      break;
    case JSTYPE_NULL:
      jsStr = __jsstr_get_builtin(JSBUILTIN_STRING_NULL);
      break;
    case JSTYPE_BOOLEAN:
      jsStr = __jsval_to_boolean(v0) ? __jsstr_get_builtin(JSBUILTIN_STRING_TRUE)
                                   : __jsstr_get_builtin(JSBUILTIN_STRING_FALSE);
      break;
    case JSTYPE_NUMBER:
      jsStr =  __js_NumberToString(__jsval_to_int32(v0));
      break;
    case JSTYPE_DOUBLE: {
      if (v0.x.u64 == NEG_ZERO) {
        jsStr =  __jsstr_get_builtin(JSBUILTIN_STRING_ZERO_CHAR);
      } else {
        jsStr = __js_DoubleToString(__jsval_to_double(v0));
      }
      break;
    }
    case JSTYPE_OBJECT: {
      TValue prim_value = __js_ToPrimitive(v0, JSTYPE_UNDEFINED);
      if (!__is_string(prim_value)) {
        GCCheckAndIncRf(GET_PAYLOAD(prim_value), IS_NEEDRC(prim_value.x.u64));
      }
      __jsstring *str = __js_ToString(prim_value);
      if (!__is_string(prim_value)) {
        GCCheckAndDecRf(GET_PAYLOAD(prim_value), IS_NEEDRC(prim_value.x.u64));
      }
      jsStr = str;
      break;
    }
    case JSTYPE_NONE:
      jsStr = __jsstr_get_builtin(JSBUILTIN_STRING_UNDEFINED);
      break;
    case JSTYPE_NAN:
      jsStr = __jsstr_get_builtin(JSBUILTIN_STRING_NAN);
      break;
    case JSTYPE_INFINITY:{
      jsStr = __jsstr_get_builtin(__is_neg_infinity(v0) ? JSBUILTIN_STRING_NEG_INFINITY_UL: JSBUILTIN_STRING_INFINITY_UL);
      break;
    }
    default:
      MAPLE_JS_ASSERT(false && "unreachable.");
  }
  return __string_value(jsStr == NULL ? __jsstr_get_builtin(JSBUILTIN_STRING_NULL) : jsStr);
}

TValue InterSource::JSopLength(TValue &mv) {
  if (IS_STRING(mv.x.u64)) {
    __jsstring *primstring = __js_ToString(mv);
    TValue length_value = __number_value(__jsstr_get_length(primstring));
    return length_value;
  } else {
    __jsobject *obj = __js_ToObject(mv);
    return __jsobj_helper_get_length_value(obj);
  }
}

TValue InterSource::JSopThis() {
  return __js_ThisBinding;
}

TValue InterSource::JSUnary(MIRIntrinsicID id, TValue &mval) {
  TValue res;
  switch (id) {
    case INTRN_JSOP_TYPEOF:
      res = __jsop_typeof(mval);
      break;
    default:
      MIR_FATAL("unsupported unary ops");
  }
  return res;
}

bool InterSource::JSopSwitchCmp(TValue &mv0, TValue &mv1, TValue &mv2) {
  if (!IS_NUMBER(mv1.x.u64))
    return false;
  Opcode cmpOp = (Opcode)mv0.x.u32;
  return JSopPrimCmp(cmpOp, mv1, mv2, PTY_i32);
}

TValue InterSource::JSopUnaryLnot(TValue &mval) {
  int32_t xval = 0;
  bool ok2convert = false;
    switch (__jsval_typeof(mval)) {
      case JSTYPE_BOOLEAN:
      case JSTYPE_DOUBLE:
      case JSTYPE_NUMBER:
      case JSTYPE_INFINITY:
        return __boolean_value(!__js_ToBoolean(mval));
      case JSTYPE_OBJECT:
      case JSTYPE_STRING:
      case JSTYPE_GPBASE: // must be simplestr type
        return __boolean_value(false);
      case JSTYPE_NAN:
      case JSTYPE_NULL:
      case JSTYPE_UNDEFINED:
        return __boolean_value(true);
      default: {
        MIR_FATAL("wrong type");
      }
    }
}

TValue InterSource::JSopUnaryBnot(TValue &mval) {
  int32_t xval = 0;
  bool ok2convert = false;
  TValue jsVal =__js_ToNumber2(mval, ok2convert);
  if (ok2convert) {
    switch (__jsval_typeof(jsVal)) {
      case JSTYPE_NAN:
        return __number_value(-1);
      case JSTYPE_DOUBLE:
      case JSTYPE_NUMBER:
      case JSTYPE_INFINITY: {
        return __number_value(~__js_ToNumber(jsVal));
      }
      default:
        MIR_FATAL("wrong type");
    }
  } else {
    switch (__jsval_typeof(jsVal)) {
      case JSTYPE_NAN:
          return __number_value(~0);
      default: {
        return JSopUnaryBnot(jsVal);
      }
    }
  }
}

TValue InterSource::JSopUnaryNeg(TValue &mval) {
  switch (__jsval_typeof(mval)) {
    case JSTYPE_BOOLEAN: {
      bool isBoo =  __jsval_to_boolean(mval);
      return __number_value(isBoo ? -1 : 0);
    }
    case JSTYPE_NUMBER: {
      int32_t xx = __jsval_to_number(mval);
      if (xx == 0) {
        return __negative_zero_value();
      } else {
        return __number_value(-xx);
      }
    }
    case JSTYPE_DOUBLE: {
      double dv = __jsval_to_double(mval);
      if (fabs(dv - 0.0f) < NumberMinValue) {
            return (__positive_zero_value());
      } else {
            return (__double_value(-dv));
      }
    }
    case JSTYPE_STRING: {
      bool isConvertible = false;
        //TValue v = (mval);
      TValue strval = __js_ToNumberSlow2(mval, isConvertible);
      TValue mVal = (strval);
      return JSopUnaryNeg(mVal);
    }
    case JSTYPE_INFINITY: {
      return (__is_neg_infinity(mval) ? __number_infinity() : __number_neg_infinity());
    }
    case JSTYPE_UNDEFINED:
    case JSTYPE_NAN:
      return (__nan_value());
    case JSTYPE_NULL:
      return (__positive_zero_value());
    case JSTYPE_OBJECT: {
      __jsobject *obj = __jsval_to_object(mval);
      TValue xval = __object_internal_DefaultValue(obj, JSTYPE_NUMBER);
      return JSopUnaryNeg(xval);
    }
    default:
     MIR_FATAL("InterpreteUnary: NYI");
  }
}

TValue InterSource::JSopUnary(TValue &mval, Opcode op, PrimType pty) {
  MIR_FATAL("InterpreteUnary: NYI");
}


TValue InterSource::JSIsNan(TValue &mval) {
  switch (GET_TYPE(mval)) {
    case JSTYPE_STRING: {
      return __number_value(__jsstr_is_number((__jsstring *)GET_PAYLOAD(mval)));
      break;
    }
    case JSTYPE_NAN: {
      return __number_value(1);
    }
    default:
      return __number_value(0);
  }
}

void InterSource::JsTry(void *tryPc, void *catchPc, void *finallyPc, DynMFunction *func) {
  JsEh *eh;
  if (EHstackReuseSize) {
    eh = EHstackReuse.Pop();
    EHstackReuseSize--;
  } else {
    eh = (JsEh *)VMMallocNOGC(sizeof(JsEh));
  }
  eh->dynFunc = func;
  eh->Init(tryPc, catchPc, finallyPc);
  EHstack.Push(eh);
  currEH = eh;
}

void* InterSource::CreateArgumentsObject(TValue *mvArgs, uint32_t passedNargs, TValue &calleeMv) {
  __jsobject *argumentsObj = __create_object();
  __jsobj_set_prototype(argumentsObj, JSBUILTIN_OBJECTPROTOTYPE);
  argumentsObj->object_class = JSARGUMENTS;
  argumentsObj->extensible = (uint8_t)true;
  argumentsObj->object_type = (uint8_t)JSREGULAR_OBJECT;
  uint32_t actualArgs = 0;
  for (int32 i = 0; i < passedNargs; i++) {
    TValue elemVal = mvArgs[i];
    if (__is_undefined(elemVal)) {
      continue;
    }
    if(IS_NEEDRC(mvArgs[i].x.u64))
      memory_manager->GCIncRf((void*)mvArgs[i].x.c.payload);
    actualArgs++;
    // __jsobj_internal_Put(argumentsObj, i, &elemVal, false);
    __jsobj_helper_init_value_propertyByValue(argumentsObj, i, elemVal, JSPROP_DESC_HAS_VWEC);
  }
  __jsobj_helper_init_value_property(argumentsObj, JSBUILTIN_STRING_CALLEE, calleeMv, JSPROP_DESC_HAS_VWUEC);
  TValue lengthJv = __number_value(actualArgs);
  __jsobj_helper_init_value_property(argumentsObj, JSBUILTIN_STRING_LENGTH, lengthJv, JSPROP_DESC_HAS_VWUEC);
  TValue jsObj = __object_value(argumentsObj);
  __jsop_set_this_prop_by_name(__js_Global_ThisBinding, __jsstr_get_builtin(JSBUILTIN_STRING_ARGUMENTS), jsObj, true);
  memory_manager->GCIncRf(argumentsObj); // so far argumentsObj is supposed to be refered twice. #1 for global this, #2 for the DynMFunction
  return (void *)argumentsObj;
}

TValue InterSource::IntrinCall(MIRIntrinsicID id, TValue *args, int numArgs) {
  TValue mval0 = args[0];
  // 11.2.3 step 4: if args[0] is not object, throw type exception
  if (!IS_OBJECT(mval0.x.u64)) {
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }
  //__jsobject *f = (__jsobject *)memory_manager->GetRealAddr(GetMvalueValue(mval0));;
  __jsobject *f = (__jsobject *)GET_PAYLOAD(mval0);
  __jsfunction *func = (__jsfunction *)f->shared.fun;
  TValue retCall;
  retCall.x.u64 = 0;
  if (!func || f->object_class != JSFUNCTION) {
    // trying to call a null function, throw TypeError directly
    MAPLE_JS_TYPEERROR_EXCEPTION();
  }
  if (func->attrs & 0xff & JSFUNCPROP_NATIVE || id == INTRN_JSOP_NEW) {
    retCall = NativeFuncCall(id, args, numArgs);
    return retCall;
  }
  if (func->attrs & 0xff & JSFUNCPROP_BOUND) {
    retCall = BoundFuncCall(args, numArgs);
    return retCall;
  }
  void *callee = func->fp;
  int nargs = func->attrs >> 16 & 0xff;
  bool strictP = func->attrs & 0xff & JSFUNCPROP_STRICT;
  JsFileInforNode *oldFin = jsPlugin->formalFileInfo;
  bool contextSwitched = false;
  if (func->fileIndex != -1 && ((uint32_t)func->fileIndex != jsPlugin->formalFileInfo->fileIndex)) {
    JsFileInforNode *newFileInfo = jsPlugin->FindJsFile((uint32_t)func->fileIndex);
    SwitchPluginContext(newFileInfo);
    contextSwitched = true;
  }
  retCall = FuncCall(callee, true, func->env, args, numArgs, 2, nargs, strictP);
  if (contextSwitched)
    RestorePluginContext(oldFin);
  return retCall;
}

TValue InterSource::NativeFuncCall(MIRIntrinsicID id, TValue *args, int numArgs) {
  int argNum = numArgs - 2;
  TValue &funcNode = args[0];
  TValue &thisNode = args[1];
  TValue *jsArgs = &args[2];
  // for __jsobj_defineProperty to arguments built-in, it will affects the actual parameters
  DynMFunction *curFunc = GetCurFunc();
  if (!curFunc->is_strict() && id != INTRN_JSOP_NEW && argNum == 3 && __js_IsCallable(funcNode)) {
    // for Object.defineProperty (arguments, "0", {})
    // the number of args is 3
    __jsobject *fObj = __jsval_to_object(funcNode);
    __jsfunction *func = fObj->shared.fun;
    if (func->fp == __jsobj_defineProperty) {
      TValue arg0 = jsArgs[0];
      TValue arg1 = jsArgs[1];
      if (__is_js_object(arg0)) {
        __jsobject *obj = __jsval_to_object(arg0);
        if (obj == (__jsobject *)curFunc->argumentsObj) {
          int32_t index = -1;
          if (__is_number(arg1)) {
            index = __jsval_to_int32(arg1);
          }else if (__is_string(arg1)) {
            bool isNum;
            index = __js_str2num2(__jsval_to_string(arg1), isNum, true);
            if (!isNum)
              index = -1;
          }
          if (index != -1 && !curFunc->IsIndexDeleted(index)) {
            assert(index >=0 && index <=32 && "extended the range");
            TValue idxJs = __number_value(index);
            TValue arg2 = jsArgs[2];
            bool isOldWritable = __jsPropertyIsWritable(obj, index);
            __jsobj_defineProperty(thisNode, arg0, idxJs, arg2);
            TValue idxValue = __jsobj_GetValueFromPropertyByValue(obj, index);
            if (!__is_undefined(idxValue) && isOldWritable) {
              void **addrFp = (void **)((uint8 *)GetFPAddr() + (index + 1)*sizeof(void *));
              if (IS_NEEDRC(*addrFp))
                memory_manager->GCDecRf(*addrFp);
              if (IS_NEEDRC(idxValue.x.u64))
                memory_manager->GCIncRf((void*)idxValue.x.c.payload);
              EmulateStore((uint8_t *)addrFp, idxValue);
            }
            SetRetval0(arg0);
            return arg0;
          }
        }
      }
    }
  }
  TValue res = (id == INTRN_JSOP_NEW) ? __jsop_new(funcNode, thisNode, &jsArgs[0], argNum) :
                        __jsop_call(funcNode, thisNode, &jsArgs[0], argNum);
  SetRetval0(res);
  return res;
}

TValue InterSource::BoundFuncCall(TValue *args, int numArgs) {
  TValue mv0 = args[0];
  int argNum = numArgs - 2;
  // TValue jsArgs[MAXCALLARGNUM];
  // __jsobject *f = (__jsobject *)memory_manager->GetRealAddr(GetMvalueValue(mv0));
  __jsobject *f = (__jsobject *)GET_PAYLOAD(mv0);
  __jsfunction *func = (__jsfunction *)f->shared.fun;
  MIR_ASSERT(func->attrs & 0xff & JSFUNCPROP_BOUND);
  TValue func_node = __object_value((__jsobject *)(func->fp));
  TValue bound_this;
  TValue *bound_args;
  if (func->env) {
    bound_this = ((TValue *)func->env)[0];
    bound_args = &((TValue *)func->env)[1];
  } else {
    bound_this = __undefined_value();
    bound_args = NULL;
  }
  int32_t bound_argnumber = (((func->attrs) >> 16) & 0xff) - 1;
  bound_argnumber = bound_argnumber >= 0 ? bound_argnumber : 0;
  TValue jsArgs[MAXCALLARGNUM];
  MIR_ASSERT(argNum + bound_argnumber <= MAXCALLARGNUM);
  for (int32_t i = 0; i < bound_argnumber; i++) {
    if (bound_args != NULL)
      jsArgs[i] = bound_args[i];
    else
      jsArgs[i] = __undefined_value();
  }
  for (uint32 i = 0; i < argNum; i++) {
    jsArgs[i + bound_argnumber] = (args[2 + i]);
  }
  TValue retCall = (__jsop_call(func_node, bound_this, &jsArgs[0], bound_argnumber + argNum));
  SetRetval0(retCall);
  return retCall;
}

TValue InterSource::FuncCall(void *callee, bool isIntrinsiccall, void *env, TValue *args, int numArgs,
                int start, int nargs, bool strictP) {
  int32_t passedNargs = numArgs - start;
  TValue mvArgs[MAXCALLARGNUM];
  MIR_ASSERT(passedNargs <= MAXCALLARGNUM);
  for (int i = 0; i < passedNargs; i++) {
    mvArgs[i] = args[i + start];
  }
  TValue thisval = (__undefined_value());
  if (isIntrinsiccall) {
    thisval = args[1];
  }
  int32_t offset = PassArguments(thisval, env, mvArgs, passedNargs, nargs);
  sp += offset;
  TValue this_arg = (thisval);

  DynamicMethodHeaderT* calleeHeader = (DynamicMethodHeaderT *)((uint8_t *)callee + 4);
  TValue old_this = __js_entry_function(this_arg, (calleeHeader->attribute & FUNCATTRSTRICT) | strictP);
  TValue ret;
  DynMFunction *oldDynFunc = GetCurFunc();
  if (DynMFunction::is_jsargument(calleeHeader)) {
    TValue oldArgs = __jsop_get_this_prop_by_name(__js_Global_ThisBinding, __jsstr_get_builtin(JSBUILTIN_STRING_ARGUMENTS));
    // bool isVargs = (passedNargs > 0) && ((calleeHeader->upFormalSize/8 - 1) != passedNargs);
    // TValue ret = maple_invoke_dynamic_method(calleeHeader, !isVargs ? nullptr : CreateArgumentsObject(mvArgs, passedNargs));
    void* argsObj = CreateArgumentsObject(mvArgs, passedNargs, args[0]);
    ret = maple_invoke_dynamic_method(calleeHeader, argsObj);
    __jsop_set_this_prop_by_name(__js_Global_ThisBinding, __jsstr_get_builtin(JSBUILTIN_STRING_ARGUMENTS), oldArgs, true);
    memory_manager->GCDecRf(argsObj);
  } else {
    ret = maple_invoke_dynamic_method(calleeHeader, NULL);
  }

  __js_exit_function(this_arg, old_this, (calleeHeader->attribute & FUNCATTRSTRICT)|strictP);
  sp -= offset;
  SetCurFunc(oldDynFunc);

  // RC-- for local vars
  // RC is increased for args and decreased after the func call, therefore, the pair of RC ops can be eliminated.
  uint8 *spaddr = (uint8 *)GetSPAddr();
  uint8 *frameEnd = spaddr + offset;  // offset is negative
  uint8 *addr = frameEnd - calleeHeader->frameSize;
  // frame: between addr and frameEnd; args: between frameEnd and spaddr
#ifdef RC_OPT_FUNC_ARGS
  while(addr < frameEnd) {
#else
  while(addr < spaddr) {
#endif
    TValue local = *(TValue *)addr;
    if (IS_NEEDRC(local.x.u64)) {
      memory_manager->GCDecRf((void*)local.x.c.payload);
    }
    addr += sizeof(void*);
  }

  return ret;
}

TValue InterSource::FuncCall_JS(__jsobject *fObject, TValue &this_arg, void *env, TValue *arg_list, int32_t nargs) {
  __jsfunction *func = fObject->shared.fun;
  void *callee = func->fp;
  if (!callee) {
    return (__undefined_value());
  }

  TValue mvArgList[MAXCALLARGNUM];
  for (int i = 0; i < nargs; i++) {
    mvArgList[i] = (arg_list[i]);
  }

  int32_t func_nargs = func->attrs >> 16 & 0xff;
  int32_t offset = PassArguments(this_arg, env, mvArgList, nargs, func_nargs);
  // Update sp_, set sp_ to sp_ + offset.
  sp += offset;
  DynamicMethodHeaderT* calleeHeader = (DynamicMethodHeaderT*)((uint8_t *)callee + 4);
  DynMFunction *oldDynFunc = GetCurFunc();
  TValue ret;
  if (DynMFunction::is_jsargument(calleeHeader)) {
    TValue oldArgs = __jsop_get_this_prop_by_name(__js_Global_ThisBinding, __jsstr_get_builtin(JSBUILTIN_STRING_ARGUMENTS));
    // bool isVargs = (nargs > 0) && ((calleeHeader->upFormalSize/8 - 1) != nargs);
    // TValue ret = maple_invoke_dynamic_method(calleeHeader, isVargs ? nullptr : CreateArgumentsObject(mvArgList, nargs));
    TValue fObjMv = (__object_value(fObject));
    ret = maple_invoke_dynamic_method(calleeHeader,  CreateArgumentsObject(mvArgList, nargs, fObjMv));
    if (GET_PAYLOAD(oldArgs) != 0) {
      __jsop_set_this_prop_by_name(__js_Global_ThisBinding, __jsstr_get_builtin(JSBUILTIN_STRING_ARGUMENTS), oldArgs);
    } else {
      __jsobj_internal_Delete(__jsval_to_object(__js_Global_ThisBinding), __jsstr_get_builtin(JSBUILTIN_STRING_ARGUMENTS));
    }
  } else {
    ret = maple_invoke_dynamic_method(calleeHeader, NULL);
  }

  // Restore sp_, set sp_ to sp_ - offset.
  sp -= offset;
  SetCurFunc(oldDynFunc);

  // RC-- for local vars
  // RC is increased for args and decreased after the func call, therefore, the pair of RC ops can be eliminated.
  uint8 *spaddr = (uint8 *)GetSPAddr();
  uint8 *frameEnd = spaddr + offset;  // offset is negative
  uint8 *addr = frameEnd - calleeHeader->frameSize;
  // frame: between addr and frameEnd; args: between frameEnd and spaddr
#ifdef RC_OPT_FUNC_ARGS
  while(addr < frameEnd) {
#else
  while(addr < spaddr) {
#endif
    TValue local = *(TValue*)addr;
    if (IS_NEEDRC(local.x.u64)) {
      memory_manager->GCDecRf((void*)local.x.c.payload);
    }
    addr += sizeof(void*);
  }

  return ret;
}


void InterSource::IntrnError(TValue *args, int numArgs) {
  for (int i = 0; i < numArgs; i++)
    JSPrint(args[i]);
  exit(8);
}

TValue InterSource::IntrinCCall(TValue *args, int numArgs) {
  TValue mval0 = args[0];
  const char *name = __jsval_to_string(mval0)->x.ascii;
  funcCallback funcptr = getCCallback(name);
  TValue &mval1 = args[1];
  uint32 argc = mval1.x.u32;
  uint32 argsSize = argc * sizeof(uint32);
  int *argsI = (int *)VMMallocNOGC(argsSize);
  for (uint32 i = 0; i < argc; i++) {
    argsI[i] = (int)args[2+i].x.i32;
  }

  // make ccall
  TValue res = __number_value(funcptr(argsI));
  SetRetval0(res);
  VMFreeNOGC(argsI, argsSize);
  return __none_value();
}

TValue InterSource::JSopRequire(TValue &mv0) {
  __jsstring *str = __js_ToString(mv0);
  uint32_t length = __jsstr_get_length(str);
  char fileName[100];
  assert(length + 5 + 1 < 100 && "required file name tool long");
  for (uint32 i = 0; i < length; i++) {
    fileName[i] = (char)__jsstr_get_char(str, i);
  }
  memcpy(&fileName[length], ".so", sizeof(".so"));
  fileName[length + 3] = '\0';
  bool isCached = false;
  JsFileInforNode *jsFileInfo = jsPlugin->LoadRequiredFile(fileName, isCached);
  if (isCached) {
    TValue objJsval = __object_value(__jsobj_get_or_create_builtin(JSBUILTIN_MODULE));
    TValue retJsval = __jsop_getprop_by_name(objJsval, __jsstr_get_builtin(JSBUILTIN_STRING_EXPORTS));
    SetRetval0(retJsval);
    return retVal0;
  }
  JsFileInforNode *formalJsFileInfo = jsPlugin->formalFileInfo;
  if (formalJsFileInfo == jsFileInfo) {
   MIR_FATAL("plugin formalfile is the same with current file");
  }
  MIR_ASSERT(jsFileInfo);
  SwitchPluginContext(jsFileInfo);
  DynMFunction *oldDynFunc = GetCurFunc();
  // DynMFunction *entryFunc = jsFileInfo->GetEntryFunction();
  DynamicMethodHeaderT * header = (DynamicMethodHeaderT *)(jsFileInfo->mainFn);
  maple_invoke_dynamic_method(header, NULL);
  SetCurFunc(oldDynFunc);
  RestorePluginContext(formalJsFileInfo);

  // RC-- for local vars
  uint8 *spaddr = (uint8 *)GetSPAddr();
  uint8 *addr = spaddr - header->frameSize;
  while(addr < spaddr) {
    TValue* local = (TValue*)addr;
    if (IS_NEEDRC(local->x.u64)) {
      memory_manager->GCDecRf((void*)local->x.c.payload);
    }
    addr += sizeof(void*);
  }

  // SetCurrFunc();
  // restore
  return retVal0;
}

uint64_t InterSource::GetIntFromJsstring(__jsstring* jsstr) {
  bool isConvertible = false;
  TValue jsDv = __js_str2double(jsstr, isConvertible);
  if (!isConvertible)
    return 0;
  return (uint64_t)__jsval_to_double(jsDv);
}

void InterSource::SwitchPluginContext(JsFileInforNode *jsFIN) {
  gp = jsFIN->gp;
  topGp = gp + jsFIN->glbMemsize;
  jsPlugin->formalFileInfo = jsFIN;
}

void InterSource::RestorePluginContext(JsFileInforNode *formaljsfileinfo) {
  jsPlugin->formalFileInfo = formaljsfileinfo;
  gp = formaljsfileinfo->gp;
  topGp = formaljsfileinfo->gp + formaljsfileinfo->glbMemsize;
}

void InterSource::JSdoubleConst(uint64_t u64Val, TValue &res) {
  res.x.u64 = u64Val;
}

void InterSource::InsertEplog() {
  sp = fp;
  fp = *(uint32_t *)((uint8_t *)memory + sp - 4);
}

void InterSource::UpdateArguments(int32_t index, TValue &mv) {
  if (!GetCurFunc()->IsIndexDeleted(index)) {
    __jsobject *obj = (__jsobject *)curDynFunction->argumentsObj;
    TValue v0 = __object_value(obj);
    TValue v1 = __number_value(index);
    __jsop_setprop(v0, v1, mv);
  }
}

void InterSource::CreateJsPlugin(char *fileName) {
  jsPlugin = (JsPlugin *)malloc(sizeof(JsPlugin));
  jsPlugin->mainFileInfo = (JsFileInforNode *)malloc(sizeof(JsFileInforNode));
  jsPlugin->fileIndex = 0;
  uint32 i = jsPlugin->GetIndexForSo(fileName);
  jsPlugin->mainFileInfo->Init(&fileName[i], jsPlugin->fileIndex, gp, topGp - gp, nullptr, nullptr);
  jsPlugin->formalFileInfo = jsPlugin->mainFileInfo;
}

TValue InterSource::JSopGetArgumentsObject(void *argumentsObject) {
  return (__object_value((__jsobject *)(argumentsObject)));
}

TValue InterSource::GetOrCreateBuiltinObj(__jsbuiltin_object_id id) {
  TValue refConVal = __object_value(__jsobj_get_or_create_builtin(id));
  TValue jsVal = __jsop_new(refConVal, __number_value(0), nullptr, 0);
  return jsVal;
}

extern "C" int64_t EngineShimDynamic(int64_t firstArg, char *appPath) {
  if (!jsGlobal) {
    void *handle = dlopen(appPath, RTLD_LOCAL | RTLD_LAZY);
    if (!handle) {
      fprintf(stderr, "failed to open %s\n", appPath);
      return 1;
    }
    uint16_t *mpljsMdD = (uint16_t *)dlsym(handle, "__mpljs_module_decl__");
    if (!mpljsMdD) {
      fprintf(stderr, "failed to open __mpljs_module_decl__ %s\n", appPath);
      return 1;
    }
    uint16_t globalMemSize = mpljsMdD[0];
    gInterSource = new InterSource();
    gInterSource->gp = (uint8_t *)malloc(globalMemSize);
    memcpy(gInterSource->gp, mpljsMdD + 1, globalMemSize);
    gInterSource->topGp = gInterSource->gp + globalMemSize;
    gInterSource->CreateJsPlugin(appPath);
    jsGlobal = new JavaScriptGlobal();
    jsGlobal->flavor = 0;
    jsGlobal->srcLang = 2;
    jsGlobal->id = 0;
    jsGlobal->globalmemsize = globalMemSize;
    jsGlobal->globalwordstypetagged = *((uint8_t *)mpljsMdD + 2 + globalMemSize + 2);
    jsGlobal->globalwordsrefcounted = *((uint8_t *)mpljsMdD + 2 + globalMemSize + 6);
  }
  TValue val;
  uint8_t* addr = (uint8_t*)firstArg + 4; // skip signature
  DynamicMethodHeaderT *header = (DynamicMethodHeaderT *)(addr);
  assert(header->upFormalSize == 0 && "main function got a frame size?");
  addr += *(int32_t*)addr; // skip to 1st instruction
  val = maple_invoke_dynamic_method_main (addr, header);
#ifdef MM_DEBUG
  memory_manager->DumpMMStats();
#endif
  return GET_PAYLOAD(val);
}

} // namespace maple
