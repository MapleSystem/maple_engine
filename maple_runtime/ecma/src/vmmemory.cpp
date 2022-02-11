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

#include <string.h>
#include "vmmemory.h"
#include "jsobject.h"
#include "jsobjectinline.h"
#include "jsvalueinline.h"
#include "jsfunction.h"
#include "cmpl.h"
#include "jsarray.h"
#include "jsiter.h"
#include "securec.h"
#include "jsdataview.h"
#include <cmath>
// This module performs memory management for both the app's heap space and the
// VM's own dynamic memory space.  For memory blocks allocated in the app's
// heap space, we rely on reference counting in order to know when a memory
// block can be recycled for use (garbage collected).  For memory blocks
// allocated in VM's own dynamic memory space, the VM is in charge of their
// recycling as it knows when something is no longer needed.
//
// Because of the need to support reference counting in the app's heap space,
// all memory blocks allocated there are enlarged by a 32 bit header
// (MemHeader) that precedes the memory space returned to the user program.
//
// In both memory spaces, MemoryChunk is used to keep track of an allocated
// block.  When a block is being used, there does not need to be a MemoryChunk
// to record it.  Its MemoryChunk is created only when the block is to be
// recycled.  To recycle a block, its MemoryChunk is kept inside MemoryHash,
// which groups them based on size. Thus, there are 2 MemoryHash instances, one
// for the app's heap space (heap_memory_bank_) and one for the VM's own
// dynamic memory space (vm_memory_bank_).
//
// MemoryChunk nodes are VM's own dynamic data structures, so their allocation
// are in the VM's own dynamic memory space, and their re-uses are managed by
// the VM.  The list of MemoryChunk nodes available for re-use is maintained
// in free_memory_chunk_.
using namespace maple;

MemoryManager *memory_manager = NULL;
#ifdef MARK_CYCLE_ROOTS
CycleRoot *cycle_roots = NULL;
CycleRoot *garbage_roots = NULL;
#else
__jsobject *obj_list = NULL;
#endif
bool is_sweep = false;

void *VMMallocGC(uint32 size, MemHeadTag tag, bool init_p) {
  uint32 alignedsize = memory_manager->Bytes4Align(size);
  uint32 head_size = MALLOCHEADSIZE;
#ifndef MARK_CYCLE_ROOTS
  if (tag == MemHeadJSObj) {
    head_size = MALLOCEXHEADSIZE;
  }
#endif
  void *memory = memory_manager->Malloc(alignedsize + head_size, init_p);
  if (memory == NULL) {
    MIR_FATAL("run out of memory");
  }

  MemHeader *memheaderp = (MemHeader *)memory;
#ifndef MARK_CYCLE_ROOTS
  if (tag == MemHeadJSObj) {
    memheaderp = (MemHeader *)((uint32 *)memory + 2);
  }
#endif
  memheaderp->memheadtag = tag;
  memheaderp->refcount = 0;
  memheaderp->in_roots = false;
  if (memory_manager->IsDebugGC()) {
    printf("memory %p was allocated with header %d size %d\n", ((void *)((uint8 *)memory + head_size)), tag, alignedsize);
  }
#ifdef MM_DEBUG
  memory_manager->mem_alloc_bytes_by_tag[(int)tag] += alignedsize + head_size;
  memory_manager->mem_alloc_count_by_tag[(int)tag]++;
  memory_manager->live_objects[(void*)((uint8_t*)memory + head_size)] = 0;
  // printf("in VMMallocGC, addr= %p size= %u tag= %d\n", (void*)((uint8_t*)memory + head_size), alignedsize + head_size, tag);
#endif
  return (void *)((uint8 *)memory + head_size);
}

void *VMReallocGC(void *origptr, uint32 origsize, uint32 newsize) {
  uint32 alignedorigsize = memory_manager->Bytes4Align(origsize);
  uint32 alignednewsize = memory_manager->Bytes4Align(newsize);
  void *memory = memory_manager->Realloc(origptr, alignedorigsize, alignednewsize);
#ifdef MM_DEBUG
  int tag = memory_manager->GetMemHeader((uint8*)memory+MALLOCHEADSIZE).memheadtag;
  memory_manager->mem_alloc_bytes_by_tag[(int)tag] += alignednewsize + MALLOCHEADSIZE;
  memory_manager->mem_alloc_count_by_tag[tag]++;
  memory_manager->live_objects[(void*)((uint8_t*)memory + MALLOCHEADSIZE)] = 0;
  //printf("in VMReallocGC, addr= %p size= %u tag= %d\n", (void*)((uint8_t*)memory + MALLOCHEADSIZE), (uint32)(alignednewsize + MALLOCHEADSIZE), tag);
#endif
  return (void *)((uint8 *)memory + MALLOCHEADSIZE);
}

#if MIR_FEATURE_FULL && MIR_DEBUG
void MemoryHash::Debug() {
  for (uint32 i = 0; i < MEMHASHTABLESIZE; i++) {
    MemoryChunk *mchunk = table_[i];
    for (MemoryChunk *node = mchunk; node; node = node->next) {
      printf("memory offset:%u, memory size:%u\n", node->offset_, node->size_);
    }
  }
}

#endif

#if DEBUGGC
bool MemoryHash::CheckOffset(uint32 offset) {
  for (uint32 i = 0; i < MEMHASHTABLESIZE; i++) {
    for (MemoryChunk *node = table_[i]; node; node = node->next) {
      if ((offset >= node->offset_) && (offset < (node->size_ + node->offset_))) {
        return false;
      }
    }
  }
  return true;
}

#endif

void MemoryHash::PutFreeChunk(MemoryChunk *mchunk) {
  uint32 index = mchunk->size_ >> 2;
  bool issmall = (index >= 1 && index < MEMHASHTABLESIZE);
  uint32 x = issmall ? (--index) : (MEMHASHTABLESIZE - 1);
  // link the mchunk to the table
  if (issmall) {
#ifdef MM_DEBUG
    MemoryChunk* c = table_[x];
    while(c) {
      assert(c->offset_ != mchunk->offset_ && "double free");
      c = c->next;
    }
#endif
    mchunk->next = table_[x];
    table_[x] = mchunk;
  } else {  // find the poisition and put free chunk to it
    MemoryChunk *node = table_[x];
    MemoryChunk *prenode = NULL;
    while (node && (node->offset_ < mchunk->offset_)) {
      prenode = node;
      node = node->next;
    }
    if (prenode) {
      mchunk->next = node;
      prenode->next = mchunk;
    } else {
      if (node) {
        mchunk->next = node;
        table_[x] = mchunk;
      } else {
        mchunk->next = NULL;
        table_[x] = mchunk;
      }
    }
  }
}

void MemoryHash::MergeBigFreeChunk() {
  // size must be >= MEMHASHTABLESIZE << 2
  MemoryChunk *node = table_[MEMHASHTABLESIZE - 1];
  if (!node) {
    return;
  }
  MemoryChunk *nextnode = node->next;
  while (nextnode) {  // merge the node
    // MemoryChunk *nextnode = node->next;
    if (node->offset_ + node->size_ == nextnode->offset_) {
      // merge node and nextnode
      node->size_ += nextnode->size_;
      node->next = nextnode->next;
      memory_manager->DeleteMemoryChunk(nextnode);
    } else if (node->offset_ + node->size_ < nextnode->offset_) {
      node = nextnode;
    } else {
      MIR_FATAL("something wrong with big free chunk node");
    }
    nextnode = node->next;
  }
}

MemoryChunk *MemoryHash::GetFreeChunk(uint32 size) {
  // the size is supposed to be 4 bytes aligned
  uint32 index = size >> 2;
  if (index >= 1 && index < MEMHASHTABLESIZE) {
    index--;
    MemoryChunk *node = table_[index];
    if (node) {
      table_[index] = node->next;
    }
    return node;
  } else {
    MemoryChunk *node = table_[MEMHASHTABLESIZE - 1];
    MemoryChunk *prenode = NULL;
    while (node && node->size_ < size) {
      prenode = node;
      node = node->next;
    }
    if (!node) {
      return NULL;
    }
    if (node->size_ == size) {
      if (prenode) {
        prenode->next = node->next;
      } else {
        table_[MEMHASHTABLESIZE - 1] = node->next;
      }
    } else {
      // node->size > size;
      MemoryChunk *newnode = memory_manager->NewMemoryChunk(node->offset_ + size, node->size_ - size, node->next);
      if (prenode) {
        prenode->next = newnode;
      } else {
        table_[MEMHASHTABLESIZE - 1] = newnode;
      }
      node->size_ = size;
    }
    return node;
  }
}

#if MACHINE64
void MemoryManager::SetF64Builtin() {
  // from 0 to 6, there are Math.E, LN10, LN2, LOG10E, LOG2E, PI, SQRT1_2, SQRT2
  f64ToU32Vec.push_back(0);
  f64ToU32Vec.push_back(MathE);
  f64ToU32Vec.push_back(MathLn10);
  f64ToU32Vec.push_back(MathLn2);
  f64ToU32Vec.push_back(MathLog10e);
  f64ToU32Vec.push_back(MathLog2e);
  f64ToU32Vec.push_back(MathPi);
  f64ToU32Vec.push_back(MathSqrt1_2);
  f64ToU32Vec.push_back(MathSqrt2);
  f64ToU32Vec.push_back(NumberMaxValue);
  f64ToU32Vec.push_back(NumberMinValue);
}

#if 0
TValue MemoryManager::GetF64Builtin(uint32_t index) {
  assert(index <= MATH_LAST_INDEX && "error");
  if (f64ToU32Vec.size() == 0) {
    SetF64Builtin();
  }
  TValue jsval;
  jsval.ptyp = JSTYPE_DOUBLE;
  jsval.x.u32 = index;
  return jsval;
}
#endif

double MemoryManager::GetF64FromU32 (uint32 index) {
  assert(index < f64ToU32Vec.size() && "false");
  return f64ToU32Vec[index];
}

#if 0
uint32_t MemoryManager::SetF64ToU32 (double f64) {
  // std::map<double, uint32>::iterator it = f64ToU32Map.find(f64);
  uint32 size = f64ToU32Vec.size();
  if (size == 0) {
    SetF64Builtin();
    size = f64ToU32Vec.size();
    assert(size == MATH_LAST_INDEX + 1 && "error");
    if (f64 == 0) {
      return DOUBLE_ZERO_INDEX;
    }
    // f64ToU32Vec.push_back(f64);
    // return f64ToU32Vec.size() - 1;
  }
  assert(size > MATH_LAST_INDEX && "error");
  for (uint32 i = 0; i < size; i++) {
    if (fabs(f64ToU32Vec[i] - f64) < NumberMinValue) {
      return i;
    }
  }
  f64ToU32Vec.push_back(f64);
  return size;
  /*
  if (it == f64ToU32Map.end()) {
    uint32 index = f64ToU32Vec.size();
    f64ToU32Map.insert(std::pair<double, uint32>(f64, index));
    f64ToU32Vec.push_back(f64);
    return index;
  } else {
    return it->second;
  }
  */
}
#endif
#endif

// malloc memory in VM internal memory region, re-cycled via MemoryChunk nodes
void *MemoryManager::MallocInternal(uint32 malloc_size) {
  uint32 alignedsize = Bytes4Align(malloc_size);
  MemoryChunk *mchunk = vm_memory_bank_->GetFreeChunk(alignedsize);
  void *return_ptr = NULL;
  if (!mchunk) {
    if (vm_memory_bank_->IsBigSize(alignedsize)) {
      if (alignedsize + vm_free_big_offset_ > vm_memory_size_) {
        // merge the big memory chunk
        vm_memory_bank_->MergeBigFreeChunk();
        mchunk = vm_memory_bank_->GetFreeChunk(malloc_size);
        if (!mchunk) {
          MIR_FATAL("run out of VM heap memory.\n");
        }
        return_ptr = (void *)((uint8 *)vm_memory_ + mchunk->offset_);
        DeleteMemoryChunk(mchunk);
      } else {
        uint32 free_offset = vm_free_big_offset_;
        vm_free_big_offset_ += alignedsize;
        return (void *)((uint8 *)vm_memory_ + free_offset);
      }
    } else {
      if (alignedsize + vm_free_small_offset_ > vm_memory_small_size_) {
        MIR_FATAL("TODO run out of VM internal small memory.\n");
      }
      return_ptr = (void *)((char *)vm_memory_ + vm_free_small_offset_);
      vm_free_small_offset_ += alignedsize;
    }
  } else {
    return_ptr = (void *)((uint8 *)vm_memory_ + mchunk->offset_);
    DeleteMemoryChunk(mchunk);
  }
  return return_ptr;
}

void MemoryManager::FreeInternal(void *addr, uint32 size) {
  uint32 alignedsize = Bytes4Align(size);
  uint32 offset = (uint32)((uint8 *)addr - (uint8 *)vm_memory_);
  MemoryChunk *mchunk = NewMemoryChunk(offset, alignedsize, NULL);
  vm_memory_bank_->PutFreeChunk(mchunk);
}

void MemoryManager::ReleaseVariables(uint8_t *base, uint8_t *typetagged, uint8_t *refcounted, uint32_t size,
                                     bool reversed) {
  for (uint32_t i = 0; i < size; i++) {
    for (uint32_t j = 0; j < 8; j++) {
      uint32_t offset = (i * 8 + j) * 4;
      uint8_t *addr = reversed ? (base - offset - 4) : (base + offset);
      if (typetagged[i] & (1 << j)) {
        MIR_ASSERT(!(refcounted[i] & (1 << j)));
        // uint64_t val = load_64bits(addr);
        uint64_t val = *((uint64 *)addr);
        assert(false&&"NYI");
        // GCCheckAndDecRf(val, IsNeedRc(GetFlagFromMemory(addr)));
      } else if (refcounted[i] & (1 << j)) {
        uint64_t val = *(uint64_t *)(addr);
        GCDecRf((void *)val);
      } else {
        continue;
      }
    }
  }
}

#ifdef MM_DEBUG
void MemoryManager::DumpMMStats() {
#ifdef MEMORY_LEAK_CHECK
  AppMemLeakCheck();
#endif
#ifdef MM_RC_STATS
  DumpRCStats();
#endif
}

// A VM self-check for memory-leak.
// When exit the app, stack-variables have been released. Only global-variables
// and builtin-variables may refer to a heap-object or a heap-string.
// If we release global-variables and builtin-variables here, there must be no heap
// memory used.
#ifdef MEMORY_LEAK_CHECK
static void BatchRCDec(void* lowAddr, void* hiAddr) {
  uint8 *addr = (uint8*)lowAddr;
  while(addr < hiAddr) {
    TValue* local = (TValue*)addr;
    if (IS_NEEDRC(local->x.u64)) {
      GCDecRf((void*)local->x.c.payload);
    }
    addr += sizeof(void*);
  }
}
#endif

void MemoryManager::DumpAllocReleaseStats() {
  printf("[MM] Num_alloc= %u num_release= %u\n", mem_alloc_count, mem_release_count);
  printf("[MM] Num Malloc calls= %u including Realloc calls= %u\n", num_Malloc_calls, num_Realloc_calls);
  printf("[MM] Mem allocation by tag:\n");
  uint32 ta_size = 0;
  uint32 ta_count = 0;
  for(int i=0; i<MemHeadLast; i++) {
    printf("[MM] tag= %d alloc_size= %10u alloc_count= %u\n", i, mem_alloc_bytes_by_tag[i], mem_alloc_count_by_tag[i]);
    ta_size += mem_alloc_bytes_by_tag[i];
    ta_count += mem_alloc_count_by_tag[i];
  }
  printf("[MM] total: alloc_size= %10u alloc_count= %u\n", ta_size, ta_count);

  uint32 tr_size = 0;
  uint32 tr_count = 0;
  for(int i=0; i<MemHeadLast; i++) {
    printf("[MM] tag= %d release_size= %10u release_count= %u\n", i, mem_release_bytes_by_tag[i], mem_release_count_by_tag[i]);
    tr_size += mem_release_bytes_by_tag[i];
    tr_count += mem_release_count_by_tag[i];
  }
  printf("[MM} total: release_size= %10u release_count= %u\n", tr_size, tr_count);
}

void MemoryManager::AppMemLeakCheck() {
#ifdef MEMORY_LEAK_CHECK

#if 0
  GCCheckAndDecRf(ginterpreter->retval0_.u64);
  ReleaseVariables(ginterpreter->gp_, ginterpreter->gp_typetagged_, ginterpreter->gp_refcounted_,
                   BlkSize2BitvectorSize(ginterpreter->gp_size_), false /* offset is positive */);
#endif

  printf("\nChecking mem leak...\n");
  printf("Size of cycle_candidate_roots= %lu\n", crc_candidate_roots.size());

  printf("Releasing builtin, app_mem_usage= %u alloc= %u release= %u max= %u\n", app_mem_usage, mem_allocated, mem_released, max_app_mem_usage);
  uint32 released_b4 = mem_released;

  __jsobj_release_builtin();

  printf("After releasing builtin, app_mem_usage= %u alloc= %u release= %u max= %u\n", app_mem_usage, mem_allocated, mem_released, max_app_mem_usage);
  printf("  Released builtin: %u B\n", mem_released - released_b4);

  printf("Releasing local variables in main()\n");
  released_b4 = mem_released;
  size_t live_objs_b4 =  live_objects.size();

  BatchRCDec(mainSP, mainFP);

  printf("After releasing main(), app_mem_usage= %u alloc= %u release= %u max= %u\n", app_mem_usage, mem_allocated, mem_released, max_app_mem_usage);
  printf("  Released main() variables: %u B released objects= %lu\n", mem_released - released_b4, live_objs_b4 - live_objects.size());

  printf("Releasing global variables\n");
  BatchRCDec(mainGP, mainTopGP);

  DumpAllocReleaseStats();

  printf("After builtin, main, global, size of cycle_candidate_roots= %lu\n", crc_candidate_roots.size());

  //------------------------------------------------
  //live objects
  //------------------------------------------------
  printf("\nLive objects: %lu\n", live_objects.size());
#if 0
  for(auto iter : live_objects) {
    printf("%p: maxRC= %u tag= %d rc= %d\n", iter.first, iter.second, GetMemHeader(iter.first).memheadtag, GCGetRf(iter.first));
  }
#endif
  int live_by_tag[MemHeadLast];
  for(int i=0; i<MemHeadLast; i++)
    live_by_tag[i] = 0;

  uint32_t t_max_rc0_live = 0;
  std::map<int, uint32_t> max_rc_histogram_live;
  for(auto iter : live_objects) {
    live_by_tag[GetMemHeader(iter.first).memheadtag]++;
    if(iter.second == 0)
      t_max_rc0_live++;
    max_rc_histogram_live[iter.second]++;
  }

  printf("Live object count by type:\n");
  for(int i=0; i<MemHeadLast; i++)
    printf("tag= %d count= %d\n", i, live_by_tag[i]);

  printf("Max RC histogram for live objects:\n");
  for(auto m : max_rc_histogram_live) {
    printf("%5d %6u\n", m.first, m.second);
  }

  //printf("Num of objects with MaxRC=0: released= %u live= %u total= %u\n", t_max_rc0_released, t_max_rc0_live, t_max_rc0_released + t_max_rc0_live);
  //printf("Num of alloc count excluding those with MaxRC=0: %u\n", ta_count - t_max_rc0_released - t_max_rc0_live);

#ifdef MARK_CYCLE_ROOTS
  printf("\nCollecting reference cycles...\n");
  RecallCycle();
  RecallRoots(cycle_roots);

  printf("\nAfter CRC, #live objects: %lu\n", live_objects.size());
#if 0
  for(auto iter : live_objects) {
    printf("%p: maxRC= %u tag= %d rc= %d\n", iter.first, iter.second, GetMemHeader(iter.first).memheadtag, GCGetRf(iter.first));
  }
#endif
#else
  MarkAndSweep();
#endif
  printf("After reclaim, app_mem_usage= %u alloc= %u release= %u max= %u\n", app_mem_usage, mem_allocated, mem_released, max_app_mem_usage);

  if (app_mem_usage != 0) {
    printf("[Memory Manager] %d Bytes heap memory leaked!\n", app_mem_usage);
    // MIR_FATAL("memory leak.\n");
  }
#endif
}

void MemoryManager::AppMemUsageSummary() {
  printf("\n");
  printf("[Memory Manager] max app heap memory usage is %d Bytes\n", app_mem_usage);
  printf("[Memory Manager] vm internal memory usage is %d Bytes\n\n",
         vm_free_small_offset_ + (vm_free_big_offset_ - vm_memory_small_size_));
}

void MemoryManager::AppMemAccessSummary() {
#ifndef RC_NO_MMAP
  printf("[Memory Manager] the max mmap node visiting length for VM APP access is %d \n", AddrMap::max_count);
  printf("[Memory Manager] the number of traversed mmap node for VM APP access is %d \n", AddrMap::total_count);
  printf("[Memory Manager] the visiting times of mmap for VM APP access is %d \n", AddrMap::find_count);
  printf("\n");
  printf("[Memory Manager] the number of used node in mmap is %d\n\n", AddrMap::nodes_count);
#endif
}

void MemoryManager::AppMemShapeSummary() {
#ifndef RC_NO_MMAP
  uint32_t i = 0, j = 0;
  AddrMap *mmap_table[3] = { NULL };
  mmap_table[0] = ginterpreter->GetHeapRefList();
  mmap_table[1] = ginterpreter->GetGlobalRefList();
  // when interpretation finishes, vmfunc_ is no longer available
  // mmap_table[2] = ginterpreter->vmfunc_->ref_list_;
  uint32_t max_length[3] = { 0, 0, 0 };
  uint32_t node_count[3];
  uint32_t efficiency[3];
  // for(i = 0; i < 3; i++) {
  for (i = 0; i < 2; i++) {
    AddrMap *map = mmap_table[i];
    if (map) {
      uint32_t free_num = 0;
      uint32_t node_num = 0;
      for (j = 0; j < map->hashtab_size; j++) {
        AddrMapNode *node = &(map->hashtab[j]);
        uint32_t list_len = 0;
        while (node) {
          node_num++;
          list_len++;
          if (MMAP_NODE_IS_FREE(node)) {
            free_num++;
          }
          node = node->next;
        }
        if (max_length[i] < list_len) {
          max_length[i] = list_len;
        }
      }
      node_count[i] = node_num;
      if (node_num != 0) {
        efficiency[i] = (uint32_t)(((double)(node_num - free_num) / (double)node_num) * 100);
      } else {
        efficiency[i] = 100;
      }
    }
  }
  if (mmap_table[0]) {
    printf("[Memory Manager] the number of node in heap_ref_list is %d\n", node_count[0]);
    printf("[Memory Manager] the use efficiency of heap_ref_list is %d percent\n", efficiency[0]);
    printf("[Memory Manager] the max length of the list in heap_ref_list hash slot is %d\n", max_length[0]);
    printf("\n");
  }
  if (mmap_table[1]) {
    printf("[Memory Manager] the number of node in global_ref_list is %d\n", node_count[1]);
    printf("[Memory Manager] the use efficiency of global_ref_list is %d percent\n", efficiency[1]);
    printf("[Memory Manager] the max length of the list in global_ref_list hash slot is %d\n", max_length[1]);
    printf("\n");
  }
  if (mmap_table[2]) {
    printf("[Memory Manager] the number of node in func_ref_list is %d\n", node_count[2]);
    printf("[Memory Manager] the use efficiency of func_ref_list is %d percent\n", efficiency[2]);
    printf("[Memory Manager] the max length of the list in func_ref_list hash slot is %d\n", max_length[2]);
    printf("\n");
  }
#endif
}

void MemoryManager::DumpRCStats() {
#ifdef MM_RC_STATS
  // dump MaxRC histogram
  printf("[RC] Max RC histogram for released objects:\n");
  for(auto m : max_rc_histogram) {
    printf("[RC] %5d: %6u\n", m.first, m.second);
  }

  uint32_t num_max_rc0_released = 0;
  printf("[RC] Num of released objects with max RC=0 by type:\n");
  for(int i=0; i<MemHeadLast; i++) {
    printf("[RC] tag= %d count= %d\n", i, max_rc0_by_tag[i]);
    num_max_rc0_released += max_rc0_by_tag[i];
  }

  printf("[RC] Num of released objects with MaxRC=0: %u total_release_count= %u diff= %u\n", num_max_rc0_released, mem_release_count, mem_release_count - num_max_rc0_released);
  printf("[RC] RCInc= %lu RCDec= %lu total= %lu op/obj= %.2f\n", num_rcinc, num_rcdec, num_rcinc + num_rcdec, (double)(num_rcinc + num_rcdec)/(mem_release_count - num_max_rc0_released));
#endif
}

#endif  // MM_DEBUG


MemoryChunk *MemoryManager::NewMemoryChunk(uint32 offset, uint32 size, MemoryChunk *next) {
  MemoryChunk *new_chunk;
  if (free_memory_chunk_) {
    new_chunk = free_memory_chunk_;
    free_memory_chunk_ = new_chunk->next;
  } else {
    // new_chunk = (MemoryChunk *) MallocInternal(sizeof(MemoryChunk));
    new_chunk = (MemoryChunk *)((uint8 *)vm_memory_ + vm_free_small_offset_);
    vm_free_small_offset_ += Bytes4Align(sizeof(MemoryChunk));
    if (vm_free_small_offset_ > vm_memory_small_size_) {
      MIR_FATAL("TODO run out of VM internal memory.\n");
    }
  }
  new_chunk->offset_ = offset;
  new_chunk->size_ = size;
  new_chunk->next = next;
  return new_chunk;
}

void MemoryManager::Init(void *app_memory, uint32 app_memory_size, void *vm_memory, uint32 vm_memory_size) {
  memory_ = app_memory;
  total_small_size_ = app_memory_size / 2;
#if DEBUGGC
  assert((total_small_size_ % 4) == 0 && (VM_MEMORY_SIZE % 4) == 0 && "make sure HEAP SIZE is aligned to 4B");
#endif
  total_size_ = app_memory_size;
  heap_end = (void *)((uint8 *)memory_ + app_memory_size);
  vm_memory_ = vm_memory;
  vm_memory_size_ = vm_memory_size;
  vm_memory_small_size_ = VM_MEMORY_SMALL_SIZE;
  vm_free_small_offset_ = 0;
  vm_free_big_offset_ = vm_memory_small_size_;
  heap_free_small_offset_ = 0;

  heap_free_big_offset_ = total_small_size_;
  free_memory_chunk_ = NULL;

  crc_alloc_size = 0;
#ifndef RC_NO_MMAP
  free_mmaps_ = NULL;
  free_mmap_nodes_ = NULL;
#endif
  // avail_link_ = NewMemoryChunk(0, app_memory_size, NULL);
  heap_memory_bank_ = (MemoryHash *)((uint8 *)vm_memory_ + vm_free_small_offset_);
  uint32 alignedsize = Bytes4Align(sizeof(MemoryHash));
  vm_free_small_offset_ += alignedsize;
  errno_t mem_ret1 = memset_s(heap_memory_bank_, alignedsize, 0, alignedsize);
  if (mem_ret1 != EOK) {
    MIR_FATAL("call memset_s firstly failed in MemoryManager::Init");
  }
  vm_memory_bank_ = (MemoryHash *)((uint8 *)vm_memory_ + vm_free_small_offset_);
  vm_free_small_offset_ += alignedsize;
  errno_t mem_ret2 = memset_s(vm_memory_bank_, alignedsize, 0, alignedsize);
  if (mem_ret2 != EOK) {
    MIR_FATAL("call memset_s secondly failed in MemoryManager::Init");
  }

#ifdef MM_DEBUG
  app_mem_usage = 0;
  max_app_mem_usage = 0;
  mem_allocated = 0;
  mem_released = 0;
  mem_alloc_count = 0;
  mem_release_count = 0;

  for(int i=0; i<MemHeadLast; i++) {
    mem_alloc_bytes_by_tag[i] = 0;
    mem_alloc_count_by_tag[i] = 0;
    mem_release_bytes_by_tag[i] = 0;
    mem_release_count_by_tag[i] = 0;

    max_rc0_by_tag[i] = 0;
  }
  num_Malloc_calls = 0;
  num_Realloc_calls = 0;
  num_rcinc = 0;
  num_rcdec = 0;
#endif  // MM_DEBUG
#ifdef MACHINE64
  addrMap.push_back(NULL);
  addrOffset = addrMap.size();
#endif
}

void *MemoryManager::Malloc(uint32 size, bool init_p) {
  MemoryChunk *mchunk;
#ifdef MM_DEBUG
  app_mem_usage += size;
  mem_allocated += size;
  if(app_mem_usage > max_app_mem_usage)
    max_app_mem_usage = app_mem_usage;
  num_Malloc_calls++;
  mem_alloc_count++;
#endif  // MM_DEBUG
#if DEBUGGC
  assert((IsAlignedBy4(size)) && "memory doesn't align by 4 bytes");
#endif
  crc_alloc_size += size;
  if (crc_alloc_size > CRC_TRIGGER_BY_ALLOC_SIZE) {
    crc_alloc_size = 0;
    RecallCycle();
  }

  mchunk = heap_memory_bank_->GetFreeChunk(size);
  void *retmem = NULL;
  if (!mchunk) {
    uint32 heap_free_offset = 0;
    if (heap_memory_bank_->IsBigSize(size)) {  // for big size we merge the free chunk to see if we can get a big memory
      if (size + heap_free_big_offset_ > total_size_) {
        // try to merge some free chunk;
        heap_memory_bank_->MergeBigFreeChunk();
        mchunk = heap_memory_bank_->GetFreeChunk(size);
        if (!mchunk) {
          MIR_FATAL("run out of VM heap memory.\n");
        }
      } else {
        heap_free_offset = heap_free_big_offset_;
        heap_free_big_offset_ += size;
        return (void *)((uint8 *)memory_ + heap_free_offset);
      }
    } else {
      if (size + heap_free_small_offset_ > total_small_size_) {
        // try to use big size heap space if it has not been used
        if (heap_free_big_offset_ == total_small_size_ &&
            heap_free_big_offset_ < total_size_ - (1024 * 1024)) {
            total_small_size_ += 1024 * 1024;
            heap_free_big_offset_ = total_small_size_;
        } else {
          MIR_FATAL("TODO run out of VM heap memory.\n");
        }
      }
      heap_free_offset = heap_free_small_offset_;
      heap_free_small_offset_ += size;
      return (void *)((uint8 *)memory_ + heap_free_offset);
    }
  }
  retmem = (void *)((uint8 *)memory_ + mchunk->offset_);
  if (init_p) {
    errno_t ret = memset_s(retmem, size, 0, size);
    if (ret != EOK) {
      MIR_FATAL("call memset_s failed in MemoryManager::Malloc");
    }
  }
  DeleteMemoryChunk(mchunk);

#if DEBUGGC
  assert(IsAlignedBy4((uint32)retmem));
#endif
  return retmem;
}

void *MemoryManager::Realloc(void *origptr, uint32 origsize, uint32 newsize) {
#if DEBUGGC
  assert((IsAlignedBy4(origsize) && IsAlignedBy4(newsize)) && "memory doesn't align by 4 bytes");
#endif

#ifdef MM_DEBUG
  num_Realloc_calls++;
#endif
  void *newptr = Malloc(newsize + MALLOCHEADSIZE);
  if (!newptr) {
    MIR_FATAL("out of memory");
  }
  if (newsize > origsize) {
    errno_t cpy_ret = memcpy_s(newptr, newsize + MALLOCHEADSIZE, (void *)((uint8 *)origptr - MALLOCHEADSIZE),
                               origsize + MALLOCHEADSIZE);
    if (cpy_ret != EOK) {
      MIR_FATAL("call memcpy_s failed in MemoryManager::Realloc");
    }
    errno_t set_ret =
      memset_s((void *)((uint8 *)newptr + origsize + MALLOCHEADSIZE), newsize + MALLOCHEADSIZE, 0, newsize - origsize);
    if (set_ret != EOK) {
      MIR_FATAL("call memcpy_s failed in MemoryManager::Realloc");
    }
  } else {
    errno_t cpy_ret =
      memcpy_s(newptr, newsize + MALLOCHEADSIZE, (void *)((uint8 *)origptr - MALLOCHEADSIZE), newsize + MALLOCHEADSIZE);
    if (cpy_ret != EOK) {
      MIR_FATAL("call memcpy_s failed in MemoryManager::Realloc");
    }
  }
  RecallMem(origptr, origsize);
  return newptr;
}

// actuall we need to recall mem - MALLOCHEADSIZE with size+MALLOCHEADSIZE
void MemoryManager::RecallMem(void *mem, uint32 size) {
  uint32 head_size = MALLOCHEADSIZE;

#ifndef MARK_CYCLE_ROOTS
  if (GetMemHeader(mem).memheadtag == MemHeadJSObj) {
    head_size = MALLOCEXHEADSIZE;
  }
#endif

  if (IsDebugGC()) {
    switch (GetMemHeader(mem).memheadtag) {
      case MemHeadAny:
        printf("unknown mem %p size %d is going to be released\n", mem, size);
        break;
      case MemHeadJSFunc:
        printf("jsfunc mem %p size %d is going to be released\n", mem, size);
        break;
      case MemHeadJSObj:
        printf("jsobj mem %p size %d is going to be released\n", mem, size);
        break;
      case MemHeadJSProp:
        printf("jsprop mem %p size %d is going to be released\n", mem, size);
        break;
      case MemHeadJSString:
        printf("jsstring mem %p size %d is going to be released\n", mem, size);
        break;
      case MemHeadJSList:
        printf("jsstring mem %p size %d is going to be released\n", mem, size);
        break;
      case MemHeadJSIter:
        printf("jsiter mem %p size %d is going to be released\n", mem, size);
        break;
      default:
        MIR_FATAL("unknown VM memory header value");
    }
  }
  uint32 alignedsize = Bytes4Align(size);
  uint32 offset = (uint32)((uint8 *)mem - (uint8 *)memory_ - head_size);
#if DEBUGGC
  assert(IsAlignedBy4(offset) && "not aligned by 4 bytes");
#endif
  //MIR_ASSERT(offset < total_size_);

#ifdef MM_DEBUG
  app_mem_usage -= (alignedsize + head_size);
  mem_released += (alignedsize + head_size);
  int tag = GetMemHeader(mem).memheadtag;
  if(tag < (int)MemHeadLast) {
    mem_release_bytes_by_tag[tag] += alignedsize + head_size;
    mem_release_count_by_tag[tag]++;
  }
  mem_release_count++;
  // printf("RecallMem mem= %p tag= %d\n", mem, tag);
  max_rc_histogram[live_objects[mem]]++;
#ifdef MM_RC_STATS
  if(live_objects[mem] == 0)
    max_rc0_by_tag[tag]++;
#endif
  live_objects.erase(mem);
#endif  // MM_DEBUG

  if (GetMemHeader(mem).memheadtag == MemHeadJSObj) {
    if (GetMemHeader(mem).in_roots == true) {
      GetMemHeader(mem).in_roots = false;
      crc_candidate_roots.erase(mem);
    }
  }

  MemoryChunk *mchunk = NewMemoryChunk(offset, alignedsize + head_size, NULL);
  // InsertMemoryChunk(mchunk);
  heap_memory_bank_->PutFreeChunk(mchunk);

}

void MemoryManager::RecallString(__jsstring *str) {
  if (TurnoffGC())
    return;
#if MIR_DEX
#else  // !MIR_DEX
  // determine whether the string content is allocated in local heap.
  // Do not need recall the JSSTRING_BUILTIN, which is in constant pool;
  if (IsHeap((void *)str)) {
    if (IsDebugGC()) {
      printf("recollect a string in heap\n");
    }
    MemHeader &header = memory_manager->GetMemHeader((void *)str);
    if (header.refcount == 0) {
      RecallMem((void *)str, __jsstr_get_bytesize(str));
    }
  }
  else {
    if (IsDebugGC()) {
      printf("recollect a global string\n");
    }
  }
#endif  // MIR_DEX
        // RecallMem((void *)str, sizeof(__jsstring));
}

void MemoryManager::RecallArray_props(TValue *array_props) {
  if (TurnoffGC())
    return;
  uint32_t length = __jsval_to_uint32(array_props[0]);
  length = length > ARRAY_MAXINDEXNUM_INTERNAL ? ARRAY_MAXINDEXNUM_INTERNAL : length;
  for (uint32_t i = 0; i < length + 1; i++) {
    if (__is_js_object(array_props[i]) || __is_string(array_props[i])) {
// #ifndef RC_NO_MMAP
#if  0
      AddrMap *mmap = ginterpreter->GetHeapRefList();
      AddrMapNode *mmap_node = mmap->FindInAddrMap(&array_props[i].x.payload.ptr);
      MIR_ASSERT(mmap_node);
      mmap->RemoveAddrMapNode(mmap_node);
#endif
#if MACHINE64
      GCDecRf((void *)array_props[i].x.c.payload);
#else
      GCDecRf(array_props[i].x.payload.ptr);
#endif
    }
  }
  RecallMem((void *)array_props, (length + 1) * sizeof(TValue));
}

void MemoryManager::RecallList(__json_list *list) {
  if (TurnoffGC())
    return;
  uint32_t count = list->count;
  __json_node *node = list->first;
  for (uint32_t i = 0; i < count; i++) {
    GCCheckAndDecRf(GET_PAYLOAD(node->value), IS_NEEDRC(node->value.x.u64));
    VMFreeNOGC(node, sizeof(__json_node));
    node = node->next;
  }
  RecallMem((void *)list, sizeof(__json_list));
}

// decrease the memory, recall it if necessary
void MemoryManager::GCDecRf(void *addr) {
  if (TurnoffGC())
    return;
  if (!IsHeap(addr)) {
    return;
  }
#ifdef MM_RC_STATS
  num_rcdec++;
#endif
  MemHeader &header = GetMemHeader(addr);
//  MIR_ASSERT(header.refcount > 0);  // must > 0
  if(header.refcount < UINT14_MAX)
    header.refcount--;
  // DEBUG
  // printf("address: 0x%x   rf - to: %d\n", true_addr, header.refcount);
  if (header.refcount == 0) {
    switch (header.memheadtag) {
      case MemHeadJSObj: {
        ManageObject((__jsobject *)addr, RECALL);
        return;
      }
      case MemHeadJSString: {
        RecallString((__jsstring *)addr);
        return;
      }
      case MemHeadEnv: {
        ManageEnvironment(addr, RECALL);
        return;
      }
      case MemHeadJSIter: {
        MemHeader &header = memory_manager->GetMemHeader(addr);
        RecallMem(addr, sizeof(__jsiterator));
        return;
      }
      default:
        MIR_FATAL("unknown GC object type");
    }
  }

  // RC != 0 after decrease; if it's jsobject, then possible root of cycle
  if (GetMemHeader(addr).memheadtag == MemHeadJSObj && GetMemHeader(addr).in_roots == false) {
    GetMemHeader(addr).in_roots = true;
    crc_candidate_roots.insert(addr);
  }
}

// #ifndef RC_NO_MMAP
#if 0
AddrMap *MemoryManager::GetMMapValList(void *plshval) {
  VMMembank membank = ginterpreter->GetMemBank(plshval);
  switch (membank) {
    case VMMEMHP:  // on heap
      return ginterpreter->GetHeapRefList();
    case VMMEMGP:
      return ginterpreter->GetGlobalRefList();
    case VMMEMST:
      assert(false && "NYI");
      return ginterpreter->vmfunc_->ref_list_;
    default:
      MIR_FATAL("unknown memory bank type");
      return NULL;
  }
}

// this function is called when we write rshval into plshval
// TODO: We should make sure the type of right value is dynamic-type before call IsMvalObject&IsMvalString.
void MemoryManager::UpdateGCReference(void *plshval, Mval rshval) {
  // MIR_ASSERT(IsMvalObject(rshval) || IsMvalString(rshval));
  AddrMap *ref_list = GetMMapValList(plshval);
  AddrMapNode *val_it = ref_list->FindInAddrMap(plshval);
  if (val_it) {
    void *rptr = val_it->ptr2;
    if (IsMvalObject(rshval) || IsMvalString(rshval) || IsMvalEnv(rshval)) {
      (*val_it).ptr2 = rshval.asptr;
      // rshval Inc
      GCIncRf(rshval.asptr);
    } else {
      // remove the memory map node
      ref_list->RemoveAddrMapNode(val_it);
      // remove_mmap_node(ref_list, val_it);
    }
    GCDecRf(rptr);  // decrease the reference counting after the increase
                    // in case store the same object into the same address
                    // then it won't recall the object if referece down to zero
  } else {
    if (IsMvalObject(rshval) || IsMvalString(rshval) || IsMvalEnv(rshval)) {
      ref_list->AddAddrMapNode(plshval, rshval.asptr);
      GCIncRf(rshval.asptr);
    }
  }
}

#endif

#if MIR_FEATURE_FULL && MIR_DEBUG
void MemoryManager::DebugMemory() {
  printf("heap big offset: %d\n", heap_free_big_offset_);
  printf("heap small offset: %d\n", heap_free_small_offset_);
  printf("heap memory table:\n");
  heap_memory_bank_->Debug();
  printf("vm memory offset: small offset = %d, big offset = %d\n", vm_free_small_offset_, vm_free_big_offset_);
  printf("vm memory table:\n");
  vm_memory_bank_->Debug();
  printf("chunk memory usage:\n");
  for (MemoryChunk *chunk = free_memory_chunk_; chunk; chunk = chunk->next) {
    printf("memory offset:%u, memory size:%u\n", chunk->offset_, chunk->size_);
  }
}

#endif  // MIR_FEATURE_FULL

#if DEBUGGC
// check if ptr is a legal address
bool MemoryManager::CheckAddress(void *ptr) {
  if (IsHeap(ptr)) {
    uint32 offset = (uint8 *)ptr - (uint8 *)memory_;
    return heap_memory_bank_->CheckOffset(offset);
  }
  return false;
}

#endif

//#ifndef RC_NO_MMAP
#if 0
AddrMap *MemoryManager::NewAddrMap(uint32_t size, uint32_t mask) {
  AddrMap *mmap;
  if (free_mmaps_) {
    mmap = free_mmaps_;
    free_mmaps_ = mmap->next;
  } else {
    mmap = (AddrMap *)VMMallocNOGC(sizeof(AddrMap));
    MIR_ASSERT(mmap);
  }
  mmap->next = NULL;
  mmap->hashtab = (AddrMapNode *)VMMallocNOGC(size * sizeof(AddrMapNode));
  memset_s((void *)(mmap->hashtab), size * sizeof(AddrMapNode), 0, size * sizeof(AddrMapNode));
  mmap->hashtab_idx_mask = mask;
  mmap->hashtab_size = size;
  return mmap;
}

AddrMapNode *MemoryManager::NewAddrMapNode() {
  AddrMapNode *node;
  if (free_mmap_nodes_) {
    node = free_mmap_nodes_;
    free_mmap_nodes_ = node->next;
    node->next = NULL;
    MMAP_NODE_SET_FREE(node);
  } else {
    node = (AddrMapNode *)VMMallocNOGC(sizeof(AddrMapNode));
  }
  return node;
}

void MemoryManager::FreeAddrMap(AddrMap *map) {
  MIR_ASSERT(map);
  map->next = free_mmaps_;
  free_mmaps_ = map;

  // add all chained mmap nodes to free nodes lists
  uint32_t i;
  for (i = 0; i < map->hashtab_size; i++) {
    AddrMapNode *node = map->hashtab[i].next;
    while (node) {
      // TODO: we can find the last node and add the whole chain
      AddrMapNode *next_node = node->next;
      node->next = free_mmap_nodes_;
      free_mmap_nodes_ = node;
      node = next_node;
    }
  }

  VMFreeNOGC(map->hashtab, map->hashtab_size * sizeof(AddrMapNode));
  map->hashtab = NULL;
  map->hashtab_idx_mask = 0;
  map->hashtab_size = 0;
}

void MemoryManager::UpdateAddrMap(uint32_t *origptr, uint32_t *newptr, uint32_t old_len, uint32_t new_len) {
  AddrMap *mmap = ginterpreter->GetHeapRefList();
  uint32_t len = new_len >= old_len ? old_len : new_len;
  for (uint32_t i = 0; i < len; i++) {
    AddrMapNode *mmap_node = mmap->FindInAddrMap((void *)origptr[i]);
    if (mmap_node != NULL) {
      mmap->AddAddrMapNode((void *)newptr[i], mmap_node->ptr2);
      mmap->RemoveAddrMapNode(mmap_node);
    }
  }
  for (uint32_t i = new_len; i < old_len; i++) {
    AddrMapNode *mmap_node = mmap->FindInAddrMap((void *)origptr[i]);
    if (mmap_node != NULL) {
      mmap->RemoveAddrMapNode(mmap_node);
    }
  }
}

#endif

#ifdef MARK_CYCLE_ROOTS
void MemoryManager::AddCycleRootNode(CycleRoot **roots_head, __jsobject *obj) {
  CycleRoot *root = (CycleRoot *)VMMallocGC(sizeof(CycleRoot));
  root->obj = obj;
  root->next = *roots_head;
  if (*roots_head) {
    (*roots_head)->pre = root;
  }
  *roots_head = root;
  (*roots_head)->pre = NULL;
}

void MemoryManager::DeleteCycleRootNode(CycleRoot *root) {
  if (root->pre) {
    root->pre->next = root->next;
  } else {
    cycle_roots = root->next;
  }
  if (root->next) {
    root->next->pre = root->pre;
  }
  RecallMem(root, sizeof(CycleRoot));
}

void MemoryManager::ManageChildObj(__jsobject *obj, ManageType flag) {
  if (!obj) {
    return;
  }
  // if (flag == DECREASE) {
  if (flag == MARK_RED) {
#if 0
    if (!(--GetMemHeader(obj).refcount)) {
      if (GetMemHeader(obj).is_root && GetMemHeader(obj).is_decreased) {
        return;
      }
      ManageObject(obj, flag);
    }
#endif
    GetMemHeader(obj).refcount--;  // parent just marked red, do RC-- regardless of color
    if (GetMemHeader(obj).color != CRC_RED) { // if already red, nothing to do
      GetMemHeader(obj).color = CRC_RED;
      ManageObject(obj, flag); // continue MARK_RED with children
    }
  } else if (flag == SCAN) { // parent just maked blue
    if (GetMemHeader(obj).color == CRC_RED) { // SCAN only applies to red nodes
      if (GetMemHeader(obj).refcount > 0) { // this node must have external references; turn green
        GetMemHeader(obj).color = CRC_GREEN;
        ManageObject(obj, SCAN_GREEN); // continue with SCAN_GREEN for children
      } else { // RC=0; this is a garbage node
        GetMemHeader(obj).color = CRC_BLUE;
        ManageObject(obj, SCAN); // continue wth SCAN for children
      }
    }
  } else if (flag == SCAN_GREEN) { // parent just turned green, so children turn green too
#if 0
    if (++GetMemHeader(obj).refcount == 1) {
      ManageObject(obj, flag);
    }
#endif
    GetMemHeader(obj).refcount++; // RC++ regardless of color
    if (GetMemHeader(obj).color != CRC_GREEN) {
      GetMemHeader(obj).color = CRC_GREEN;
      ManageObject(obj, SCAN_GREEN); // continue SCAN_GREEN with children
    }
  } else if (flag == COLLECT) { // collect garbage (blue) nodes
/*
    if ((!GetMemHeader(obj).refcount) && (!GetMemHeader(obj).is_collected)) {
      AddCycleRootNode(&garbage_roots, obj);
      GetMemHeader(obj).is_collected = true;
      ManageObject(obj, flag);
    }
*/
    if (GetMemHeader(obj).color == CRC_BLUE) {
      GetMemHeader(obj).color = CRC_GREEN;
      AddCycleRootNode(&garbage_roots, obj);
      ManageObject(obj, COLLECT);
    }
  } else if (flag == RECALL) {
    GCDecRf(obj);
#ifdef MM_DEBUG
  } else if (flag == CLOSURE) {
    if(GetMemHeader(obj).visited == false) {
      GetMemHeader(obj).visited = true;
      crc_closure.push_back(obj);
      ManageObject(obj, CLOSURE);
    }
#endif
  } else {
    return;
  }
}

void MemoryManager::ManageJsvalue(TValue &val, ManageType flag) {
  if (!IS_NEEDRC(val.x.u64))
    return;
  if (flag == RECALL) {
    GCDecRf((void *)val.x.c.payload);
    //GCCheckAndDecRf(GET_PAYLOAD(val), IS_NEEDRC(val.x.u64));
  } else if (flag == SWEEP || is_sweep) {
    // if the val is an object, then do nothing
    if (__is_string(val)) {
#ifdef MACHINE64
      GCDecRf((void *)val.x.c.payload);
#else
      GCDecRf(val->x.ptr);
#endif
    }
  } else {
    if (__is_js_object(val)) {
#ifdef MACHINE64
      ManageChildObj((__jsobject *)val.x.c.payload, flag);
#else
      ManageChildObj(val->x.obj, flag);
#endif
    }
  }
}

void MemoryManager::ManageEnvironment(void *envptr, ManageType flag) {
  if (!envptr) {
    return;
  }

#ifdef MACHINE64
  //assert(false && "NYI");
  uint64* ptr = (uint64*)envptr;
  uint32 argnums = *(uint32*)ptr;
  ptr++;
  void *parentenv = (void*)(*ptr);
  if (parentenv) {
    TValue* tvptr = (TValue*)ptr;
    if (flag == RECALL) {
      GCDecRf((void*)tvptr->x.c.payload);
    } else {
// ToDo: when flag is not RECALL
     ManageEnvironment((void*)tvptr->x.c.payload, flag);
    }
  }

  void* obj = (void*)envptr; // for cyclic RC algorithm, the env object is treated as a __jsobject.
  ManageType childFlag = flag;  // childFlag is for child objects
  bool child_no_action = false;
  if (flag == MARK_RED) {
    GetMemHeader(obj).refcount--;  // parent just marked red, do RC-- regardless of color
    if (GetMemHeader(obj).color != CRC_RED) { // if already red, nothing to do
      GetMemHeader(obj).color = CRC_RED;
      // continue MARK_RED with children
    }
    else
      child_no_action = true;
  } else if (flag == SCAN) { // parent just maked blue
    if (GetMemHeader(obj).color == CRC_RED) { // SCAN only applies to red nodes
      if (GetMemHeader(obj).refcount > 0) { // this node must have external references; turn green
        GetMemHeader(obj).color = CRC_GREEN;
        // continue with SCAN_GREEN for children
        childFlag = SCAN_GREEN; // continue with SCAN_GREEN for children
      } else { // RC=0; this is a garbage node
        GetMemHeader(obj).color = CRC_BLUE;
        // continue wth SCAN for children
      }
    }
    else
      child_no_action = true;
  } else if (flag == SCAN_GREEN) { // parent just turned green, so children turn green too
    GetMemHeader(obj).refcount++; // RC++ regardless of color
    if (GetMemHeader(obj).color != CRC_GREEN) {
      GetMemHeader(obj).color = CRC_GREEN;
      // continue SCAN_GREEN with children
    }
    else
      child_no_action = true;
  } else if (flag == COLLECT) { // collect garbage (blue) nodes
    if (GetMemHeader(obj).color == CRC_BLUE) {
      GetMemHeader(obj).color = CRC_GREEN;
      // AddCycleRootNode(&garbage_roots, obj);
      // env objects are not collected in garbage_roots because the latter only holds __jsobject.
      // we can and should reclaim now because sweep will not be called for env objects.
      uint32 totalsize = sizeof(uint64) + sizeof(void *) + argnums * sizeof(void*);
      RecallMem(envptr, totalsize);
      // continue COLLECT with children
    }
    else
      child_no_action = true;
  } else if (flag == RECALL) {
    // must recall children first; so do nothing here.
#ifdef MM_DEBUG
  } else if (flag == CLOSURE) {
    if(GetMemHeader(obj).visited == false) {
      GetMemHeader(obj).visited = true;
      // crc_closure.push_back(obj);  // todo: can we skip the env obj?
      // continue CLOSURE with children
    }
#endif
  } else {
    // Note that SWEEP shouldn't reach an env object
    assert(0);
  }

  ptr++;
  if (child_no_action == false) {
    for (uint32 i = 1; i <= argnums; i++) {
      TValue val = {.x.u64 = (*ptr)};
      void *true_addr = (void*)(val.x.c.payload);

      if(__is_js_object(val)) {
        ManageChildObj((__jsobject*)true_addr, childFlag);
      }
      ptr++;
    }
  }

  if (flag == RECALL) {
    uint32 totalsize = sizeof(uint64) + sizeof(void *) + argnums * sizeof(void*);
    RecallMem(envptr, totalsize);
  }
  return;
#else
  uint32 *u32envptr = (uint32 *)envptr;
  Mval *mvalptr = (Mval *)envptr;
  void *parentenv = (void *)u32envptr[1];
  if (parentenv) {
    if (flag != RECALL) {
      ManageEnvironment(parentenv, flag);
    } else {
      GCDecRf(parentenv);
    }
  }
  uint32 argnums = u32envptr[0];
  uint32 totalsize = sizeof(uint32) + sizeof(void *);  // basic size
  for (uint32 i = 1; i <= argnums; i++) {
    TValue val = MvalToJsval(mvalptr[i]);
    ManageJsvalue(&val, flag);
    totalsize += sizeof(Mval);
  }
  if (flag == RECALL) {
    RecallMem(envptr, totalsize);
  }
#endif
}

void MemoryManager::ManageProp(__jsprop *prop, ManageType flag) {
  __jsprop_desc desc = prop->desc;
  if (__has_value(desc)) {
    TValue jsvalue = __get_value(desc);
    ManageJsvalue(jsvalue, flag);
  }
  if (__has_set(desc)) {
    ManageChildObj(__get_set(desc), flag);
  }
  if (__has_get(desc)) {
    ManageChildObj(__get_get(desc), flag);
  }
  if (flag == SWEEP || flag == RECALL) {
    if(prop->isIndex == false) {
      GCDecRf(prop->n.name);
    }
    RecallMem((void *)prop, sizeof(__jsprop));
  }
}

void MemoryManager::ManageObject(__jsobject *obj, ManageType flag) {
  if (!obj) {
    return;
  }
  __jsprop *jsprop = obj->prop_list;
  while (jsprop) {
    __jsprop *next_jsprop = jsprop->next;
    ManageProp(jsprop, flag);
    jsprop = next_jsprop;
  }

  if (!obj->proto_is_builtin) {
    ManageChildObj(obj->prototype.obj, flag);
  }

  switch (obj->object_class) {
    case JSSTRING:
      if (flag == SWEEP || flag == RECALL) {
        GCDecRf(obj->shared.prim_string);
      }
      break;
    case JSARRAY:
      if (obj->object_type == JSREGULAR_ARRAY) {
        TValue *array = obj->shared.array_props;
        uint32 arrlen = (uint32_t)__jsval_to_number(array[0]);
        for (uint32_t i = 0; i < arrlen; i++) {
          TValue jsvalue = obj->shared.array_props[i + 1];
          if (IS_NEEDRC(jsvalue.x.u64))
            ManageJsvalue(jsvalue, flag);
        }
        if (flag == SWEEP || flag == RECALL) {
          uint32_t size = (arrlen + 1) * sizeof(TValue);
          RecallMem((void *)obj->shared.array_props, size);
        }
      }
      break;
    case JSFUNCTION:
      // Some builtins may not have a function.
      if (!obj->is_builtin || obj->shared.fun) {
        // Release bound function first.
        __jsfunction *fun = obj->shared.fun;
        if (fun->attrs & JSFUNCPROP_BOUND) {
          ManageChildObj((__jsobject *)(fun->fp), flag);
          uint32_t bound_argc = ((fun->attrs >> 16) & 0xff);
          TValue *bound_args = (TValue *)fun->env;
          for (uint32_t i = 0; i < bound_argc; i++) {
            TValue val = bound_args[i];
            ManageJsvalue(val, flag);
          }
          if (flag == SWEEP || flag == RECALL) {
            if (fun->env != nullptr)
              RecallMem(fun->env, bound_argc * sizeof(TValue));
          }
        } else {
          if ((flag != SWEEP) && (flag != RECALL)) {
            ManageEnvironment(fun->env, flag);
          } else {
            if (flag == RECALL)
              GCDecRf(fun->env);
          }
        }
        if (flag == SWEEP || flag == RECALL) {
          RecallMem(fun, sizeof(__jsfunction));
        }
      }
      break;
    case JSARRAYBUFFER: {
      __jsarraybyte *arrayByte = obj->shared.arrayByte;
      RecallMem((void *)arrayByte, sizeof(uint8_t) * __jsval_to_number(arrayByte->length));
    }
    break;
    case JSDATAVIEW:
    case JSOBJECT:
    case JSBOOLEAN:
    case JSNUMBER:
    case JSGLOBAL:
    case JSON:
    case JSMATH:
    case JSDOUBLE:
    case JSREGEXP:
    case JSDATE:
    case JSINTL_COLLATOR:
    case JSINTL_NUMBERFORMAT:
    case JSINTL_DATETIMEFORMAT:
    case JSARGUMENTS:
    case JSERROR:
      break;
    default:
      MIR_FATAL("manage unknown js object class");
  }
  if (flag == SWEEP || flag == RECALL) {
    if (obj->prop_index_map)
      delete(obj->prop_index_map);
    if (obj->prop_string_map)
      delete(obj->prop_string_map);
    RecallMem((void *)obj, sizeof(__jsobject));
  }
}

void MemoryManager::RecallRoots(CycleRoot *root) {
  if (TurnoffGC())
    return;
  if (!root) {
    return;
  }
  while (root) {
    CycleRoot *cur_root = root;
    root = root->next;
    RecallMem(cur_root, sizeof(CycleRoot));
  }
}

void MemoryManager::ResetCycleRoots() {
  if (!cycle_roots) {
    return;
  }
  CycleRoot *root = cycle_roots;
  while (root) {
/*
    GetMemHeader(root->obj).is_decreased = false;
    GetMemHeader(root->obj).need_restore = false;
    GetMemHeader(root->obj).is_collected = false;
*/
    GetMemHeader(root->obj).color = CRC_GREEN;
    GetMemHeader(root->obj).in_roots = false;
    root = root->next;
  }
}

#ifdef MM_DEBUG
void MemoryManager::DumpClosures() {
  const char* CRCColorName [] = {"GRN", "RED", "BLU"};
  printf("Closures:\n");
  CycleRoot *root = cycle_roots;
  while (root) {
    assert(crc_closure.size() == 0);
    {
      crc_closure.push_back(root->obj);
      GetMemHeader(root->obj).visited = true;
      ManageObject(root->obj, CLOSURE);
      // dump closure
      for (auto a : crc_closure) {
        // printf("%p (color= %d rc= %d) ", a, (int)GetMemHeader(a).color, (int)GetMemHeader(a).refcount);
        printf("%p (color= %s rc= %d) ", a, CRCColorName[(int)GetMemHeader(a).color], (int)GetMemHeader(a).refcount);
        GetMemHeader(a).visited = false;
      }
      printf("\n");
      crc_closure.clear();
    }
    root = root->next;
  }
}
#endif

void MemoryManager::RecallCycle() {
  if (crc_candidate_roots.empty()) {
    return;
  }

#ifdef MM_DEBUG
  // for (auto a : crc_candidate_roots)
    // printf("candidate: %p rc= %d\n", (void*)a, (int)GetMemHeader(a).refcount);
int num_roots = 0;
#endif
  for (auto a : crc_candidate_roots) {
    if (GetMemHeader(a).refcount > 0) { // if RC=0, it's garbage already
      AddCycleRootNode(&cycle_roots, (__jsobject*)a);
#ifdef MM_DEBUG
      num_roots++;
#endif
    }
  }
  crc_candidate_roots.clear();

  if (!cycle_roots) {
    return;
  }

  // Step 1: mark red transitive closure of root. All nodes in the closure are marked red.
  // In the process, whenever a node is visited its RC is decreased.
#ifdef DBG_CRC_LMS
  printf("In RecallCycle, num of candidates= %d\n", num_roots);
  printf("##### step 1: mark red all nodes in transitive closure\n");
#endif
  CycleRoot *root = cycle_roots;
  while (root) {
#if 0
    if (GetMemHeader(root->obj).refcount == 0) { // if RC=0, it's garbage
      DeleteCycleRootNode(root);
    } else
#endif
    {
      if (GetMemHeader(root->obj).color != CRC_RED) {
        GetMemHeader(root->obj).color = CRC_RED;
        ManageObject(root->obj, MARK_RED);
      }
    }
    root = root->next;
  }

#ifdef DBG_CRC_LMS
  // DumpClosures();
  printf("##### step 2: mark nodes blue or green\n");
#endif

  // Step 2 scan the closure and distinguish live and dead nodes. If a red node's RC=0,
  // it is garbage and marked blue. And SCAN is continued with its children. Otherwise,
  // it must have external reference and is marked green. SCAN_GREEN is continued with
  // its children. When SCAN_GREEN reaches a node, its RC is increased to undo the RC--
  // during MARK_RED.
  root = cycle_roots;
  while (root) {
    if (GetMemHeader(root->obj).color == CRC_RED) {
      if (GetMemHeader(root->obj).refcount > 0) {
        GetMemHeader(root->obj).color = CRC_GREEN;
        ManageObject(root->obj, SCAN_GREEN);
      }
      else {
        GetMemHeader(root->obj).color = CRC_BLUE;
        ManageObject(root->obj, SCAN);
      }
    }
    root = root->next;
  }

#ifdef MM_DEBUG
  // DumpClosures();
#endif

  // Step 3 collect BLUE nodes, i.e., garbage.
#ifdef MM_DEBUG
  int num_garbage_cycles = 0;
#endif
  root = cycle_roots;
  while (root) {
    CycleRoot *next_root = root->next;  // in case the current root is deleted after collected
    if (GetMemHeader(root->obj).color == CRC_BLUE) {
#ifdef MM_DEBUG
      num_garbage_cycles++;
#endif
      GetMemHeader(root->obj).color = CRC_GREEN;
      AddCycleRootNode(&garbage_roots, root->obj);
      ManageObject(root->obj, COLLECT);
      DeleteCycleRootNode(root);
    }
    root = next_root;
  }

#ifdef DBG_CRC_LMS
  printf("Num_garbage_cycles= %d\n", num_garbage_cycles);
#endif

  // Step 4 release the collected garbage cycle nodes.
  root = garbage_roots;
  // Sweep garbage-cycle
  is_sweep = true;
  while (root) {
    ManageObject(root->obj, SWEEP);
    root = root->next;
  }
  is_sweep = false;
  ResetCycleRoots();
  RecallRoots(cycle_roots); // ToDo: the remaining roots are not garbage, but could they become cycle garbage later?
  cycle_roots = nullptr;
 
  RecallRoots(garbage_roots);
  garbage_roots = nullptr;
}

#else
void MemoryManager::ManageChildObj(__jsobject *obj, ManageType flag) {
  if (!obj) {
    return;
  }
  if (flag == MARK) {
    ManageObject(obj, MARK);
  } else if (flag == DECRF) {
    GetExMemHeader((void *)obj).gc_refs--;
  } else if (flag == RECALL) {
    GCDecRf(obj);
  } else if (GetExMemHeader((void *)obj).marked) {
    GCDecRf(obj);
  }
}

void MemoryManager::ManageJsvalue(TValue *val, ManageType flag) {
  if (flag == MARK || flag == DECRF) {
    if (__is_js_object(val)) {
#ifdef MACHINE64
      ManageChildObj((val->x.obj), flag);
#else
      ManageChildObj(val->x.obj, flag);
#endif
    }
  } else if (is_sweep && NotMarkedObject(val)) {
    // do nothing
  } else {
// #ifdef RC_NO_MMAP
#if 1
    GCCheckAndDecRf(val->asbits);
#else
    AddrMap *mmap = ginterpreter->GetHeapRefList();
    AddrMapNode *mmap_node = mmap->FindInAddrMap(&val->x.payload.ptr);
    MIR_ASSERT(mmap_node);
    mmap->RemoveAddrMapNode(mmap_node);
    // remove_mmap_node(mmap, mmap_node);
    GCCheckAndDecRf(val->payload.asbits);
#endif
  }
}

void MemoryManager::ManageEnvironment(void *envptr, ManageType flag) {
  /*
     type $bar_env_type <struct {
     @argnums u32,
     @parentenv <* void>,
     @arg1 dynany,
     @arg2 dynany,
     ....
     @argn dynany}
   */
  // get the argnums
  // not that the following code can only work on 32-bits machine
  if (!envptr) {
    return;
  }
  assert(false&&"NYI");
  /*
  uint32 *u32envptr = (uint32 *)envptr;
  Mval *mvalptr = (Mval *)envptr;
  void *parentenv = (void *)u32envptr[1];
  if (parentenv) {
    if (flag == MARK || flag == DECRF) {
      ManageEnvironment(parentenv, flag);
    } else {
      GCDecRf(parentenv);
    }
  }
  uint32 argnums = u32envptr[0];
  uint32 totalsize = sizeof(uint32) + sizeof(void *);  // basic size
  for (uint32 i = 1; i <= argnums; i++) {
    TValue val = MvalToJsval(mvalptr[i]);
    ManageJsvalue(&val, flag);
    totalsize += sizeof(Mval);
  }
  if (flag == RECALL || flag == SWEEP) {
    RecallMem(envptr, totalsize);
  }
  */
}

void MemoryManager::ManageProp(__jsprop *prop, ManageType flag) {
  __jsprop_desc desc = prop->desc;
  if (__has_value(desc)) {
    TValue jsvalue = __get_value(desc);
    ManageJsvalue(&jsvalue, flag);
  }
  if (__has_set(desc)) {
    ManageChildObj(__get_set(desc), flag);
  }
  if (__has_get(desc)) {
    ManageChildObj(__get_get(desc), flag);
  }

  if (flag == RECALL || flag == SWEEP) {
    GCDecRf(prop->name);
    RecallMem((void *)prop, sizeof(__jsprop));  // recall the prop
  }
}

void MemoryManager::ManageObject(__jsobject *obj, ManageType flag) {
  if (flag == MARK) {
    ExMemHeader &exheader = GetExMemHeader((void *)obj);
    if (exheader.marked) {
      return;
    } else {
      exheader.marked = true;
    }
  }

  __jsprop *jsprop = obj->prop_list;
  while (jsprop) {
    __jsprop *next_jsprop = jsprop->next;
    ManageProp(jsprop, flag);
    jsprop = next_jsprop;
  }

  if (!obj->proto_is_builtin) {
    ManageChildObj(obj->prototype.obj, flag);
  }

  switch (obj->object_class) {
    case JSSTRING:
      if (flag == RECALL || flag == SWEEP) {
        GCDecRf(obj->shared.prim_string);
      }
      break;
    case JSARRAY:
      /*TODO : Be related to VMReallocGC, AddrMapNode information will miss after VMReallocGC.  */
      if (obj->object_type == JSREGULAR_ARRAY) {
        TValue *array = obj->shared.array_props;
        uint32 arrlen = (uint32_t)__jsval_to_number(&array[0]);
        for (uint32_t i = 0; i < arrlen; i++) {
          TValue jsvalue = obj->shared.array_props[i + 1];
          ManageJsvalue(&jsvalue, flag);
        }
        if (flag == RECALL || flag == SWEEP) {
          uint32_t size = (arrlen + 1) * sizeof(TValue);
          RecallMem((void *)obj->shared.array_props, size);
        }
      }
      break;
    case JSFUNCTION:
      // Some builtins may not have a function.
      if (!obj->is_builtin || obj->shared.fun) {
        // Release bound function first.
        __jsfunction *fun = obj->shared.fun;
        if (fun->attrs & JSFUNCPROP_BOUND) {
          ManageChildObj((__jsobject *)(fun->fp), flag);
          uint32_t bound_argc = ((fun->attrs >> 16) & 0xff);
          TValue *bound_args = (TValue *)fun->env;
          for (uint32_t i = 0; i < bound_argc; i++) {
            TValue val = bound_args[i];
            ManageJsvalue(&val, flag);
          }
          if ((flag == RECALL || flag == SWEEP) && bound_argc > 0) {
            RecallMem(fun->env, bound_argc * sizeof(TValue));
          }
        } else {
          if (flag == MARK || flag == DECRF) {
            ManageEnvironment(fun->env, flag);
          } else {
            GCDecRf(fun->env);
          }
        }
        if (flag == RECALL || flag == SWEEP) {
          RecallMem(fun, sizeof(__jsfunction));
        }
      }
      break;
    case JSOBJECT:
    case JSBOOLEAN:
    case JSNUMBER:
    case JSGLOBAL:
    case JSON:
    case JSMATH:
      break;
    default:
      MIR_FATAL("manage unknown js object class");
  }
  if (flag == RECALL || flag == SWEEP) {
    DeleteObjListNode(obj);
    RecallMem((void *)obj, sizeof(__jsobject));
  }
}

bool MemoryManager::NotMarkedObject(TValue *val) {
  bool not_marked_obj = true;
  if (__is_js_object(val)) {
#ifdef MACHINE64
    assert(false && "NYI");
#else
    not_marked_obj = !(GetExMemHeader((void *)val->payload.ptr).marked);
#endif
  }
  return not_marked_obj;
}

void MemoryManager::Mark() {
  __jsobject *obj = obj_list;
  while (obj) {
    ExMemHeader &exheader = GetExMemHeader((void *)obj);
    if (exheader.gc_refs > 0) {
      ManageObject(obj, MARK);
    }
    obj = exheader.next;
  }
}

/* steps to find garbage reference cycles:
   1.For each container object, set gc_refs equal to the object's reference count.
   2.For each container object, find which container objects it references and decrement
     the referenced container's gc_refs field.
   3.All container objects that now have a gc_refs field greater than zero are referenced
     from outside the set of container objects. We cannot free these objects so we set the
     marked flag of them to true. Any objects referenced from the objects marked also cannot
     be freed. We mark them and all the objects reachable from them too.
   4.Objects unmarked in the obj_list are only referenced by objects within cycles (ie. they
     are inaccessible and are garbage). We can now go about freeing these objects. */
void MemoryManager::DetectRCCycle() {
  __jsobject *obj = obj_list;
  while (obj) {
    ExMemHeader &exheader = GetExMemHeader((void *)obj);
    uint16_t refcount = exheader.refcount;
    // in case a created object is unmaintained with RC befor invoking of mark-and-sweep
    if (refcount == 0) {
      ManageObject(obj, MARK);
    } else {
      exheader.gc_refs = refcount;
    }
    obj = exheader.next;
  }
  obj = obj_list;
  while (obj) {
    ExMemHeader &exheader = GetExMemHeader((void *)obj);
    if (!exheader.marked) {
      ManageObject(obj, DECRF);
    }
    obj = exheader.next;
  }
}

void MemoryManager::Sweep() {
  is_sweep = true;
  __jsobject *obj = obj_list;
  while (obj) {
    ExMemHeader &exheader = GetExMemHeader((void *)obj);
    if (exheader.marked) {
      exheader.marked = false;
      obj = exheader.next;
    } else {
      __jsobject *pre = exheader.pre;
      ManageObject(obj, SWEEP);
      if (pre) {
        obj = GetExMemHeader((void *)pre).next;
      } else {
        obj = obj_list;
      }
    }
  }
  is_sweep = false;
}

void MemoryManager::AddObjListNode(__jsobject *obj) {
  GetExMemHeader((void *)obj).next = obj_list;
  if (obj_list) {
    GetExMemHeader((void *)obj_list).pre = obj;
  }
  obj_list = obj;
  GetExMemHeader((void *)obj_list).pre = NULL;
}

void MemoryManager::DeleteObjListNode(__jsobject *obj) {
  ExMemHeader &exheader = GetExMemHeader((void *)obj);
  if (exheader.pre) {
    GetExMemHeader((void *)(exheader.pre)).next = exheader.next;
  } else {
    obj_list = exheader.next;
  }
  if (exheader.next) {
    GetExMemHeader((void *)(exheader.next)).pre = exheader.pre;
  }
}

void MemoryManager::MarkAndSweep() {
  DetectRCCycle();
  Mark();
  Sweep();
}

#endif
