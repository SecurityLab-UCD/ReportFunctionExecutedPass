#include "llvm/ADT/APInt.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/AbstractCallSite.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
using namespace llvm;

#define DEBUG_TYPE "report"
STATISTIC(ReportCounter, "Counts number of functions executed");

namespace {
struct ReportPass : public FunctionPass {
  static char ID;
  ReportPass() : FunctionPass(ID) {}

  virtual bool runOnFunction(Function &F);
};
} // namespace

bool ReportPass::runOnFunction(Function &F) {
  std::string fname = F.getName().str();
  IRBuilder<> Builder(F.getContext());
  Module *M = F.getParent();

  if (fname == "main") {
    // report at end of main, before it returns
    for (Function::iterator bb = F.begin(); bb != F.end(); bb++) {
      for (BasicBlock::iterator it = bb->begin(); it != bb->end(); it++) {
        if ((std::string)it->getOpcodeName() == "ret") {
          Builder.SetInsertPoint(&*bb, it);
          FunctionType *FTy = FunctionType::get(
              Type::getInt32Ty((*it).getContext()), {}, false);
          std::vector<Value *> args = {};
          FunctionCallee dump = M->getOrInsertFunction("dump_count", FTy);
          CallInst::Create(dump, {}, "dump_count", &*it);
        }
      }
    }
  } else {
    // report at the entry of F
    BasicBlock &entry = F.getEntryBlock();
    Builder.SetInsertPoint(&entry);

    std::vector<Type *> FTyArgs;
    FTyArgs.push_back(Type::getInt8PtrTy(F.getContext()));
    FunctionType *FTy =
        FunctionType::get(Type::getInt32Ty(entry.getContext()), FTyArgs, false);
    FunctionCallee report = M->getOrInsertFunction("report_count", FTy);
    Value *name = Builder.CreateGlobalStringPtr(fname, "name");
    std::vector<Value *> args = {name};
    CallInst::Create(report, args, "report_count", &*entry.begin());
  }
  return true;
}

char ReportPass::ID = 0;
static RegisterPass<ReportPass> X("report", "Report Function Executed Pass",
                                  true, true);
