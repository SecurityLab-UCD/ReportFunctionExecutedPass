#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <string>
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
  Module *M = F.getParent();

  if (fname == "main") {
    // report at end of main, before it returns
    for (Function::iterator bb = F.begin(); bb != F.end(); bb++) {
      for (BasicBlock::iterator it = bb->begin(); it != bb->end(); it++) {
        if ((std::string)it->getOpcodeName() == "ret") {
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

    Type *ArgTy = Type::getInt32Ty(F.getContext());
    FunctionType *FTy =
        FunctionType::get(Type::getInt32Ty(entry.getContext()), ArgTy, true);
    FunctionCallee report = M->getOrInsertFunction("report_count", FTy);

    // use char args to avoid allocating space in IR
    Value *len =
        ConstantInt::get(Type::getInt32Ty(F.getContext()), fname.length());
    std::vector<Value *> args = {len};
    for (char c : fname) {
      Value *fc = ConstantInt::get(Type::getInt8Ty(F.getContext()), c);
      args.push_back(fc);
    }

    CallInst::Create(report, args, "report_count", &*entry.begin());
  }
  return true;
}

char ReportPass::ID = 0;
static RegisterPass<ReportPass> X("report", "Report Function Executed Pass",
                                  true, true);
