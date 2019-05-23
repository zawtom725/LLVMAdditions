//===- ScalarReplAggregates.cpp - Scalar Replacement of Aggregates --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This transformation implements the well known scalar replacement of
// aggregates transformation.  This xform breaks up alloca instructions of
// structure type into individual alloca instructions for
// each member (if possible).  Then, if possible, it transforms the individual
// alloca instructions into nice clean scalar SSA form.
//
// This combines an SRoA algorithm with Mem2Reg because they
// often interact, especially for C++ programs.  As such, this code
// iterates between SRoA and Mem2Reg until we run out of things to promote.
//
//===----------------------------------------------------------------------===//
//
// Author: Ziang Wan (ziangw2)
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "scalarrepl"
#include "llvm/Pass.h"

#include "llvm/Analysis/AssumptionCache.h"

#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/ArrayRef.h"

#include <vector>

using namespace llvm;
using std::vector;
using std::string;
using std::to_string;


// flag for errs() printing
// #define _SROA_ZIANG_DEBUG 0


STATISTIC(NumReplaced,  "Number of aggregate allocas broken up");
STATISTIC(NumPromoted,  "Number of scalar allocas promoted to register");


namespace {
  struct SROA : public FunctionPass {
    static char ID; // Pass identification
    SROA() : FunctionPass(ID) { }

    // Entry point for the overall scalar-replacement pass
    bool runOnFunction(Function &F);

    // getAnalysisUsage - List passes required by this pass.  We also know it
    // will not alter the CFG, so say so.
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }

  private:

    //-- step 1: Promote some scalar allocas to virtual registers --//

    // promote some scalar allocas to virtual registers and 
    // return the number of alloca instructions promoted
    // the first step of the iterative algorithm
    size_t promoteScalarAllocasToVirtualReg(Function &F);

    // return whether the given alloca instruction is promotable or not
    // a helper function used in promoteScalarAllocasToVirtualReg()
    bool isAllocaPromotable(const AllocaInst *AI);

    //-- step 2: Replace allocas with allocas of individual fields --//

    // replace some struct/array allocas with allocas of individual fields
    // return the number of alloca instructions replaced
    // the second step of the iterative algorithm
    size_t replaceStructAllocsWithIndividualFields(Function &F);

    //-- step 2.1: determine whether an alloca is eliminatable or not --//

    // return whether the given alloca instruction can be eliminated or not
    // a helper function used in replaceStructAllocsWithIndividualFields()
    bool canBeEliminatedStructAlloca(const AllocaInst *AI);

    // return true if the given getelementptr instruction satisfy the following:
    // It is of the form: getelementptr ptr, 0, constant[, ... constant].
    // The result of the getelementptr is only used in instructions of type U1 or U2,
    // or as the pointer argument of a load or store instruction
    // a helper function used in canBeEliminatedStructAlloca()
    bool isU1TypeGetElementPtr(const GetElementPtrInst *GEPI, const User *Val);

    // return true if the given comparison instruction satisfy the following:
    // In a ’eq’ or ’ne’ comparison instruction,
    // where the other operand is the NULL pointer value
    // a helper function used in canBeEliminatedStructAlloca()
    bool isU2TypeEqOrNe(const CmpInst *CI, const User *Val);

    //-- step 2.2 : eliminate the aggregate alloca --//

    // actually eliminate the struct alloca
    // replace it with allocas of individual fields
    // return a vector of sub-aggregate types
    // a helper function used in replaceStructAllocsWithIndividualFields()
    vector<AllocaInst *> * eliminateStructAlloca(AllocaInst *AI, Function &F);

    // when eliminating an alloca, the U1 type getelementptr use should be
    // modified apropriately
    // a helper function used in eliminateStructAlloca()
    void replaceU1TypeGetElementPtr(GetElementPtrInst *GEPI,
                                    Function &F,
                                    vector<AllocaInst *> &NewAllocs);

    // when eliminating an alloca, the U2 type nq/ne use should be
    // modified apropriately
    // a helper function used in eliminateStructAlloca()
    void replaceU2TypeEqOrNe(CmpInst *CI,
                             Function &F,
                             vector<AllocaInst *> &NewAllocs);
  };  // end of struct SROA
}

char SROA::ID = 0;
static RegisterPass<SROA> X("scalarrepl-ziangw2",
          "Scalar Replacement of Aggregates (by <netid>)",
          false /* does not modify the CFG */,
          false /* transformation, not just analysis */);


// Public interface to create the ScalarReplAggregates pass.
// This function is provided to you.
// Ziang: Why do I need that?
FunctionPass *createMyScalarReplAggregatesPass() {
  return new SROA();
}


//===----------------------------------------------------------------------===//
//                      SKELETON FUNCTION TO BE IMPLEMENTED
//===----------------------------------------------------------------------===//
//


// Function runOnFunction:
// Entry point for the overall ScalarReplAggregates function pass.
// This function is provided to you.
bool SROA::runOnFunction(Function &F) {

  #ifdef _SROA_ZIANG_DEBUG
  errs() << "SROA::runOnFunction: [" << F.getName() << "]\n";
  #endif

  // the top-level of my pass iteratively performs the two steps until no more changes
  // 1. prmote some scalar allocas to virtual registers (mem2reg)
  // 2. replace some struct/array allocas with allocas of individual fields

  bool Changed = false;
  bool ChangedOneIteration = false;

  do {
    #ifdef _SROA_ZIANG_DEBUG
    errs() << "One iteration on function\n";
    #endif

    size_t ReplacementCount = replaceStructAllocsWithIndividualFields(F);
    size_t PromoteCount = promoteScalarAllocasToVirtualReg(F);

    ChangedOneIteration = (ReplacementCount > 0) || (PromoteCount > 0);
    Changed = Changed || ChangedOneIteration;
  } while (ChangedOneIteration);

  return Changed;
}


//===----------------------------------------------------------------------===//
//        step 1: prmote some scalar allocas to virtual registers
//===----------------------------------------------------------------------===//
// 


// promote some scalar allocas to virtual registers and 
// return the number of alloca instructions promoted
// the first step of the iterative algorithm
size_t SROA::promoteScalarAllocasToVirtualReg(Function &F) {

  // iterate over every instruction in the function
  // find all the promotable allocas instructions and store them in PromAllocaOfFunc

  vector<AllocaInst *> VecPromAllocaOfFunc;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {

      // only alloca instructions are promotable
      if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
        if (SROA::isAllocaPromotable(/*AI=*/AI)) {

          #ifdef _SROA_ZIANG_DEBUG
          errs() << "Promotable: [" << *AI << "]\n";
          #endif

          // safety check against the llvm isAllocaPromotable
          // make sure that my algorithm is as strict as the llvm one

          assert(llvm::isAllocaPromotable(/*AI=*/AI) &&
                 "SROA::isAllocaPromotable wrong result.");

          VecPromAllocaOfFunc.push_back(AI);
        }
      }

    }
  }

  size_t NumAllocToProm = VecPromAllocaOfFunc.size();

  // do the mem2reg pass only if the vector isn't empty

  if (NumAllocToProm > 0) {
    DominatorTree DomTreeOfFunc(F);
    AssumptionCache AsspCacheOfFunc(F);
    ArrayRef<AllocaInst *> ArrayRefPromAllocaOfFunc(VecPromAllocaOfFunc);

    PromoteMemToReg(/*Allocas=*/ArrayRefPromAllocaOfFunc,
                    /*DT=*/DomTreeOfFunc, /*AC=*/&AsspCacheOfFunc);

    // update the llvm STATISTIC
    NumPromoted += NumAllocToProm;
  }

  return NumAllocToProm;
}


// it will return true if the alloca instruction can be promoted or false otherwise
// a helper function used in promoteScalarAllocasToVirtualReg()
bool SROA::isAllocaPromotable(const AllocaInst *AI) {

  #ifdef _SROA_ZIANG_DEBUG
  errs() << "isAllocaPromotable: [" << *AI << "]\n";
  #endif

  // an alloca instruction is promotable to live in a register
  // if the alloca satisfies all these requirements:
  // (P1) The alloca is a “first-class” type, which you can approximate conservatively with
  // isFPOrFPVectorTy() || isIntOrIntVectorTy() || isPtrOrPtrVectorTy.
  
  Type *AllocType = AI->getAllocatedType();
  bool IsFirstClsType = AllocType->isFPOrFPVectorTy() ||
                        AllocType->isIntOrIntVectorTy() ||
                        AllocType->isPtrOrPtrVectorTy();

  if (!IsFirstClsType) {
    return false;
  }

  // (P2) The alloca is only used in a load or store instruction and
  // the instruction satisfies !isVolatile().
  // important clarification: alloca must be used as the pointer argument of the load or store.

  for (const User *U : AI->users()) {
    if (const LoadInst *LI = dyn_cast<LoadInst>(U)) {
      if (LI->isVolatile() || LI->getOperand(0) != AI) {
        return false;
      }
    } else if (const StoreInst *SI = dyn_cast<StoreInst>(U)) {
      if (SI->isVolatile() || SI->getOperand(1) != AI) {
        return false;
      }
    } else {
      return false;
    }
  }

  return true;
}


//===----------------------------------------------------------------------===//
//            step 2: replace some struct/array allocas
//                    with allocas of individual fields
//===----------------------------------------------------------------------===//
// 


// replace some struct/array allocas with allocas of individual fields
// the second step of the iterative algorithm
size_t SROA::replaceStructAllocsWithIndividualFields(Function &F) {

  // iterate over the function to identify the first set of allocas to 
  // be eliminated

  vector<AllocaInst *> VecElimatableStructAlloca;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
        if (canBeEliminatedStructAlloca(/*AI=*/AI)) {
          VecElimatableStructAlloca.push_back(AI);
        }
      }
    }
  }

  // then, use a worklist-style algorithm to eliminate allocas
  // notice, only newly generated allocas can be eliminatable

  size_t ReplacementCount = 0;

  while (VecElimatableStructAlloca.size() > 0) {
    AllocaInst *AIToBeEliminated = VecElimatableStructAlloca.back();
    VecElimatableStructAlloca.pop_back();

    vector<AllocaInst *> *SubAggrAllocas = eliminateStructAlloca(
                                           /*AI=*/AIToBeEliminated,
                                           /*F=*/F);
    ReplacementCount += 1;

    // append eligible sub aggregate allocas to the worklist

    if (SubAggrAllocas != NULL) {
      for (AllocaInst *SubAggrAlloca : *SubAggrAllocas) {
        if (canBeEliminatedStructAlloca(/*AI=*/SubAggrAlloca)) {
          VecElimatableStructAlloca.push_back(SubAggrAlloca);
        }
      }
    }

    free(SubAggrAllocas);
  }

  // update LLVM STATISTIC NumReplaced
  NumReplaced += ReplacementCount;

  return ReplacementCount;
}


//===----------------------------------------------------------------------===//
//         step 2.1: determine whether an alloca is eliminatable or not
//===----------------------------------------------------------------------===//
//


// return whether the given alloca instruction can be eliminated or not
// a helper function used in replaceStructAllocsWithIndividualFields()
bool SROA::canBeEliminatedStructAlloca(const AllocaInst *AI) {
  #ifdef _SROA_ZIANG_DEBUG
  errs() << "canBeEliminatedStructAlloca: [" << *AI << "]\n";
  #endif

  // type check

  if (!AI->getAllocatedType()->isStructTy()) {
    return false;
  }

  // the resulting pointer is used only in two ways: U1 and U2

  for (const User *U : AI->users()) {
    if (const GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(U)) {
      if (!isU1TypeGetElementPtr(/*GEPI=*/GEPI, /*Val=*/AI)) {
        #ifdef _SROA_ZIANG_DEBUG
        errs() << "Not U1: [" << *U << "]\n";
        #endif
        return false;
      }
    } else if (const CmpInst *CI = dyn_cast<CmpInst>(U)) {
      if (!isU2TypeEqOrNe(/*CI=*/CI, /*Val=*/AI)) {
        #ifdef _SROA_ZIANG_DEBUG
        errs() << "Not U2: [" << *U << "]\n";
        #endif
        return false;
      }
    } else {
      #ifdef _SROA_ZIANG_DEBUG
      errs() << "Not U1 or U2: [" << *U << "]\n";
      #endif
      return false;
    }
  }

  #ifdef _SROA_ZIANG_DEBUG
  errs() << "Eliminatable Struct Alloca: [" << *AI << "]\n";
  #endif
  return true;
}


// return true if the given getelementptr instruction satisfy the following:
// 1. It is of the form: getelementptr ptr, 0, constant[, ... constant].
// 2. The result of the getelementptr is only used in instructions of type U1 or U2,
// or as the pointer argument of a load or store instruction
// a helper function used in canBeEliminatedStructAlloca()
bool SROA::isU1TypeGetElementPtr(const GetElementPtrInst *GEPI, const User *Val) {
  #ifdef _SROA_ZIANG_DEBUG
  errs() << "isU1TypeGetElementPtr: [" << *GEPI << "]\n";
  #endif

  // 1. check its operands are ptr, 0, constant [, ... constant]

  unsigned NumOperands = GEPI->getNumOperands();
  if (NumOperands < 3) {
    return false;
  }

  if (GEPI->getOperand(0) != Val) {
    return false;
  }

  ConstantInt *GEPIOperand1ConstInt = dyn_cast<ConstantInt>(GEPI->getOperand(1));
  if (GEPIOperand1ConstInt == NULL || !GEPIOperand1ConstInt->isZero()) {
    return false;
  }

  for (unsigned i = 2; i < NumOperands; ++i) {
    if (!isa<Constant>(GEPI->getOperand(i))) {
      return false;
    }
  }

  // 2. check that the result is only used in U1, U2, load, store (not of but into)

  for (const User *U : GEPI->users()) {
    if (const GetElementPtrInst *UGEPI = dyn_cast<GetElementPtrInst>(U)) {
      if (!isU1TypeGetElementPtr(/*GEPI=*/UGEPI, /*Val=*/GEPI)) {
        return false;
      }
    } else if (const CmpInst *CI = dyn_cast<CmpInst>(U)) {
      if (!isU2TypeEqOrNe(/*CI=*/CI, /*Val=*/GEPI)) {
        return false;
      }
    } else if (const LoadInst *LI = dyn_cast<LoadInst>(U)) {

      // need to ensure that GEPI actually is the pointer argument of the load

      if (LI->getOperand(0) != GEPI) {
        #ifdef _SROA_ZIANG_DEBUG
        errs() << "isU1TypeGetElementPtr: invalid load [" << *LI << "]\n";
        #endif
        return false;
      }
    } else if (const StoreInst *SI = dyn_cast<StoreInst>(U)) {

      // need to ensure that GEPI actually is the pointer argument of the store

      if (SI->getOperand(1) != GEPI) {
        #ifdef _SROA_ZIANG_DEBUG
        errs() << "isU1TypeGetElementPtr: invalid store [" << *SI << "]\n";
        #endif
        return false;
      }
    } else {
      #ifdef _SROA_ZIANG_DEBUG
      errs() << "isU1TypeGetElementPtr: invalid use [" << *U << "]\n";
      #endif
      return false;
    }
  }

  #ifdef _SROA_ZIANG_DEBUG
  errs() << "U1: [" << *GEPI << "]\n";
  #endif
  return true;
}


// return true if the given comparison instruction satisfy the following:
// In a ’eq’ or ’ne’ comparison instruction,
// where the other operand is the NULL pointer value
// a helper function used in replaceStructAllocsWithIndividualFields()
bool SROA::isU2TypeEqOrNe(const CmpInst *CI, const User *Val) {

  // 1. check that it is a 'eq' or 'ne' comparison
  // Ziang: it seems that Ptr==NULL is actually ICMP_EQ and ICMP_NEQ
  // I tested it using llvm-dis and clang

  #ifdef _SROA_ZIANG_DEBUG
  errs() << "isU2TypeEqOrNe: [" << *CI << "]\n";
  #endif

  CmpInst::Predicate CIPredicate = CI->getPredicate();
  bool IsNeOrEq = (CIPredicate == CmpInst::ICMP_EQ) || (CIPredicate == CmpInst::ICMP_NE);
  if (!IsNeOrEq) {
    return false;
  }

  // 2. check that one operand is Val and the other is Null

  unsigned NumOperands = CI->getNumOperands();
  if (NumOperands != 2) {
    return false;
  }

  Value *CIOperand0 = CI->getOperand(0);
  Value *CIOperand1 = CI->getOperand(1);

  if (CIOperand0 == Val) {
    if (!isa<ConstantPointerNull>(CIOperand1)) {
      return false;
    }
  } else if (CIOperand1 == Val) {
    if (!isa<ConstantPointerNull>(CIOperand0)) {
      return false;
    }
  } else {

    // not possible to reach here because one of the two operands
    // must be Val

    assert(false && "isU2TypeEqOrNe reaches unpossible place - internal error");
    return false;
  }

  #ifdef _SROA_ZIANG_DEBUG
  errs() << "U2: [" << *CI << "]\n";
  #endif
  return true;
}


//===----------------------------------------------------------------------===//
//                      step 2.2: eliminate the aggregate alloca
//===----------------------------------------------------------------------===//
//


// actually eliminate the struct alloca
// replace it with allocas of individual fields
// return a vector of sub-aggregate types
// a helper function used in replaceStructAllocsWithIndividualFields()
vector<AllocaInst *> * SROA::eliminateStructAlloca(AllocaInst *AI, Function &F) {
  #ifdef _SROA_ZIANG_DEBUG
  errs() << "eliminateStructAlloca: [" << *AI << "]\n";
  #endif

  // firstly, replace the entire struct alloca with individual allocs
  // of each fields. These new allocas will be placed at the begining
  // of the function.

  BasicBlock *FirstBlockInFunc = &F.getEntryBlock();
  Instruction *FirstInstInFunc = FirstBlockInFunc->getFirstNonPHI();

  StructType *AllocaStructTy = dyn_cast<StructType>(AI->getAllocatedType());
  if (AllocaStructTy == NULL) {

    // since the alloca instruction passes my previous test
    // the alloca type has to be struct

    assert(false && "the type of the alloca instruction isn't StructType");
    return NULL;
  }

  // I keep a vector of new alloca instructions for fields of
  // the old alloca instructions in order so that when I am about
  // to replace the usage of old alloca, I know how to deal with it.
  // NameCountForNewAllocas is used for handling getelementptr

  // Also, maintain a vector of new aggregate type allocas because
  // they may also be eliminatable.

  vector<AllocaInst *> NewAllocsForAI;
  vector<AllocaInst *> *SubAggrAllocas = new vector<AllocaInst *>();

  // add an alloca for each field of the struct

  for (unsigned i = 0, NumElements = AllocaStructTy->getNumElements();
       i < NumElements; ++i) {

    Type *IthAllocaTy = AllocaStructTy->getElementType(i);

    // depends on whether the first basic block in this function has 
    // an instruction or not.

    AllocaInst *IthAllocaInst;

    if (FirstInstInFunc != NULL) {
      IthAllocaInst = new AllocaInst(
                      /*Ty=*/IthAllocaTy,
                      /*AddrSpace=*/AI->getType()->getAddressSpace(), 
                      /*ArraySize=*/NULL,
                      /*Name=*/"",    // let LLVM resolves naming conflict
                      /*InsertBefore=*/FirstInstInFunc);
    } else {
      IthAllocaInst = new AllocaInst(
                      /*Ty=*/IthAllocaTy,
                      /*AddrSpace=*/AI->getType()->getAddressSpace(), 
                      /*ArraySize=*/NULL,
                      /*Name=*/"",    // let LLVM resolves naming conflict
                      /*InsertAtEnd=*/FirstBlockInFunc);
    }

    #ifdef _SROA_ZIANG_DEBUG
    errs() << i << "th inst: [" << *IthAllocaInst << "]\n";
    #endif

    NewAllocsForAI.push_back(IthAllocaInst);

    if (IthAllocaTy->isAggregateType()) {
      SubAggrAllocas->push_back(IthAllocaInst);
    }
  }

  // Secondly, I handle each usage of the replaced alloca.
  // because of the previous test canBeEliminatedStructAlloca()
  // each usage should only be U1 or U2
  // in various places below I've insert some asserts to check that

  // maintain a vector of old usages of the old allocas to be erased later
  // we can't erase them in the for loop because it might affect the iterator

  vector<Instruction *> UserInstToBeErased;

  for (User *U : AI->users()) {
    if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(U)) {
      replaceU1TypeGetElementPtr(/*GEPI=*/GEPI,
                                 /*F=*/F,
                                 /*NewAllocas=*/NewAllocsForAI);
      UserInstToBeErased.push_back(GEPI);
    } else if (CmpInst *CI = dyn_cast<CmpInst>(U)) {
      replaceU2TypeEqOrNe(/*CI=*/CI,
                          /*F=*/F,
                          /*NewAllocas=*/NewAllocsForAI);
      UserInstToBeErased.push_back(CI);
    } else {
      assert(false && "One user of the replaced alloca isn't U1 or U2");
    }
  }

  // Finally, eliminate all the uses of the old allocas and then
  // erase the old alloca

  for (Instruction *UserInst : UserInstToBeErased) {
    UserInst->eraseFromParent();
  }
  AI->eraseFromParent();

  // return a vector of new aggregate type allocas because
  // they may also be eliminatable.

  return SubAggrAllocas;
}


// when eliminating an alloca, the U1 type getelementptr use should be
// modified apropriately
// a helper function used in eliminateStructAlloca()
void SROA::replaceU1TypeGetElementPtr(GetElementPtrInst *GEPI,
                                      Function &F,
                                      vector<AllocaInst *> &NewAllocs) {
  
  #ifdef _SROA_ZIANG_DEBUG
  errs() << "replaceU1TypeGetElementPtr: [" << *GEPI << "]\n";
  #endif

  unsigned NumOperands = GEPI->getNumOperands();
  if (NumOperands < 3) {
    assert(false && "the numOperands for getelementptr can't be < 3");
    return;
  }

  // figure out which field this getelementptr is trying to access

  ConstantInt *FieldConst = dyn_cast<ConstantInt>(GEPI->getOperand(2));
  if (FieldConst == NULL) {
    assert(false && "the third argument of U1 Type must be a ConstantInt");
    return;
  }
  int FieldConstVal = FieldConst->getSExtValue();

  AllocaInst *StructFieldValue = NewAllocs[FieldConstVal];

  // there are essentially two types of getelementptr to replace

  if (NumOperands == 3) {
    // 1. getelementptr %struct %1, i32 0, i32 n (where n is an integer)
    // for this type of getelementptr, we can simply get rid of the whole GEPI
    // and replace each use of it with the alloca of the individual field

    GEPI->replaceAllUsesWith(/*V=*/StructFieldValue);
  } else {
    // 2. getelementptr %struct %1, i32 0, i32 n, i32 a ...
    // for this type, it means that the new alloca is also an aggregate type
    // in this case, create a new getelementptr inst to replace the old one
    // with one less layer of pointer arithmetic:
    // getelementptr %struct %1, i32 0, i32 n, i32 a ...
    // -> getelementptr %field %2, i32 0, i32 a, ...

    vector<Value *> IdxVec;
    IdxVec.push_back(GEPI->getOperand(1));  // ConstantInt of value 0
    for (unsigned i = 3; i < NumOperands; i++) {
      IdxVec.push_back(GEPI->getOperand(i));
    }
    ArrayRef<Value *> IdxList(IdxVec);

    GetElementPtrInst *NewGEPI = GetElementPtrInst::CreateInBounds(
                                  /*PointeeType=*/StructFieldValue->getAllocatedType(),
                                  /*Ptr=*/StructFieldValue,
                                  /*IdxList=*/IdxList,
                                  /*NameStr=*/"",       // let LLVM resolves naming conflicts
                                  /*InsertBefore=*/GEPI);

    #ifdef _SROA_ZIANG_DEBUG
    errs() << "NewGEPI: " << *NewGEPI << "\n";
    #endif

    GEPI->replaceAllUsesWith(/*V=*/NewGEPI);
  }
}


// when eliminating an alloca, the U2 type nq/ne use should be
// modified apropriately
// a helper function used in eliminateStructAlloca()
void SROA::replaceU2TypeEqOrNe(CmpInst *CI,
                               Function &F,
                               vector<AllocaInst *> &NewAllocs) {

  #ifdef _SROA_ZIANG_DEBUG
  errs() << "replaceU2TypeEqOrNe: [" << *CI << "]\n";
  #endif

  // if the alloca is used in a comparison instruction with NULL
  // I can directly infer whether the result is true or false based on the predicate
  // if the predicate is EQ, then the result is false.
  // if the predicate is NE, then the result is true.

  ConstantInt *ComparisonResult;

  CmpInst::Predicate CIPredicate = CI->getPredicate();
  if (CIPredicate == CmpInst::ICMP_EQ) {
    ComparisonResult = ConstantInt::getFalse(F.getContext());
  } else if (CIPredicate == CmpInst::ICMP_NE) {
    ComparisonResult = ConstantInt::getTrue(F.getContext());
  } else {
    assert(false && "CIPredicate isn't ICMP_EQ or ICMP_NE");
    return;
  }

  // Then, I simply propagate the comparison result

  CI->replaceAllUsesWith(ComparisonResult);
}


//===----------------------------------------------------------------------===//
//                extra credit - eliminate small array
//===----------------------------------------------------------------------===//
// 

// this file only fulfills the basic requirement
// for additional work, see ScalarReplAggregates-ziangw2-ec.cpp
