#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <stdlib.h>
#include <string>
#include <tuple>
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

bool isStructPtrTy(Type *T) {
  return T->isPointerTy() && T->getPointerElementType()->isStructTy();
}

/**
 * @brief Expand a struct value into a vector of its elements
 * @param v: struct value that can be casted to ConstantStruct
 * @param F: function that the struct value is in
 * @param I: instruction to insert the GetElementPtrInst before
 * @param expend_level: max number of levels of struct to expand if nested
 * @return vector of values in the struct
 */
std::vector<Value *> ExpandStruct(Value *v, Function *F, Instruction *I,
                                  int expend_level = 5) {
  /**
   * todo: consider the difference between
   * struct S { int s }; struct T { float t };
   * struct ST { struct s, struct t } v.s. struct ST { int s, float t }
   */
  std::vector<Value *> Expanded;
  if (expend_level == 0) {
    return Expanded;
  }

  LLVMContext &Ctx = F->getContext();
  StructType *StructTy =
      dyn_cast<StructType>(v->getType()->getPointerElementType());
  if (!StructTy) {
    errs() << "Calling ExpandStruct on non-struct type\n";
    return Expanded;
  }

  auto &entry = F->getEntryBlock();
  int len = StructTy->getStructNumElements();
  for (int i = 0; i < len; i++) {
    // recursively expanding, so the first index is always 0
    Value *indices[] = {ConstantInt::get(Type::getInt32Ty(Ctx), 0),
                        ConstantInt::get(Type::getInt32Ty(Ctx), i)};
    GetElementPtrInst *gep =
        GetElementPtrInst::CreateInBounds(StructTy, v, indices, "", I);

    Type *BaseTy = gep->getType()->getPointerElementType();
    if (BaseTy->isPointerTy()) {
      /**
       * https://github.com/SecurityLab-UCD/ReportFunctionExecutedPass/pull/4#discussion_r1149958372
       * struct in a struct is **not** always a pointer
       * therefore we need to check the type of gep's pointer operand
       * if no, there might be a Exit Code 139 Segmentation fault for
       * referencing null pointer
       */
      Value *po = gep->getPointerOperand();
      Type *poTy = po->getType();
      if (po && poTy->isPointerTy() && isStructPtrTy(poTy)) {
        std::vector<Value *> elems = ExpandStruct(po, F, I, expend_level - 1);
        Expanded.insert(Expanded.end(), elems.begin(), elems.end());
      }
    } else {
      Expanded.push_back(gep);
    }
  }

  return Expanded;
}

std::string GetTyStr(std::vector<Value *> &elems, std::string delimiter) {
  std::string TyStr;
  llvm::raw_string_ostream rso(TyStr);
  for (Value *elem : elems) {
    elem->getType()->print(rso);
    rso << delimiter;
  }
  return TyStr;
}

std::vector<std::string> ffmpeg_skip{"abort_codec_experimental"};

bool ReportPass::runOnFunction(Function &F) {
  std::string fname = F.getName().str();
  Module *M = F.getParent();
  BasicBlock &entry = F.getEntryBlock();
  LLVMContext &Ctx = F.getContext();
  std::string file_name = F.getParent()->getSourceFileName();
  std::string file_name_wo_ext =
      file_name.substr(0, file_name.find_last_of('.'));

  errs() << "ReportPass: " << file_name << " " << fname << "\n";
  if (std::find(ffmpeg_skip.begin(), ffmpeg_skip.end(), fname) !=
      ffmpeg_skip.end()) {
    errs() << "Skip " << fname << "\n";
    return false;
  }
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
    Instruction *AtexitInst =
        CallInst::Create(DumpAtexit, AtexitArgs, "atexit", &*entry.begin());

  } else {

    // report param
    std::vector<Type *> ParamArgTys(
        {Type::getInt1Ty(Ctx), Type::getInt8PtrTy(Ctx), Type::getInt32Ty(Ctx)});
    FunctionType *ParamFTy =
        FunctionType::get(Type::getInt32Ty(Ctx), ParamArgTys, true);
    FunctionCallee ReportParam =
        M->getOrInsertFunction("report_param", ParamFTy);

    // use something that would never be a substring of function name or llvm
    // type as delimiter, for now use ">>="
    // "," will be in function type
    std::string delimiter = ">>=";
    // NOTE type string
    // the first token is function name
    // the middle tokens are parameter types
    // the last token is return type
    // func_name>>=param1_type>>=...>>=rnt_type>>=
    std::string TypeStr = file_name_wo_ext + "_" + fname + delimiter;
    std::vector<Value *> ParamArgs;
    llvm::raw_string_ostream rso(TypeStr);
    for (Value &Arg : F.args()) {
      if (isStructPtrTy(Arg.getType())) {
        std::vector<Value *> elems = ExpandStruct(&Arg, &F, &*entry.begin());
        ParamArgs.insert(ParamArgs.end(), elems.begin(), elems.end());

        // todo: get this type string from ExpandStruct
        rso << GetTyStr(elems, delimiter);
      } else {
        ParamArgs.push_back(&Arg);
        Arg.getType()->print(rso);
      }
      rso << delimiter;
    }
    APInt ParamArgsLen = APInt(32, ParamArgs.size(), false);
    ParamArgs.insert(ParamArgs.begin(), ConstantInt::get(Ctx, ParamArgsLen));

    // find rnt value and terminating instruction
    // todo: expand return struct type
    Instruction *ReportInsertB4 = &*entry.begin();
    bool has_rnt = false;
    for (BasicBlock &BB : F) {
      Instruction *Term = BB.getTerminator();

      // match on different type of terminator
      // empty else-if branches are reserved for later changes
      if (ReturnInst *RI = dyn_cast<ReturnInst>(Term)) {
        ReportInsertB4 = RI;
        if (RI->getNumOperands() == 1) {
          Value *ReturnValue = RI->getOperand(0);
          if (isStructPtrTy(ReturnValue->getType())) {
            std::vector<Value *> elems = ExpandStruct(ReturnValue, &F, RI);
            ParamArgs.insert(ParamArgs.end(), elems.begin(), elems.end());

            // todo: get this type string from ExpandStruct
            rso << GetTyStr(elems, delimiter);
          } else {
            ParamArgs.push_back(ReturnValue);
          }
          has_rnt = true;
        }
      } else if (SwitchInst *SI = dyn_cast<SwitchInst>(Term)) {
      } else if (BranchInst *BI = dyn_cast<BranchInst>(Term)) {
      } else if (IndirectBrInst *IBI = dyn_cast<IndirectBrInst>(Term)) {
      } else if (CallBrInst *CBI = dyn_cast<CallBrInst>(Term)) {
      } else if (InvokeInst *II = dyn_cast<InvokeInst>(Term)) {
      } else if (ResumeInst *RI = dyn_cast<ResumeInst>(Term)) {
      } else if (CatchSwitchInst *CSI = dyn_cast<CatchSwitchInst>(Term)) {
      } else if (CatchReturnInst *CRI = dyn_cast<CatchReturnInst>(Term)) {
      } else if (CleanupReturnInst *CuRI = dyn_cast<CleanupReturnInst>(Term)) {
      } else if (UnreachableInst *UI = dyn_cast<UnreachableInst>(Term)) {
        errs() << F.getName() << " Found Unreachable Terminator:\n\t";
        UI->print(errs());
        errs() << "\n";
      } else {
        errs() << "Unknown Terminator:\n\t";
        Term->print(errs());
      }
    }

    // add rnt type to the end of type string
    if (has_rnt) {
      F.getReturnType()->print(rso);
      rso << delimiter;
    }

    ParamArgs.insert(ParamArgs.begin(), MakeGlobalString(M, TypeStr));

    APInt HasRnt = APInt(1, has_rnt, false);
    ParamArgs.insert(ParamArgs.begin(), ConstantInt::get(Ctx, HasRnt));

    CallInst::Create(ReportParam, ParamArgs, "report_param", ReportInsertB4);
  }
  return true;
}

char ReportPass::ID = 0;
static RegisterPass<ReportPass> X("report", "Report Function Executed Pass",
                                  true, true);

static void registerMyPass(const PassManagerBuilder &,
                           legacy::PassManagerBase &PM) {
  PM.add(new ReportPass());
}
static RegisterStandardPasses
    RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible, registerMyPass);
