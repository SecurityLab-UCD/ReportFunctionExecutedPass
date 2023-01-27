#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <string>
#include <vector>
using namespace llvm;

#define DEBUG_TYPE "report"
STATISTIC(ReportCounter, "Counts number of functions executed");

namespace {
struct ReportPass : public ModulePass {
  static char ID;
  ReportPass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M);
  virtual bool runOnFunction(Function &F, Module &M);

  bool init(Module &M);      // create global variables to store exec count
  bool finialize(Module &M); // write count to file
};
} // namespace

bool ReportPass::runOnModule(Module &M) {
  bool modified = init(M);

  for (Module::iterator it = M.begin(); it != M.end(); it++) {
    modified |= runOnFunction(*it, M);
  }
  modified |= finialize(M);

  return modified;
}

bool ReportPass::runOnFunction(Function &F, Module &M) {
  bool modified = false;
  // errs() << F.getName() << "\n";
  for (auto it = F.begin(); it != F.end(); it++) {

    if (it == F.begin()) {
      Type *I64Ty = Type::getInt64Ty(M.getContext());
      IRBuilder<> Builder(F.getContext());
      Twine s = F.getName() + ".glob";
      Value *atomicCounter = M.getOrInsertGlobal(s.str(), I64Ty);
      Value *One = ConstantInt::get(Type::getInt64Ty(F.getContext()), 1);
      // ensure the address will be a multiple of 16
      Align align_16 = Align(16);

      new AtomicRMWInst(AtomicRMWInst::Add, atomicCounter,
                        ConstantInt::get(Type::getInt64Ty(F.getContext()), 1),
                        align_16, AtomicOrdering::SequentiallyConsistent,
                        SyncScope::System, it->getFirstNonPHI());
    }
  }

  return modified;
}

bool ReportPass::init(Module &M) {
  IRBuilder<> Builder(M.getContext());
  Function *mainFunc = M.getFunction("main");

  // not the main module
  if (!mainFunc)
    return false;

  Type *I64Ty = Type::getInt64Ty(M.getContext());

  Module::FunctionListType &functionList = M.getFunctionList();
  for (Function &f : functionList) {
    // create global variable to store function exec count
    Value *atomic_counter =
        new GlobalVariable(M, I64Ty, false, GlobalValue::CommonLinkage,
                           ConstantInt::get(I64Ty, 0), f.getName() + ".glob");
  }

  return true;
}

bool ReportPass::finialize(Module &M) {
  IRBuilder<> Builder(M.getContext());
  Function *mainFunc = M.getFunction("main");

  // not the main module
  if (!mainFunc)
    return false;
  // Build printf function handle
  // specify the first argument, i8* is the return type of CreateGlobalStringPtr
  std::vector<Type *> FTyArgs;
  FTyArgs.push_back(Type::getInt8PtrTy(M.getContext()));
  // create function type with return type,
  // argument types and if const argument
  FunctionType *FTy =
      FunctionType::get(Type::getInt32Ty(M.getContext()), FTyArgs, true);
  // create function if not extern or defined
  FunctionCallee printF = M.getOrInsertFunction("printf", FTy);
  std::string fname = mainFunc->getParent()->getSourceFileName();

  for (Function::iterator bb = mainFunc->begin(); bb != mainFunc->end(); bb++) {
    for (BasicBlock::iterator it = bb->begin(); it != bb->end(); it++) {
      // insert at the end of main function
      if ((std::string)it->getOpcodeName() == "ret") {
        // insert printf at the end of main function, before return function
        Builder.SetInsertPoint(&*bb, it);

        // print a separation for later analysis
        Value *sep = Builder.CreateGlobalStringPtr(
            "--- " + fname + " exec report ---\n", "sepFormat");
        std::vector<Value *> sep_argv = {sep};
        CallInst::Create(printF, sep_argv, "printf", &*it);

        Value *format_long;

        auto &functionList = M.getFunctionList(); // gets the list of functions
        Type *I64Ty = Type::getInt64Ty(M.getContext());
        for (auto &function : functionList) { // iterates over the list

          std::string func_name = function.getName().str();
          format_long = Builder.CreateGlobalStringPtr(func_name + ": %ld\n",
                                                      "formatLong");
          std::vector<Value *> argVec;
          argVec.push_back(format_long);
          Twine s = function.getName() + ".glob";
          Value *atomicCounter = M.getGlobalVariable(s.str(), I64Ty);

          Value *finalVal = new LoadInst(I64Ty, atomicCounter,
                                         function.getName() + ".val", &*it);

          argVec.push_back(finalVal);
          CallInst::Create(printF, argVec, "printf", &*it);
        }
      }
    }
  }
  return true;
}
char ReportPass::ID = 0;
static RegisterPass<ReportPass> X("report", "Report Function Executed Pass",
                                  true, false);
