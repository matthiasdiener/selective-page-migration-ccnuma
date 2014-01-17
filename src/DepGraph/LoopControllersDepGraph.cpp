/*
 * LoopControllersDepGraph.cpp
 *
 *  Created on: 24/10/2013
 *      Author: raphael
 */

#include "LoopControllersDepGraph.h"

void LoopControllersDepGraph::getAnalysisUsage(AnalysisUsage &AU) const{
	AU.addRequired<functionDepGraph> ();
	AU.addRequired<LoopInfoEx> ();
    AU.setPreservesAll();
}


bool LoopControllersDepGraph::runOnFunction(Function& F){
    //Step 1: Get the complete dependence graph
	functionDepGraph& DepGraph = getAnalysis<functionDepGraph> ();
    depGraph = DepGraph.depGraph;

    //Step 2: Get the list of values that control the loop exit
    LoopInfoEx& li = getAnalysis<LoopInfoEx>();
    std::set<Value*> loopExitPredicates;

    for (LoopInfoEx::iterator lit = li.begin(), lend = li.end(); lit != lend; lit++) {

    	Loop* l = *lit;


    	SmallVector<BasicBlock*, 4> loopExitingBlocks;
    	l->getExitingBlocks(loopExitingBlocks);

		for(SmallVectorImpl<BasicBlock*>::iterator BB = loopExitingBlocks.begin(); BB != loopExitingBlocks.end(); BB++){
			if (BranchInst* BI = dyn_cast<BranchInst>((*BB)->getTerminator())) {
				loopExitPredicates.insert(BI->getCondition());
			} else if (SwitchInst* SI = dyn_cast<SwitchInst>((*BB)->getTerminator())) {
				loopExitPredicates.insert(SI->getCondition());
			} else if (IndirectBrInst* IBI = dyn_cast<IndirectBrInst>((*BB)->getTerminator())) {
				loopExitPredicates.insert(IBI->getAddress());
			} else if (InvokeInst* II = dyn_cast<InvokeInst>((*BB)->getTerminator())) {
				loopExitPredicates.insert(II);
			}
		}

    }


    //Step 3: Make a list of graph nodes that represent the dependencies of the loop controllers
    std::set<GraphNode*> visitedNodes;

    for(std::set<Value*>::iterator v = loopExitPredicates.begin(); v != loopExitPredicates.end(); v++){
    	if (GraphNode* valueNode = depGraph->findNode(*v))
    		depGraph->dfsVisitBack(valueNode, visitedNodes);
    	else
    		errs() << "Function : " << F.getName() << " - Value not found in the graph : " << **v
    				<< "\n";
    }

    //Step 4: Remove from the graph all the nodes that are not in the list of dependencies
    std::set<GraphNode*> nodesToRemove;
    for(Graph::iterator node = depGraph->begin(); node != depGraph->end(); node++ ){
    	if (!visitedNodes.count(*node)) nodesToRemove.insert(*node);
    }

    for(std::set<GraphNode*>::iterator node = nodesToRemove.begin(); node != nodesToRemove.end(); node++ ){
    	depGraph->removeNode(*node);
    }

    //Step 5: ta-da! The graph is ready to use :)

    return false;
}

char LoopControllersDepGraph::ID = 0;
static RegisterPass<LoopControllersDepGraph> Z("LoopControllersDepGraph",
		"Loop Controllers Dependence Graph", true, true);

char ViewLoopControllersDepGraphSCCs::ID = 0;
static RegisterPass<ViewLoopControllersDepGraphSCCs> X("ViewLoopControllersDepGraphSCCs",
		"View Loop Controllers Dependence Graph SCCs", true, true);



void ModuleLoopControllersDepGraph::getAnalysisUsage(AnalysisUsage &AU) const{
	AU.addRequired<LoopInfoEx>();
	AU.addRequired<moduleDepGraph> ();
    AU.setPreservesAll();
}


bool ModuleLoopControllersDepGraph::runOnModule(Module& M){
    //Step 1: Get the complete dependence graph
	moduleDepGraph& DepGraph = getAnalysis<moduleDepGraph> ();
    depGraph = DepGraph.depGraph;

    //Step 2: Get the list of values that control the loop exits
    std::set<Value*> loopExitPredicates;
    for (Module::iterator Fit = M.begin(), Fend = M.end(); Fit != Fend; Fit++) {

    	LoopInfoEx& li = getAnalysis<LoopInfoEx>(*Fit);
		std::set<Value*> loopExitPredicates;

		for (LoopInfoEx::iterator lit = li.begin(), lend = li.end(); lit != lend; lit++) {

			Loop* l = *lit;

			SmallVector<BasicBlock*, 4> loopExitingBlocks;
			l->getExitingBlocks(loopExitingBlocks);

			for(SmallVectorImpl<BasicBlock*>::iterator BB = loopExitingBlocks.begin(); BB != loopExitingBlocks.end(); BB++){
				if (BranchInst* BI = dyn_cast<BranchInst>((*BB)->getTerminator())) {
					loopExitPredicates.insert(BI->getCondition());
				} else if (SwitchInst* SI = dyn_cast<SwitchInst>((*BB)->getTerminator())) {
					loopExitPredicates.insert(SI->getCondition());
				} else if (IndirectBrInst* IBI = dyn_cast<IndirectBrInst>((*BB)->getTerminator())) {
					loopExitPredicates.insert(IBI->getAddress());
				} else if (InvokeInst* II = dyn_cast<InvokeInst>((*BB)->getTerminator())) {
					loopExitPredicates.insert(II);
				}
			}

		}

    }


    //Step 3: Make a list of graph nodes that represent the dependencies of the loop controllers
    std::set<GraphNode*> visitedNodes;

    for(std::set<Value*>::iterator v = loopExitPredicates.begin(); v != loopExitPredicates.end(); v++){
    	if (GraphNode* valueNode = depGraph->findNode(*v))
    		depGraph->dfsVisitBack(valueNode, visitedNodes);
    	else
    		errs() << "Value not found in the graph : " << **v
    				<< "\n";
    }

    //Step 4: Remove from the graph all the nodes that are not in the list of dependencies
    std::set<GraphNode*> nodesToRemove;
    for(Graph::iterator node = depGraph->begin(); node != depGraph->end(); node++ ){
    	if (!visitedNodes.count(*node)) nodesToRemove.insert(*node);
    }

    for(std::set<GraphNode*>::iterator node = nodesToRemove.begin(); node != nodesToRemove.end(); node++ ){
    	depGraph->removeNode(*node);
    }

    //Step 5: ta-da! The graph is ready to use :)

    return false;
}

char ModuleLoopControllersDepGraph::ID = 0;
static RegisterPass<ModuleLoopControllersDepGraph> Y("ModuleLoopControllersDepGraph",
		"Loop Controllers Dependence Graph (Module)");

