#include "Interface/IR/PassManager.h"
#include "Interface/Core/OpcodeDispatcher.h"

#include <FEXCore/IR/IR.h>

#include <array>

namespace FEXCore::IR {

class DeadCodeElimination final : public FEXCore::IR::Pass {
  bool Run(IREmitter *IREmit) override;

private:
  void markUsed(OrderedNodeWrapper *CodeOp, IROp_Header *IROp);
};

bool DeadCodeElimination::Run(IREmitter *IREmit) {
  auto CurrentIR = IREmit->ViewIR();

  uintptr_t ListBegin = CurrentIR.GetListData();
  uintptr_t DataBegin = CurrentIR.GetData();

  auto Begin = CurrentIR.begin();
  auto Op = Begin();

  OrderedNode *RealNode = Op->GetNode(ListBegin);
  auto HeaderOp = RealNode->Op(DataBegin)->CW<FEXCore::IR::IROp_IRHeader>();
  LogMan::Throw::A(HeaderOp->Header.Op == OP_IRHEADER, "First op wasn't IRHeader");

  OrderedNode *BlockNode = HeaderOp->Blocks.GetNode(ListBegin);

  int NumRemoved = 0;

  while (1) {
    auto BlockIROp = BlockNode->Op(DataBegin)->CW<FEXCore::IR::IROp_CodeBlock>();
    LogMan::Throw::A(BlockIROp->Header.Op == OP_CODEBLOCK, "IR type failed to be a code block");

    // We grab these nodes this way so we can iterate easily
    auto CodeBegin = CurrentIR.at(BlockIROp->Begin);
    auto CodeLast = CurrentIR.at(BlockIROp->Last);
    auto CodeEnd = CurrentIR.at(BlockIROp->Last);

    while (1) {
      auto CodeOp = CodeLast();
      OrderedNode *CodeNode = CodeOp->GetNode(ListBegin);
      auto IROp = CodeNode->Op(DataBegin);

      // A BFE that extract all bits is a nop
      if (IROp->Op == OP_BFE) {
        auto Op = IROp->CW<IR::IROp_Bfe>();

        if (IROp->Size == Op->Header.Args[0].GetNode(ListBegin)->Op(DataBegin)->Size && Op->Width == (IROp->Size * 8) && Op->lsb == 0 ) {
          IREmit->ReplaceAllUsesWithInclusive(CodeNode, Op->Header.Args[0].GetNode(ListBegin), CodeLast, CodeEnd);
        }
      }

      //And, OR with itself are NOPS
      if (IROp->Op == OP_AND || IROp->Op == OP_OR) {
        auto Op = IROp->CW<IR::IROp_And>();

        if (Op->Header.Args[0].ID() == Op->Header.Args[1].ID()) {
          //printf("AND/OR IDD\n");
          IREmit->ReplaceAllUsesWithInclusive(CodeNode, Op->Header.Args[0].GetNode(ListBegin), CodeLast, CodeEnd);
        }
      }

      if (IROp->Op == OP_XOR) {
        auto Op = IROp->CW<IR::IROp_And>();

        if (Op->Header.Args[0].ID() == Op->Header.Args[1].ID()) {
          //printf("XOR IDD\n");
          IREmit->SetWriteCursor(CodeNode);
          IREmit->ReplaceAllUsesWithInclusive(CodeNode, IREmit->_Constant(0), CodeLast, CodeEnd);
        }
      }

      // Skip over anything that has side effects
      // Use count tracking can't safely remove anything with side effects
      if (!IR::HasSideEffects(IROp->Op)) {
        if (CodeNode->GetUses() == 0) {
          NumRemoved++;
          IREmit->Remove(CodeNode);
        }
      }

      if (CodeLast == CodeBegin) {
        break;
      }
      --CodeLast;
    }

    if (BlockIROp->Next.ID() == 0) {
      break;
    } else {
      BlockNode = BlockIROp->Next.GetNode(ListBegin);
    }
  }

  return NumRemoved != 0;
}

void DeadCodeElimination::markUsed(OrderedNodeWrapper *CodeOp, IROp_Header *IROp) {

}


FEXCore::IR::Pass* CreatePassDeadCodeElimination() {
  return new DeadCodeElimination{};
}


}
