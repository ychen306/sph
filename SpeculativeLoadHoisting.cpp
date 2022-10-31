#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"

using namespace llvm;

struct SpeculativeLoadHoistingPass : public PassInfoMixin<SpeculativeLoadHoistingPass> {
  PreservedAnalyses run(Function &, FunctionAnalysisManager &);
};
