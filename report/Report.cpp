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
  // insert a function call to reportFunctionExec()
  std::string fname = F.getName().str();
  BasicBlock &entry = F.getEntryBlock();
  IRBuilder<> Builder(F.getContext());
  Builder.SetInsertPoint(&entry);

  std::vector<Type *> FTyArgs;
  FTyArgs.push_back(Type::getInt8PtrTy(F.getContext()));
  // create function type with return type,
  // argument types and if const argument
  FunctionType *FTy =
      FunctionType::get(Type::getInt32Ty(entry.getContext()), FTyArgs, false);
  Module *M = F.getParent();
  FunctionCallee printF = M->getOrInsertFunction("count", FTy);
  Value *name = Builder.CreateGlobalStringPtr(fname, "name");
  std::vector<Value *> args = {name};
  CallInst::Create(printF, args, "count", &*entry.begin());
  return true;
}

char ReportPass::ID = 0;
static RegisterPass<ReportPass> X("func_report",
                                  "Report Function Executed Pass", true, true);
