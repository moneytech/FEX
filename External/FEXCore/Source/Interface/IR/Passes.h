#pragma once

namespace FEXCore::IR {
class Pass;
class RegisterAllocationPass;
class RegisterAllocationData;

FEXCore::IR::Pass* CreateConstProp(bool InlineConstants);
FEXCore::IR::Pass* CreateContextLoadStoreElimination();
FEXCore::IR::Pass* CreateSyscallOptimization();
FEXCore::IR::Pass* CreateDeadFlagCalculationEliminination();
FEXCore::IR::Pass* CreateDeadStoreElimination();
FEXCore::IR::Pass* CreatePassDeadCodeElimination();
FEXCore::IR::Pass* CreateIRCompaction();
FEXCore::IR::RegisterAllocationPass* CreateRegisterAllocationPass(FEXCore::IR::Pass* CompactionPass, bool OptimizeSRA);
FEXCore::IR::Pass* CreateStaticRegisterAllocationPass();

namespace Validation {
FEXCore::IR::Pass* CreateIRValidation();
FEXCore::IR::Pass* CreatePhiValidation();
FEXCore::IR::Pass* CreateValueDominanceValidation();
}
}

