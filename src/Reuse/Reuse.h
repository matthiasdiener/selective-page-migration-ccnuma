#ifndef _GENEXPR_H_
#define _GENEXPR_H_

#include "RExpr.h"
#include "../DepGraph/TripCountAnalysis.h"

#include "llvm/Pass.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instructions.h"

namespace llvm {

/*********
 * Reuse *
 *********/
class Reuse : public FunctionPass {
public:
  static char ID;
  Reuse() : FunctionPass(ID) { }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnFunction(Function &F);

private:
  RExpr genExprFromUntil(Value *From, PHINode *Until, Loop *L);
  RExpr composeLoopExpr(RExpr Ex, PHINode *Arg, Value *Start, Value *Times);

  LoopInfo *LI_;
  DominatorTree *DT_;
  TripCountAnalysis *TCA_;
};

}

#endif

