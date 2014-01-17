/*
 * TripCountProfiler.cpp
 *
 *  Created on: Dec 10, 2013
 *      Author: raphael
 */

#ifndef DEBUG_TYPE
#define DEBUG_TYPE "TripCountProfiler"
#endif

#include "TripCountProfiler.h"

STATISTIC(NumInstrumentedLoops, "Number of Instrumented Loops");

STATISTIC(NumIntervalLoops, "Number of Interval Loops");
STATISTIC(NumEqualityLoops, "Number of Equality Loops");
STATISTIC(NumOtherLoops,    "Number of Other Loops");

STATISTIC(NumUnknownConditionsIL, "Number of Interval Loops With Unknown TripCount");
STATISTIC(NumUnknownConditionsEL, "Number of Equality Loops With Unknown TripCount");
STATISTIC(NumUnknownConditionsOL, "Number of Other Loops With Unknown TripCount");

STATISTIC(NumIncompatibleOperandTypes, "Number of Loop Conditions With Incompatible Operands");


using namespace llvm;


AllocaInst* insertAlloca(BasicBlock *BB, Constant* initialValue) {
  Type* Ty = Type::getInt64Ty(BB->getParent()->getContext());
  AllocaInst* A = new AllocaInst(Ty, "", BB->getFirstNonPHI());
  new StoreInst(initialValue, A, false, 4, BB->getTerminator());
  return A;
}

Value* insertAdd(BasicBlock *BB, AllocaInst *A) {
  IRBuilder<> Builder(BB->getFirstNonPHI());
  Value* L = Builder.CreateAlignedLoad(A, 4);
  ConstantInt* One = ConstantInt::get(Type::getInt64Ty(BB->getParent()->getContext()), 1);
  Value* I = Builder.CreateAdd(L, One);
  Builder.CreateAlignedStore(I, A, 4);
  return I;
}

void llvm::TripCountProfiler::saveTripCount(std::set<BasicBlock*> BBs, AllocaInst* tripCountPtr, Value* estimatedTripCount,  BasicBlock* loopHeader, int LoopClass){


	for(std::set<BasicBlock*>::iterator it = BBs.begin(), end = BBs.end(); it != end; it++){

		BasicBlock* BB = *it;

		IRBuilder<> Builder(BB->getFirstInsertionPt());

		ConstantInt* loopIdentifier = ConstantInt::get(Type::getInt64Ty(*context), (int64_t)loopHeader);
		ConstantInt* loopClass = ConstantInt::get(Type::getInt32Ty(*context), (int64_t)LoopClass);


		//Value* stderr = Builder.CreateAlignedLoad(GVstderr, 4);
		Value* tripCount = Builder.CreateAlignedLoad(tripCountPtr, 4);

		std::vector<Value*> args;
		args.push_back(loopIdentifier);
		args.push_back(tripCount);
		args.push_back(estimatedTripCount);
		args.push_back(loopClass);
		llvm::ArrayRef<llvm::Value *> arrayArgs(args);
		Builder.CreateCall(collectLoopData, arrayArgs, "");


		int count = 0;
		for(pred_iterator pred = pred_begin(BB); pred != pred_end(BB); pred++) {
			count ++;
		}

	}

//	std::vector<Value*> args2;
//	args2.push_back(stderr);
//	args2.push_back(formatStr);
//	args2.push_back(loopIdentifier);
//	args2.push_back(estimatedTripCount);
//	args2.push_back(tripCount);
//	llvm::ArrayRef<llvm::Value *> arrayArgs2(args2);
//	Builder.CreateCall(fPrintf, arrayArgs2, "");

}

void llvm::TripCountProfiler::createFPrintf(Module& M) {

	LLVMContext* context = &M.getContext();

	Type* IO_FILE_PTR_ty;
	GVstderr = M.getGlobalVariable("stderr");
	if (GVstderr != NULL) {
		IO_FILE_PTR_ty = GVstderr->getType();
		IO_FILE_PTR_ty = IO_FILE_PTR_ty->getContainedType(0);
	} else {
		StructType* IO_FILE_ty = StructType::create(*context,
													"struct._IO_FILE");
		IO_FILE_PTR_ty = PointerType::getUnqual(IO_FILE_ty);
		StructType* IO_marker_ty = StructType::create(*context,
													  "struct._IO_marker");
		PointerType* IO_marker_ptr_ty = PointerType::getUnqual(IO_marker_ty);

		std::vector<Type*> Elements;
		Elements.push_back(Type::getInt32Ty(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(IO_marker_ptr_ty);
		Elements.push_back(IO_FILE_PTR_ty);
		Elements.push_back(Type::getInt32Ty(*context));
		Elements.push_back(Type::getInt32Ty(*context));
		Elements.push_back(Type::getInt32Ty(*context));
		Elements.push_back(Type::getInt16Ty(*context));
		Elements.push_back(Type::getInt8Ty(*context));
		Elements.push_back(ArrayType::get(Type::getInt8Ty(*context), 1));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt64Ty(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt8PtrTy(*context));
		Elements.push_back(Type::getInt32Ty(*context));
		Elements.push_back(Type::getInt32Ty(*context));
		Elements.push_back(ArrayType::get(Type::getInt8Ty(*context), 40));
		IO_FILE_ty->setBody(Elements, false);

		std::vector<Type*> Elements2;
		Elements2.push_back(IO_marker_ptr_ty);
		Elements2.push_back(IO_FILE_PTR_ty);
		Elements2.push_back(Type::getInt32Ty(*context));
		IO_marker_ty->setBody(Elements2, false);
		GVstderr = new GlobalVariable(M, IO_FILE_PTR_ty, false,
									  GlobalValue::ExternalLinkage, NULL,
									  "stderr");
	}

	//Here we declare the types of the parameters of our fprintf
	std::vector<Type*> Params;
	Params.push_back(IO_FILE_PTR_ty);										//FILE* 	>> stderr
	Params.push_back(PointerType::getUnqual(Type::getInt8Ty(*context)));	//char* 	>> Format string, like "TripCount: %d %d /n"
	Params.push_back(Type::getInt64Ty(*context));							//Int64 	>> Loop Identifier
	Params.push_back(Type::getInt64Ty(*context));							//Int64 	>> Estimated Trip Count
	Params.push_back(Type::getInt64Ty(*context));							//Int64 	>> Actual Trip Count

	Type* Ty = Type::getVoidTy(*context);
	FunctionType *T = FunctionType::get(Ty, Params, true);
	fPrintf = M.getOrInsertFunction("fprintf", T);

}



bool llvm::TripCountProfiler::doInitialization(Module& M) {
	createFPrintf(M);
	context = &M.getContext();

	NumInstrumentedLoops = 0;
	NumIntervalLoops = 0;
	NumEqualityLoops = 0;
	NumOtherLoops = 0;

	NumUnknownConditionsIL = 0;
	NumUnknownConditionsEL = 0;
	NumUnknownConditionsOL = 0;


	/*
	 * We will insert calls to functions in specific
	 * points of the program.
	 *
	 * Before doing that, we must declare the functions.
	 * Here we have our declarations.
	 */

	Type* Ty = Type::getVoidTy(*context);
	FunctionType *T = FunctionType::get(Ty, true);
	initLoopList = M.getOrInsertFunction("initLoopList", T);

	std::vector<Type*> args;
	args.push_back(Type::getInt64Ty(*context));   // Loop Identifier
	args.push_back(Type::getInt64Ty(*context));   // Actual TripCount
	args.push_back(Type::getInt64Ty(*context));   // Estimated TripCount
	args.push_back(Type::getInt32Ty(*context));   // LoopClass (0=Interval; 1=Equality; 2=Other)
	llvm::ArrayRef<Type*> arrayArgs(args);
	FunctionType *T1 = FunctionType::get(Ty, arrayArgs, true);
	collectLoopData = M.getOrInsertFunction("collectLoopData", T1);

	std::vector<Type*> args2;
	args2.push_back(Type::getInt8PtrTy(*context));
	llvm::ArrayRef<Type*> arrayArgs2(args2);
	FunctionType *T2 = FunctionType::get(Ty, arrayArgs2, true);
	flushLoopStats = M.getOrInsertFunction("flushLoopStats", T2);

	return true;
}

Value* TripCountProfiler::generateEstimatedTripCount(BasicBlock* header, BasicBlock* entryBlock, Value* Op1, Value* Op2, CmpInst* CI){

	bool isSigned = CI->isSigned();


	BasicBlock* GT = BasicBlock::Create(*context, "", header->getParent(), header);
	BasicBlock* LE = BasicBlock::Create(*context, "", header->getParent(), header);
	BasicBlock* PHI = BasicBlock::Create(*context, "", header->getParent(), header);

	TerminatorInst* T = entryBlock->getTerminator();

	IRBuilder<> Builder(T);

	Value* cmp;

	//Make sure the two operands have the same type
	if (Op1->getType() != Op2->getType()) {

		if (Op1->getType()->getIntegerBitWidth() > Op2->getType()->getIntegerBitWidth() ) {
			//expand op2
			if (isSigned) Op2 = Builder.CreateSExt(Op2, Op1->getType(), "");
			else Op2 = Builder.CreateZExt(Op2, Op1->getType(), "");

		} else {
			//expand op1
			if (isSigned) Op1 = Builder.CreateSExt(Op1, Op2->getType(), "");
			else Op1 = Builder.CreateZExt(Op1, Op2->getType(), "");

		}

	}

	assert(Op1->getType() == Op2->getType() && "Operands with different data types, even after adjust!");

	Builder.SetInsertPoint(T);

	if (isSigned)
		cmp = Builder.CreateICmpSGT(Op1,Op2,"");
	else
		cmp = Builder.CreateICmpUGT(Op1,Op2,"");

	Builder.CreateCondBr(cmp, GT, LE, NULL);
	T->eraseFromParent();

	/*
	 * estimatedTripCount = |Op1 - Op2|
	 *
	 * We will create the same sub in both GT and in LE blocks, but
	 * with inverted operand order. Thus, the result of the subtraction
	 * will be always positive.
	 */

	Builder.SetInsertPoint(GT);
	Value* sub1;
	if (isSigned) {
		//We create a signed sub
		sub1 = Builder.CreateNSWSub(Op1, Op2);
	} else {
		//We create an unsigned sub
		sub1 = Builder.CreateNUWSub(Op1, Op2);
	}
	Builder.CreateBr(PHI);

	Builder.SetInsertPoint(LE);
	Value* sub2;
	if (isSigned) {
		//We create a signed sub
		sub2 = Builder.CreateNSWSub(Op2, Op1);
	} else {
		//We create an unsigned sub
		sub2 = Builder.CreateNUWSub(Op2, Op1);
	}
	Builder.CreateBr(PHI);

	Builder.SetInsertPoint(PHI);
	PHINode* sub = Builder.CreatePHI(sub2->getType(), 2, "");
	sub->addIncoming(sub1, GT);
	sub->addIncoming(sub2, LE);

	Value* EstimatedTripCount;
	if (isSigned) 	EstimatedTripCount = Builder.CreateSExtOrBitCast(sub, Type::getInt64Ty(*context), "EstimatedTripCount");
	else			EstimatedTripCount = Builder.CreateZExtOrBitCast(sub, Type::getInt64Ty(*context), "EstimatedTripCount");

	switch(CI->getPredicate()){
		case CmpInst::ICMP_UGE:
		case CmpInst::ICMP_ULE:
		case CmpInst::ICMP_SGE:
		case CmpInst::ICMP_SLE:
			{
				Constant* One = ConstantInt::get(EstimatedTripCount->getType(), 1);
				EstimatedTripCount = Builder.CreateAdd(EstimatedTripCount, One);
				break;
			}
		default:
			break;
	}



	Builder.CreateBr(header);

	//Adjust the PHINodes of the loop header accordingly
	for (BasicBlock::iterator it = header->begin(); it != header->end(); it++){
		Instruction* tmp = it;

		if (PHINode* I = dyn_cast<PHINode>(tmp)){
			int i = I->getBasicBlockIndex(entryBlock);
			if (i >= 0){
				I->setIncomingBlock(i,PHI);
			}
		}

	}



	return EstimatedTripCount;
}

Instruction* getNextInstruction(Instruction* I){

	Instruction* result = NULL;

	if(!isa<TerminatorInst>(I)){
		BasicBlock::iterator it(I);
		it++;
		result = it;
	}

	return result;

}

Value* TripCountProfiler::getValueAtEntryPoint(Value* source, BasicBlock* loopHeader){

	LoopInfoEx& li = getAnalysis<LoopInfoEx>();
	LoopNormalizerAnalysis& ln = getAnalysis<LoopNormalizerAnalysis>();

	Loop* loop = li.getLoopFor(loopHeader);

	//Option 1: Loop invariant. Return the value itself
	if (loop->isLoopInvariant(source)) return source;

	//Option 2: Sequence of redefinitions with PHI node in the loop header. Return the incoming value from the entry block
	LoopControllersDepGraph& lcd = getAnalysis<LoopControllersDepGraph>();
	GraphNode* node = lcd.depGraph->findNode(source);
	if (!node) {
		return NULL;
	}

	int SCCID = lcd.depGraph->getSCCID(node);
	Graph sccGraph = lcd.depGraph->generateSubGraph(SCCID);

	for(Graph::iterator it =  sccGraph.begin(); it != sccGraph.end(); it++){

		Value* V = NULL;

		if (VarNode* VN = dyn_cast<VarNode>(*it)) {
			V = VN->getValue();
		} else	if (OpNode* ON = dyn_cast<OpNode>(*it)) {
			V = ON->getValue();
		}

		if (V) {
			if (PHINode* PHI = dyn_cast<PHINode>(V)) {
				if(PHI->getParent() == loopHeader ) {

					Value* IncomingFromEntry = PHI->getIncomingValueForBlock(ln.entryBlocks[loopHeader]);
					return IncomingFromEntry;

				}
			}
		}
	}

	Instruction *InstToCopy = NULL;

	//Option 3: Sequence of loads/stores in the same memory location. Create load in the entry block and return the loaded value
	if (LoadInst* LI = dyn_cast<LoadInst>(source)){
		InstToCopy = LI;
	}


	//Option 4: Cast Instruction (Bitcast, Zext, SExt, Trunc etc...): Propagate search (The value theoretically is the same)
	if (CastInst* CI = dyn_cast<CastInst>(source)){
		return getValueAtEntryPoint(CI->getOperand(0), loopHeader);
	}

	//Option 5: GetElementPTR - Create a similar getElementPtr in the entry block
	if (GetElementPtrInst* GEPI = dyn_cast<GetElementPtrInst>(source)){

		// Do the copy only if all the operands are loop-invariant
		bool isInvariant = true;

		for(unsigned int i = 0; i < GEPI->getNumOperands(); i++){
			if (!loop->isLoopInvariant(GEPI->getOperand(i))) {
				isInvariant = false;
				break;
			}
		}

		if (isInvariant) InstToCopy = GEPI;
	}



	//Here we try to copy the instruction in the entry block
	//we adjust the operands  to the value dominate all of its uses.
	if (InstToCopy) {

		unsigned int prev_size = ln.entryBlocks[loopHeader]->getInstList().size();

		Instruction* NEW_INST = InstToCopy->clone();
		ln.entryBlocks[loopHeader]->getInstList().push_front(NEW_INST);

		for(unsigned int i = 0; i < InstToCopy->getNumOperands(); i++){

			Value* op = getValueAtEntryPoint(InstToCopy->getOperand(i), loopHeader);

			if (op){
				if (op->getType() != InstToCopy->getOperand(i)->getType()) op = NULL;
			}

			if (!op) {

				//Undo changes in the entry block
				while (ln.entryBlocks[loopHeader]->getInstList().size() > prev_size) {
					ln.entryBlocks[loopHeader]->getInstList().pop_front();
				}

				return NULL;
			}

			NEW_INST->setOperand(i, op);
		}

		return NEW_INST;
	}

	//Option 9999: unknown. Return NULL
	return NULL;
}


/*
 * Given a natural loop, find the basic block that is more likely block that
 * controls the number of iteration of a loop.
 *
 * In for-loops and while-loops, the loop header is the controller, for instance
 * In repeat-until-loops, the loop controller is a basic block that has a successor
 * 		outside the loop and the loop header as a successor.
 *
 *  Otherwise, return the first exit block
 */
BasicBlock* TripCountProfiler::findLoopControllerBlock(Loop* l){

	BasicBlock* header = l->getHeader();

	SmallVector<BasicBlock*, 2>  exitBlocks;
	l->getExitingBlocks(exitBlocks);

	//Case 1: for/while (the header is an exiting block)
	for (SmallVectorImpl<BasicBlock*>::iterator It = exitBlocks.begin(), Iend = exitBlocks.end(); It != Iend; It++){
		BasicBlock* BB = *It;
		if (BB == header) {
			return BB;
		}
	}

	//Case 2: repeat-until/do-while (the exiting block can branch directly to the header)
	for (SmallVectorImpl<BasicBlock*>::iterator It = exitBlocks.begin(), Iend = exitBlocks.end(); It != Iend; It++){

		BasicBlock* BB = *It;

		//Here we iterate over the successors of BB to check if it is a block that leads the control
		//back to the header.
		for(succ_iterator s = succ_begin(BB); s != succ_end(BB); s++){

			if (*s == header) {
				return BB;
			}
		}

	}

	//Otherwise, return the first exit block
	return *(exitBlocks.begin());
}


bool TripCountProfiler::runOnFunction(Function &F){

	IRBuilder<> Builder(F.getEntryBlock().getTerminator());
//	if (!formatStr){
//		formatStr = Builder.CreateGlobalStringPtr("TripCount " + F.getParent()->getModuleIdentifier() + ".%" PRId64 ": %" PRId64 " %d\n",    "formatStr");
//	}

	if (!moduleIdentifierStr){
		moduleIdentifierStr = Builder.CreateGlobalStringPtr(F.getParent()->getModuleIdentifier(), "moduleIdentifierStr");
	}

	Value* main = F.getParent()->getFunction("main");
	if(!main) main = F.getParent()->getFunction("MAIN__"); //Fortan hack

	bool isMain = (&F == main);

	if (isMain){
		Builder.CreateCall(initLoopList, "");
	}


	LoopInfoEx& li = getAnalysis<LoopInfoEx>();


	/*
	 * Here we have all the instructions that will stop the program
	 *
	 * E.g.: abort, exit, return of function main
	 *
	 * Before those instructions, we will print all the data we have collected.
	 */
	ExitInfo& eI = getAnalysis<ExitInfo>();
	for(std::set<Instruction*>::iterator Iit = eI.exitPoints.begin(), Iend = eI.exitPoints.end(); Iit != Iend; Iit++){

		Instruction* I = *Iit;

		if(I->getParent()->getParent() == &F){
			Builder.SetInsertPoint(I);

			std::vector<Value*> args;
			args.push_back(moduleIdentifierStr);
			llvm::ArrayRef<llvm::Value *> arrayArgs(args);
			Builder.CreateCall(flushLoopStats, arrayArgs, "");
		}

	}



	LoopNormalizerAnalysis& ln = getAnalysis<LoopNormalizerAnalysis>();


	Constant* constZero = ConstantInt::get(Type::getInt64Ty(F.getContext()), 0);

	Constant* unknownTripCount = ConstantInt::get(Type::getInt64Ty(F.getContext()), -2);


	for(LoopInfoEx::iterator lit = li.begin(); lit != li.end(); lit++){

		//Indicates if we don't have ways to determine the trip count
		bool unknownTC = false;

		Loop* loop = *lit;

		BasicBlock* header = loop->getHeader();
		BasicBlock* entryBlock = ln.entryBlocks[header];

		/*
		 * Here we are looking for the predicate that stops the loop.
		 *
		 * At this moment, we are only considering loops that are controlled by
		 * integer comparisons.
		 */
		BasicBlock* exitBlock = findLoopControllerBlock(loop);

		if (!exitBlock){

			errs() 	<< "Exit block not found!\n"
					<< "Function: " << header->getParent()->getName() << "\n"
					<< "Loop header:" << *header;

			continue;

		}

		TerminatorInst* T = exitBlock->getTerminator();
		BranchInst* BI = dyn_cast<BranchInst>(T);
		ICmpInst* CI = BI ? dyn_cast<ICmpInst>(BI->getCondition()) : NULL;

		Value* Op1 = NULL;
		Value* Op2 = NULL;

		int LoopClass;

		if (!CI) {

			unknownTC = true;
			NumOtherLoops++;
			NumUnknownConditionsOL++;

			LoopClass = 2;
		}
		else {

			switch(CI->getPredicate()){
			case ICmpInst::ICMP_SGE:
			case ICmpInst::ICMP_SGT:
			case ICmpInst::ICMP_UGE:
			case ICmpInst::ICMP_UGT:
			case ICmpInst::ICMP_SLE:
			case ICmpInst::ICMP_SLT:
			case ICmpInst::ICMP_ULE:
			case ICmpInst::ICMP_ULT:
				LoopClass = 0;
				NumIntervalLoops++;
				break;
			default:
				LoopClass = 1;
				NumEqualityLoops++;
			}


			Op1 = getValueAtEntryPoint(CI->getOperand(0), header);
			Op2 = getValueAtEntryPoint(CI->getOperand(1), header);


			if((!Op1) || (!Op2) ) {

//				errs() << "\n\n=======================UNKNOWN BEGIN==============================\n";
//				errs() << "Op1:" << *CI->getOperand(0) << "\n";
//				if (Op1) errs() << "Op1_Entry: " << *Op1 << "\n";
//				else errs() << "Op1_Entry: NULL\n";
//				errs() << "\n";
//
//				errs() << "Op2:" << *CI->getOperand(1) << "\n";
//				if (Op2) errs() << "Op2_Entry: " << *Op2 << "\n";
//				else errs() << "Op2_Entry: NULL\n";
//				errs() << "\n";
//
//				errs() << *header << "\n";
//				errs() << "===============================END================================\n\n\n";

				if (!LoopClass) NumUnknownConditionsIL++;
				else 			NumUnknownConditionsEL++;

				unknownTC = true;
			} else if((!Op1->getType()->isIntegerTy()) || (!Op2->getType()->isIntegerTy())) {
				NumIncompatibleOperandTypes++;
				unknownTC = true;
			}
		}

		Value* estimatedTripCount;

		if(unknownTC){
			estimatedTripCount = unknownTripCount;
		}else {
			estimatedTripCount = generateEstimatedTripCount(header, entryBlock, Op1, Op2, CI);
		}



		//Before the loop starts, the trip count is zero
		AllocaInst* tripCount = insertAlloca(entryBlock, constZero);

		//Every time the loop header is executed, we increment the trip count
		insertAdd(header, tripCount);


		/*
		 * We will collect the actual trip count and the estimate trip count in every
		 * basic block that is outside the loop
		 */
		std::set<BasicBlock*> blocksToInstrument;
		SmallVector<BasicBlock*, 2> exitBlocks;
		loop->getExitBlocks(exitBlocks);
		for (SmallVectorImpl<BasicBlock*>::iterator eb = exitBlocks.begin(); eb !=  exitBlocks.end(); eb++){

			BasicBlock* CurrentEB = *eb;

			/*
			 * Does not instrument landingPad (exception handling) blocks
			 * TODO: Handle LandingPad blocks (if possible)
			 */
			if(!CurrentEB->isLandingPad())
				blocksToInstrument.insert(CurrentEB);

		}

		saveTripCount(blocksToInstrument, tripCount, estimatedTripCount, header, LoopClass);

		NumInstrumentedLoops++;

	}

	return true;

}


char llvm::TripCountProfiler::ID = 0;
static RegisterPass<TripCountProfiler> X("tc-profiler","Trip Count Profiler");
