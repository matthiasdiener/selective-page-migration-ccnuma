/*
 * TripCountGenerator.h
 *
 *  Created on: Dec 10, 2013
 *      Author: raphael
 */

#ifndef TripCountGenerator_H_
#define TripCountGenerator_H_

#include "LoopInfoEx.h"
#include "ExitInfo.h"
#include "LoopControllersDepGraph.h"
#include "LoopNormalizerAnalysis.h"
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include <inttypes.h>
#include <map>
#include <set>
#include <stack>
#include <string>

namespace llvm {

	class TripCountGenerator: public FunctionPass {
	private:
		void MarkAsTripCount(Instruction& inst);
		bool IsTripCount(Instruction& inst);
	public:
		static char ID;

		llvm::LLVMContext* context;

		TripCountGenerator(): FunctionPass(ID), context(NULL) {};
		~TripCountGenerator(){};

		virtual void getAnalysisUsage(AnalysisUsage &AU) const{

			AU.addRequired<LoopInfoEx>();

			AU.addRequired<LoopControllersDepGraph>();
			AU.addRequired<LoopNormalizerAnalysis>();

		}

		virtual bool doInitialization(Module &M);

		bool runOnFunction(Function &F);

		Value* generatePericlesEstimatedTripCount(BasicBlock* header, BasicBlock* entryBlock, Value* Op1, Value* Op2, CmpInst* CI);

		Value* getValueAtEntryPoint(Value* source, BasicBlock* loopHeader);

		BasicBlock* findLoopControllerBlock(Loop* l);

	};

}

#endif
