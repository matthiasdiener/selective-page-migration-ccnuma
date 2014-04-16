#include "RangedAddressSanitizer.h"

#include "llvm/Transforms/Instrumentation.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/CFG.h"
#if 0
#include "llvm/Linker.h"
#endif

#include "FAsanConfig.hpp"

#include <sstream>
#include <vector>

// Enables array re-use computatio (currently broken)
#if 0
#define ENABLE_REUSE
#endif

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

char RangedAddressSanitizer::ID = 0;
INITIALIZE_PASS(RangedAddressSanitizer, "spm", "Fast address sanitation in loop nests", false, false)

#if 0
#define SPM_DEBUG(X) { if (ClDebug) { X; } }
#else
#define SPM_DEBUG(X) { X; }
#endif

FunctionPass* llvm::createRangedAddressSanitizerPass()
{
	errs() << "Pass created!\n";
	return new RangedAddressSanitizer();
}

/* ************************************************************************** */
/* ************************************************************************** */
bool RangedAddressSanitizer::doInitialization(Module &M)
{
// Link FastAddressSanitizer functions into the target module
    LLVMContext & context = M.getContext();
    const char * fasanPath = getenv("FASANMODULE");
    
    if (! fasanPath) {
        return false;        
    }
    std::stringstream ss;
    ss << fasanPath;
    SMDiagnostic diag;
    Module * fasanModule = ParseIRFile(ss.str(), diag, context);

    if (!fasanModule) {
    	abort();
    }

#if 0 /* using LLVM linking facilities */
    Linker linker(&M);
    std::string linkErr;
    if (linker.linkInModule(fasanModule, Linker::DestroySource, &linkErr)) {
    	errs() << "[FASAN] Error while linking runtime module: " << fasanModule << "(!!)\n";
    	abort();
    }

#else
    PointerType * voidPtrTy = PointerType::getInt8PtrTy(context, 0);
    IntegerType * boolTy = IntegerType::get(context, 1);
    Type * voidTy = Type::getVoidTy(context);
    FunctionType * touchFunType = FunctionType::get(voidTy, ArrayRef<Type*>(voidPtrTy), false);
    FunctionType * verifyFunType = FunctionType::get(boolTy, ArrayRef<Type*>(voidPtrTy), false);

    ValueToValueMapTy reMap;
    reMap[fasanModule->getFunction("__fasan_touch")] = M.getOrInsertFunction("__fasan_touch", touchFunType);
    reMap[fasanModule->getFunction("__fasan_verify")] = M.getOrInsertFunction("__fasan_verify", verifyFunType);
    
    // migrate check function
    {
        std::string errMsg;
        Function * checkFunc = fasanModule->getFunction("__fasan_check");
        if (!checkFunc) {
                abort();
        }

#if 1
        FunctionType * checkFuncType = checkFunc->getFunctionType();
        Function * targetFunc = dyn_cast<Function>(M.getOrInsertFunction("__fasan_check", checkFuncType));
        assert(targetFunc && "function cast to const by getOrInsertFunc..?");

      // Loop over the arguments, copying the names of the mapped arguments over...
        Function::arg_iterator DestI = targetFunc->arg_begin();
        for (Function::const_arg_iterator I = checkFunc->arg_begin(), E = checkFunc->arg_end();
             I != E; ++I)
           if (reMap.count(I) == 0) {   // Is this argument preserved?
            DestI->setName(I->getName()); // Copy the name over...
            reMap[I] = DestI++;        // Add mapping to VMap
        }
        SmallVector<ReturnInst*, 8> Returns;  // Ignore returns cloned.
        CloneFunctionInto(targetFunc, checkFunc, reMap, false, Returns, "", nullptr);

        targetFunc->addAttribute(0,Attribute::SanitizeAddress);

#else
        Function * clonedCheckFunc = CloneFunction(checkFunc, reMap, false, 0);
        assert(!M.getFunction("__fasan_check") && "already exists in module");
        M.getFunctionList().push_back(clonedCheckFunc);

        ReuseFn_ = clonedCheckFunc;
        clonedCheckFunc->setLinkage(GlobalValue::InternalLinkage); // avoid conflicts during linking

        // re-map fake use to local copy
        for (auto & BB : *clonedCheckFunc) {
            for (auto & Inst : BB) {
                RemapInstruction(&Inst, reMap, RF_IgnoreMissingEntries, 0, 0);
            }
        }
#endif
#endif
    }
    delete fasanModule;
    return true;
}

bool RangedAddressSanitizer::doFinalization(Module &M)
{
    Function * checkFunc = M.getFunction("__fasan_check");
    if (checkFunc) {
    	for (Value::use_iterator itUse = checkFunc->use_begin(); itUse != checkFunc->use_end(); ++itUse) {
    		CallInst * call = dyn_cast<CallInst>(*itUse);
    	}

        checkFunc->eraseFromParent();
    }
    return true;

}

void RangedAddressSanitizer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<DataLayout>();
  AU.addRequired<DominatorTree>();
  AU.addRequired<LoopInfo>();
  AU.addRequired<ReduceIndexation>();  
#ifdef ENABLE_REUSE
  AU.addRequired<RelativeExecutions>();
#endif
  AU.addRequired<RelativeMinMax>();
  AU.addRequired<SymPyInterface>();
  // AU.setPreservesAll();
}

void RangedAddressSanitizer::ii_visitLoop(Loop * loop)
{
    for (Loop * childLoop : *loop) {
        ii_visitLoop(childLoop);
    }

    outs () << *loop;
    BasicBlock * header = loop->getHeader();
    for (auto & inst : *header) {
        if (auto * phi = dyn_cast<PHINode>(&inst)) {
            outs() << "\tIteration variable " << *phi << "\n";
            for (	Loop * parentLoop = loop;
                    parentLoop != 0;
                    parentLoop = parentLoop->getParentLoop()
            ) {
                Loop * currLoop = parentLoop->getParentLoop();
                Expr phiExpr(phi);
                Expr minExpr, maxExpr;
                if (RMM_->getMinMax(phiExpr, minExpr, maxExpr)) {
                    outs() << "\t\t[" << minExpr << ", " << maxExpr << "]\n";
                } else {
                    outs() << "\t\t<unknown>\n";
                }                
            }
        }
    }
}


void RangedAddressSanitizer::markSafeArrayUse(Instruction * inst, Value * array)
{
	Value * usedArray;
	Expr subscript;
	unsigned size;
#if 1
	if (reduceMemoryAccess(inst, usedArray, subscript, size)) {
		SPM_DEBUG( dbgs() << "FASan: will not check " << *inst << "\n" );
		safeUseSet.insert(inst);
	}
#else
	Value * ptr = 0;
	if (LoadInst * loadInst = dyn_cast<LoadInst>(&inst)) {
		ptr = loadInst->getPointerOperand();

	} else if (StoreInst * storeInst = dyn_cast<StoreInst>(&inst)) {
		ptr = storeInst->getPointerOperand();
	}
#endif
}

bool RangedAddressSanitizer::runOnFunction(Function &F) {
    if (getenv("FASAN_DISABLE")) {
      SPM_DEBUG( dbgs() << "FASan : disabled\n" );
      return false;
    }

    DL_  = &getAnalysis<DataLayout>();
    DT_  = &getAnalysis<DominatorTree>();
    LI_  = &getAnalysis<LoopInfo>();
    RI_  = &getAnalysis<ReduceIndexation>();
#ifdef ENABLE_REUSE
    RE_  = &getAnalysis<RelativeExecutions>();
#endif
    RMM_ = &getAnalysis<RelativeMinMax>();

    Module_  = F.getParent();
    Context_ = &Module_->getContext();

    Type        *VoidTy    = Type::getVoidTy(*Context_);
    IntegerType *IntTy     = IntegerType::getInt64Ty(*Context_);
    IntegerType *BoolTy     = IntegerType::getInt1Ty(*Context_);
    PointerType *IntPtrTy  = PointerType::getUnqual(IntTy);
    PointerType *VoidPtrTy = PointerType::getInt8PtrTy(*Context_);

    SPM_DEBUG( F.dump() );
    outs() << "[IterationInfo]\n";
    for (Loop * loop : *LI_) {
            ii_visitLoop(loop);
    }
    outs() << "[EndOfIterationInfo]\n";
    
#if 0 // disabled initialization,shutdown sequence for FASan
  if (F.getName() == "main") {
    SPM_DEBUG(dbgs() << "RangedAddressSanitizer: inserting hwloc calls into "
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
#endif

  if (!ClFunc.empty() && F.getName() != ClFunc) {
    SPM_DEBUG(dbgs() << "RangedAddressSanitizer: skipping function "
                     << F.getName() << "\n");
    return false;
  }

  Calls_.clear();

  SPM_DEBUG(dbgs() << "RangedAddressSanitizer: processing function "
                   << F.getName() << "\n");

  std::vector<Type*> ReuseFnFormals = { VoidPtrTy, IntTy, IntTy, IntTy };
  FunctionType *ReuseFnType = FunctionType::get(BoolTy, ReuseFnFormals, false);
  ReuseFn_ =
    F.getParent()->getOrInsertFunction("__fasan_check", ReuseFnType);
  ReuseFnDestroy_ =
    F.getParent()->getOrInsertFunction("__spm_give", ReuseFnType);

// Visit all loops in bottom-up order (innter-most loops first)
  std::set<BasicBlock*> Processed;
  auto Entry = DT_->getRootNode();
  for (auto ET = po_begin(Entry), EE = po_end(Entry); ET != EE; ++ET) {
    BasicBlock *Header = (*ET)->getBlock();

    if (LI_->isLoopHeader(Header)) {
      SPM_DEBUG(dbgs() << "RangedAddressSanitizer: processing loop at "
                       << Header->getName() << "\n");
      Loop *L = LI_->getLoopFor(Header);

      if (L->getNumBackEdges() != 1 ||
          std::distance(pred_begin(Header), pred_end(Header)) != 2) {
        SPM_DEBUG(dbgs() << "RangedAddressSanitizer: loop has multiple "
                         << "backedges or multiple incoming outer blocks\n");
        continue;
      }

      SPM_DEBUG(dbgs() << "RangedAddressSanitizer: processing loop at "
                       << Header->getName() << "\n");

    // visit all memory acccesses in this loop
      for (auto BB = L->block_begin(), BE = L->block_end(); BB != BE; ++BB) {
        if (!Processed.count(*BB)) {
          Processed.insert(*BB);
          for (auto &I : *(*BB))
            generateCallFor(L, &I);
        }
      }
    }
  }

  // FAsan logic goes here

  std::map<const BasicBlock*,BasicBlock*> clonedBlockMap; // keeps track of cloned regions to avoid redundant cloning

  std::vector<CallInst*> ToInline;

  for (auto &CI : Calls_) {
    BasicBlock * Preheader = CI.Preheader;
    
  // TODO decide whether it is worthwhile to optimize for this case

  // insert range check
    IRBuilder<> IRB(Preheader->getTerminator());
    Value *VoidArray = IRB.CreateBitCast(CI.Array, VoidPtrTy);
    std::vector<Value*> Args = { VoidArray, CI.Min, CI.Max, CI.Reuse };
    CallInst *CR = IRB.CreateCall(ReuseFn_, Args);
    ToInline.push_back(CR);
    
 // verify if this loop was already instrumented
    TerminatorInst * preHeaderTerm = CR->getParent()->getTerminator();
    BranchInst * preHeaderBranch = dyn_cast<BranchInst>(preHeaderTerm);

    if (preHeaderBranch && preHeaderBranch->isConditional()) {

    // discover the structure of the instrumented code (safe and default region)
    // abort, if this does not look like instrumented code
    	BasicBlock * firstTarget = preHeaderBranch->getSuccessor(0);
    	BasicBlock * secondTarget = preHeaderBranch->getSuccessor(1);
    	BasicBlock * safeHeader, * defHeader;
    	if (clonedBlockMap.count(firstTarget)) {
    		defHeader = firstTarget;
    		safeHeader = clonedBlockMap[firstTarget];
    		assert(safeHeader == secondTarget);
    	} else {
    		assert(clonedBlockMap.count(secondTarget));
    		defHeader = secondTarget;
			safeHeader = clonedBlockMap[secondTarget];
			assert(safeHeader == firstTarget);
    	}

    	SPM_DEBUG( dbgs() << "FASan: (Unsupported) second array in safe region controlled by " << * preHeaderBranch << "\n" );
    	Loop * defLoop = LI_->getLoopFor(defHeader);
    	assert(defLoop && "default region is not a loop!");

		Loop::block_iterator itBodyBlock,S,E;
		S = defLoop->block_begin();
		E = defLoop->block_end();

	// mark accesses in cloned region as safe
    	for (itBodyBlock = S;itBodyBlock != E; ++itBodyBlock) {
    		BasicBlock * defBodyBlock = *itBodyBlock;
    		BasicBlock * safeBodyBlock = clonedBlockMap[defBodyBlock];

    		for(auto & inst : *safeBodyBlock) {
				markSafeArrayUse(&inst, CI.Array);
			}
    	}

    // add conjunctive test
    	Value * oldCond = preHeaderBranch->getCondition();
    	Value * joinedCond = IRB.CreateAnd(oldCond, CR, "allsafe");
    	preHeaderBranch->setCondition(joinedCond);

    } else {

	  // get loop
		Loop* finalLoop = CI.FinalLoop;
		Loop::block_iterator itBodyBlock,S,E;
		S = finalLoop->block_begin();
		E = finalLoop->block_end();

	  // clone loop body (cloned loop will run unchecked)
		ValueToValueMapTy cloneMap;

		BasicBlock * clonedHeader = 0;
		std::vector<BasicBlock*> clonedBlocks;

		for (itBodyBlock = S;itBodyBlock != E; ++itBodyBlock) {

			const BasicBlock * bodyBlock = *itBodyBlock;
			BasicBlock * clonedBlock = CloneBasicBlock(bodyBlock, cloneMap, "_checked", &F, 0);

			cloneMap[bodyBlock] = clonedBlock;
			clonedBlockMap[bodyBlock] = clonedBlock;
			clonedBlocks.push_back(clonedBlock);

			if (bodyBlock == finalLoop->getHeader()) {
				clonedHeader = clonedBlock;
				SPM_DEBUG( dbgs() << "FASan: loop header case at " << bodyBlock->getName() << "\n" );
			} else {
				SPM_DEBUG( dbgs() << "FASan: non-header block at " << bodyBlock->getName() << "\n" );
			}
		}

		if (!clonedHeader) {
			// TODO run clean-up code
			SPM_DEBUG( dbgs() << "FASan: could not find header!\n");
			abort();
		}

	  // Remap uses inside cloned region (mark pointers in the region as unguarded)
		for (BasicBlock * block : clonedBlocks) {
			for(auto & inst : *block) {
				RemapInstruction(&inst, cloneMap, RF_IgnoreMissingEntries);
				markSafeArrayUse(&inst, CI.Array);
			}
		}

	   // TODO fix PHI-nodes in exit blocks

	   // Rewire terminator of the range check to branch to the cloned region
		TerminatorInst * checkTermInst = CR->getParent()->getTerminator();

		if (BranchInst * checkBranchInst = dyn_cast<BranchInst>(checkTermInst)) {
			if (checkBranchInst->isUnconditional()) {
				BasicBlock * defTarget = checkBranchInst->getSuccessor(0);
				BranchInst * modifiedBranchInst = BranchInst::Create(clonedHeader, defTarget, CR, checkBranchInst);
				checkBranchInst->replaceAllUsesWith(modifiedBranchInst);
				checkBranchInst->eraseFromParent();
			} else {
				SPM_DEBUG( dbgs() << "FASan: Unexpected conditional branch (preheader should branch unconditional, other array checks will introduce conditional branches) " << * checkTermInst << "\n" );
				abort();
			}
		} else {
			SPM_DEBUG( dbgs() << "FASan: unsupported terminator type " << * checkTermInst << "\n" );
			abort();
		}
    }
    
#if 0
    IRB.SetInsertPoint(&(*CI.Final->begin()));
    IRB.CreateCall(ReuseFnDestroy_, Args);
#endif
    SPM_DEBUG(dbgs() << "RangedAddressSanitizer: call instruction: " << *CR
                     << "\n");
  }


  // inline calls
#ifdef FASAN_INLINE_RUNTIME
  for (CallInst * call : ToInline) {
	assert(call);
	InlineFunctionInfo IFI;
	InlineFunction(call, IFI, false);
  }
#endif

  SPM_DEBUG( F.dump() );
  return true;
}

bool RangedAddressSanitizer::reduceMemoryAccess(Instruction * I, Value *& oArray, Expr & oSubscript, unsigned & oSize)
{
  if (LoadInst * loadInst = dyn_cast<LoadInst>(I)) {
    if (!RI_->reduceLoad(loadInst, oArray, oSubscript)) {
      SPM_DEBUG(dbgs() << "RangedAddressSanitizer: could not reduce load "
                        << *I << "\n");
      return false;
    }
    oSize = DL_->getTypeAllocSize(I->getType());
    SPM_DEBUG(dbgs() << "RangedAddressSanitizer: reduced load " << *I
                     << " to: " << *oArray  << " + " << oSubscript << "\n");
      return true;
  } else if (StoreInst * storeInst = dyn_cast<StoreInst>(I)) {
    if (!RI_->reduceStore(storeInst, oArray, oSubscript)) {
      SPM_DEBUG(dbgs() << "RangedAddressSanitizer: could not reduce store "
                        << *I << "\n");
      return false;
    }
    oSize = DL_->getTypeAllocSize(I->getOperand(0)->getType());
    SPM_DEBUG(dbgs() << "RangedAddressSanitizer: reduced store " << *I
                     << " to: " << *oArray  << " + " << oSubscript << "\n");
      return true;
  }

  return false;
}

bool RangedAddressSanitizer::generateCallFor(Loop *L, Instruction *I) {
  if (!isa<LoadInst>(I) && !isa<StoreInst>(I))
    return false;

// Reduce memory access to array base pointer + offset
  Value *Array;
  unsigned Size;
  Expr Subscript;
  if (!reduceMemoryAccess(I, Array, Subscript, Size)) {
     return false;
  }

#ifdef ENABLE_REUSE
  Loop *Final;
  Expr ReuseEx = RE_->getExecutionsRelativeTo(L, nullptr, Final);
  if (!ReuseEx.isValid()) {
    SPM_DEBUG(dbgs() << "RangedAddressSanitizer: could not calculate reuse for "
                        "loop " << L->getHeader()->getName() << "\n");
    return false;
  }
  SPM_DEBUG(dbgs() << "RangedAddressSanitizer: reuse of "
                   << L->getHeader()->getName() << " relative to "
                   << Final->getHeader()->getName() << ": " << ReuseEx << "\n");
#else /* ! ENABLE_REUSE */
  // TODO find Final loop
  Loop * Final = L; // FIXME
  for (;Final->getParentLoop() != nullptr; Final = Final->getParentLoop()) {}
#endif
  
  BasicBlock *Preheader = Final->getLoopPreheader();
  typedef GraphTraits< BasicBlock * > CFG;
  typedef GraphTraits< Inverse< BasicBlock * > > InverseCFG;
  // typedef InverseCFG::pred_iterator pred_iterator;
  
  if (!Preheader) {
    SPM_DEBUG(dbgs() << "RangedAddressSanitizer: trying to recover pre-header\n" );
    BasicBlock * header = Final->getHeader();
    for (pred_iterator itPred = InverseCFG::child_begin(header); itPred != InverseCFG::child_end(header); ++itPred)
    {
        BasicBlock * pred = *itPred;
        if (! LI_->getLoopFor(pred)) {
            Preheader = pred;
            break;
        }
    }    
  }
  
  assert(Preheader && "could not find nor recover pre-header");
  BasicBlock *Exit      = Final->getExitBlock();
  if (!Exit) {
	  errs() << "[ERROR] Non unique exit block in loop. Leaving loop uninstrumented " << *L << "\n";
	  return false;

  }
  assert(Exit && "loop w/o unique exit block");
  
  // FIXME: instead of bailing, we should set the toplevel loop in the call to
  // getExecutionsRelativeTo.
  if (Instruction *AI = dyn_cast<Instruction>(Array)) {
    if (!DT_->dominates(AI->getParent(), Preheader) &&
         AI->getParent() != Preheader) {
      SPM_DEBUG(dbgs() << "RangedAddressSanitizer: array does not dominate "
                          "loop preheader\n");
      return false;
    }
  }

// Query array offset range
  Expr MinEx, MaxEx;
  if (!RMM_->getMinMax(Subscript, MinEx, MaxEx)) {
    SPM_DEBUG(dbgs() << "RangedAddressSanitizer: could calculate min/max for "
                        " subscript " << Subscript << "\n");
    return false;
  }
  SPM_DEBUG(dbgs() << "RangedAddressSanitizer: min/max for subscript "
                   << Subscript << ": " << MinEx << ", " << MaxEx << "\n");

// Materialize expressions in the loop header
  IRBuilder<> IRB(Preheader->getTerminator());
#ifdef ENABLE_REUSE
  Value *Reuse = (ReuseEx * Size).getExprValue(64, IRB, Module_);
#else
  Value *Reuse = ConstantInt::get(IntegerType::get(IRB.getContext(), 64), 0); // bogus
#endif
  Value *Min   = MinEx.getExprValue(64, IRB, Module_);
  Value *Max   = MaxEx.getExprValue(64, IRB, Module_);

  SPM_DEBUG(dbgs() << "RangedAddressSanitizer: values for reuse, min, max: "
                   << *Reuse << ", " << *Min << ", " << *Max << "\n");

// If there is already a range check for this array and loop cached, merge the intervals
  CallInfo CI = { Final, Preheader, Exit, Array, Min, Max, Reuse };
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

