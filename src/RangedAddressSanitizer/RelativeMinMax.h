#ifndef _RELATIVEMINMAX_H_
#define _RELATIVEMINMAX_H_

#include "LoopInfoExpr.h"
#include "PythonInterface.h"

#include "llvm/Pass.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
//#include "llvm/IR/Instructions.h"
//#include "llvm/Support/raw_ostream.h"
//#include "llvm/Support/CommandLine.h"
//#include "llvm/Support/Debug.h"

#include <python2.7/Python.h>
#include <vector>

class RelativeMinMax : public FunctionPass {
public:
  static char ID;
  RelativeMinMax() : FunctionPass(ID) { }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnFunction(Function &F);

  // return expressions for the minimum and maximum value that V can attain in L
  bool getMinMaxRelativeTo(Loop *L, Value *V, Expr &Min, Expr &Max);
  bool getMinMax(Expr Ex, Expr &Min, Expr &Max);

private:
  // interval logic on expression trees
  void addMinMax(Expr PrevMin, Expr PrevMax, Expr OtherMin, Expr OtherMax,
                 Expr &Min, Expr &Max);
  void mulMinMax(Expr PrevMin, Expr PrevMax, Expr OtherMin, Expr OtherMax,
                 Expr &Min, Expr &Max);

  Module * fasanModule;
  LoopInfo *LI_;
  DominatorTree *DT_;
  SymPyInterface *SPI_;
  LoopInfoExpr *LIE_;
};

#endif

