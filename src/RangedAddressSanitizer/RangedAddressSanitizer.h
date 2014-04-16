#ifndef _RANGEDADDRESSSANITIZER_H_
#define _RANGEDADDRESSSANITIZER_H_

#include "PythonInterface.h"
#include "ReduceIndexation.h"
#include "RelativeExecutions.h"
#include "RelativeMinMax.h"

#include "llvm/Pass.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"

#include <unordered_set>

using namespace llvm;
typedef std::set<const Value*> ValueSet;

/*
 * RangedAddressSanitizer
 *
 * RangedAddressSanitizer optimizes AddressSanitizer when applied to loop nests with
 * linear array accesses as shown below:
 *
 * Example code:
 * void foo(int* V, int N) {
 *   for(int i = 0; i < N-1; i++){
 *      //All vector operations are checked
 *      V[i] = V[i] + V[i+1];
 *  }
 * }
 *
 * It detects the range of the array accesses and lets AddressSanitizer verify that any
 * access in that range is safe. If this applies, the actual loop is executed efficiently
 * Without any memory checks. In case the range check touches poisoned memory, execution
 * continues on regular instrumented code.
 *
 *
 * Code after applying RangedAddressSanitizer:
 *
 * void foo(int* V, int N) {
 *  if (__fasan_check(V,0,N-1)){ // This check is O(N)
 *    for(int i = 0; i < N-1; i++){
 *      //The following operation has no bounds checks
 *      V[i] = V[i] + V[i+1];
 *    }
 *  } else {
 *    for(int i = 0; i < N-1; i++){
 *      //All accesses are checked
 *      V[i] = V[i] + V[i+1];
 *    }
 *  }
 * }
 *
 *
 * The environment variable FASANMODULE must point to the BC-compiled code of Runtime/FASanRuntime.cpp
 * If FASAN_DISABLE is set to any value, the pass will execute without any effect
 *
 * Use AddressSanitizer.cpp that ships with this pass and apply the accompanying patches to clang
 * (clang_patches).
 */
class RangedAddressSanitizer : public FunctionPass {
public:
  static char ID;
  RangedAddressSanitizer() : FunctionPass(ID) { }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnFunction(Function &F);
  
  bool doInitialization(Module &M);
  bool doFinalization(Module &M);
  
  // set of memory instructions that must not be instrumented
  const ValueSet & getSafeUses() const
  {
      return safeUseSet;
  }
  
  // set of pointer-yielding values that must be instrumented
  const ValueSet & getForcedChecks() const
  {
      return forcedCheckSet;
  }

private:
  void ii_visitLoop(Loop * loop);

  DataLayout         *DL_;
  DominatorTree      *DT_;
  LoopInfo           *LI_;
  ReduceIndexation   *RI_;  // Decomposes pointer based memory accesses into Array+Offset
  RelativeExecutions *RE_;  // array re-use
  RelativeMinMax     *RMM_; // induction variable bounds
  SymPyInterface     *SPI_;

  LLVMContext *Context_;
  Module      *Module_;
  Constant    *FakeUseFn_;
  Constant    *ReuseFn_;
  Constant    *ReuseFnDestroy_;
  
  ValueSet safeUseSet;
  ValueSet forcedCheckSet;

  // will add inst to the safe use set, if it is a memory instruction accessing array
  void markSafeArrayUse(Instruction * inst, Value * array);

  // try to decompose the instruction in a base pointer plus array offset
  bool reduceMemoryAccess(Instruction * I, Value *& oArray, Expr & oSubscript, unsigned & oSize);

  // Detects the offset range of the array accesses by instruction I (load or store)
  // the call info is stored in Calls_
  bool generateCallFor(Loop *L, Instruction *I);

  struct CallInfo {
    Loop * FinalLoop;
    BasicBlock *Preheader, *Final;
    Value *Array, *Min, *Max, *Reuse;

    bool operator==(const CallInfo &Other) const {
      return Preheader == Other.Preheader && Array == Other.Array;
    }
  };

  struct CallInfoHasher {
    size_t operator()(const CallInfo &CI) const {
      return (size_t)((long)CI.Preheader + (long)CI.Array);
    }
  };

  std::unordered_set<CallInfo, CallInfoHasher> Calls_;
};

#endif /* _RANGEDADDRESSSANITIZER_H_ */

