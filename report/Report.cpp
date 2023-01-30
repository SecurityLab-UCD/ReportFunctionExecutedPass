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
  BasicBlock &entry = F.getEntryBlock();
  IRBuilder<> Builder(F.getContext());
  Builder.SetInsertPoint(&entry);

  std::vector<Type *> FTyArgs;
  FTyArgs.push_back(Type::getInt8PtrTy(F.getContext()));
  // create function type with return type,
  // argument types and if const argument
  std::string func_name = "printf";
  FunctionType *FTy =
      FunctionType::get(Type::getInt32Ty(entry.getContext()), FTyArgs, true);
  Module *M = F.getParent();
  FunctionCallee printF = M->getOrInsertFunction(func_name, FTy);
  std::string msg = "hello: " + F.getName().str() + "\n";
  Value *name = Builder.CreateGlobalStringPtr(msg, "name");
  std::vector<Value *> sep_argv = {name};
  CallInst::Create(printF, sep_argv, func_name, &*entry.begin());
  return true;
}

char ReportPass::ID = 0;
static RegisterPass<ReportPass> X("func_report",
                                  "Report Function Executed Pass", true, true);
