/**
 * Author: Ziang Wan
 */

// original imports
#include "llvm/IR/Function.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Scalar.h"

// isSafeToSpeculativelyExecute
#include "llvm/Analysis/ValueTracking.h"

#include <iostream>

using namespace llvm;

namespace {
  class LICM : public LoopPass {
  private:
    Loop *curLoop;
    
  public:
    static char ID; // Pass identification, replacement for typeid
    LICM() : LoopPass(ID) {}
    virtual bool runOnLoop(Loop *L, LPPassManager &LPM) override {
	  curLoop = L;
	  return doLICM();
    }
    
    /// This transformation requires natural loop information & requires that
    /// loop preheaders be inserted into the CFG...
    // 
    // !!!PLEASE READ getLoopAnalysisUsage(AU) defined in lib/Transforms/Utils/LoopUtils.cpp
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesCFG();
      getLoopAnalysisUsage(AU);
    } 
    
  private:
    bool doLICM();

    // helper functions
    void doLICMRecursive(DomTreeNode *Node, LoopInfo *LI, DominatorTree *DT, BasicBlock *Preheader);
    bool isLoopInvariance(Instruction *I);
    bool safeToHoist(Instruction *I, DominatorTree *DT);
  };
}

char LICM::ID = 0;
RegisterPass<LICM> X("mp5-licm", "Loop Invariant Code Motion (MP5)", false /* Only looks at CFG? */, false /* Analysis Pass? */);

// Implement LICM algorithm here
bool LICM::doLICM()
{
    // errs() << "Current Loop: " << *curLoop << "\n";

    // get loop info and dominator tree
    LoopInfo* LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    DominatorTree* DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();

    // every loop has a preheader
    BasicBlock *Preheader = this->curLoop->getLoopPreheader();
    assert(Preheader != nullptr);

    // iterate over all the basic block, starting from the head, pre-order
    DomTreeNode *HN = DT->getNode(this->curLoop->getHeader());
    this->doLICMRecursive(HN, LI, DT, Preheader);

    return true;
}

// pre-order traversal
void LICM::doLICMRecursive(DomTreeNode *Node, LoopInfo *LI, DominatorTree *DT, BasicBlock *Preheader){
    // get the basic block
    BasicBlock *BB = Node->getBlock();

    // if (BB is immediately within L) : not in an inner loop or outside L
    //  for (each instruction I in BB) {
    //      if (isLoopInvariant(I) && safeToHoist(I))
    //          move I to pre-header basic block;

    if(LI->getLoopFor(BB) == this->curLoop && this->curLoop->contains(BB)){
        // errs() << "doLICMRec: " << *BB << "\n";

        // store the point for later hoisting -> preven segfault during the iterator traversal
        std::vector<Instruction*> instructionToHoist;

        for (BasicBlock::iterator II = BB->begin(), E = BB->end(); II != E; II++) {
            Instruction *I = &(*II);

            if (isLoopInvariance(I) && safeToHoist(I, DT)){
                instructionToHoist.push_back(I);
            }
        }

        // do the actual hoisting
        for (Instruction *&hoistI : instructionToHoist) {
            // errs() << "MoveBefore: " << *hoistI << "\n";
            hoistI->moveBefore(Preheader->getTerminator());
        }
    }

    // pre-order traversal: handles the other basic blocks
    for (unsigned i = 0; i < Node->getChildren().size(); i += 1) {
        this->doLICMRecursive(Node->getChildren()[i], LI, DT, Preheader);
    }
}

bool LICM::isLoopInvariance(Instruction *I){
    // errs() << "Check Loop Invariance: " << *I << "\n";
    // It is one of the following LLVM instructions or instruction classes: binary
    // operator, shift, select, cast, getelementptr.
    if (!I->isBinaryOp() && !I->isShift() && !isa<SelectInst>(I) && !I->isCast() && !isa<GetElementPtrInst>(I)) {
        // errs() << "Wrong Type of Inst - Fail\n";
        return false;
    }

    // Every operand of the instruction is either (a) constant or (b) computed
    // outside the loop. You can use the Loop::contains() method to check (b).

    for (unsigned i = 0; i != I->getNumOperands(); i += 1){
        Value *operandV = I->getOperand(i);
        // errs() << "Operand: " << *operandV << "\n";

        // it is a constant
        if(isa<Constant>(operandV)){
            continue;
        }

        // it is an instruction that is computed outside the loop
        if (Instruction *operandI = dyn_cast<Instruction>(I->getOperand(i))){
            if(this->curLoop->contains(operandI)){
                // errs() << "Computed inside the loop - Fail\n";
                return false;
            }
        }
    }
    // errs() << "Is Loop Invariance - Pass\n";
    // all checked
    return true;
}

bool LICM::safeToHoist(Instruction *I, DominatorTree *DT){
    // errs() << "Check Hoist Safe: " << *I << "\n";
    // It has no side effects (use isSafeToSpeculativelyExecute(Instruction*);
    if(isSafeToSpeculativelyExecute(I)){
        // errs() << "No side effects - Pass\n";
        return true;
    }

    // The basic block containing the instruction dominates all exit blocks for the loop.
    SmallVector<BasicBlock*, 8> ExitBlocks;
    this->curLoop->getExitBlocks(ExitBlocks);

    if(ExitBlocks.empty()){
        // errs() << "No exit blocks - Fail\n";
        return false;
    }else{
        for (unsigned i = 0; i < ExitBlocks.size(); i += 1){
            if (!DT->dominates(I->getParent(), ExitBlocks[i])){
                // errs() << "Not dominating all exit blocks - Fail\n";
                return false;
            }
        }
        // errs() << "Dominate all exit blocks - Pass\n";
        return true;
    }
}