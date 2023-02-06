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

Constant *make_global_str(Module *M, std::string str) {
  // https://lists.llvm.org/pipermail/llvm-dev/2010-June/032075.html
  // https://stackoverflow.com/questions/51809274/llvm-defining-strings-and-arrays-via-c-api

  LLVMContext &Ctx = M->getContext();
  std::vector<llvm::Constant *> chars(str.size());
  for (unsigned int i = 0; i < str.size(); i++) {
    chars[i] = ConstantInt::get(Type::getInt8Ty(Ctx), str[i]);
  }
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
  std::vector<Value *> args;

  if (fname == "main") {
    // dump report at main exit

    // find function pointer to dump_count
    FunctionType *dump_FTy =
        FunctionType::get(Type::getInt32Ty(Ctx), {}, false);
    M->getOrInsertFunction("dump_count", dump_FTy);
    Function *dump = M->getFunction("dump_count");
    Value *dump_ptr = ConstantExpr::getBitCast(&*dump, Type::getInt8PtrTy(Ctx));

    // insert call to atexit at entry of main
    std::vector<Type *> atexit_argsTy = {Type::getInt8PtrTy(Ctx)};
    FunctionType *atexit_FTy =
        FunctionType::get(Type::getInt32Ty(Ctx), atexit_argsTy, false);
    args = {dump_ptr};
    FunctionCallee dump_atexit = M->getOrInsertFunction("atexit", atexit_FTy);
    CallInst::Create(dump_atexit, args, "atexit", &*entry.begin());

  } else {

    std::vector<Type *> ArgTys = {};
    ArgTys.push_back(Type::getInt8PtrTy(Ctx));
    FunctionType *FTy = FunctionType::get(Type::getInt32Ty(Ctx), ArgTys, true);
    FunctionCallee report = M->getOrInsertFunction("report_count", FTy);

    Value *str_ptr = make_global_str(M, fname);
    args = {str_ptr};

    CallInst::Create(report, args, "report_count", &*entry.begin());
  }
  return true;
}

char ReportPass::ID = 0;
static RegisterPass<ReportPass> X("report", "Report Function Executed Pass",
                                  true, true);
