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
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <signal.h>
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
                                  int expend_level = 1) {
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

bool is_in(std::string str, std::vector<std::string> &vec) {
  return std::find(vec.begin(), vec.end(), str) != vec.end();
}

std::vector<Value *> ReportInputs(Function &F, FunctionCallee &ReportParam,
                                  raw_string_ostream &rso,
                                  std::string delimiter) {
  Instruction *EntryInst = &*F.getEntryBlock().begin();
  LLVMContext &Ctx = F.getContext();
  Module *M = F.getParent();

  std::vector<Value *> InputArgs;
  std::vector<Value *> PointerArgs;
  for (Value &Arg : F.args()) {
    if (isStructPtrTy(Arg.getType())) {
      std::vector<Value *> elems = ExpandStruct(&Arg, &F, EntryInst);
      InputArgs.insert(InputArgs.end(), elems.begin(), elems.end());
      PointerArgs.insert(PointerArgs.end(), elems.begin(), elems.end());

      // todo: get this type string from ExpandStruct
      rso << GetTyStr(elems, delimiter);
    } else {
      InputArgs.push_back(&Arg);
      Arg.getType()->print(rso);

      if (Arg.getType()->isPointerTy()) {
        PointerArgs.push_back(&Arg);
      }
    }
    rso << delimiter;
  }

  APInt InputArgsLen = APInt(32, InputArgs.size(), false);
  InputArgs.insert(InputArgs.begin(), ConstantInt::get(Ctx, InputArgsLen));
  InputArgs.insert(InputArgs.begin(), MakeGlobalString(M, rso.str()));
  APInt IsRnt = APInt(1, false, false);
  InputArgs.insert(InputArgs.begin(), ConstantInt::get(Ctx, IsRnt));
  CallInst::Create(ReportParam, InputArgs, "report_param", EntryInst);
  return PointerArgs;
}

void ReportOutputs(Function &F, FunctionCallee &ReportParam,
                   std::vector<Value *> &PrevPointerInputs,
                   raw_string_ostream &rso, std::string delimiter) {
  std::vector<Value *> RetVals;
  LLVMContext &Ctx = F.getContext();
  Module *M = F.getParent();

  // find rnt value and terminating instruction
  Instruction *ReportInsertB4 = nullptr;
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
          RetVals.insert(RetVals.end(), elems.begin(), elems.end());

          // todo: get this type string from ExpandStruct
          rso << GetTyStr(elems, delimiter);
        } else {
          RetVals.push_back(ReturnValue);
          ReturnValue->getType()->print(rso);
          rso << delimiter;
        }
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
      LLVM_DEBUG({
        dbgs() << F.getName() << " Found Unreachable Terminator:\n\t";
        UI->print(dbgs());
        dbgs() << "\n";
      });
    } else {
      LLVM_DEBUG({
        dbgs() << "Unknown Terminator:\n\t";
        Term->print(dbgs());
      });
    }
  }

  if (ReportInsertB4) {
    // reinsert pointer inputs
    for (Value *PointerInput : PrevPointerInputs) {
      RetVals.push_back(PointerInput);
      PointerInput->getType()->print(rso);
      rso << delimiter;
    }
    APInt RetValsLen = APInt(32, RetVals.size(), false);
    RetVals.insert(RetVals.begin(), ConstantInt::get(Ctx, RetValsLen));
    RetVals.insert(RetVals.begin(), MakeGlobalString(M, rso.str()));
    APInt IsRnt = APInt(1, true, false);
    RetVals.insert(RetVals.begin(), ConstantInt::get(Ctx, IsRnt));
    CallInst::Create(ReportParam, RetVals, "report_param", ReportInsertB4);
  }
}

/**
 * @brief Insert a signal handler before the given instruction
 * @param F Function to insert signal handler into
 * @param InsertBefore Instruction to insert signal handler before
 * @param SignalNum Signal number to register handler for
 */
void InsertSignalBefore(Function *F, Instruction *InsertBefore, int SignalNum) {
  LLVMContext &Ctx = F->getContext();
  Module *M = F->getParent();

  // find function pointer to signal handler
  std::vector<Type *> SignalHandlerArgsTys({Type::getInt32Ty(Ctx)});
  FunctionType *SignalHandlerFTy =
      FunctionType::get(Type::getVoidTy(Ctx), SignalHandlerArgsTys, false);
  M->getOrInsertFunction("signal_handler", SignalHandlerFTy);
  Function *SignalHandler = M->getFunction("signal_handler");

  // find function pointer to signal
  std::vector<Type *> SignalArgsTys(
      {Type::getInt32Ty(Ctx), SignalHandler->getType()});
  FunctionType *SignalFTy =
      FunctionType::get(Type::getVoidTy(Ctx), SignalArgsTys, false);
  FunctionCallee Signal = M->getOrInsertFunction("signal", SignalFTy);
  Value *SIGTERM_Value = ConstantInt::get(Type::getInt32Ty(Ctx), SignalNum);
  std::vector<Value *> SignalArgs({SIGTERM_Value, SignalHandler});

  // insert call to signal at entry of main
  Instruction *SignalInst =
      CallInst::Create(Signal, SignalArgs, "", InsertBefore);
}

bool ReportPass::runOnFunction(Function &F) {
  std::string fname = F.getName().str();
  Module *M = F.getParent();
  BasicBlock &entry = F.getEntryBlock();
  Instruction *EntryInst = &*entry.begin();
  LLVMContext &Ctx = F.getContext();
  std::string file_name = F.getParent()->getSourceFileName();
  std::string file_name_wo_ext =
      file_name.substr(0, file_name.find_last_of('.'));

  LLVM_DEBUG(dbgs() << "ReportPass: " << file_name << " " << fname << "\n");
  // insert atexit() at LLVMFuzzerTestOneInput function so fuzzers can dump
  if (fname == "main" || fname == "LLVMFuzzerTestOneInput") {
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
        CallInst::Create(DumpAtexit, AtexitArgs, "atexit", EntryInst);

    InsertSignalBefore(&F, AtexitInst, SIGTERM);
    InsertSignalBefore(&F, AtexitInst, SIGINT);

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
    std::string TypeStrStarter = file_name_wo_ext + "_" + fname + delimiter;

    // insert call to report at entry with input parameters
    std::string InputsTyStr = TypeStrStarter;
    raw_string_ostream irso(InputsTyStr);
    std::vector<Value *> PrevPointerInputs =
        ReportInputs(F, ReportParam, irso, delimiter);

    // todo: insert call to report at exit with return values
    // and pointer inputs
    std::string OutputsTyStr = TypeStrStarter;
    raw_string_ostream orso(OutputsTyStr);
    ReportOutputs(F, ReportParam, PrevPointerInputs, orso, delimiter);
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
