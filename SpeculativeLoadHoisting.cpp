#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

struct SpeculativeLoadHoistingPass
    : public PassInfoMixin<SpeculativeLoadHoistingPass> {
  PreservedAnalyses run(Function &, FunctionAnalysisManager &);
};

static void buildPasses(PassBuilder &PB) {
  PB.registerPipelineParsingCallback(
      [](StringRef Name, FunctionPassManager &FPM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (Name == "sph") {
          FPM.addPass(SpeculativeLoadHoistingPass());
          return true;
        }
        return false;
      });

  PB.registerScalarOptimizerLateEPCallback(
      [](FunctionPassManager &FPM, OptimizationLevel) {
        FPM.addPass(SpeculativeLoadHoistingPass());
      });
}

PreservedAnalyses SpeculativeLoadHoistingPass::run(Function &F, FunctionAnalysisManager &AM) {
  errs() << "!!! here\n";
  return PreservedAnalyses::all();
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "sph", LLVM_VERSION_STRING, buildPasses};
}
