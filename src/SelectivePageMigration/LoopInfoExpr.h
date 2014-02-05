#ifndef _LOOPINFOEXPR_H_
#define _LOOPINFOEXPR_H_

#include "Expr.h"
#include "PythonInterface.h"

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instructions.h"

class LoopInfoExpr : public FunctionPass {
public:
  static char ID;
  LoopInfoExpr() : FunctionPass(ID) { }

  static bool IsLoopInvariant(Loop *L, Expr Ex);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnFunction(Function &F);

  bool isInductionVariable(PHINode *Phi);
  Loop *getLoopForInductionVariable(PHINode *Phi);

  // Builds an expression for V, stopping at loop-invariant atoms.
  Expr getExprForLoop(Loop *L, Value *V);
  // Builds an expression for V, stopping at any value that is not an
  // instruction.
  Expr getExpr(Value *V);

  // Returns the induction variable for the given loop & its start, end, & step.
  bool getLoopInfo(Loop *L, PHINode *&Indvar, Expr &IndvarStart,
                   Expr &IndvarEnd, Expr &IndvarStep);

private:
  PHINode *getSingleLoopVariantPhi(Loop *L, Expr Ex);

  LoopInfo *LI_;
  SymPyInterface *SPI_;
};

#endif

