/**
 * Author: Ziang Wan
 */

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"

#include <iostream>
#include <set>
#include <vector>

using namespace llvm;

namespace {
  //===-------------------------------------------------------------------===//
  // ADCE Class
  //
  // This class does all of the work of Aggressive Dead Code Elimination.
  //
  class ADCE : public FunctionPass {
  private:
    Function *Func;                      // Function we are working on
    std::vector<Instruction*> WorkList;  // Instructions that just became live
    std::set<Instruction*>    LiveSet;   // Set of live instructions
    
    //===-----------------------------------------------------------------===//
    // The public interface for this class
    //
  public:
    static char ID; // Pass identification
    ADCE() : FunctionPass(ID) {}
    
    // Execute the Aggressive Dead Code Elimination algorithm on one function
    //
    virtual bool runOnFunction(Function &F) {
      Func = &F;
      bool Changed = doADCE();
      assert(WorkList.empty());
      LiveSet.clear();
      return Changed;
    }
    
    // getAnalysisUsage
    //
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }
    
  private:
    // doADCE() - Run the Aggressive Dead Code Elimination algorithm, returning
    // true if the function was modified.
    //
    bool doADCE();

    // helper function
    void markLive(Instruction *I);
    bool isTriviallyLive(Instruction *I);
  };
}  // End of anonymous namespace

char ADCE::ID = 0;
static RegisterPass<ADCE> X("mp5-adce", "Aggressive Dead Code Elimination (MP5)", false /* Only looks at CFG? */, false /* Analysis Pass? */);

// Implement ADCE algorithm here
bool ADCE::doADCE()
{
    // LiveSet = emptySet;
    this->LiveSet.clear();
    this->WorkList.clear();

    // keep track of all the reachable basic blocks
    std::set<BasicBlock*> ReachableBBs;

    // since all refs are dropped, keep track of dead / zero use insts to be removed
    std::vector<Instruction*> deadInstructions;

    // whether any modification has been done or not
    bool changed = false;

    // for (each BB in F in depth-first order)
    //  for (each instruction I in BB)
    //    if (isTriviallyLive(I))
    //      markLive(I); insert I in LiveSet if I is not present in LiveSet
    //    else if (I.use_empty()) // trivially dead
    //      remove I from BB;

    for (df_ext_iterator<BasicBlock*> BBI = df_ext_begin(&Func->front(), ReachableBBs), BBE = df_ext_end(&Func->front(), ReachableBBs); BBI != BBE; BBI++) {
        BasicBlock *BB = *BBI;

        for (BasicBlock::iterator II = BB->begin(), EI = BB->end(); II != EI; II++) {
            Instruction *I = &(*II);

            if (this->isTriviallyLive(I)) {
                // errs() << "Mark Alive: " << *I << "\n";
                this->markLive(I);
            }else if (I->use_empty()) {
                // add to the dead instruction vector
                // errs() << "Push Zero: " << *I << "\n";
                deadInstructions.push_back(I);
            }
        }
    }

    for(Instruction *deadI : deadInstructions){
        // errs() << "Erase Zero: " << *deadI << "\n";
        deadI->eraseFromParent();
        changed = true;
    }

    // clear out for other dead instructions
    deadInstructions.clear();

    // while (WorkList is not empty) {
    //  I = get instruction at head of work list;
    //  if (basic block containing I is reachable)
    //    for (all operands op of I)
    //      if (operand op is an instruction)
    //        markLive(op, LiveSet, WorkList);

    while (!this->WorkList.empty()) {
        Instruction *I = this->WorkList.back();
        this->WorkList.pop_back();

        if(ReachableBBs.count(I->getParent()) > 0){
            for (unsigned opIdx = 0; opIdx != I->getNumOperands(); opIdx += 1){
                if (Instruction *operandI = dyn_cast<Instruction>(I->getOperand(opIdx))){
                    // errs() << "Propagated: " << *operandI << "\n";
                    this->markLive(operandI);
                }
            }
        }
    }

    // for (each BB in F in any order)
    //  if (BB is reachable)
    //    for (each non-live instruction I in BB)
    //      I.dropAllReferences();

    for (Function::iterator BBI = Func->begin(), BBE = Func->end(); BBI != BBE; BBI++) {
        BasicBlock *BB = &(*BBI);

        if(ReachableBBs.count(BB) > 0) {
            for (BasicBlock::iterator II = BB->begin(), EI = BB->end(); II != EI; II++) {
                Instruction *I = &(*II);

                if(this->LiveSet.count(I) == 0){
                    // errs() << "Push Dead: " << *I << "\n";
                    I->dropAllReferences();
                    deadInstructions.push_back(I);
                }
            }
        }
    }

    // for (each BB in F in any order)
    //  if (BB is reachable)
    //    for (each non-live instruction I in BB)
    //      erase I from BB;

    for(Instruction *deadI : deadInstructions){
        // errs() << "Erase Dead: " << *deadI << "\n";
        deadI->eraseFromParent();
        changed = true;
    }

    return changed;
}


void ADCE::markLive(Instruction *I){
    // markLive()
    //  if I is not in LiveSet
    //    insert I in LiveSet
    //    append I at the end of WorkList

    if(this->LiveSet.count(I) == 0){
        this->LiveSet.insert(I);
        this->WorkList.push_back(I);
    }
}


bool ADCE::isTriviallyLive(Instruction *I){
    // may have side effects
    if(I->mayHaveSideEffects()){
        return true;
    }

    // is a terminator instruction: ret or unwind
    if(I->isTerminator()){
        return true;
    }

    // may write to memory
    if(I->mayWriteToMemory()){
        return true;
    }

    // volatile load
    if (LoadInst *loadI = dyn_cast<LoadInst>(I)){
        if(loadI->isVolatile()){
            return true;
        }
    }

    // store, call, free <- unknown
    if(isa<StoreInst>(I) || isa<CallInst>(I)){
        return true;
    }

    // not a trivially alive instruction
    return false;
}
