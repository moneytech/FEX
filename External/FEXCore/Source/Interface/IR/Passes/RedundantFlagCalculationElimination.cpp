#include "Interface/IR/PassManager.h"
#include "Interface/Core/OpcodeDispatcher.h"

namespace FEXCore::IR {

class DeadFlagCalculationEliminination final : public FEXCore::IR::Pass {
public:
  bool Run(IREmitter *IREmit) override;
};

struct FlagInfo {
  uint32_t reads { 0 };
  uint32_t writes { 0 };
  uint32_t kill { 0 };
};

/**
 * @brief (UNSAFE) This pass removes flag calculations that will otherwise be unused INSIDE of that block
 *
 * Compilers don't really do any form of cross-block flag allocation like they do RA with GPRs.
 * This ends up with them recalculating flags across blocks regardless of if it is actually possible to reuse the flags.
 * This is an additional burden in x86 that most instructions change flags when called, so it is easier to recalculate anyway.
 *
 * This is unsafe since handwritten code can easily break this assumption.
 * This may be more interesting with full function level recompilation since flags definitely won't be used across function boundaries.
 *
 */
bool DeadFlagCalculationEliminination::Run(IREmitter *IREmit) {
  std::map<OrderedNode*, FlagInfo> FlagMap;

  bool Changed = false;
  auto CurrentIR = IREmit->ViewIR();
  uintptr_t ListBegin = CurrentIR.GetListData();
  uintptr_t DataBegin = CurrentIR.GetData();

  auto Begin = CurrentIR.begin();
  auto Op = Begin();

  OrderedNode *RealNode = Op->GetNode(ListBegin);
  auto HeaderOp = RealNode->Op(DataBegin)->CW<FEXCore::IR::IROp_IRHeader>();
  LogMan::Throw::A(HeaderOp->Header.Op == OP_IRHEADER, "First op wasn't IRHeader");

  {
    OrderedNode *BlockNode = HeaderOp->Blocks.GetNode(ListBegin);

    while (1) {
      auto BlockIROp = BlockNode->Op(DataBegin)->CW<FEXCore::IR::IROp_CodeBlock>();
      LogMan::Throw::A(BlockIROp->Header.Op == OP_CODEBLOCK, "IR type failed to be a code block");

      // We grab these nodes this way so we can iterate easily
      auto CodeBegin = CurrentIR.at(BlockIROp->Begin);
      auto CodeLast = CurrentIR.at(BlockIROp->Last);
      while (1) {
        auto CodeOp = CodeBegin();
        OrderedNode *CodeNode = CodeOp->GetNode(ListBegin);
        auto IROp = CodeNode->Op(DataBegin);

        if (IROp->Op == OP_STOREFLAG) {
          auto Op = IROp->CW<IR::IROp_StoreFlag>();
          FlagMap[BlockNode].writes |= 1 << Op->Flag;
        }
        else if (IROp->Op == OP_LOADFLAG) {
          auto Op = IROp->CW<IR::IROp_LoadFlag>();
          FlagMap[BlockNode].reads |= 1 << Op->Flag;
        }

        // CodeLast is inclusive. So we still need to dump the CodeLast op as well
        if (CodeBegin == CodeLast) {
          break;
        }
        ++CodeBegin;
      }

      if (BlockIROp->Next.ID() == 0) {
        break;
      } else {
        BlockNode = BlockIROp->Next.GetNode(ListBegin);
      }
    }
  }

  {
    OrderedNode *BlockNode = HeaderOp->Blocks.GetNode(ListBegin);

    while (1) {
      auto BlockIROp = BlockNode->Op(DataBegin)->CW<FEXCore::IR::IROp_CodeBlock>();
      LogMan::Throw::A(BlockIROp->Header.Op == OP_CODEBLOCK, "IR type failed to be a code block");

      // We grab these nodes this way so we can iterate easily
      auto CodeBegin = CurrentIR.at(BlockIROp->Begin);
      auto CodeLast = CurrentIR.at(BlockIROp->Last);
      while (1) {
        auto CodeOp = CodeBegin();
        OrderedNode *CodeNode = CodeOp->GetNode(ListBegin);
        auto IROp = CodeNode->Op(DataBegin);

        if (IROp->Op == OP_JUMP) {
          auto Op = IROp->CW<IR::IROp_Jump>();
          OrderedNode *TargetNode = Op->Header.Args[0].GetNode(ListBegin);

          FlagMap[BlockNode].kill = FlagMap[TargetNode].writes & ~(FlagMap[TargetNode].reads);
        }
        else if (IROp->Op == OP_CONDJUMP) {
          auto Op = IROp->CW<IR::IROp_CondJump>();

          OrderedNode *TrueTargetNode = Op->Header.Args[1].GetNode(ListBegin);
          OrderedNode *FalseTargetNode = Op->Header.Args[2].GetNode(ListBegin);

          FlagMap[BlockNode].kill = FlagMap[TrueTargetNode].writes & ~(FlagMap[TrueTargetNode].reads);
          FlagMap[BlockNode].kill &= FlagMap[FalseTargetNode].writes & ~(FlagMap[FalseTargetNode].reads);
        }

        // CodeLast is inclusive. So we still need to dump the CodeLast op as well
        if (CodeBegin == CodeLast) {
          break;
        }
        ++CodeBegin;
      }

      if (BlockIROp->Next.ID() == 0) {
        break;
      } else {
        BlockNode = BlockIROp->Next.GetNode(ListBegin);
      }
    }
  }

  {
    OrderedNode *BlockNode = HeaderOp->Blocks.GetNode(ListBegin);

    while (1) {
      auto BlockIROp = BlockNode->Op(DataBegin)->CW<FEXCore::IR::IROp_CodeBlock>();
      LogMan::Throw::A(BlockIROp->Header.Op == OP_CODEBLOCK, "IR type failed to be a code block");

      // We grab these nodes this way so we can iterate easily
      auto CodeBegin = CurrentIR.at(BlockIROp->Begin);
      auto CodeLast = CurrentIR.at(BlockIROp->Last);
      while (1) {
        auto CodeOp = CodeBegin();
        OrderedNode *CodeNode = CodeOp->GetNode(ListBegin);
        auto IROp = CodeNode->Op(DataBegin);

        if (IROp->Op == OP_STOREFLAG) {
          auto Op = IROp->CW<IR::IROp_StoreFlag>();
          if (FlagMap[BlockNode].kill & (1 << Op->Flag)) {
            // printf("FLANGO!\n");
            IREmit->Remove(CodeNode);
          }
        }

        // CodeLast is inclusive. So we still need to dump the CodeLast op as well
        if (CodeBegin == CodeLast) {
          break;
        }
        ++CodeBegin;
      }

      if (BlockIROp->Next.ID() == 0) {
        break;
      } else {
        BlockNode = BlockIROp->Next.GetNode(ListBegin);
      }
    }
  }

  return Changed;
}

FEXCore::IR::Pass* CreateDeadFlagCalculationEliminination() {
  return new DeadFlagCalculationEliminination{};
}

}
