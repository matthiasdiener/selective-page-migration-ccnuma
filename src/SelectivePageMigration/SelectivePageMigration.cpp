#include "SelectivePageMigration.h"

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#include <vector>

using namespace llvm;

/* ************************************************************************** */
/* ************************************************************************** */

static cl::opt<bool>
  ClDebug("spm-debug",
          cl::desc("Enable debugging for the selective page migration "
                   "transformation"),
          cl::Hidden, cl::init(false));

static cl::opt<std::string>
  ClFunc("spm-pthread-function",
         cl::desc("Only analyze/transform the given function"),
         cl::Hidden, cl::init(""));

static RegisterPass<SelectivePageMigration>
  X("spm", "ccNUMA selective page migration transformation");
char SelectivePageMigration::ID = 0;

#define SPM_DEBUG(X) { if (ClDebug) { X; } }

/* ************************************************************************** */
/* ************************************************************************** */

void SelectivePageMigration::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<DataLayout>();
  AU.addRequired<DominatorTree>();
  AU.addRequired<LoopInfo>();
  AU.addRequired<ReduceIndexation>();
  AU.addRequired<RelativeExecutions>();
  AU.addRequired<RelativeMinMax>();
  AU.addRequired<SymPyInterface>();
  AU.setPreservesAll();
}

bool SelectivePageMigration::runOnFunction(Function &F) {
  DL_  = &getAnalysis<DataLayout>();
  DT_  = &getAnalysis<DominatorTree>();
  LI_  = &getAnalysis<LoopInfo>();
  RI_  = &getAnalysis<ReduceIndexation>();
  RE_  = &getAnalysis<RelativeExecutions>();
  RMM_ = &getAnalysis<RelativeMinMax>();

  Module_  = F.getParent();
  Context_ = &Module_->getContext();

  Type        *VoidTy    = Type::getVoidTy(*Context_);
  IntegerType *IntTy     = IntegerType::getInt64Ty(*Context_);
  PointerType *IntPtrTy  = PointerType::getUnqual(IntTy);
  PointerType *VoidPtrTy = PointerType::getInt8PtrTy(*Context_);

  if (F.getName() == "main") {
    SPM_DEBUG(dbgs() << "SelectivePageMigration: inserting hwloc calls into "
                        "main function\n");

    FunctionType *FnType = FunctionType::get(VoidTy, ArrayRef<Type*>(), false);
    IRBuilder<> IRB(&(*F.getEntryBlock().begin()));

    Constant *Init = Module_->getOrInsertFunction("__spm_init", FnType);
    IRB.CreateCall(Init);

    Constant *End = Module_->getOrInsertFunction("__spm_end", FnType);
    for (auto &BB : F) {
      TerminatorInst *TI = BB.getTerminator();
      if (isa<ReturnInst>(TI)) {
        IRB.SetInsertPoint(TI);
        IRB.CreateCall(End);
      }
    }
  }

  if (!ClFunc.empty() && F.getName() != ClFunc) {
    SPM_DEBUG(dbgs() << "SelectivePageMigration: skipping function "
                     << F.getName() << "\n");
    return false;
  }

  Calls_.clear();

  SPM_DEBUG(dbgs() << "SelectivePageMigration: processing function "
                   << F.getName() << "\n");

  std::vector<Type*> ReuseFnFormals = { VoidPtrTy, IntTy, IntTy, IntTy };
  FunctionType *ReuseFnType = FunctionType::get(VoidTy, ReuseFnFormals, false);
  ReuseFn_ =
    F.getParent()->getOrInsertFunction("__spm_get", ReuseFnType);
  ReuseFnDestroy_ =
    F.getParent()->getOrInsertFunction("__spm_give", ReuseFnType);

  std::set<BasicBlock*> Processed;
  auto Entry = DT_->getRootNode();
  for (auto ET = po_begin(Entry), EE = po_end(Entry); ET != EE; ++ET) {
    BasicBlock *Header = (*ET)->getBlock();

    if (LI_->isLoopHeader(Header)) {
      SPM_DEBUG(dbgs() << "SelectivePageMigration: processing loop at "
                       << Header->getName() << "\n");
      Loop *L = LI_->getLoopFor(Header);

      if (L->getNumBackEdges() != 1 ||
          std::distance(pred_begin(Header), pred_end(Header)) != 2) {
        SPM_DEBUG(dbgs() << "SelectivePageMigration: loop has multiple "
                         << "backedges or multiple incoming outer blocks\n");
        continue;
      }

      SPM_DEBUG(dbgs() << "SelectivePageMigration: processing loop at "
                       << Header->getName() << "\n");

      for (auto BB = L->block_begin(), BE = L->block_end(); BB != BE; ++BB) {
        if (!Processed.count(*BB)) {
          Processed.insert(*BB);
          for (auto &I : *(*BB))
            generateCallFor(L, &I);
        }
      }
    }
  }

  for (auto &CI : Calls_) {
    IRBuilder<> IRB(CI.Preheader->getTerminator());
    Value *VoidArray = IRB.CreateBitCast(CI.Array, VoidPtrTy);
    std::vector<Value*> Args = { VoidArray, CI.Min, CI.Max, CI.Reuse };
    CallInst *CR = IRB.CreateCall(ReuseFn_, Args);
    IRB.SetInsertPoint(&(*CI.Final->begin()));
    IRB.CreateCall(ReuseFnDestroy_, Args);
    SPM_DEBUG(dbgs() << "SelectivePageMigration: call instruction: " << *CR
                     << "\n");
  }

  return false;
}

bool SelectivePageMigration::generateCallFor(Loop *L, Instruction *I) {
  if (!isa<LoadInst>(I) && !isa<StoreInst>(I))
    return false;

  Value *Array;
  unsigned Size;
  Expr Subscript;
  if (isa<LoadInst>(I)) {
    if (!RI_->reduceLoad(cast<LoadInst>(I), Array, Subscript)) {
      SPM_DEBUG(dbgs() << "SelectivePageMigration: could not reduce load "
                        << *I << "\n");
      return false;
    }
    Size = DL_->getTypeAllocSize(I->getType());
    SPM_DEBUG(dbgs() << "SelectivePageMigration: reduced load " << *I
                     << " to: " << *Array  << " + " << Subscript << "\n");
  } else {
    if (!RI_->reduceStore(cast<StoreInst>(I), Array, Subscript)) {
      SPM_DEBUG(dbgs() << "SelectivePageMigration: could not reduce store "
                        << *I << "\n");
      return false;
    }
    Size = DL_->getTypeAllocSize(I->getOperand(0)->getType());
    SPM_DEBUG(dbgs() << "SelectivePageMigration: reduced store " << *I
                     << " to: " << *Array  << " + " << Subscript << "\n");
  }

  Loop *Final;
  Expr ReuseEx = RE_->getExecutionsRelativeTo(L, nullptr, Final);
  if (!ReuseEx.isValid()) {
    SPM_DEBUG(dbgs() << "SelectivePageMigration: could not calculate reuse for "
                        "loop " << L->getHeader()->getName() << "\n");
    return false;
  }
  SPM_DEBUG(dbgs() << "SelectivePageMigration: reuse of "
                   << L->getHeader()->getName() << " relative to "
                   << Final->getHeader()->getName() << ": " << ReuseEx << "\n");

  BasicBlock *Preheader = Final->getLoopPreheader();
  BasicBlock *Exit      = Final->getExitBlock();

  // FIXME: instead of bailing, we should set the toplevel loop in the call to
  // getExecutionsRelativeTo.
  if (Instruction *AI = dyn_cast<Instruction>(Array)) {
    if (!DT_->dominates(AI->getParent(), Preheader) &&
         AI->getParent() != Preheader) {
      SPM_DEBUG(dbgs() << "SelectivePageMigration: array does not dominate "
                          "loop preheader\n");
      return false;
    }
  }

  Expr MinEx, MaxEx;
  if (!RMM_->getMinMax(Subscript, MinEx, MaxEx)) {
    SPM_DEBUG(dbgs() << "SelectivePageMigration: could calculate min/max for "
                        " subscript " << Subscript << "\n");
    return false;
  }
  SPM_DEBUG(dbgs() << "SelectivePageMigration: min/max for subscript "
                   << Subscript << ": " << MinEx << ", " << MaxEx << "\n");

  IRBuilder<> IRB(Final->getLoopPreheader()->getTerminator());
  Value *Reuse = (ReuseEx * Size).getExprValue(64, IRB, Module_);
  Value *Min   = MinEx.getExprValue(64, IRB, Module_);
  Value *Max   = MaxEx.getExprValue(64, IRB, Module_);

  SPM_DEBUG(dbgs() << "SelectivePageMigration: values for reuse, min, max: "
                   << *Reuse << ", " << *Min << ", " << *Max << "\n");

  CallInfo CI = { Preheader, Exit, Array, Min, Max, Reuse };
  auto Call = Calls_.insert(CI);
  if (!Call.second) {
    IRBuilder<> IRB(Preheader->getTerminator());
    CallInfo SCI = *Call.first;

    Value *CmpMin = IRB.CreateICmp(CmpInst::ICMP_SLT, SCI.Min, CI.Min);
    SCI.Min = IRB.CreateSelect(CmpMin, SCI.Min, CI.Min);

    Value *CmpMax = IRB.CreateICmp(CmpInst::ICMP_SGT, SCI.Max, CI.Max);
    SCI.Max = IRB.CreateSelect(CmpMax, SCI.Max, CI.Max);

    SCI.Reuse = IRB.CreateAdd(SCI.Reuse, CI.Reuse);

    Calls_.erase(SCI);
    Calls_.insert(SCI);
  }

  return true;
}

