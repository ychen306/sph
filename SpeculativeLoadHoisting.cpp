#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MustExecute.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"

using namespace llvm;

struct SpeculativeLoadHoistingPass
    : public PassInfoMixin<SpeculativeLoadHoistingPass> {
  PreservedAnalyses run(Function &, FunctionAnalysisManager &);
};

static void buildPasses(PassBuilder &PB) {
  PB.registerPipelineParsingCallback(
      [](StringRef Name, FunctionPassManager &FPM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (Name != "sph")
          return false;
        FPM.addPass(SpeculativeLoadHoistingPass());
        return true;
      });

  PB.registerScalarOptimizerLateEPCallback(
      [](FunctionPassManager &FPM, OptimizationLevel) {
        FPM.addPass(LoopSimplifyPass());
        FPM.addPass(SpeculativeLoadHoistingPass());
      });
}

static bool isInvalidated(MemoryLocation Loc, Loop *L, AliasAnalysis &AA) {
  for (auto *BB : L->blocks()) {
    for (auto &I : *BB) {
      if (isModSet(AA.getModRefInfo(&I, Loc)))
        return true;
    }
  }
  for (Loop *SubL : *L)
    if (isInvalidated(Loc, SubL, AA))
      return true;
  return false;
}

static bool requiresReload(MemoryLocation Loc, Loop *L, AliasAnalysis &AA) {
  for (auto *BB : L->blocks()) {
    for (auto &I : *BB) {
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        auto StoreLoc = MemoryLocation::get(SI);
        if (AA.alias(Loc, StoreLoc) && Loc.Size != StoreLoc.Size)
          return true;
        // Can forward the store value without reload
        continue;
      }

      if (isModSet(AA.getModRefInfo(&I, Loc)))
        return true;
    }
  }
  for (Loop *SubL : *L)
    if (isInvalidated(Loc, SubL, AA))
      return true;
  return false;
}

static bool runOnLoop(Loop *L, Function &F, LoopInfo &LI, DominatorTree &DT,
                      AliasAnalysis &AA) {
  bool Changed = false;
  for (Loop *SubL : *L)
    Changed |= runOnLoop(SubL, F, LI, DT, AA);
  auto *Preheader = L->getLoopPreheader();
  if (!Preheader)
    return Changed;

  ICFLoopSafetyInfo SafetyInfo;
  SafetyInfo.computeLoopSafetyInfo(L);
  DenseMap<std::pair<Value *, Type *>, LoadInst *> Candidates;
  for (auto &BB : L->blocks()) {
    for (auto &I : *BB) {
      auto *Load = dyn_cast<LoadInst>(&I);
      if (!Load)
        continue;
      auto *Ptr = Load->getPointerOperand();
      auto *PtrInst = dyn_cast<Instruction>(Ptr);
      if (PtrInst && L->contains(PtrInst))
        continue;
      if (!SafetyInfo.isGuaranteedToExecute(*Load, &DT, L))
        continue;
      if (requiresReload(MemoryLocation::get(Load), L, AA))
        continue;
      Candidates.try_emplace({Ptr, Load->getType()}, Load);
    }
  }

  Changed |= !Candidates.empty();

  SmallVector<AllocaInst *, 8> Allocas;

  auto &Entry = F.getEntryBlock();
  for (auto &KV : Candidates) {
    auto [Ptr, Ty] = KV.first;
    LoadInst *Load = KV.second;
    auto *Preload = new LoadInst(Ty, Ptr, Load->getName() + ".preload",
                                 Preheader->getTerminator());
    errs() << "Promoting load from " << *Ptr << '\n';
    auto *Alloca =
        new AllocaInst(Ty, 0 /*address space*/, nullptr /*array size*/,
                       Load->getName() + ".repl", Entry.getTerminator());
    Allocas.push_back(Alloca);
    new StoreInst(Preload, Alloca, Preheader->getTerminator());

    auto Loc = MemoryLocation::get(Load);

    for (auto *BB : L->blocks()) {
      for (auto &I : *BB) {
        auto *Load2 = dyn_cast<LoadInst>(&I);
        if (Load2 && Load2->getType() == Ty &&
            Load2->getPointerOperand() == Ptr) {
          auto *Reload =
              new LoadInst(Ty, Alloca, Load->getName() + ".reload", Load2);
          Load2->replaceAllUsesWith(Reload);
          continue;
        }
        auto *Store = dyn_cast<StoreInst>(&I);
        if (!Store)
          continue;
        if (!isModSet(AA.getModRefInfo(Store, Loc)))
          continue;
        // Forward the store value if the pointers are the same
        IRBuilder<> IRB(Store);
        auto *Eq =
            IRB.CreateICmp(CmpInst::ICMP_EQ, Store->getPointerOperand(), Ptr);
        auto *OrigVal = IRB.CreateLoad(Ty, Alloca);
        auto *Cast = IRB.CreateBitOrPointerCast(Store->getValueOperand(), Ty);
        auto *NewVal = IRB.CreateSelect(Eq, Cast, OrigVal);
        IRB.CreateStore(NewVal, Alloca);
      }
    }
  }
  PromoteMemToReg(Allocas, DT);

  return Changed;
}

PreservedAnalyses
SpeculativeLoadHoistingPass::run(Function &F, FunctionAnalysisManager &AM) {
  auto &LI = AM.getResult<LoopAnalysis>(F);
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  auto &AA = AM.getResult<AAManager>(F);

  bool Changed = false;

  for (Loop *L : LI) {
    Changed |= runOnLoop(L, F, LI, DT, AA);
  }

  if (!Changed)
    return PreservedAnalyses::all();
  // TODO: maybe we preserve LI and DT?
  return PreservedAnalyses::none();
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "sph", LLVM_VERSION_STRING, buildPasses};
}
