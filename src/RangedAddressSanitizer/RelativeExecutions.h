#ifndef _RELATIVEEXECUTIONS_H_
#define _RELATIVEEXECUTIONS_H_

#include "LoopInfoExpr.h"
#include "PythonInterface.h"

#include "llvm/Pass.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"

class RelativeExecutions : public FunctionPass {
public:
  static char ID;
  RelativeExecutions() : FunctionPass(ID) { }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnFunction(Function &F);

  // Returns the number of times a basic block whose immediate dominator is
  // L's loop header executes relative to Toplevel. Final will indicate the
  // outer-most loop that was reached.
  Expr getExecutionsRelativeTo(Loop *L, Loop *Toplevel, Loop *&Final);

private:
  DominatorTree  *DT_;
  LoopInfo       *LI_;
  LoopInfoExpr   *LIE_;
  SymPyInterface *SPI_;
};

#endif

