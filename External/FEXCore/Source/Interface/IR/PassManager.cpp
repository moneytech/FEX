#include "Interface/IR/Passes.h"
#include "Interface/IR/Passes/RegisterAllocationPass.h"
#include "Interface/IR/PassManager.h"

#include <FEXCore/Config/Config.h>

namespace FEXCore::IR {

void PassManager::AddDefaultPasses(bool InlineConstants, bool StaticRegisterAllocation) {
  FEXCore::Config::Value<bool> DisablePasses{FEXCore::Config::CONFIG_DEBUG_DISABLE_OPTIMIZATION_PASSES, false};

  if (!DisablePasses()) {
    InsertPass(CreateContextLoadStoreElimination());

    InsertPass(CreateDeadStoreElimination());
    InsertPass(CreatePassDeadCodeElimination());
    InsertPass(CreateConstProp(InlineConstants));

    ////// InsertPass(CreateDeadFlagCalculationEliminination());

    InsertPass(CreateSyscallOptimization());
    InsertPass(CreatePassDeadCodeElimination());

    // only do SRA if enabled and JIT
    if (InlineConstants && StaticRegisterAllocation)
      InsertPass(CreateStaticRegisterAllocationPass());

    InsertOldPass(Old::CreateContextLoadStoreElimination());
    InsertOldPass(Old::CreateDeadFlagStoreElimination());
    InsertOldPass(Old::CreateDeadGPRStoreElimination());
    InsertOldPass(Old::CreateDeadFPRStoreElimination());
    InsertOldPass(CreatePassDeadCodeElimination());
    InsertOldPass(Old::CreateConstProp(InlineConstants));

    ////// InsertPass(CreateDeadFlagCalculationEliminination());

    InsertOldPass(CreateSyscallOptimization());
    InsertOldPass(CreatePassDeadCodeElimination());

    // only do SRA if enabled and JIT
    if (InlineConstants && StaticRegisterAllocation)
      InsertOldPass(CreateStaticRegisterAllocationPass());
  }
  else {
    // only do SRA if enabled and JIT
    if (InlineConstants && StaticRegisterAllocation)
      InsertPass(CreateStaticRegisterAllocationPass());
  }

  CompactionPass = CreateIRCompaction();
  // If the IR is compacted post-RA then the node indexing gets messed up and the backend isn't able to find the register assigned to a node
  // Compact before IR, don't worry about RA generating spills/fills
  InsertPass(CompactionPass);
  InsertOldPass(CompactionPass);
}

void PassManager::AddDefaultValidationPasses() {
#if defined(ASSERTIONS_ENABLED) && ASSERTIONS_ENABLED
  InsertValidationPass(Validation::CreatePhiValidation());
  InsertValidationPass(Validation::CreateIRValidation());
  InsertValidationPass(Validation::CreateValueDominanceValidation());
#endif
}

void PassManager::InsertRegisterAllocationPass(bool OptimizeSRA) {
    RAPass = IR::CreateRegisterAllocationPass(CompactionPass, OptimizeSRA);
    InsertPass(RAPass);

    RAPassOld = Old::CreateRegisterAllocationPass(CompactionPass, OptimizeSRA);
    InsertOldPass(RAPassOld);
}

bool PassManager::Run(IREmitter *IREmit) {

  IREmitter* old = new IREmitter();
  old->CopyData(*IREmit);

  bool Changed = false;
  for (auto &Pass : Passes) {
    Changed |= Pass->Run(IREmit);
  }

  for (auto &Pass : PassesOld) {
    Pass->Run(old);
  }

  std::stringstream NewStr;
  auto NewIR = IREmit->ViewIR();
  FEXCore::IR::Dump(&NewStr, &NewIR, reinterpret_cast<IR::RegisterAllocationPass*>(RAPass));

  std::stringstream OldStr;
  auto OldIR = old->ViewIR();
  FEXCore::IR::Dump(&OldStr, &OldIR, reinterpret_cast<IR::RegisterAllocationPass*>(RAPassOld));

  delete old;

  if (NewStr.str() != OldStr.str()) {
    puts("Missmatching IR\n");
    puts("NEW:\n");
    puts(NewStr.str().c_str());
    puts("OLD:\n");
    puts(OldStr.str().c_str());
  }

#if defined(ASSERTIONS_ENABLED) && ASSERTIONS_ENABLED
  for (auto const &Pass : ValidationPasses) {
    Changed |= Pass->Run(IREmit);
  }
#endif

  return Changed;
}

}
