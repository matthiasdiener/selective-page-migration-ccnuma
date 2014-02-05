#ifndef _SELECTIVEPAGEMIGRATION_H_
#define _SELECTIVEPAGEMIGRATION_H_

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

class SelectivePageMigration : public FunctionPass {
public:
  static char ID;
  SelectivePageMigration() : FunctionPass(ID) { }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnFunction(Function &F);

private:
  DataLayout         *DL_;
  DominatorTree      *DT_;
  LoopInfo           *LI_;
  ReduceIndexation   *RI_;
  RelativeExecutions *RE_;
  RelativeMinMax     *RMM_;
  SymPyInterface     *SPI_;

  LLVMContext *Context_;
  Module      *Module_;
  Constant    *ReuseFn_;
  Constant    *ReuseFnDestroy_;

  bool generateCallFor(Loop *L, Instruction *I);

  struct CallInfo {
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

#endif

