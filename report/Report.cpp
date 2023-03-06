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

#include <stdlib.h>
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

Constant *MakeGlobalString(Module *M, std::string str) {
  // https://lists.llvm.org/pipermail/llvm-dev/2010-June/032075.html
  // https://stackoverflow.com/questions/51809274/llvm-defining-strings-and-arrays-via-c-api

  LLVMContext &Ctx = M->getContext();
  std::vector<llvm::Constant *> chars(str.size());
  for (unsigned int i = 0; i < str.size(); i++) {
    chars[i] = ConstantInt::get(Type::getInt8Ty(Ctx), str[i]);
  }
  // add a zero terminator
  chars.push_back(llvm::ConstantInt::get(Type::getInt8Ty(Ctx), 0));

  auto init = ConstantArray::get(
      ArrayType::get(Type::getInt8Ty(Ctx), chars.size()), chars);
  GlobalVariable *v = new GlobalVariable(
      *M, init->getType(), true, GlobalVariable::ExternalLinkage, init, str);
  return ConstantExpr::getBitCast(v, Type::getInt8Ty(Ctx)->getPointerTo());
}

bool ReportPass::runOnFunction(Function &F) {
  std::string fname = F.getName().str();
  Module *M = F.getParent();
  BasicBlock &entry = F.getEntryBlock();
  LLVMContext &Ctx = F.getContext();

  if (fname == "main") {
    // dump report at main exit

    // find function pointer to dump_count
    std::vector<Type *> dump_ArgTys({});
    FunctionType *DumpFTy =
        FunctionType::get(Type::getVoidTy(Ctx), dump_ArgTys, false);
    M->getOrInsertFunction("dump_count", DumpFTy);
    Function *dump = M->getFunction("dump_count");

    // insert call to atexit at entry of main
    std::vector<Type *> AtexitArgsTys({dump->getType()});
    FunctionType *AtexitFTy =
        FunctionType::get(Type::getInt32Ty(Ctx), AtexitArgsTys, false);
    std::vector<Value *> AtexitArgs({dump});
    FunctionCallee DumpAtexit = M->getOrInsertFunction("atexit", AtexitFTy);
    CallInst::Create(DumpAtexit, AtexitArgs, "atexit", &*entry.begin());

  } else {

    // report param
    std::vector<Type *> ParamArgTys(
        {Type::getInt8PtrTy(Ctx), Type::getInt32Ty(Ctx)});
    FunctionType *ParamFTy =
        FunctionType::get(Type::getInt32Ty(Ctx), ParamArgTys, true);
    FunctionCallee ReportParam =
        M->getOrInsertFunction("report_param", ParamFTy);

    // use something that would never be a substring of function name or llvm
    // type as delimiter, for now use ">>="
    // "," will be in function type
    std::string delimiter = ">>=";
    std::string ParamMetadata = fname + delimiter;
    std::vector<Value *> ParamArgs;
    llvm::raw_string_ostream rso(ParamMetadata);
    for (Value &Arg : F.args()) {
      ParamArgs.push_back(&Arg);
      Arg.getType()->print(rso);
      rso << delimiter;
    }
    APInt ParamArgsLen = APInt(32, ParamArgs.size(), false);
    ParamArgs.insert(ParamArgs.begin(), ConstantInt::get(Ctx, ParamArgsLen));
    ParamArgs.insert(ParamArgs.begin(), MakeGlobalString(M, ParamMetadata));

    CallInst::Create(ReportParam, ParamArgs, "report_param", &*entry.begin());
  }
  return true;
}

char ReportPass::ID = 0;
static RegisterPass<ReportPass> X("report", "Report Function Executed Pass",
                                  true, true);
