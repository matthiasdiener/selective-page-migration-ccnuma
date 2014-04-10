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

class RangedAddressSanitizer : public FunctionPass {
public:
  static char ID;
  RangedAddressSanitizer() : FunctionPass(ID) { }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnFunction(Function &F);
  
  bool doInitialization(Module &M);
  bool doFinalization(Module &M);
  
  const ValueSet & getSafeUses() const
  {
      return safeUseSet;
  }
  
  const ValueSet & getForcedChecks() const
  {
      return forcedCheckSet;
  }

private:
  void ii_visitLoop(Loop * loop);

  DataLayout         *DL_;
  DominatorTree      *DT_;
  LoopInfo           *LI_;
  ReduceIndexation   *RI_;
  RelativeExecutions *RE_;
  RelativeMinMax     *RMM_;
  SymPyInterface     *SPI_;

  LLVMContext *Context_;
  Module      *Module_;
  Constant    *FakeUseFn_;
  Constant    *ReuseFn_;
  Constant    *ReuseFnDestroy_;
  
  ValueSet safeUseSet;
  ValueSet forcedCheckSet;

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

