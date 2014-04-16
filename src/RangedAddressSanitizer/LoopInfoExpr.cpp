#include "LoopInfoExpr.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include <cassert>
#include "FAsanConfig.hpp"

/* ************************************************************************** */
/* ************************************************************************** */

static cl::opt<bool>
  ClDebug("loop-info-expr-debug",
          cl::desc("Enable debugging for the loop information pass"),
          cl::Hidden, cl::init(false));

static RegisterPass<LoopInfoExpr>
  X("loop-info-expr", "Loop information");
char LoopInfoExpr::ID = 0;

#if 0
#define LIE_DEBUG(X) { if (ClDebug) { X; } }
#else
#define LIE_DEBUG(X) { X; }
#endif

/* ************************************************************************** */
/* ************************************************************************** */

bool LoopInfoExpr::IsLoopInvariant(Loop *L, Expr Ex) {
  auto Symbols = Ex.getSymbols();
  for (auto& Sym : Symbols)
    if (!L->isLoopInvariant(Sym.getSymbolValue()))
      return false;
  return true;
}

/* ************************************************************************** */
/* ************************************************************************** */

void LoopInfoExpr::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfo>();
  AU.addRequired<SymPyInterface>();
  AU.setPreservesAll();
}

bool LoopInfoExpr::runOnFunction(Function &F) {
  LI_ = &getAnalysis<LoopInfo>();
  SPI_ = &getAnalysis<SymPyInterface>();
  return false;
}

bool LoopInfoExpr::isInductionVariable(PHINode *Phi) {
  if (!LI_->isLoopHeader(Phi->getParent()))
    return false;

  Loop *L = LI_->getLoopFor(Phi->getParent());
  PHINode *Indvar;
  Expr IndvarStart, IndvarEnd, IndvarStep;
  return getLoopInfo(L, Indvar, IndvarStart, IndvarEnd, IndvarStep) &&
         Phi == Indvar;
}

Loop *LoopInfoExpr::getLoopForInductionVariable(PHINode *Phi) {
  if (!LI_->isLoopHeader(Phi->getParent()))
    return nullptr;

  Loop *L = LI_->getLoopFor(Phi->getParent());
  PHINode *Indvar;
  Expr IndvarStart, IndvarEnd, IndvarStep;
  return getLoopInfo(L, Indvar, IndvarStart, IndvarEnd, IndvarStep) &&
         Phi == Indvar ? L : nullptr;
}

Expr LoopInfoExpr::getExprForLoop(Loop *L, Value *V) {
  if (L && L->isLoopInvariant(V))
    return Expr(V);
  else if (!L && !isa<Instruction>(V))
    return Expr(V);

  Instruction *I = cast<Instruction>(V);

  switch (I->getOpcode()) {
    case Instruction::Add:
      return getExprForLoop(L, I->getOperand(0)) +
             getExprForLoop(L, I->getOperand(1));
    case Instruction::Sub:
      return getExprForLoop(L, I->getOperand(0)) -
             getExprForLoop(L, I->getOperand(1));
    case Instruction::Mul:
      return getExprForLoop(L, I->getOperand(0)) *
             getExprForLoop(L, I->getOperand(1));
    case Instruction::SDiv:
    case Instruction::UDiv:
      return getExprForLoop(L, I->getOperand(0)) /
             getExprForLoop(L, I->getOperand(1));
    case Instruction::SExt:
    case Instruction::ZExt:
    case Instruction::Trunc:
      return getExprForLoop(L, I->getOperand(0));
    case Instruction::PHI: {
    	BasicBlock * phiBlock = I->getParent();
    	Loop *loop = LI_->getLoopFor(phiBlock);

    	if (!loop) {
    		return Expr(V);
    	}

    	if (loop->getHeader() == phiBlock) {
    		return Expr(V);
    	} else {
    		dbgs() << "[LIE] Work-around: trying to expand non-induction PHI-node\n";
    		PHINode * phi = dyn_cast<PHINode>(V);
    		uint numIncoming = phi->getNumIncomingValues();
    		Expr firstExpr = getExprForLoop(L, phi->getIncomingValue(0));
    		for (uint i = 1; i < numIncoming; ++i) {
    			Expr inExpr = getExprForLoop(L, phi->getIncomingValue(i));
    			if (firstExpr != inExpr) {
    				dbgs() << "[LIE] Non-trivial PHI-node usage out of loop header (Unsupported)..\n";
    				return Expr(V); // FIXME
    			}
    		}
    		return firstExpr;
    	}

    } break;
    default:
      return Expr(V);
  }
}

Expr LoopInfoExpr::getExpr(Value *V) {
  return getExprForLoop(nullptr, V);
}

bool LoopInfoExpr::
    getColocatedLoopInfo(Loop *L, PHINode *coPHI, Expr & coVarStart, Expr & coVarStep)
{
  assert(coPHI->getNumIncomingValues() == 2 && "only accept single entry&latch loops");
  PHINode * IndPHI;
  Expr IndStart, IndEnd, IndStep;
  if (! getLoopInfo(L, IndPHI, IndStart, IndEnd, IndStep)) {
    return false;
  }
  // coincides with the induction variable
  if (IndPHI == coPHI) {
      coVarStart = IndStart;
      coVarStep = IndStep;
      return true;    
  }
  
  Expr coTrip;
  bool foundCoStart = false, foundCoTrip = false;
  for (int i = 0; i < coPHI->getNumIncomingValues() && !(foundCoStart && foundCoTrip); ++i) {
      BasicBlock * inBlock = coPHI->getIncomingBlock(i);
      Loop * srcLoop = LI_->getLoopFor(inBlock);
      Value * inVal = coPHI->getIncomingValue(i);
      if (!srcLoop || L->getParentLoop() == srcLoop) {
          coVarStart = Expr(inVal);
          foundCoStart = true;          
      } else {
          coTrip = getExpr(inVal);
          foundCoTrip = true;
      }
  }
  FASAN_CHECK(foundCoStart && foundCoTrip, return false);
  
 // Determine the step size (over-approximate the step size)
    ExprMap Repls;
    Expr PhiEx(coPHI), Wild = Expr::WildExpr();
    if (!coTrip.match(PhiEx + Wild, Repls) || Repls.size() != 1 ||
            Repls[Wild].has(PhiEx)) {
      LIE_DEBUG(dbgs() << "LoopInfoExpr: could not determine accurate step\n");
      return false;
    }
    
    coVarStep = Repls[Wild];
    // compute coVarEnd from the main iteration variable
    // coVarEnd = coVarStart + ((IndEnd - IndStart) / IndStep) * coVarStep;
    
    return true;
}

bool LoopInfoExpr::
      getLoopInfo(Loop *L, PHINode *&Indvar, Expr &IndvarStart,
                  Expr &IndvarEnd, Expr &IndvarStep) {
  FASAN_CHECK(L,return false)

  BasicBlock *Exit = L->getExitingBlock();
  if (!Exit)
    return false;

  TerminatorInst *TI = L->getExitingBlock()->getTerminator();
  BranchInst *BI = dyn_cast<BranchInst>(TI);
  if (!BI)
    return false;

  ICmpInst *ICI = dyn_cast<ICmpInst>(BI->getCondition());
  if (!ICI)
    return false;

  unsigned PredicateCorrection;
  switch (ICI->getPredicate()) {
    case CmpInst::ICMP_SLT:
    case CmpInst::ICMP_SGT:
    case CmpInst::ICMP_ULT:
    case CmpInst::ICMP_UGT:
    case CmpInst::ICMP_SLE:
    case CmpInst::ICMP_SGE:
    case CmpInst::ICMP_ULE:
    case CmpInst::ICMP_UGE:
    case CmpInst::ICMP_EQ:
      break;
    default:
      LIE_DEBUG(dbgs() << "LoopInfoExpr: invalid loop comparison predicate (was " << *ICI << "\n");
      return false;
  }

  // Get the toplevel loop and use it to generate all lasting expressions.
  Loop *Toplevel = L;
  while (Toplevel->getParentLoop())
    Toplevel = Toplevel->getParentLoop();

  // Get the expression that gives the LHS. If all the atoms are loop-invariant
  // except for a single phi-node, RHS is loop-invariant, and the found phi-node
  // varies with a pattern i = i + <loop invariant expression>, then we've found
  // the induction variable.
  Expr LHS = getExprForLoop(Toplevel, ICI->getOperand(0));
  Expr RHS = getExprForLoop(Toplevel, ICI->getOperand(1));
  LIE_DEBUG(dbgs() << "LoopInfoExpr: toplevel LHS & RHS: " << LHS << ", " << RHS
                   << "\n");
  if (!LHS.isValid() || !RHS.isValid())
    return false;

  Expr Var, Invar;
  if (!IsLoopInvariant(L, LHS) && IsLoopInvariant(L, RHS)) {
    Var   = LHS;
    Invar = RHS;
  } else if (IsLoopInvariant(L, LHS) && !IsLoopInvariant(L, RHS)) {
    Var   = RHS;
    Invar = LHS;
  } else {
    LIE_DEBUG(dbgs() << "LoopInfoExpr: LHS & RHS have incorrect "
                        "loop-variance\n");
    return false;
  }
  
  if (PHINode *Phi = getSingleLoopVariantPhi(L, Var)) {
      assert(Phi->getParent() == L->getHeader() && "PHI-node in the path from loop header to CmpInst");

      LIE_DEBUG(dbgs() << "LIE:\n\tIndvar=" << *Phi << "\n\tchecked expr=" << Var << "\n\tboundary expr=" << Invar << "\n")

            
#if 1
#warning "Buildling with latch finder-hack"
      int latchIndex = Phi->getBasicBlockIndex(L->getLoopLatch());
      Value * LatchIncoming = Phi->getIncomingValue(latchIndex);
      int preheaderIndex = 1 - latchIndex;
      Value * PreheaderIncoming = Phi->getIncomingValue(preheaderIndex);
#else 
    Value
      *PreheaderIncoming = Phi->getIncomingValueForBlock(L->getLoopPreheader()),
      *LatchIncoming     = Phi->getIncomingValueForBlock(L->getLoopLatch());
#endif
    if (!L->isLoopInvariant(PreheaderIncoming) ||
        L->isLoopInvariant(LatchIncoming)) {
      LIE_DEBUG(dbgs() << "LoopInfoExpr: incoming value have incorrect"
                          " loop-variance\n");
      return false;
    }
    
#if 1
// Determine the offset at which "var" is checked in the CmpInst
    Expr varOffsetExpr;
    {
        ExprMap Repls;
        Expr PhiEx(Phi), Wild = Expr::WildExpr();
        if (!Var.match(PhiEx + Wild, Repls) || Repls.size() != 1 ||
                Repls[Wild].has(PhiEx)) {
          LIE_DEBUG(dbgs() << "LoopInfoExpr: could not determine checked expr\n");
          return false;
        }
        varOffsetExpr = Repls[Wild];
    }
#endif
    dbgs() << "[LIE] Checked offset " << varOffsetExpr << "\n";
    
// Determine the step size (change of var) in a single loop iteration
    {
        ExprMap Repls;
        Expr PhiEx(Phi), LatchIncomingEx = getExprForLoop(Toplevel, LatchIncoming),
                 Wild = Expr::WildExpr();
        if (!LatchIncomingEx.match(PhiEx + Wild, Repls) || Repls.size() != 1 ||
                Repls[Wild].has(PhiEx)) {
          LIE_DEBUG(dbgs() << "LoopInfoExpr: could not determine accurate step\n");
          return false;
        }
        IndvarStep = Repls[Wild];
    }

// Approximate the last feasible value (closest to the boundary)
    // FIXME this is dangerous as we do not check which branch is the loop exit    
    Indvar = Phi;   
    
    IndvarStart = getExprForLoop(Toplevel, PreheaderIncoming);
    switch (ICI->getPredicate()) {
      case CmpInst::ICMP_SLT:
      case CmpInst::ICMP_ULT:
        IndvarEnd = Invar - 1;
        break;
      case CmpInst::ICMP_SGT:
      case CmpInst::ICMP_UGT:
        IndvarEnd = Invar + 1;
        break;
      case CmpInst::ICMP_SLE:
      case CmpInst::ICMP_ULE:
      case CmpInst::ICMP_UGE:
      case CmpInst::ICMP_SGE:
        IndvarEnd = Invar;
        break;
      case CmpInst::ICMP_EQ:
    	  if (IndvarStep.isNegative()) {
    		  IndvarEnd = Invar + 1;
    	  } else {
    		  assert(IndvarStep.isPositive());
    		  IndvarEnd = Invar - 1;
    	  }
    	  break;

      default:
        LIE_DEBUG(dbgs() << "LoopInfoExpr: invalid loop comparison predicate\n");
        return false;
    }

  

    LIE_DEBUG(dbgs() << "LoopInfoExpr: induction variable, start, end, step: "
                     << *Indvar << " => (" << IndvarStart << ", " << IndvarEnd
                     << ", +" << IndvarStep << ")\n");
    return true;
  }

  return false;
}

/*
 * Simple loop
 * 
 * Properties:
 * - Single exit
 * - Exit is controlled by a single CmpInst
 * - CmpInst has one loop variant operand and one loop invariant operand
 * - Iteration state is representable by a single PHI-node in the header
 *
 * Expression tree leaves:
 * - loop header PHI-nodes
 * - completely loop invariant values
 * 
 * @loopExpr used header PHI-nodes after one iteration
 * @variantExitTerm loop variant operand of the CmpInst
 * @limitTerm loop invariant operand of the CmpInst
 * @headerPHI single PHI-node capturing the iteration state of the loop
 * 
 * @return true on success
 */
#if 0
bool LoopInfoExpr::getSimpleLoopStructure(Loop * L, PHINode &* headerPHI, Expr & loopExpr, Expr & variantExitTerm, Expr & limitTerm)
{
  BasicBlock * latchBlock = L->getLoopLatch();
  BasicBlock * headerBlock = L->getHeader();
  BasicBlock * exitBlock = L->getExitBlock();
  BasicBlock * exitingBlock = L->getExitingBlock();
  
  if (!latchBlock || ! headerBlock || !exitBlock || !exitingBlock) {
    dbgs() << "SimpleLoopStructure: latch,header,exit or exiting block missing!\n";
    return false;
  }
    
  // Check the exit branch
  Instruction * termInst = exitingBlock->getTerminator();
  BranchInst * branchInst = dyn_cast<BranchInst>(termInst);
  if (!branchInst) {
      return false;
  }
  
  branchInst->getOperand(0);
  branchInst->getOperand(1);
}
#endif

PHINode *LoopInfoExpr::getSingleLoopVariantPhi(Loop *L, Expr Ex) {
  auto Symbols = Ex.getSymbols();
  PHINode *Phi = nullptr;
  for (auto& Sym : Symbols) {
    Value *V = Sym.getSymbolValue();
    if (!L->isLoopInvariant(V)) {
      if (!isa<PHINode>(V) || Phi)
        return nullptr;
              
      PHINode * candPhi = cast<PHINode>(V);
      // FIXME requires a more sophisticated solution. only accept PHI-nodes in the loop header
      if (L->getHeader() == candPhi->getParent()) {
        Phi = candPhi;
      }
    }
  }
  return Phi;
}

