#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

#define DEBUG_TYPE "report"
STATISTIC(ReportCounter, "Counts number of functions greeted");

namespace {
struct ReportPass : public FunctionPass {
  static char ID;
  ReportPass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) {
    ++ReportCounter;
    errs() << "called " << F.getName() << " with type ";
    F.getFunctionType()->print(errs());
    errs() << "\n";
    return false;
  }
};
} // namespace

char ReportPass::ID = 0;
static RegisterPass<ReportPass> X("reportpass", "Report Function Executed Pass",
                                  true /* Only looks at CFG */,
                                  false /* Analysis Pass */);

// static void registerReportPass(const PassManagerBuilder &,
//                                legacy::PassManagerBase &PM) {
//   PM.add(new ReportPass());
// }
// static RegisterStandardPasses
//     RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
//     registerReportPass);
