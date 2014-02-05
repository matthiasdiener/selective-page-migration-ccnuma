#ifndef _REDUCEINDEXATION_H_
#define _REDUCEINDEXATION_H_

#include "Expr.h"
#include "LoopInfoExpr.h"

#include "llvm/Pass.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"

class ReduceIndexation : public FunctionPass {
public:
  static char ID;
  ReduceIndexation() : FunctionPass(ID) { }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnFunction(Function &F);

  bool reduceStore(StoreInst *SI, Value *&Array, Expr &Offset) const;
  bool reduceLoad(LoadInst *LI, Value *&Array, Expr &Offset)   const;
  bool reduceGetElementPtr(GetElementPtrInst *GEP, Value *&Array,
                           Expr &Offset) const;
  bool reduceMemoryOp(Value *V, Value *&Array, Expr& Offset)   const;

private:
  DataLayout *DL_;
  LoopInfoExpr *LIE_;
};

#endif

