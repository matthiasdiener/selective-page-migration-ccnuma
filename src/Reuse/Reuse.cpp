//===---------------------------- Reuse.cpp -----------------------------===//
//===----------------------------------------------------------------------===//

#include "Reuse.h"

#include "llvm/Support/CommandLine.h"

/* ************************************************************************** */
/* ************************************************************************** */

using namespace llvm;

static cl::opt<bool>
  ClDebug("reuse-debug",
          cl::desc("Enable debugging for the reuse calculation pass"),
          cl::Hidden, cl::init(false));

char Reuse::ID = 0;
static RegisterPass<Reuse>
  X("reuse", "Runtime array reuse calculation");

#define GENEXPR_DEBUG(X) { if (ClDebug) { X; } }

/* ************************************************************************** */
/* ************************************************************************** */

// Generates the given Expr with the given IRBuilder.
static Value *InsertExprForIRB(RExpr Ex, IRBuilder<> &IRB, LLVMContext &C,
                               unsigned BitWidth = 64) {
  // Stop recursion when the expression is a single atom - a symbol or a
  // constant.
  if (Ex.isSymbol() || Ex.isConstant()) {
    return Ex.getValue(BitWidth, C, IRB);
  } else {
    bool IsAdd = Ex.isAdd(), IsMul = Ex.isMul();
    if (IsAdd || IsMul) {
      // The accumulator will keep track of the last generated expression, to be
      // used in the next iteration.
      Value *Acc = nullptr;
      for (auto EI : Ex) {
        Value *Curr = InsertExprForIRB(EI, IRB, C);
        Acc = Acc ? IsAdd ? IRB.CreateAdd(Acc, Curr) : IRB.CreateMul(Acc, Curr)
                  : Curr;
      }
      return Acc;
    }
  }
  return nullptr;
}

/* ************************************************************************** */
/* ************************************************************************** */

/*********
 * Reuse *
 *********/
// getAnalysisUsage
void Reuse::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfo>();
  AU.addRequired<DominatorTree>();
  AU.addRequired<TripCountAnalysis>();
  AU.setPreservesAll();
}

// runOnFunction
bool Reuse::runOnFunction(Function &F) {
  LI_ = &getAnalysis<LoopInfo>();
  DT_ = &getAnalysis<DominatorTree>();
  TCA_ = &getAnalysis<TripCountAnalysis>();

  LLVMContext &C = F.getContext();

  // Formal parameters and function type of the reuse function
  // (__pact_reuse_{add, sub, mul}).
  Type *VoidPtrTy = PointerType::getInt8PtrTy(C),
       *IntTy     = IntegerType::getInt64Ty(C);
  std::vector<Type*> ReuseFnFormals = { VoidPtrTy, IntTy, IntTy, IntTy, IntTy };
  FunctionType *ReuseFnType = FunctionType::get(Type::getVoidTy(C),
                                                ReuseFnFormals, false);

  for (auto &BB : F) {
    if (LI_->isLoopHeader(&BB)) {
      GENEXPR_DEBUG(dbgs() << "Reuse: ==== NEXT LOOP ====\n");
      // Attempt to get the loop's trip-count.
      Value *Trip = TCA_->getTripCount(&BB);
      if (!Trip) {
        GENEXPR_DEBUG(dbgs() << "Reuse: unable to infer a valid trip-count "
                                "for header: " << BB.getName() << "\n");
        continue;
      }
      GENEXPR_DEBUG(dbgs() << "Reuse: trip-count is: " << *Trip << "\n");

      // Only function on loops with a single backedge and one incoming outer
      // basic block. This means no breaks and a single possible initial value
      // for the induction variable.
      // Note: running -break-crit-edges and -simplifycfg might help with the
      // incoming edges.
      Loop *L = LI_->getLoopFor(&BB);
      if (L->getNumBackEdges() != 1 ||
          std::distance(pred_begin(&BB), pred_end(&BB)) != 2) {
        GENEXPR_DEBUG(dbgs() << "Reuse: loop has multiple backedges or "
                             << "multiple incoming outer blocks\n");
        continue;
      }
      GENEXPR_DEBUG(dbgs() << "Reuse: loop has a single backedge and "
                           << "a single incoming outer block\n");

      // Get the (at this point) unique backedge.
      BasicBlock *BackEdge =
        *GraphTraits< Inverse<BasicBlock*> >::child_begin(&BB);
      GENEXPR_DEBUG(dbgs() << "Reuse: backedge is: '" << BackEdge->getName()
                           << "' (" << BackEdge << ")\n");

      // Gathers all GEPs inside the loop body.
      // TODO: Might not work for multiple nested loops.
      // TODO: We currently assume GEPs are immediately before a load/store.
      std::vector<Instruction*> GEPs;
      for (auto BI = L->block_begin(), BE = L->block_end(); BI != BE; ++BI)
        for (auto& I : **BI)
          if (isa<GetElementPtrInst>(I)) {
            GENEXPR_DEBUG(dbgs() << "Reuse: found GEP: " << I << "\n");
            GEPs.push_back(&I);
          }

      // Iterate through the phi-nodes.
      PHINode *Indvar;
      for (auto I = BB.begin(); (Indvar = dyn_cast<PHINode>(I)); ++I) {
        GENEXPR_DEBUG(dbgs() << "Reuse: checking phi-node: "
                             << *Indvar << "\n");

        // Index of the backedge block in the phi-node.
        unsigned BackEdgeIdx = Indvar->getIncomingBlock(0) == BackEdge;

        // Get the starting value of the phi-node and the value after each
        // iteration.
        Value *IndvarStart = Indvar->getIncomingValue(!BackEdgeIdx),
              *Step        = Indvar->getIncomingValue(BackEdgeIdx);
        GENEXPR_DEBUG(dbgs() << "Reuse: starting value, step value: "
                             << *IndvarStart << ", " << *Step << "\n");

        // Try to generate a step function for the (supposed) induction
        // variable.
        RExpr StepFn = genExprFromUntil(Step, Indvar, L);
        if (!StepFn.isValid()) {
          GENEXPR_DEBUG(dbgs() << "Reuse: invalid step function\n");
          continue;
        }
        GENEXPR_DEBUG(dbgs() << "Reuse: step function is: " << StepFn << "\n");

        // Compose the step function the amount of times the loop executes.
        RExpr StepCmps = composeLoopExpr(StepFn, Indvar, IndvarStart, Trip);
        if (!StepCmps.isValid()) {
          GENEXPR_DEBUG(dbgs() << "Reuse: invalid step function "
                                  "composition\n");
          continue;
        }
        GENEXPR_DEBUG(dbgs() << "Reuse: step function composition is: "
                             << StepCmps << "\n");

        // Applying the composed step function to the initial value of the
        // induction variable gives the its final value.
        RExpr IndvarEnd = StepCmps.subs(Indvar, IndvarStart);
        GENEXPR_DEBUG(dbgs() << "Reuse: induction variable's final value is: "
                             << IndvarEnd << "\n");

        // Get the appropriate RT reuse function for the step.
        RExpr Wild = RExpr::WildExpr();
        RExprMap Map;
        StringRef ReuseFnName =
          StepFn.match(RExpr(Indvar) + Wild, Map) ? "__pact_reuse_add" :
          StepFn.match(RExpr(Indvar) - Wild, Map) ? "__pact_reuse_sub" :
          StepFn.match(RExpr(Indvar) * Wild, Map) ? "__pact_reuse_mul" : "";

        if (ReuseFnName.empty()) {
          GENEXPR_DEBUG(dbgs() << "Reuse: could not match step function\n");
          continue;
        }
        GENEXPR_DEBUG(dbgs() << "Reuse: reuse function is: " << ReuseFnName
                             << "\n");

        // Get or insert the reuse function into the module.
        Constant *ReuseFn = F.getParent()->getOrInsertFunction(ReuseFnName,
                                                               ReuseFnType);

        RExpr StepRhs = Map[Wild];
        GENEXPR_DEBUG(dbgs() << "Reuse: step is: " << StepRhs << "\n");

        for (auto &GEP : GEPs) {
          // TODO: Handle GEPs with more than two operands.
          if (GEP->getNumOperands() == 2) {
            GENEXPR_DEBUG(dbgs() << "Reuse: visiting GEP: " << *GEP << "\n");

            Value *Array = GEP->getOperand(0);

            // Generate an expression for the subscript, reaching up to the
            // induction variable.
            Value *Subscript = GEP->getOperand(1);
            RExpr SubscriptFn = genExprFromUntil(Subscript, Indvar, L);
            if (!SubscriptFn.isValid() || !SubscriptFn.has(Indvar)) {
              GENEXPR_DEBUG(dbgs() << "Reuse: invalid subscript function: "
                                   << SubscriptFn << "\n");
              continue;
            }
            GENEXPR_DEBUG(dbgs() << "Reuse: subscript function is: "
                                 << SubscriptFn << "\n");

            RExpr SubscriptStartExpr = SubscriptFn.subs(Indvar, IndvarStart);
            RExpr SubscriptEndExpr   = SubscriptFn.subs(Indvar, IndvarEnd);
            GENEXPR_DEBUG(dbgs() << "Reuse: subscript start: "
                                 << SubscriptStartExpr << "\n");
            GENEXPR_DEBUG(dbgs() << "Reuse: subscript end: "
                                 << SubscriptEndExpr << "\n");

            // TODO: consider replacing other induction variables with their
            // know bounds. This would let us hoist more function calls at
            // at the cost of (probably small) precision.

            // Place a function call with the following information:
            // * the array being accessed;
            // * the first index accessed;
            // * the last index accessed;
            // * the step between accesses;
            // * a multiplier indicating how many times the access will be
            //   executed.

            IRBuilder<>
              IRB(&(*Indvar->getIncomingBlock(!BackEdgeIdx)->rbegin()++));

            Loop *Outter = L;
            Value *Multiplyer = ConstantInt::get(IntTy, 1, true);
            while (Outter->getParentLoop()) {
              Value *OutterTrip = TCA_->getTripCount(Outter->getHeader());
              Multiplyer = Outter == L ? OutterTrip
                                       : IRB.CreateMul(Multiplyer, OutterTrip);
              Outter = Outter->getParentLoop();
            }
            GENEXPR_DEBUG(dbgs() << "Reuse: created multiplier: "
                                 << *Multiplyer << "\n");

            Value *SubscriptStartVal =
              InsertExprForIRB(SubscriptStartExpr, IRB, C);
            Value *SubscriptEndVal = InsertExprForIRB(SubscriptEndExpr, IRB, C);
            Value *StepVal = InsertExprForIRB(StepRhs, IRB, C);

            GENEXPR_DEBUG(dbgs() << "Reuse: subscript start/end, "
                                 << "step: " << *SubscriptStartVal
                                 << ", " << *SubscriptEndVal << ", "
                                 << *StepVal << "\n");

            Value *VoidArray = IRB.CreateBitCast(Array, VoidPtrTy);

            std::vector<Value*> Args = {
              VoidArray, SubscriptStartVal, SubscriptEndVal, StepVal, Multiplyer
            };
            CallInst *CI = IRB.CreateCall(ReuseFn, Args);
            GENEXPR_DEBUG(dbgs() << "Reuse: created call: " << *CI << "\n");
          }
        }
      }
    }
  }

  return true;
}

// getExprFromUntil
// Generates a step function for a given loop. This function gives the
// per-iteration variation of the induction variable.
RExpr Reuse::genExprFromUntil(Value *From, PHINode *Until, Loop *L) {
  // Reached the final instruction.
  if (From == Until) {
    return RExpr(Until);
  }
  // Value is a loop invariant, can insert as a symbol.
  else if (L->isLoopInvariant(From))
    return RExpr(From);
  // Value is not an instruction.
  else if (!isa<Instruction>(From))
    return RExpr::InvalidExpr();

  Instruction *FromI = cast<Instruction>(From);

  // Make sure we haven't reached an instruction before the final.
  BasicBlock *FromBB = FromI->getParent(), *UntilBB = Until->getParent();
  if ((FromBB == UntilBB && isa<PHINode>(FromI)) ||
      DT_->dominates(FromBB, UntilBB))
    return RExpr::InvalidExpr();

  switch (FromI->getOpcode()) {
    case Instruction::Add:
      return genExprFromUntil(FromI->getOperand(0), Until, L) +
             genExprFromUntil(FromI->getOperand(1), Until, L);
    case Instruction::Sub:
      return genExprFromUntil(FromI->getOperand(0), Until, L) -
             genExprFromUntil(FromI->getOperand(1), Until, L);
    case Instruction::Mul:
      return genExprFromUntil(FromI->getOperand(0), Until, L) *
             genExprFromUntil(FromI->getOperand(1), Until, L);
    case Instruction::SExt:
    case Instruction::ZExt:
      return genExprFromUntil(FromI->getOperand(0), Until, L);
    default:
      return RExpr::InvalidExpr();
  }
}

// composeLoopExpr
// Composes a given step function in a loop with the given induction
// variable, starting value, and the loop's trip count.
RExpr Reuse::composeLoopExpr(RExpr Ex, PHINode *Indvar, Value *Start,
                              Value *Times) {
  RExprMap Repls;
  RExpr Wild = RExpr::WildExpr(), IndvarExpr = RExpr(Indvar),
       StartExpr = RExpr(Start), TimesExpr = RExpr(Times);

  // +
  if (Ex.match(IndvarExpr + Wild, Repls) && Repls.size() == 1) {
    // TODO: Check if Repls[Wild] contains IndvarExpr.
    //       AFAIK, though, it's not possible - must check...
    return IndvarExpr + Repls[Wild] * TimesExpr;
  }
  // TODO: *, -

  return RExpr::InvalidExpr();
}

