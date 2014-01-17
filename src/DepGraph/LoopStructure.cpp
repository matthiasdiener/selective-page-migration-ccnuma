/*
 * LoopStructure.cpp
 *
 *  Created on: Jan 10, 2014
 *      Author: raphael
 */

#ifndef DEBUG_TYPE
#define DEBUG_TYPE "LoopStructure"
#endif

#include "LoopStructure.h"

using namespace llvm;

STATISTIC(NumLoops			, "Total Number of Loops");
STATISTIC(NumNestedLoops	, "Number of CFG Nested Loops");
STATISTIC(NumLoopsSingleExit, "Number of Loops with a Single Exit point");
STATISTIC(NumUnhandledExits	, "Number of not handled exit points (not branch or switch)");

STATISTIC(NumAnalyzedSCCs			, "Number of Analyzed DepGraph SCCs");
STATISTIC(NumSinglePathSCCs			, "Number of Single-Path DepGraph SCCs");
STATISTIC(NumTwoPathSCCs			, "Number of 2-Path DepGraph SCCs");
STATISTIC(NumThreePathSCCs			, "Number of 3-Path DepGraph SCCs");
STATISTIC(NumFourPathSCCs			, "Number of 4-Path DepGraph SCCs");
STATISTIC(NumFivePathSCCs			, "Number of 5-Path DepGraph SCCs");
STATISTIC(NumThousandPlusPathSCCs	, "Number of +999-Path DepGraph SCCs");


/*
 * Function getDelta
 *
 * Given two sets, generate the list of items that are present in the first set
 * but are not present on the second.
 */
template <typename T>
std::set<T> getDelta(std::set<T> t1, std::set<T> t2) {

	std::set<T> result;
	for(typename std::set<T>::iterator it = t1.begin(), iend = t1.end(); it != iend;  it++ ) {
		T item = *it;
		if (!t2.count(item)) {
			result.insert(item);
		}
	}

	return result;

}


bool LoopStructure::runOnFunction(Function& F) {

//	std::string Filename = "/tmp/DepGraphLoopPaths.txt";
//	std::string ErrorInfo;
//	raw_fd_ostream File(Filename.c_str(), ErrorInfo, raw_fd_ostream::F_Append);
//	if (!ErrorInfo.empty()){
//	  errs() << "Error opening file /tmp/DepGraphLoopPaths.txt for writing! Error Info: " << ErrorInfo  << " \n";
//	  return false;
//	}

	std::set<int> analyzedSCCs;
	std::set<GraphNode*> visitedNodes;


	LoopControllersDepGraph & lcd = getAnalysis<LoopControllersDepGraph>();
	Graph* graph = lcd.depGraph;
	graph->recomputeSCCs();

	LoopInfoEx & li = getAnalysis<LoopInfoEx>();
	for (LoopInfoEx::iterator it = li.begin(); it != li.end();  it++) {

		// Section 1: Structure of the loops in the CFG
		NumLoops++;

		Loop* L = *it;

		if (L->getLoopDepth() > 1) NumNestedLoops++;

		SmallVector<BasicBlock*, 4> exitingBlocks;
		L->getExitingBlocks(exitingBlocks);

		if (exitingBlocks.size() == 1) {
			NumLoopsSingleExit++;
		}


		//Section 2: Structure of the loops in the Dep.Graph
		for (SmallVectorImpl<BasicBlock*>::iterator bIt = exitingBlocks.begin(); bIt != exitingBlocks.end(); bIt++){
			BasicBlock* exitingBlock = *bIt;

			TerminatorInst *T = exitingBlock->getTerminator();

			Value* Condition = NULL;

			if (BranchInst* BI = dyn_cast<BranchInst>(T)){
				Condition = BI->getCondition();
			} else if (SwitchInst* SI = dyn_cast<SwitchInst>(T)){
				Condition = SI->getCondition();
			} else {
				NumUnhandledExits++;
			}

			if (Condition) {

				GraphNode* ConditionNode = graph->findNode(Condition);

				std::set<GraphNode*> tmp = visitedNodes;
				std::map<int, GraphNode*> firstNodeVisitedPerSCC;

				//Avoid visiting the same node twice
				graph->dfsVisitBack_ext(ConditionNode, visitedNodes, firstNodeVisitedPerSCC);

				std::set<GraphNode*> delta = getDelta(visitedNodes, tmp);

				//Iterate over the dependencies
				for (std::set<GraphNode*>::iterator nIt = delta.begin(); nIt != delta.end(); nIt++ ) {

					GraphNode* CurrentNode = *nIt;

					int SCCID = graph->getSCCID(CurrentNode);

					if (graph->getSCC(SCCID).size() > 1) {

						//Count the number of paths only once per SCC
						if (!analyzedSCCs.count(SCCID)) {
							analyzedSCCs.insert(SCCID);

							NumAnalyzedSCCs++;

							GraphNode* firstNodeVisitedInSCC = firstNodeVisitedPerSCC[SCCID];

							std::set<std::stack<GraphNode*> > paths = graph->getAcyclicPathsInsideSCC(firstNodeVisitedInSCC, firstNodeVisitedInSCC);

							switch(paths.size()){
							case 1:
								NumSinglePathSCCs++;
								break;
							case 2:
								NumTwoPathSCCs++;
								break;
							case 3:
								NumThreePathSCCs++;
								break;
							case 4:
								NumFourPathSCCs++;
								break;
							case 5:
								NumFivePathSCCs++;
								break;
							default:
								if (paths.size() > 999){
									NumThousandPlusPathSCCs++;
								}
							}

//
//							errs() << firstNodeVisitedInSCC->getLabel() << " " << paths.size() <<"\n";
//
//							std::string tmp = F.getParent()->getModuleIdentifier();
//							replace(tmp.begin(), tmp.end(), ' ', '_');
//							File << tmp << " " << paths.size() << "\n";

						}

					}

				}

			}

		}

	}


	//We don't make changes to the source code; return False
	return false;
}


char LoopStructure::ID = 0;
static RegisterPass<LoopStructure> Y("LoopStructure",
                "Loop Structure Analysis");
