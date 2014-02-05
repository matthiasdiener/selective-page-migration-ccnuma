#include "RelativeExecutions.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

/* ************************************************************************** */
/* ************************************************************************** */

static cl::opt<bool>
  ClDebug("rel-exec-debug",
          cl::desc("Enable debugging for the relative execution pass"),
          cl::Hidden, cl::init(false));

static RegisterPass<RelativeExecutions>
  X("rel-exec", "Location-relative execution count inference");
char RelativeExecutions::ID = 0;

#define RE_DEBUG(X) { if (ClDebug) { X; } }

/* ************************************************************************** */
/* ************************************************************************** */

void RelativeExecutions::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<DominatorTree>();
  AU.addRequired<LoopInfo>();
  AU.addRequired<LoopInfoExpr>();
  AU.addRequired<SymPyInterface>();
  AU.setPreservesAll();
}

bool RelativeExecutions::runOnFunction(Function &F) {
  DT_  = &getAnalysis<DominatorTree>();
  LI_  = &getAnalysis<LoopInfo>();
  LIE_ = &getAnalysis<LoopInfoExpr>();
  SPI_ = &getAnalysis<SymPyInterface>();
  return false;
}

Expr RelativeExecutions::getExecutionsRelativeTo(Loop *L, Loop *Toplevel,
                                                 Loop *&Final) {
  PHINode *Indvar;
  Expr IndvarStart, IndvarEnd, IndvarStep;

  if (!LIE_->getLoopInfo(L, Indvar, IndvarStart, IndvarEnd, IndvarStep)) {
    RE_DEBUG(dbgs() << "RelativeExecutions: could not get loop info for loop at "
                    << L->getHeader()->getName() << "\n");
    return Expr::InvalidExpr();
  }

  RE_DEBUG(dbgs() << "RelativeExecutions: induction variable, start, end, step: "
                  << *Indvar << " => (" << IndvarStart << ", " << IndvarEnd
                  << ", +" << IndvarStep << ")\n");

  PyObject *Summation = nullptr;

  {
    PyObject *IndvarObj       = SPI_->conv(Indvar);
    PyObject *IndvarStepObj   = SPI_->conv(IndvarStep);
    PyObject *IndvarStartObj  = SPI_->conv(IndvarStart);
    PyObject *IndvarEndObj    = SPI_->conv(IndvarEnd);
    assert(IndvarObj && IndvarStepObj && IndvarStartObj && IndvarEndObj &&
           "Conversion error");

    PyObject *Summand = SPI_->inverse(IndvarStepObj);
    Summation = SPI_->summation(Summand, IndvarObj, IndvarStartObj,
                                IndvarEndObj);
    RE_DEBUG(dbgs() << "RelativeExecutions: summation for loop at "
                    << L->getHeader()->getName() << " is: " << *Summation
                    << "\n");
  }

  while ((Final = L) && (L = L->getParentLoop())) {
    if (!LIE_->getLoopInfo(L, Indvar, IndvarStart, IndvarEnd, IndvarStep)) {
      RE_DEBUG(dbgs() << "RelativeExecutions: could not get loop info for loop "
                         "at " << L->getHeader()->getName() << "\n");
      Expr Ret = SPI_->conv(Summation);
      RE_DEBUG(dbgs() << "RelativeExecutions: partial success; returning "
                      << Ret << "\n");
      return Ret;
    }

    RE_DEBUG(dbgs() << "RelativeExecutions: induction variable, start, end, "
                       "step: " << *Indvar << " => (" << IndvarStart << ", "
                     << IndvarEnd << ", +" << IndvarStep << ")\n");

    Expr SummationEx = SPI_->conv(Summation);
    Summation = SPI_->conv(SummationEx);

    PyObject *IndvarObj       = SPI_->conv(Indvar);
    PyObject *IndvarStepObj   = SPI_->conv(IndvarStep);
    PyObject *IndvarStartObj  = SPI_->conv(IndvarStart);
    PyObject *IndvarEndObj    = SPI_->conv(IndvarEnd);
    assert(IndvarObj && IndvarStepObj && IndvarStartObj && IndvarEndObj &&
           "Conversion error");

    PyObject *Summand = SPI_->mul(Summation, SPI_->inverse(IndvarStepObj));
    Summation = SPI_->summation(Summand, IndvarObj, IndvarStartObj,
                                IndvarEndObj);
    Summation = SPI_->expand(Summation);
    RE_DEBUG(dbgs() << "RelativeExecutions: summation for loop at "
                    << L->getHeader()->getName() << " is: " << *Summation
                    << "\n");

    if (L == Toplevel)
      break;
  }

  if (L == Toplevel || !Toplevel) {
    Expr Ret = SPI_->conv(Summation);
    RE_DEBUG(dbgs() << "RelativeExecutions: success; returning " << Ret << "\n");
    return Ret;
  } else {
    // Expr Ret = SPI_->conv(Summation);
    RE_DEBUG(dbgs() << "RelativeExecutions: toplevel loop has not been "
                       "reached\n");
    // return Ret;
    return Expr::InvalidExpr();
  }
}

