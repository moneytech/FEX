#include "Interface/Core/OpcodeDispatcher.h"
#include <FEXCore/Core/CoreState.h>
#include <climits>
#include <cstddef>
#include <cstdint>

#include <FEXCore/Core/X86Enums.h>

namespace FEXCore::IR {

auto OpToIndex = [](uint8_t Op) constexpr -> uint8_t {
  switch (Op) {
  // Group 1
  case 0x80: return 0;
  case 0x81: return 1;
  case 0x82: return 2;
  case 0x83: return 3;
  // Group 2
  case 0xC0: return 0;
  case 0xC1: return 1;
  case 0xD0: return 2;
  case 0xD1: return 3;
  case 0xD2: return 4;
  case 0xD3: return 5;
  // Group 3
  case 0xF6: return 0;
  case 0xF7: return 1;
  // Group 4
  case 0xFE: return 0;
  // Group 5
  case 0xFF: return 0;
  // Group 11
  case 0xC6: return 0;
  case 0xC7: return 1;
  }
  return 0;
};

#define OpcodeArgs [[maybe_unused]] FEXCore::X86Tables::DecodedOp Op

void OpDispatchBuilder::SyscallOp(OpcodeArgs) {
  constexpr size_t SyscallArgs = 7;
  constexpr std::array<uint64_t, SyscallArgs> GPRIndexes = {
    FEXCore::X86State::REG_RAX,
    FEXCore::X86State::REG_RDI,
    FEXCore::X86State::REG_RSI,
    FEXCore::X86State::REG_RDX,
    FEXCore::X86State::REG_R10,
    FEXCore::X86State::REG_R8,
    FEXCore::X86State::REG_R9,
  };

  auto SyscallOp = _Syscall(
    _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs) + GPRIndexes[0] * 8),
    _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs) + GPRIndexes[1] * 8),
    _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs) + GPRIndexes[2] * 8),
    _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs) + GPRIndexes[3] * 8),
    _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs) + GPRIndexes[4] * 8),
    _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs) + GPRIndexes[5] * 8),
    _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs) + GPRIndexes[6] * 8));

  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), SyscallOp);
}

void OpDispatchBuilder::LEAOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1, false);
  StoreResult(Op, Src, -1);
}

void OpDispatchBuilder::NOPOp(OpcodeArgs) {
}

void OpDispatchBuilder::RETOp(OpcodeArgs) {
  auto Constant = _Constant(8);

  auto OldSP = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]));

  auto NewRIP = _LoadMem(8, OldSP, 8);

  OrderedNode *NewSP;
  if (Op->OP == 0xC2) {
    auto Offset = LoadSource(Op, Op->Src1, Op->Flags, -1);
    NewSP = _Add(_Add(OldSP, Constant), Offset);
  }
  else {
    NewSP = _Add(OldSP, Constant);
  }

  // Store the new stack pointer
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]), NewSP);

  // Store the new RIP
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, rip), NewRIP);
  _ExitFunction();
  BlockSetRIP = true;
}

void OpDispatchBuilder::SecondaryALUOp(OpcodeArgs) {
  FEXCore::IR::IROps IROp;
#define OPD(group, prefix, Reg) (((group - FEXCore::X86Tables::TYPE_GROUP_1) << 6) | (prefix) << 3 | (Reg))
  switch (Op->OP) {
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 0):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 0):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 0):
    IROp = FEXCore::IR::IROps::OP_ADD;
  break;
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 1):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 1):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 1):
    IROp = FEXCore::IR::IROps::OP_OR;
  break;
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 4):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 4):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 4):
    IROp = FEXCore::IR::IROps::OP_AND;
  break;
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 5):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 5):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 5):
    IROp = FEXCore::IR::IROps::OP_SUB;
  break;
  case OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF6), 5):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF7), 5):
    IROp = FEXCore::IR::IROps::OP_MUL;
  break;
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 6):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 6):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 6):
    IROp = FEXCore::IR::IROps::OP_XOR;
  break;
  default:
    IROp = FEXCore::IR::IROps::OP_LAST;
    LogMan::Msg::A("Unknown ALU Op: 0x%x", Op->OP);
  break;
  };
#undef OPD
  // X86 basic ALU ops just do the operation between the destination and a single source
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto ALUOp = _Add(Dest, Src);
  // Overwrite our IR's op type
  ALUOp.first->Header.Op = IROp;

  StoreResult(Op, ALUOp, -1);

  // Flags set
  {
    auto Size = GetSrcSize(Op) * 8;
    switch (IROp) {
    case FEXCore::IR::IROps::OP_ADD:
      GenerateFlags_ADD(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
    break;
    case FEXCore::IR::IROps::OP_SUB:
      GenerateFlags_SUB(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
    break;
    case FEXCore::IR::IROps::OP_MUL:
      GenerateFlags_MUL(Op, _Bfe(Size, 0, ALUOp), _MulH(Dest, Src));
    break;
    case FEXCore::IR::IROps::OP_AND:
    case FEXCore::IR::IROps::OP_XOR:
    case FEXCore::IR::IROps::OP_OR: {
      GenerateFlags_Logical(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
    break;
    }
    default: break;
    }
  }
}

void OpDispatchBuilder::ADCOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto CF = GetRFLAG(FEXCore::X86State::RFLAG_CF_LOC);

  auto ALUOp = _Add(_Add(Dest, Src), CF);

  StoreResult(Op, ALUOp, -1);
  GenerateFlags_ADC(Op, ALUOp, Dest, Src, CF);
}

void OpDispatchBuilder::SBBOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto CF = GetRFLAG(FEXCore::X86State::RFLAG_CF_LOC);

  auto ALUOp = _Sub(_Sub(Dest, Src), CF);
  StoreResult(Op, ALUOp, -1);
  GenerateFlags_SBB(Op, ALUOp, Dest, Src, CF);
}

void OpDispatchBuilder::PUSHOp(OpcodeArgs) {
  uint8_t Size = GetSrcSize(Op);
  OrderedNode *Src;
  if (Op->OP == 0x68 || Op->OP == 0x6A) { // Immediate Push
    Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  }
  else {
    if (Op->OP == 0xFF && Size == 4) LogMan::Msg::A("Woops. Can't do 32bit for this PUSH op");
    Src = LoadSource(Op, Op->Dest, Op->Flags, -1);
  }

  auto Constant = _Constant(Size);

  auto OldSP = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]));

  auto NewSP = _Sub(OldSP, Constant);

  // Store the new stack pointer
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]), NewSP);

  // Store our value to the new stack location
  _StoreMem(Size, NewSP, Src, Size);
}

void OpDispatchBuilder::POPOp(OpcodeArgs) {
  uint8_t Size = GetSrcSize(Op);
  auto Constant = _Constant(Size);

  auto OldSP = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]));

  auto NewGPR = _LoadMem(Size, OldSP, Size);

  auto NewSP = _Add(OldSP, Constant);

  if (Op->OP == 0x8F && Size == 4) LogMan::Msg::A("Woops. Can't do 32bit for this POP op");

  // Store the new stack pointer
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]), NewSP);

  // Store what we loaded from the stack
  StoreResult(Op, NewGPR, -1);
}

void OpDispatchBuilder::LEAVEOp(OpcodeArgs) {
  // First we move RBP in to RSP and then behave effectively like a pop
  uint8_t Size = GetSrcSize(Op);
  auto Constant = _Constant(Size);

  LogMan::Throw::A(Size == 8, "Can't handle a LEAVE op with size %d", Size);

  auto OldBP = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RBP]));

  auto NewGPR = _LoadMem(Size, OldBP, Size);

  auto NewSP = _Add(OldBP, Constant);

  // Store the new stack pointer
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]), NewSP);

  // Store what we loaded to RBP
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RBP]), NewGPR);
}

void OpDispatchBuilder::CALLOp(OpcodeArgs) {
  BlockSetRIP = true;
  auto ConstantPC = _Constant(Op->PC + Op->InstSize);

  OrderedNode *JMPPCOffset = LoadSource(Op, Op->Src1, Op->Flags, -1);

  auto NewRIP = _Add(JMPPCOffset, ConstantPC);

  auto ConstantPCReturn = _Constant(Op->PC + Op->InstSize);

  auto ConstantSize = _Constant(8);
  auto OldSP = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]));

  auto NewSP = _Sub(OldSP, ConstantSize);

  // Store the new stack pointer
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]), NewSP);

  _StoreMem(8, NewSP, ConstantPCReturn, 8);

  // Store the RIP
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, rip), NewRIP);
  _ExitFunction(); // If we get here then leave the function now
}

void OpDispatchBuilder::CALLAbsoluteOp(OpcodeArgs) {
  BlockSetRIP = true;

  OrderedNode *JMPPCOffset = LoadSource(Op, Op->Src1, Op->Flags, -1);

  auto ConstantPCReturn = _Constant(Op->PC + Op->InstSize);

  auto ConstantSize = _Constant(8);
  auto OldSP = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]));

  auto NewSP = _Sub(OldSP, ConstantSize);

  // Store the new stack pointer
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSP]), NewSP);

  _StoreMem(8, NewSP, ConstantPCReturn, 8);

  // Store the RIP
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, rip), JMPPCOffset);
  _ExitFunction(); // If we get here then leave the function now
}

void OpDispatchBuilder::CondJUMPOp(OpcodeArgs) {
  BlockSetRIP = true;

  auto ZeroConst = _Constant(0);
  auto OneConst = _Constant(1);
  IRPair<IROp_Header> SrcCond;

  IRPair<IROp_Constant> TakeBranch;
  IRPair<IROp_Constant> DoNotTakeBranch;
  TakeBranch = _Constant(1);
  DoNotTakeBranch = _Constant(0);

  switch (Op->OP) {
    case 0x70:
    case 0x80: { // JO - Jump if OF == 1
      auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_NEQ,
          Flag, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x71:
    case 0x81: { // JNO - Jump if OF == 0
      auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          Flag, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x72:
    case 0x82: { // JC - Jump if CF == 1
      auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_CF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_NEQ,
          Flag, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x73:
    case 0x83: { // JNC - Jump if CF == 0
      auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_CF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          Flag, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x74:
    case 0x84: { // JE - Jump if ZF == 1
      auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_NEQ,
          Flag, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x75:
    case 0x85: { // JNE - Jump if ZF == 0
      auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          Flag, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x76:
    case 0x86: { // JNA - Jump if CF == 1 || ZC == 1
      auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
      auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_CF_LOC);
      auto Check = _Or(Flag1, Flag2);
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          Check, OneConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x77:
    case 0x87: { // JA - Jump if CF == 0 && ZF == 0
      auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
      auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_CF_LOC);
      auto Check = _Or(Flag1, _Lshl(Flag2, _Constant(1)));
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          Check, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x78:
    case 0x88: { // JS - Jump if SF == 1
      auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_NEQ,
          Flag, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x79:
    case 0x89: { // JNS - Jump if SF == 0
      auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          Flag, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x7A:
    case 0x8A: { // JP - Jump if PF == 1
      auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_PF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_NEQ,
          Flag, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x7B:
    case 0x8B: { // JNP - Jump if PF == 0
      auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_PF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          Flag, ZeroConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x7C: // SF <> OF
    case 0x8C: {
      auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
      auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_NEQ,
          Flag1, Flag2, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x7D: // SF = OF
    case 0x8D: {
      auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
      auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          Flag1, Flag2, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x7E: // ZF = 1 || SF <> OF
    case 0x8E: {
      auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
      auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
      auto Flag3 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);

      auto Select1 = _Select(FEXCore::IR::COND_EQ,
          Flag1, OneConst, OneConst, ZeroConst);

      auto Select2 = _Select(FEXCore::IR::COND_NEQ,
          Flag2, Flag3, OneConst, ZeroConst);

      auto Check = _Or(Select1, Select2);
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          Check, OneConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    case 0x7F: // ZF = 0 && SF = OF
    case 0x8F: {
      auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
      auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
      auto Flag3 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);

      auto Select1 = _Select(FEXCore::IR::COND_EQ,
          Flag1, ZeroConst, OneConst, ZeroConst);

      auto Select2 = _Select(FEXCore::IR::COND_EQ,
          Flag2, Flag3, OneConst, ZeroConst);

      auto Check = _And(Select1, Select2);
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          Check, OneConst, TakeBranch, DoNotTakeBranch);
    break;
    }
    default: LogMan::Msg::A("Unknown Jmp Op: 0x%x\n", Op->OP); return;
  }

  LogMan::Throw::A(Op->Src1.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_LITERAL, "Src1 needs to be literal here");

  uint64_t Target = Op->PC + Op->InstSize + Op->Src1.TypeLiteral.Literal;

  auto TrueBlock = JumpTargets.find(Target);
  auto FalseBlock = JumpTargets.find(Op->PC + Op->InstSize);

  // Fallback
  {
    auto CondJump = _CondJump(SrcCond);

    // Taking branch block
    if (TrueBlock != JumpTargets.end()) {
      SetTrueJumpTarget(CondJump, TrueBlock->second.BlockEntry);
    }
    else {
      // Make sure to start a new block after ending this one
      auto JumpTarget = CreateNewCodeBlock();
      SetTrueJumpTarget(CondJump, JumpTarget);
      SetCurrentCodeBlock(JumpTarget);

      auto RIPOffset = LoadSource(Op, Op->Src1, Op->Flags, -1);
      auto RIPTargetConst = _Constant(Op->PC + Op->InstSize);

      auto NewRIP = _Add(RIPOffset, RIPTargetConst);

      // Store the new RIP
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, rip), NewRIP);
      _ExitFunction();
    }

    // Failure to take branch
    if (FalseBlock != JumpTargets.end()) {
      SetFalseJumpTarget(CondJump, FalseBlock->second.BlockEntry);
    }
    else {
      // Make sure to start a new block after ending this one
      auto JumpTarget = CreateNewCodeBlock();
      SetFalseJumpTarget(CondJump, JumpTarget);
      SetCurrentCodeBlock(JumpTarget);

      // Leave block
      auto RIPTargetConst = _Constant(Op->PC + Op->InstSize);

      // Store the new RIP
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, rip), RIPTargetConst);
      _ExitFunction();
    }
  }
}

void OpDispatchBuilder::JUMPOp(OpcodeArgs) {
  BlockSetRIP = true;

  // This is just an unconditional relative literal jump
  // XXX: Reenable
  if (Multiblock) {
    LogMan::Throw::A(Op->Src1.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_LITERAL, "Src1 needs to be literal here");
    uint64_t Target = Op->PC + Op->InstSize + Op->Src1.TypeLiteral.Literal;
    auto JumpBlock = JumpTargets.find(Target);
    if (JumpBlock != JumpTargets.end()) {
      _Jump(GetNewJumpBlock(Target));
    }
    else {
      // If the block isn't a jump target then we need to create an exit block
      auto Jump = _Jump();

      auto JumpTarget = CreateNewCodeBlock();
      SetJumpTarget(Jump, JumpTarget);
      SetCurrentCodeBlock(JumpTarget);
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, rip), _Constant(Target));
      _ExitFunction();
    }
    return;
  }

  // Fallback
  {
    // This source is a literal
    auto RIPOffset = LoadSource(Op, Op->Src1, Op->Flags, -1);

    auto RIPTargetConst = _Constant(Op->PC + Op->InstSize);

		auto NewRIP = _Add(RIPOffset, RIPTargetConst);

    // Store the new RIP
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, rip), NewRIP);
    _ExitFunction();
  }
}

void OpDispatchBuilder::JUMPAbsoluteOp(OpcodeArgs) {
  BlockSetRIP = true;
  // This is just an unconditional jump
  // This uses ModRM to determine its location
  // No way to use this effectively in multiblock
  auto RIPOffset = LoadSource(Op, Op->Src1, Op->Flags, -1);

  // Store the new RIP
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, rip), RIPOffset);
  _ExitFunction();
}

void OpDispatchBuilder::SETccOp(OpcodeArgs) {
  enum CompareType {
    COMPARE_ZERO,
    COMPARE_NOTZERO,
    COMPARE_EQUALMASK,
    COMPARE_OTHER,
  };
  uint32_t FLAGMask;
  CompareType Type = COMPARE_OTHER;
  OrderedNode *SrcCond;

  auto ZeroConst = _Constant(0);
  auto OneConst = _Constant(1);

  switch (Op->OP) {
  case 0x90:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_OF_LOC;
    Type = COMPARE_NOTZERO;
  break;
  case 0x91:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_OF_LOC;
    Type = COMPARE_ZERO;
  break;
  case 0x92:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_CF_LOC;
    Type = COMPARE_NOTZERO;
  break;
  case 0x93:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_CF_LOC;
    Type = COMPARE_ZERO;
  break;
  case 0x94:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_ZF_LOC;
    Type = COMPARE_NOTZERO;
  break;
  case 0x95:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_ZF_LOC;
    Type = COMPARE_ZERO;
  break;
  case 0x96:
    FLAGMask = (1 << FEXCore::X86State::RFLAG_ZF_LOC) | (1 << FEXCore::X86State::RFLAG_CF_LOC);
    Type = COMPARE_NOTZERO;
  break;
  case 0x97:
    FLAGMask = (1 << FEXCore::X86State::RFLAG_ZF_LOC) | (1 << FEXCore::X86State::RFLAG_CF_LOC);
    Type = COMPARE_ZERO;
  break;
  case 0x98:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_SF_LOC;
    Type = COMPARE_NOTZERO;
  break;
  case 0x99:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_SF_LOC;
    Type = COMPARE_ZERO;
  break;
  case 0x9A:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_PF_LOC;
    Type = COMPARE_NOTZERO;
  break;
  case 0x9B:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_PF_LOC;
    Type = COMPARE_ZERO;
  break;
  case 0x9D: { // SF = OF
    auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
    auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);
    SrcCond = _Select(FEXCore::IR::COND_EQ,
        Flag1, Flag2, OneConst, ZeroConst);
  break;
  }
  case 0x9C: { // SF <> OF
    auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
    auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);
    SrcCond = _Select(FEXCore::IR::COND_NEQ,
        Flag1, Flag2, OneConst, ZeroConst);
  break;
  }
  case 0x9E: { // ZF = 1 || SF <> OF
    auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
    auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
    auto Flag3 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);

    auto Select1 = _Select(FEXCore::IR::COND_EQ,
        Flag1, OneConst, OneConst, ZeroConst);

    auto Select2 = _Select(FEXCore::IR::COND_NEQ,
        Flag2, Flag3, OneConst, ZeroConst);
    SrcCond = _Or(Select1, Select2);
  break;
  }
  case 0x9F: { // ZF = 0 && SF = OF
    auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
    auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
    auto Flag3 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);

    auto Select1 = _Select(FEXCore::IR::COND_EQ,
        Flag1, ZeroConst, OneConst, ZeroConst);

    auto Select2 = _Select(FEXCore::IR::COND_EQ,
        Flag2, Flag3, OneConst, ZeroConst);
    SrcCond = _And(Select1, Select2);
  break;
  }
  default:
    LogMan::Msg::A("Unhandled SetCC op: 0x%x", Op->OP);
  break;
  }

  if (Type != COMPARE_OTHER) {
    auto MaskConst = _Constant(FLAGMask);

    auto RFLAG = GetPackedRFLAG(false);

    auto AndOp = _And(RFLAG, MaskConst);

    switch (Type) {
    case COMPARE_ZERO: {
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          AndOp, ZeroConst, OneConst, ZeroConst);
    break;
    }
    case COMPARE_NOTZERO: {
      SrcCond = _Select(FEXCore::IR::COND_NEQ,
          AndOp, ZeroConst, OneConst, ZeroConst);
    break;
    }
    case COMPARE_EQUALMASK: {
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          AndOp, MaskConst, OneConst, ZeroConst);
    break;
    }
    case COMPARE_OTHER: break;
    }
  }

  StoreResult(Op, SrcCond, -1);
}

void OpDispatchBuilder::TESTOp(OpcodeArgs) {
  // TEST is an instruction that does an AND between the sources
  // Result isn't stored in result, only writes to flags
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto ALUOp = _And(Dest, Src);
  auto Size = GetSrcSize(Op) * 8;
  GenerateFlags_Logical(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
}

void OpDispatchBuilder::MOVSXDOp(OpcodeArgs) {
  // This instruction is a bit special
  // if SrcSize == 2
  //  Then lower 16 bits of destination is written without changing the upper 48 bits
  // else /* Size == 4 */
  //  if REX_WIDENING:
  //   Sext(32, Src)
  //  else
  //   Zext(32, Src)
  //
  uint8_t Size = std::min(static_cast<uint8_t>(4), GetSrcSize(Op));

  OrderedNode *Src = LoadSource_WithOpSize(Op, Op->Src1, Size, Op->Flags, -1);
  if (Size == 2) {
    // This'll make sure to insert in to the lower 16bits without modifying upper bits
    StoreResult_WithOpSize(Op, Op->Dest, Src, Size, -1);
  }
  else if (Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_REX_WIDENING) {
    // With REX.W then Sext
    Src = _Sext(Size * 8, Src);
    StoreResult(Op, Src, -1);
  }
  else {
    // Without REX.W then Zext
    Src = _Zext(Size * 8, Src);
    StoreResult(Op, Src, -1);
  }
}

void OpDispatchBuilder::MOVSXOp(OpcodeArgs) {
  // This will ZExt the loaded size
  // We want to Sext it
  uint8_t Size = GetSrcSize(Op);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  Src = _Sext(Size * 8, Src);
  StoreResult(Op, Op->Dest, Src, -1);
}

void OpDispatchBuilder::MOVZXOp(OpcodeArgs) {
  uint8_t Size = GetSrcSize(Op);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  // Just make sure this is zero extended
  Src = _Zext(Size * 8, Src);
  StoreResult(Op, Src, -1);
}

void OpDispatchBuilder::CMPOp(OpcodeArgs) {
  // CMP is an instruction that does a SUB between the sources
  // Result isn't stored in result, only writes to flags
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto Size = GetDstSize(Op) * 8;
  auto ALUOp = _Sub(Dest, Src);
  GenerateFlags_SUB(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
}

void OpDispatchBuilder::CQOOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  auto BfeOp = _Bfe(1, GetSrcSize(Op) * 8 - 1, Src);
  auto ZeroConst = _Constant(0);
  auto MaxConst = _Constant(~0ULL);
  auto SelectOp = _Select(FEXCore::IR::COND_EQ, BfeOp, ZeroConst, ZeroConst, MaxConst);

  StoreResult(Op, SelectOp, -1);
}

void OpDispatchBuilder::XCHGOp(OpcodeArgs) {
  // Load both the source and the destination
  if (Op->OP == 0x90 &&
      GetSrcSize(Op) >= 4 &&
      Op->Src1.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR &&
      Op->Src1.TypeGPR.GPR == FEXCore::X86State::REG_RAX &&
      Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR &&
      Op->Dest.TypeGPR.GPR == FEXCore::X86State::REG_RAX) {
    // This is one heck of a sucky special case
    // If we are the 0x90 XCHG opcode (Meaning source is GPR RAX)
    // and destination register is ALSO RAX
    // and in this very specific case we are 32bit or above
    // Then this is a no-op
    // This is because 0x90 without a prefix is technically `xchg eax, eax`
    // But this would result in a zext on 64bit, which would ruin the no-op nature of the instruction
    // So x86-64 spec mandates this special case that even though it is a 32bit instruction and
    // is supposed to zext the result, it is a true no-op
    return;
  }

  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  if (DestIsLockedMem(Op)) {
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1, false);
    auto Result = _AtomicSwap(Dest, Src, GetSrcSize(Op));
    StoreResult(Op, Op->Src1, Result, -1);
  }
  else {
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

    // Swap the contents
    // Order matters here since we don't want to swap context contents for one that effects the other
    StoreResult(Op, Op->Dest, Src, -1);
    StoreResult(Op, Op->Src1, Dest, -1);
  }
}

void OpDispatchBuilder::CDQOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  uint8_t SrcSize = GetSrcSize(Op);

  Src = _Sext(SrcSize * 4, Src);
  if (SrcSize == 4) {
    Src = _Zext(SrcSize * 8, Src);
    SrcSize *= 2;
  }
  StoreResult_WithOpSize(Op, Op->Dest, Src, SrcSize * 8, -1);
}

void OpDispatchBuilder::SAHFOp(OpcodeArgs) {
  OrderedNode *Src = _LoadContext(1, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]) + 1);

  // Clear bits that aren't supposed to be set
  Src = _And(Src, _Constant(~0b101000));

  // Set the bit that is always set here
  Src = _Or(Src, _Constant(0b10));

  // Store the lower 8 bits in to RFLAGS
  SetPackedRFLAG(true, Src);
}
void OpDispatchBuilder::LAHFOp(OpcodeArgs) {
  // Load the lower 8 bits of the Rflags register
    auto RFLAG = GetPackedRFLAG(true);

  // Store the lower 8 bits of the rflags register in to AH
  _StoreContext(1, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]) + 1, RFLAG);
}

void OpDispatchBuilder::FLAGControlOp(OpcodeArgs) {
  enum OpType {
    OP_CLEAR,
    OP_SET,
    OP_COMPLEMENT,
  };
  OpType Type;
  uint64_t Flag;
  switch (Op->OP) {
  case 0xF5: // CMC
    Flag= FEXCore::X86State::RFLAG_CF_LOC;
    Type = OP_COMPLEMENT;
  break;
  case 0xF8: // CLC
    Flag= FEXCore::X86State::RFLAG_CF_LOC;
    Type = OP_CLEAR;
  break;
  case 0xF9: // STC
    Flag= FEXCore::X86State::RFLAG_CF_LOC;
    Type = OP_SET;
  break;
  case 0xFC: // CLD
    Flag= FEXCore::X86State::RFLAG_DF_LOC;
    Type = OP_CLEAR;
  break;
  case 0xFD: // STD
    Flag= FEXCore::X86State::RFLAG_DF_LOC;
    Type = OP_SET;
  break;
  }

  OrderedNode *Result{};
  switch (Type) {
  case OP_CLEAR: {
    Result = _Constant(0);
  break;
  }
  case OP_SET: {
    Result = _Constant(1);
  break;
  }
  case OP_COMPLEMENT: {
    auto RFLAG = GetRFLAG(Flag);
    Result = _Xor(RFLAG, _Constant(1));
  break;
  }
  }

  SetRFLAG(Result, Flag);
}

void OpDispatchBuilder::MOVSegOp(OpcodeArgs) {
  // In x86-64 mode the accesses to the segment registers end up being constant zero moves
  // Aside from FS/GS
  LogMan::Msg::A("Wanting reg: %d\n", Op->Src1.TypeGPR.GPR);
  //  StoreResult(Op, Src);
}

void OpDispatchBuilder::MOVOffsetOp(OpcodeArgs) {
  OrderedNode *Src;
  const FEXCore::X86Tables::DecodedOperand *Dest;

  switch (Op->OP) {
  case 0xA0:
  case 0xA1:
    // Source is memory(literal)
    // Dest is GPR
    Src = LoadSource(Op, Op->Src1, Op->Flags, -1, true);
    Dest = &Op->Dest;
    break;
  case 0xA2:
  case 0xA3:
    // Source is GPR
    // Dest is memory(literal)
    Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
    Dest = &Op->Src2;
    break;
  }
  StoreResult(Op, *Dest, Src, -1);
}

void OpDispatchBuilder::CMOVOp(OpcodeArgs) {
  enum CompareType {
    COMPARE_ZERO,
    COMPARE_NOTZERO,
    COMPARE_EQUALMASK,
    COMPARE_OTHER,
  };
  uint32_t FLAGMask;
  CompareType Type = COMPARE_OTHER;
  OrderedNode *SrcCond;

  auto ZeroConst = _Constant(0);
  auto OneConst = _Constant(1);

  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  switch (Op->OP) {
  case 0x40:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_OF_LOC;
    Type = COMPARE_NOTZERO;
  break;
  case 0x41:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_OF_LOC;
    Type = COMPARE_ZERO;
  break;
  case 0x42:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_CF_LOC;
    Type = COMPARE_NOTZERO;
  break;
  case 0x43:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_CF_LOC;
    Type = COMPARE_ZERO;
  break;
  case 0x44:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_ZF_LOC;
    Type = COMPARE_NOTZERO;
  break;
  case 0x45:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_ZF_LOC;
    Type = COMPARE_ZERO;
  break;
  case 0x46:
    FLAGMask = (1 << FEXCore::X86State::RFLAG_ZF_LOC) | (1 << FEXCore::X86State::RFLAG_CF_LOC);
    Type = COMPARE_NOTZERO;
  break;
  case 0x47:
    FLAGMask = (1 << FEXCore::X86State::RFLAG_ZF_LOC) | (1 << FEXCore::X86State::RFLAG_CF_LOC);
    Type = COMPARE_ZERO;
  break;
  case 0x48:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_SF_LOC;
    Type = COMPARE_NOTZERO;
  break;
  case 0x49:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_SF_LOC;
    Type = COMPARE_ZERO;
  break;
  case 0x4A:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_PF_LOC;
    Type = COMPARE_NOTZERO;
  break;
  case 0x4B:
    FLAGMask = 1 << FEXCore::X86State::RFLAG_PF_LOC;
    Type = COMPARE_ZERO;
  break;
  case 0x4C: { // SF <> OF
    auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
    auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);
    SrcCond = _Select(FEXCore::IR::COND_NEQ,
        Flag1, Flag2, Src, Dest);
  break;
  }
  case 0x4D: { // SF = OF
    auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
    auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);
    SrcCond = _Select(FEXCore::IR::COND_EQ,
        Flag1, Flag2, Src, Dest);
  break;
  }

  case 0x4E: { // ZF = 1 || SF <> OF
    auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
    auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
    auto Flag3 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);

    auto Select1 = _Select(FEXCore::IR::COND_EQ,
        Flag1, OneConst, OneConst, ZeroConst);

    auto Select2 = _Select(FEXCore::IR::COND_NEQ,
        Flag2, Flag3, OneConst, ZeroConst);
    auto Check = _Or(Select1, Select2);

    SrcCond = _Select(FEXCore::IR::COND_EQ,
        Check, OneConst, Src, Dest);
  break;
  }

  case 0x4F: { // ZF = 0 && SSF = OF
    auto Flag1 = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
    auto Flag2 = GetRFLAG(FEXCore::X86State::RFLAG_SF_LOC);
    auto Flag3 = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);

    auto Select1 = _Select(FEXCore::IR::COND_EQ,
        Flag1, ZeroConst, OneConst, ZeroConst);

    auto Select2 = _Select(FEXCore::IR::COND_EQ,
        Flag2, Flag3, OneConst, ZeroConst);
    auto Check = _And(Select1, Select2);

    SrcCond = _Select(FEXCore::IR::COND_EQ,
        Check, OneConst, Src, Dest);
  break;
  }
  default:
    LogMan::Msg::A("Unhandled CMOV op: 0x%x", Op->OP);
  break;
  }

  if (Type != COMPARE_OTHER) {
    auto MaskConst = _Constant(FLAGMask);

    auto RFLAG = GetPackedRFLAG(false);

    auto AndOp = _And(RFLAG, MaskConst);

    switch (Type) {
    case COMPARE_ZERO: {
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          AndOp, ZeroConst, Src, Dest);
    break;
    }
    case COMPARE_NOTZERO: {
      SrcCond = _Select(FEXCore::IR::COND_NEQ,
          AndOp, ZeroConst, Src, Dest);
    break;
    }
    case COMPARE_EQUALMASK: {
      SrcCond = _Select(FEXCore::IR::COND_EQ,
          AndOp, MaskConst, Src, Dest);
    break;
    }

    case COMPARE_OTHER: break;
    }
  }

  StoreResult(Op, SrcCond, -1);
}

void OpDispatchBuilder::CPUIDOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  auto Res = _CPUID(Src);

  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), _ExtractElement(Res, 0));
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RBX]), _ExtractElement(Res, 1));
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), _ExtractElement(Res, 2));
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]), _ExtractElement(Res, 3));
}

template<bool SHL1Bit>
void OpDispatchBuilder::SHLOp(OpcodeArgs) {
  OrderedNode *Src;
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  if (SHL1Bit) {
    Src = _Constant(1);
  }
  else {
    Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  }
  auto Size = GetSrcSize(Op) * 8;

  // x86 masks the shift by 0x3F or 0x1F depending on size of op
  if (Size == 64)
    Src = _And(Src, _Constant(0x3F));
  else
    Src = _And(Src, _Constant(0x1F));

  auto ALUOp = _Lshl(Dest, Src);

  StoreResult(Op, ALUOp, -1);

  // XXX: This isn't correct
  // GenerateFlags_Shift(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
}

template<bool SHR1Bit>
void OpDispatchBuilder::SHROp(OpcodeArgs) {
  OrderedNode *Src;
  auto Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  if (SHR1Bit) {
    Src = _Constant(1);
  }
  else {
    Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  }

  auto Size = GetSrcSize(Op) * 8;

  // x86 masks the shift by 0x3F or 0x1F depending on size of op
  if (Size == 64)
    Src = _And(Src, _Constant(0x3F));
  else
    Src = _And(Src, _Constant(0x1F));

  if (Size != 64) {
    Dest = _Bfe(Size, 0, Dest);
  }

  auto ALUOp = _Lshr(Dest, Src);

  StoreResult(Op, ALUOp, -1);

  // XXX: This isn't correct
  GenerateFlags_Logical(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));

  if (SHR1Bit) {
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(_Bfe(1, Size - 1, Src));
  }
}

void OpDispatchBuilder::SHLDOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  OrderedNode *Shift{};
  if (Op->OP == 0xA4)  {
    LogMan::Throw::A(Op->Src2.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_LITERAL, "Src1 needs to be literal here");
    Shift = _Constant(Op->Src2.TypeLiteral.Literal);
  }
  else
    Shift = _LoadContext(1, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]));

  auto Size = GetSrcSize(Op) * 8;

  // x86 masks the shift by 0x3F or 0x1F depending on size of op
  if (Size == 64)
    Shift = _And(Shift, _Constant(0x3F));
  else
    Shift = _And(Shift, _Constant(0x1F));

  auto ShiftRight = _Neg(Shift);

  OrderedNode *Res{};
  if (Size == 16) {
    LogMan::Msg::A("Unsupported Op");
  }
  else {
    auto Tmp1 = _Lshl(Dest, Shift);
    auto Tmp2 = _Lshr(Src, ShiftRight);
    Res = _Or(Tmp1, Tmp2);
  }

  StoreResult(Op, Res, -1);

  // XXX: This isn't correct
  // GenerateFlags_Shift(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
}

void OpDispatchBuilder::SHRDOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  OrderedNode *Shift{};
  if (Op->OP == 0xAC)  {
    LogMan::Throw::A(Op->Src2.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_LITERAL, "Src1 needs to be literal here");
    Shift = _Constant(Op->Src2.TypeLiteral.Literal);
  }
  else
    Shift = _LoadContext(1, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]));

  auto Size = GetSrcSize(Op) * 8;

  // x86 masks the shift by 0x3F or 0x1F depending on size of op
  if (Size == 64)
    Shift = _And(Shift, _Constant(0x3F));
  else
    Shift = _And(Shift, _Constant(0x1F));

  auto ShiftRight = _Neg(Shift);

  OrderedNode *Res{};
  if (Size == 16) {
    LogMan::Msg::A("Unsupported Op");
  }
  else {
    auto Tmp1 = _Lshr(Dest, Shift);
    auto Tmp2 = _Lshl(Src, ShiftRight);
    Res = _Or(Tmp1, Tmp2);
  }

  StoreResult(Op, Res, -1);

  // XXX: This isn't correct
  // GenerateFlags_Shift(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
}

void OpDispatchBuilder::ASHROp(OpcodeArgs) {
  bool SHR1Bit = false;
#define OPD(group, prefix, Reg) (((group - FEXCore::X86Tables::TYPE_GROUP_1) << 6) | (prefix) << 3 | (Reg))
  switch (Op->OP) {
  case OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD0), 7):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD1), 7):
    SHR1Bit = true;
  break;
  }
#undef OPD

  OrderedNode *Src;
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto Size = GetSrcSize(Op) * 8;

  if (SHR1Bit) {
    Src = _Constant(Size, 1);
  }
  else {
    Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  }


  // x86 masks the shift by 0x3F or 0x1F depending on size of op
  if (Size == 64)
    Src = _And(Src, _Constant(Size, 0x3F));
  else
    Src = _And(Src, _Constant(Size, 0x1F));

  auto ALUOp = _Ashr(Dest, Src);

  StoreResult(Op, ALUOp, -1);

  // XXX: This isn't correct
  GenerateFlags_Logical(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));

  if (SHR1Bit) {
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(_Bfe(1, Size - 1, Src));
  }
}

void OpDispatchBuilder::ROROp(OpcodeArgs) {
  bool Is1Bit = false;
#define OPD(group, prefix, Reg) (((group - FEXCore::X86Tables::TYPE_GROUP_1) << 6) | (prefix) << 3 | (Reg))
  switch (Op->OP) {
  case OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD0), 1):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD1), 1):
    Is1Bit = true;
  break;
  }
#undef OPD

  OrderedNode *Src;
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto Size = GetSrcSize(Op) * 8;
  if (Is1Bit) {
    Src = _Constant(Size, 1);
  }
  else {
    Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  }

  // x86 masks the shift by 0x3F or 0x1F depending on size of op
  if (Size == 64)
    Src = _And(Src, _Constant(Size, 0x3F));
  else
    Src = _And(Src, _Constant(Size, 0x1F));

  auto ALUOp = _Ror(Dest, Src);

  StoreResult(Op, ALUOp, -1);

  // XXX: This is incorrect
  GenerateFlags_Rotate(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));

  if (Is1Bit) {
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(_Bfe(1, Size - 1, Src));
  }
}
void OpDispatchBuilder::ROLOp(OpcodeArgs) {
  bool Is1Bit = false;
#define OPD(group, prefix, Reg) (((group - FEXCore::X86Tables::TYPE_GROUP_1) << 6) | (prefix) << 3 | (Reg))
  switch (Op->OP) {
  case OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD0), 0):
  case OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD1), 0):
    Is1Bit = true;
  break;
  default: break;
  }
#undef OPD

  OrderedNode *Src;
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto Size = GetSrcSize(Op) * 8;

  if (Is1Bit) {
    Src = _Constant(Size, 1);
  }
  else {
    Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  }

  // x86 masks the shift by 0x3F or 0x1F depending on size of op
  if (Size == 64)
    Src = _And(Src, _Constant(Size, 0x3F));
  else
    Src = _And(Src, _Constant(Size, 0x1F));

  auto ALUOp = _Rol(Dest, Src);

  StoreResult(Op, ALUOp, -1);

  // XXX: This is incorrect
  GenerateFlags_Rotate(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));

  if (Is1Bit) {
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(_Bfe(1, Size - 1, Src));
  }
}

void OpDispatchBuilder::BTOp(OpcodeArgs) {
  OrderedNode *Result;
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
    Result = _Lshr(Dest, Src);
  }
  else {
    // Load the address to the memory location
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1, false);
    uint32_t Size = GetSrcSize(Op);
    uint32_t Mask = Size * 8 - 1;
    OrderedNode *SizeMask = _Constant(Mask);
    OrderedNode *AddressShift = _Constant(32 - __builtin_clz(Mask));

    // Get the bit selection from the src
    OrderedNode *BitSelect = _And(Src, SizeMask);

    // First shift out the selection bits
    Src = _Lshr(Src, AddressShift);

    // Now multiply by operand size to get correct indexing
    if (Size != 1) {
      Src = _Lshl(Src, _Constant(Size - 1));
    }

    // Get the address offset by shifting out the size of the op (To shift out the bit selection)
    // Then use that to index in to the memory location by size of op

    // Now add the addresses together and load the memory
    OrderedNode *MemoryLocation = _Add(Dest, Src);
    Result = _LoadMem(Size, MemoryLocation, Size);

    // Now shift in to the correct bit location
    Result = _Lshr(Result, BitSelect);
  }
  SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(Result);
}

void OpDispatchBuilder::BTROp(OpcodeArgs) {
  OrderedNode *Result;
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
    Result = _Lshr(Dest, Src);

    uint32_t Size = GetSrcSize(Op);
    uint32_t Mask = Size * 8 - 1;
    OrderedNode *SizeMask = _Constant(Mask);

    // Get the bit selection from the src
    OrderedNode *BitSelect = _And(Src, SizeMask);

    OrderedNode *BitMask = _Lshl(_Constant(1), BitSelect);
    BitMask = _Neg(BitMask);
    Dest = _And(Dest, BitMask);
    StoreResult(Op, Dest, -1);
  }
  else {
    // Load the address to the memory location
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1, false);
    uint32_t Size = GetSrcSize(Op);
    uint32_t Mask = Size * 8 - 1;
    OrderedNode *SizeMask = _Constant(Mask);
    OrderedNode *AddressShift = _Constant(32 - __builtin_clz(Mask));

    // Get the bit selection from the src
    OrderedNode *BitSelect = _And(Src, SizeMask);

    // First shift out the selection bits
    Src = _Lshr(Src, AddressShift);

    // Now multiply by operand size to get correct indexing
    if (Size != 1) {
      Src = _Lshl(Src, _Constant(Size - 1));
    }

    // Get the address offset by shifting out the size of the op (To shift out the bit selection)
    // Then use that to index in to the memory location by size of op

    // Now add the addresses together and load the memory
    OrderedNode *MemoryLocation = _Add(Dest, Src);
    OrderedNode *Value = _LoadMem(Size, MemoryLocation, Size);

    // Now shift in to the correct bit location
    Result = _Lshr(Value, BitSelect);
    OrderedNode *BitMask = _Lshl(_Constant(1), BitSelect);
    BitMask = _Neg(BitMask);
    Value = _And(Value, BitMask);
    _StoreMem(Size, MemoryLocation, Value, Size);
  }
  SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(Result);
}

void OpDispatchBuilder::BTSOp(OpcodeArgs) {
  OrderedNode *Result;
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
    Result = _Lshr(Dest, Src);

    uint32_t Size = GetSrcSize(Op);
    uint32_t Mask = Size * 8 - 1;
    OrderedNode *SizeMask = _Constant(Mask);

    // Get the bit selection from the src
    OrderedNode *BitSelect = _And(Src, SizeMask);

    OrderedNode *BitMask = _Lshl(_Constant(1), BitSelect);
    Dest = _Or(Dest, BitMask);
    StoreResult(Op, Dest, -1);
  }
  else {
    // Load the address to the memory location
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1, false);
    uint32_t Size = GetSrcSize(Op);
    uint32_t Mask = Size * 8 - 1;
    OrderedNode *SizeMask = _Constant(Mask);
    OrderedNode *AddressShift = _Constant(32 - __builtin_clz(Mask));

    // Get the bit selection from the src
    OrderedNode *BitSelect = _And(Src, SizeMask);

    // First shift out the selection bits
    Src = _Lshr(Src, AddressShift);

    // Now multiply by operand size to get correct indexing
    if (Size != 1) {
      Src = _Lshl(Src, _Constant(Size - 1));
    }

    // Get the address offset by shifting out the size of the op (To shift out the bit selection)
    // Then use that to index in to the memory location by size of op

    // Now add the addresses together and load the memory
    OrderedNode *MemoryLocation = _Add(Dest, Src);
    OrderedNode *Value = _LoadMem(Size, MemoryLocation, Size);

    // Now shift in to the correct bit location
    Result = _Lshr(Value, BitSelect);
    OrderedNode *BitMask = _Lshl(_Constant(1), BitSelect);
    Value = _Or(Value, BitMask);
    _StoreMem(Size, MemoryLocation, Value, Size);
  }
  SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(Result);
}

void OpDispatchBuilder::BTCOp(OpcodeArgs) {
  OrderedNode *Result;
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
    Result = _Lshr(Dest, Src);

    uint32_t Size = GetSrcSize(Op);
    uint32_t Mask = Size * 8 - 1;
    OrderedNode *SizeMask = _Constant(Mask);

    // Get the bit selection from the src
    OrderedNode *BitSelect = _And(Src, SizeMask);

    OrderedNode *BitMask = _Lshl(_Constant(1), BitSelect);
    Dest = _Xor(Dest, BitMask);
    StoreResult(Op, Dest, -1);
  }
  else {
    // Load the address to the memory location
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1, false);
    uint32_t Size = GetSrcSize(Op);
    uint32_t Mask = Size * 8 - 1;
    OrderedNode *SizeMask = _Constant(Mask);
    OrderedNode *AddressShift = _Constant(32 - __builtin_clz(Mask));

    // Get the bit selection from the src
    OrderedNode *BitSelect = _And(Src, SizeMask);

    // First shift out the selection bits
    Src = _Lshr(Src, AddressShift);

    // Now multiply by operand size to get correct indexing
    if (Size != 1) {
      Src = _Lshl(Src, _Constant(Size - 1));
    }

    // Get the address offset by shifting out the size of the op (To shift out the bit selection)
    // Then use that to index in to the memory location by size of op

    // Now add the addresses together and load the memory
    OrderedNode *MemoryLocation = _Add(Dest, Src);
    OrderedNode *Value = _LoadMem(Size, MemoryLocation, Size);

    // Now shift in to the correct bit location
    Result = _Lshr(Value, BitSelect);
    OrderedNode *BitMask = _Lshl(_Constant(1), BitSelect);
    Value = _Xor(Value, BitMask);
    _StoreMem(Size, MemoryLocation, Value, Size);
  }
  SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(Result);
}

void OpDispatchBuilder::IMUL1SrcOp(OpcodeArgs) {
  OrderedNode *Src1 = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src2 = LoadSource(Op, Op->Src1, Op->Flags, -1);

  auto Dest = _Mul(Src1, Src2);
  StoreResult(Op, Dest, -1);
  GenerateFlags_MUL(Op, Dest, _MulH(Src1, Src2));
}

void OpDispatchBuilder::IMUL2SrcOp(OpcodeArgs) {
  OrderedNode *Src1 = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Src2 = LoadSource(Op, Op->Src2, Op->Flags, -1);

  auto Dest = _Mul(Src1, Src2);
  StoreResult(Op, Dest, -1);
  GenerateFlags_MUL(Op, Dest, _MulH(Src1, Src2));
}

void OpDispatchBuilder::IMULOp(OpcodeArgs) {
  uint8_t Size = GetSrcSize(Op);
  OrderedNode *Src1 = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src2 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));

  if (Size != 8) {
    Src1 = _Sext(Size * 8, Src1);
    Src2 = _Sext(Size * 8, Src2);
  }

  OrderedNode *Result = _Mul(Src1, Src2);
  OrderedNode *ResultHigh{};
  if (Size == 1) {
    // Result is stored in AX
    _StoreContext(2, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), Result);
    ResultHigh = _Bfe(8, 8, Result);
    ResultHigh = _Sext(Size * 8, ResultHigh);
  }
  else if (Size == 2) {
    // 16bits stored in AX
    // 16bits stored in DX
    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), Result);
    ResultHigh = _Bfe(16, 16, Result);
    ResultHigh = _Sext(Size * 8, ResultHigh);
    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), ResultHigh);
  }
  else if (Size == 4) {
    // 32bits stored in EAX
    // 32bits stored in EDX
    // Make sure they get Zext correctly
    OrderedNode *ResultLow = _Bfe(32, 0, Result);
    ResultLow = _Zext(Size * 8, ResultLow);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), ResultLow);
    ResultHigh = _Bfe(32, 32, Result);
    ResultHigh = _Zext(Size * 8, ResultHigh);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), ResultHigh);
  }
  else if (Size == 8) {
    // 64bits stored in RAX
    // 64bits stored in RDX
    ResultHigh = _MulH(Src1, Src2);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), Result);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), ResultHigh);
  }

  GenerateFlags_MUL(Op, Result, ResultHigh);
}

void OpDispatchBuilder::MULOp(OpcodeArgs) {
  uint8_t Size = GetSrcSize(Op);
  OrderedNode *Src1 = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src2 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));
  if (Size != 8) {
    Src1 = _Zext(Size * 8, Src1);
    Src2 = _Zext(Size * 8, Src2);
  }
  OrderedNode *Result = _UMul(Src1, Src2);
  OrderedNode *ResultHigh{};

  if (Size == 1) {
   // Result is stored in AX
    _StoreContext(2, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), Result);
    ResultHigh = _Bfe(8, 8, Result);
  }
  else if (Size == 2) {
    // 16bits stored in AX
    // 16bits stored in DX
    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), Result);
    ResultHigh = _Bfe(16, 16, Result);
    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), ResultHigh);
  }
  else if (Size == 4) {
    // 32bits stored in EAX
    // 32bits stored in EDX
    // Make sure they get Zext correctly
    OrderedNode *ResultLow = _Bfe(32, 0, Result);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), ResultLow);
    ResultHigh = _Bfe(32, 32, Result);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), ResultHigh);

  }
  else if (Size == 8) {
    // 64bits stored in RAX
    // 64bits stored in RDX
    ResultHigh = _UMulH(Src1, Src2);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), Result);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), ResultHigh);
  }

  GenerateFlags_UMUL(Op, ResultHigh);
}

void OpDispatchBuilder::NOTOp(OpcodeArgs) {
  uint8_t Size = GetSrcSize(Op);
  OrderedNode *MaskConst{};
  if (Size == 8) {
    MaskConst = _Constant(~0ULL);
  }
  else {
    MaskConst = _Constant((1ULL << (Size * 8)) - 1);
  }

  OrderedNode *Src = LoadSource(Op, Op->Dest, Op->Flags, -1);
  Src = _Xor(Src, MaskConst);
  StoreResult(Op, Src, -1);
}

void OpDispatchBuilder::XADDOp(OpcodeArgs) {
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1, false);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    // If this is a GPR then we can just do an Add and store
    auto Result = _Add(Dest, Src);
    StoreResult(Op, Result, -1);

    // Previous value in dest gets stored in src
    StoreResult(Op, Op->Src1, Dest, -1);

    auto Size = GetSrcSize(Op) * 8;
    GenerateFlags_ADD(Op, _Bfe(Size, 0, Result), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
  }
  else {
    auto Before = _AtomicFetchAdd(Dest, Src, GetSrcSize(Op));
    StoreResult(Op, Op->Src1, Before, -1);
  }
}

void OpDispatchBuilder::RDTSCOp(OpcodeArgs) {
  auto Counter = _CycleCounter();
  auto CounterLow = _Bfe(32, 0, Counter);
  auto CounterHigh = _Bfe(32, 32, Counter);
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), CounterLow);
  _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), CounterHigh);
}

void OpDispatchBuilder::INCOp(OpcodeArgs) {
  LogMan::Throw::A(!(Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_REP_PREFIX), "Can't handle REP on this\n");

  if (DestIsLockedMem(Op)) {
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1, false);
    auto OneConst = _Constant(1);
    auto AtomicResult = _AtomicFetchAdd(Dest, OneConst, GetSrcSize(Op));
    auto ALUOp = _Add(AtomicResult, OneConst);

    StoreResult(Op, ALUOp, -1);

    auto Size = GetSrcSize(Op) * 8;
    GenerateFlags_ADD(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, OneConst));
  }
  else {
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
    auto OneConst = _Constant(1);
    auto ALUOp = _Add(Dest, OneConst);

    StoreResult(Op, ALUOp, -1);

    auto Size = GetSrcSize(Op) * 8;
    GenerateFlags_ADD(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, OneConst));
  }
}

void OpDispatchBuilder::DECOp(OpcodeArgs) {
  LogMan::Throw::A(!(Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_REP_PREFIX), "Can't handle REP on this\n");

  if (DestIsLockedMem(Op)) {
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1, false);
    auto OneConst = _Constant(1);
    auto AtomicResult = _AtomicFetchSub(Dest, OneConst, GetSrcSize(Op));
    auto ALUOp = _Sub(AtomicResult, OneConst);

    StoreResult(Op, ALUOp, -1);

    auto Size = GetSrcSize(Op) * 8;
    GenerateFlags_SUB(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, OneConst));
  }
  else {
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
    auto OneConst = _Constant(1);
    auto ALUOp = _Sub(Dest, OneConst);

    StoreResult(Op, ALUOp, -1);

    auto Size = GetSrcSize(Op) * 8;
    GenerateFlags_SUB(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, Dest), _Bfe(Size, 0, OneConst));
  }
}

void OpDispatchBuilder::STOSOp(OpcodeArgs) {
  LogMan::Throw::A(Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_REP_PREFIX, "Can't handle REP not existing on STOS\n");

  auto Size = GetSrcSize(Op);

  // Create all our blocks
  auto LoopHead = CreateNewCodeBlock();
  auto LoopTail = CreateNewCodeBlock();
  auto LoopEnd = CreateNewCodeBlock();

  // At the time this was written, our RA can't handle accessing nodes across blocks.
  // So we need to re-load and re-calculate essential values each iteration of the loop.

  // First thing we need to do is finish this block and jump to the start of the loop.
  _Jump(LoopHead);

    SetCurrentCodeBlock(LoopHead);
    {
      OrderedNode *Counter = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]));
      auto ZeroConst = _Constant(0);

      // Can we end the block?
      auto CanLeaveCond = _Select(FEXCore::IR::COND_EQ,
          Counter, ZeroConst,
          _Constant(1), ZeroConst);

      _CondJump(CanLeaveCond, LoopEnd, LoopTail);
    }

    SetCurrentCodeBlock(LoopTail);
    {
      OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
      OrderedNode *Dest = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]));

      // Store to memory where RDI points
      _StoreMem(Size, Dest, Src, Size);

      OrderedNode *TailCounter = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]));
      OrderedNode *TailDest = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]));

      // Decrement counter
      TailCounter = _Sub(TailCounter, _Constant(1));

      // Store the counter so we don't have to deal with PHI here
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]), TailCounter);

      auto SizeConst = _Constant(Size);
      auto NegSizeConst = _Constant(-Size);

      // Calculate direction.
      auto DF = GetRFLAG(FEXCore::X86State::RFLAG_DF_LOC);
      auto PtrDir = _Select(FEXCore::IR::COND_EQ,
          DF,  _Constant(0),
          SizeConst, NegSizeConst);

      // Offset the pointer
      TailDest = _Add(TailDest, PtrDir);
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]), TailDest);

      // Jump back to the start, we have more work to do
      _Jump(LoopHead);
    }

  // Make sure to start a new block after ending this one

  SetCurrentCodeBlock(LoopEnd);
}

void OpDispatchBuilder::MOVSOp(OpcodeArgs) {
  LogMan::Throw::A(!(Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_REPNE_PREFIX), "Can't handle REPNE on MOVS\n");

  if (Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_REP_PREFIX) {
    auto Size = GetSrcSize(Op);

    // Create all our blocks
    auto LoopHead = CreateNewCodeBlock();
    auto LoopTail = CreateNewCodeBlock();
    auto LoopEnd = CreateNewCodeBlock();

    // At the time this was written, our RA can't handle accessing nodes across blocks.
    // So we need to re-load and re-calculate essential values each iteration of the loop.

    // First thing we need to do is finish this block and jump to the start of the loop.
    _Jump(LoopHead);

      SetCurrentCodeBlock(LoopHead);
      {
        OrderedNode *Counter = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]));
        auto ZeroConst = _Constant(0);

        // Can we end the block?
        auto CanLeaveCond = _Select(FEXCore::IR::COND_EQ,
            Counter, ZeroConst,
            _Constant(1), ZeroConst);

        _CondJump(CanLeaveCond, LoopEnd, LoopTail);
      }

      SetCurrentCodeBlock(LoopTail);
      {
        OrderedNode *Src = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSI]));
        OrderedNode *Dest = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]));

        Src = _LoadMem(Size, Src, Size);

        // Store to memory where RDI points
        _StoreMem(Size, Dest, Src, Size);

        OrderedNode *TailCounter = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]));

        // Decrement counter
        TailCounter = _Sub(TailCounter, _Constant(1));

        // Store the counter so we don't have to deal with PHI here
        _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]), TailCounter);

        auto SizeConst = _Constant(Size);
        auto NegSizeConst = _Constant(-Size);

        // Calculate direction.
        auto DF = GetRFLAG(FEXCore::X86State::RFLAG_DF_LOC);
        auto PtrDir = _Select(FEXCore::IR::COND_EQ,
            DF,  _Constant(0),
            SizeConst, NegSizeConst);

        // Offset the pointer
        OrderedNode *TailSrc = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSI]));
        OrderedNode *TailDest = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]));
        TailSrc = _Add(TailSrc, PtrDir);
        TailDest = _Add(TailDest, PtrDir);
        _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSI]), TailSrc);
        _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]), TailDest);

        // Jump back to the start, we have more work to do
        _Jump(LoopHead);
      }

    // Make sure to start a new block after ending this one

    SetCurrentCodeBlock(LoopEnd);
  }
  else {
    auto Size = GetSrcSize(Op);
    OrderedNode *RSI = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSI]));
    OrderedNode *RDI = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]));

    auto Src = _LoadMem(Size, RSI, Size);

    // Store to memory where RDI points
    _StoreMem(Size, RDI, Src, Size);

    auto SizeConst = _Constant(Size);
    auto NegSizeConst = _Constant(-Size);

    // Calculate direction.
    auto DF = GetRFLAG(FEXCore::X86State::RFLAG_DF_LOC);
    auto PtrDir = _Select(FEXCore::IR::COND_EQ,
        DF,  _Constant(0),
        SizeConst, NegSizeConst);

    RSI = _Add(RSI, PtrDir);
    RDI = _Add(RDI, PtrDir);

    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSI]), RSI);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]), RDI);
  }
}

void OpDispatchBuilder::CMPSOp(OpcodeArgs) {
  LogMan::Throw::A(Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_REP_PREFIX, "Can't only handle REP\n");

  auto Size = GetSrcSize(Op);

  auto JumpStart = _Jump();
  // Make sure to start a new block after ending this one
    auto LoopStart = CreateNewCodeBlock();
    SetJumpTarget(JumpStart, LoopStart);
    SetCurrentCodeBlock(LoopStart);

    OrderedNode *Counter = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]));

    // Can we end the block?
    auto CanLeaveCond = _Select(FEXCore::IR::COND_EQ,
      Counter, _Constant(0),
      _Constant(1), _Constant(0));
    auto CondJump = _CondJump(CanLeaveCond);

    auto LoopTail = CreateNewCodeBlock();
    SetFalseJumpTarget(CondJump, LoopTail);
    SetCurrentCodeBlock(LoopTail);

    // Working loop
    {
      OrderedNode *Dest_RDI = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]));
      OrderedNode *Dest_RSI = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSI]));

      auto Src1 = _LoadMem(Size, Dest_RDI, Size);
      auto Src2 = _LoadMem(Size, Dest_RSI, Size);

      auto ALUOp = _Sub(Src1, Src2);
      GenerateFlags_SUB(Op, _Bfe(Size * 8, 0, ALUOp), _Bfe(Size * 8, 0, Src1), _Bfe(Size * 8, 0, Src2));

      OrderedNode *TailCounter = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]));

      // Decrement counter
      TailCounter = _Sub(TailCounter, _Constant(1));

      // Store the counter so we don't have to deal with PHI here
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]), TailCounter);

      auto DF = GetRFLAG(FEXCore::X86State::RFLAG_DF_LOC);
      auto PtrDir = _Select(FEXCore::IR::COND_EQ,
          DF, _Constant(0),
          _Constant(Size), _Constant(-Size));

      // Offset the pointer
      Dest_RDI = _Add(Dest_RDI, PtrDir);
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]), Dest_RDI);

      // Offset second pointer
      Dest_RSI = _Add(Dest_RSI, PtrDir);
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RSI]), Dest_RSI);

      // Jump back to the start, we have more work to do
      _Jump(LoopStart);
    }

  // Make sure to start a new block after ending this one
  auto LoopEnd = CreateNewCodeBlock();
  SetTrueJumpTarget(CondJump, LoopEnd);
  SetCurrentCodeBlock(LoopEnd);
}

void OpDispatchBuilder::SCASOp(OpcodeArgs) {
  LogMan::Throw::A(Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_REPNE_PREFIX, "Can only handle REPNE\n");

  auto Size = GetSrcSize(Op);

  SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(_Constant(0));

  auto JumpStart = _Jump();
  // Make sure to start a new block after ending this one
    auto LoopStart = CreateNewCodeBlock();
    SetJumpTarget(JumpStart, LoopStart);
    SetCurrentCodeBlock(LoopStart);

      auto ZeroConst = _Constant(0);
      auto OneConst = _Constant(1);

      OrderedNode *Counter = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]));

      // Can we end the block?
      OrderedNode *CanLeaveCond = _Select(FEXCore::IR::COND_EQ,
        Counter, ZeroConst,
        OneConst, ZeroConst);

      auto ZF = GetRFLAG(FEXCore::X86State::RFLAG_ZF_LOC);
      CanLeaveCond = _Or(CanLeaveCond, ZF);

    // We leave if RCX = 0
    auto CondJump = _CondJump(CanLeaveCond);

    auto LoopTail = CreateNewCodeBlock();
    SetFalseJumpTarget(CondJump, LoopTail);
    SetCurrentCodeBlock(LoopTail);

    // Working loop
    {
      OrderedNode *Dest_RDI = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]));

      auto Src1 = LoadSource(Op, Op->Src1, Op->Flags, -1);
      auto Src2 = _LoadMem(Size, Dest_RDI, Size);

      auto ALUOp = _Sub(Src1, Src2);
      GenerateFlags_SUB(Op, ALUOp, Src1, Src2);

      OrderedNode *TailCounter = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]));
      OrderedNode *TailDest_RDI = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]));

      // Decrement counter
      TailCounter = _Sub(TailCounter, _Constant(1));

      // Store the counter so we don't have to deal with PHI here
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RCX]), TailCounter);

      auto SizeConst = _Constant(Size);
      auto NegSizeConst = _Constant(-Size);

      auto DF = GetRFLAG(FEXCore::X86State::RFLAG_DF_LOC);
      auto PtrDir = _Select(FEXCore::IR::COND_EQ,
          DF, _Constant(0),
          SizeConst, NegSizeConst);

      // Offset the pointer
      TailDest_RDI = _Add(TailDest_RDI, PtrDir);
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDI]), TailDest_RDI);
      _Jump(LoopStart);
  }
  // Make sure to start a new block after ending this one
  auto LoopEnd = CreateNewCodeBlock();
  SetTrueJumpTarget(CondJump, LoopEnd);
  SetCurrentCodeBlock(LoopEnd);
}

void OpDispatchBuilder::BSWAPOp(OpcodeArgs) {
  OrderedNode *Dest;
  if (GetSrcSize(Op) == 2) {
    // BSWAP of 16bit is undef. ZEN+ causes the lower 16bits to get zero'd
    Dest = _Constant(0);
  }
  else {
    Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
    Dest = _Rev(Dest);
  }
  StoreResult(Op, Dest, -1);
}

void OpDispatchBuilder::NEGOp(OpcodeArgs) {
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
  auto ZeroConst = _Constant(0);
  auto ALUOp = _Sub(ZeroConst, Dest);

  StoreResult(Op, ALUOp, -1);

  auto Size = GetSrcSize(Op) * 8;

  GenerateFlags_SUB(Op, _Bfe(Size, 0, ALUOp), _Bfe(Size, 0, ZeroConst), _Bfe(Size, 0, Dest));
}

void OpDispatchBuilder::DIVOp(OpcodeArgs) {
  // This loads the divisor
  OrderedNode *Divisor = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto Size = GetSrcSize(Op);

  if (Size == 1) {
    OrderedNode *Src1 = _LoadContext(2, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));

    auto UDivOp = _UDiv(Src1, Divisor);
    auto URemOp = _URem(Src1, Divisor);

    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), UDivOp);
    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]) + 1, URemOp);
  }
  else if (Size == 2) {
    OrderedNode *Src1 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));
    OrderedNode *Src2 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]));
    auto UDivOp = _LUDiv(Src1, Src2, Divisor);
    auto URemOp = _LURem(Src1, Src2, Divisor);

    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), UDivOp);
    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), URemOp);
  }
  else if (Size == 4) {
    OrderedNode *Src1 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));
    OrderedNode *Src2 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]));

    auto UDivOp = _LUDiv(Src1, Src2, Divisor);
    auto URemOp = _LURem(Src1, Src2, Divisor);

    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), _Zext(32, UDivOp));
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), _Zext(32, URemOp));
  }
  else if (Size == 8) {
    OrderedNode *Src1 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));
    OrderedNode *Src2 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]));

    auto UDivOp = _LUDiv(Src1, Src2, Divisor);
    auto URemOp = _LURem(Src1, Src2, Divisor);

    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), UDivOp);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), URemOp);
  }
}

void OpDispatchBuilder::IDIVOp(OpcodeArgs) {
  // This loads the divisor
  OrderedNode *Divisor = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto Size = GetSrcSize(Op);

  if (Size == 1) {
    OrderedNode *Src1 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));

    auto UDivOp = _Div(Src1, Divisor);
    auto URemOp = _Rem(Src1, Divisor);

    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), UDivOp);
    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]) + 1, URemOp);
  }
  else if (Size == 2) {
    OrderedNode *Src1 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));
    OrderedNode *Src2 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]));
    auto UDivOp = _LDiv(Src1, Src2, Divisor);
    auto URemOp = _LRem(Src1, Src2, Divisor);

    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), UDivOp);
    _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), URemOp);
  }
  else if (Size == 4) {
    OrderedNode *Src1 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));
    OrderedNode *Src2 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]));

    auto UDivOp = _LDiv(Src1, Src2, Divisor);
    auto URemOp = _LRem(Src1, Src2, Divisor);

    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), _Zext(32, UDivOp));
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), _Zext(32, URemOp));
  }
  else if (Size == 8) {
    OrderedNode *Src1 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));
    OrderedNode *Src2 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]));

    auto UDivOp = _LDiv(Src1, Src2, Divisor);
    auto URemOp = _LRem(Src1, Src2, Divisor);

    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), UDivOp);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RDX]), URemOp);
  }
}

void OpDispatchBuilder::BSFOp(OpcodeArgs) {
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  // Find the LSB of this source
  auto Result = _FindLSB(Src);

  auto ZeroConst = _Constant(0);
  auto OneConst = _Constant(1);

  // If Src was zero then the destination doesn't get modified
  auto SelectOp = _Select(FEXCore::IR::COND_EQ,
      Src, ZeroConst,
      Dest, Result);

  // ZF is set to 1 if the source was zero
  auto ZFSelectOp = _Select(FEXCore::IR::COND_EQ,
      Src, ZeroConst,
      OneConst, ZeroConst);

  StoreResult(Op, SelectOp, -1);
  SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(ZFSelectOp);
}

void OpDispatchBuilder::BSROp(OpcodeArgs) {
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  // Find the MSB of this source
  auto Result = _FindMSB(Src);

  auto ZeroConst = _Constant(0);
  auto OneConst = _Constant(1);

  // If Src was zero then the destination doesn't get modified
  auto SelectOp = _Select(FEXCore::IR::COND_EQ,
      Src, ZeroConst,
      Dest, Result);

  // ZF is set to 1 if the source was zero
  auto ZFSelectOp = _Select(FEXCore::IR::COND_EQ,
      Src, ZeroConst,
      OneConst, ZeroConst);

  StoreResult(Op, SelectOp, -1);
  SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(ZFSelectOp);
}

void OpDispatchBuilder::MOVAPSOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  StoreResult(Op, Src, -1);
}

void OpDispatchBuilder::MOVUPSOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, 1);
  StoreResult(Op, Src, 1);
}

void OpDispatchBuilder::MOVLHPSOp(OpcodeArgs) {
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, 8);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, 8);
  auto Result = _VInsElement(16, 8, 1, 0, Dest, Src);
  StoreResult(Op, Result, 8);
}

void OpDispatchBuilder::MOVHPDOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  // This instruction is a bit special that if the destination is a register then it'll ZEXT the 64bit source to 128bit
  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    // If the destination is a GPR then the source is memory
    // xmm1[127:64] = src
    OrderedNode *Dest = LoadSource_WithOpSize(Op, Op->Dest, 16, Op->Flags, -1);
    Src = _VCastFromGPR(16, GetSrcSize(Op), Src);
    auto Result = _VInsElement(16, 8, 1, 0, Dest, Src);
    StoreResult(Op, Result, -1);
  }
  else {
    // In this case memory is the destination and the high bits of the XMM are source
    // Mem64 = xmm1[127:64]
    auto Result = _VExtractToGPR(16, 8, Src, 1);
    StoreResult(Op, Result, -1);
  }
}

void OpDispatchBuilder::MOVSDOp(OpcodeArgs) {
  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR &&
      Op->Src1.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    // xmm1[63:0] <- xmm2[63:0]
    OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
    OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
    auto Result = _VInsElement(16, 8, 0, 0, Dest, Src);
    StoreResult(Op, Result, -1);
  }
  else if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    // xmm1[127:0] <- zext(mem64)
    OrderedNode *Src = LoadSource_WithOpSize(Op, Op->Src1, 8, Op->Flags, -1);
    Src = _Zext(64, Src);
    StoreResult(Op, Src, -1);
  }
  else {
    // In this case memory is the destination and the low bits of the XMM are source
    // Mem64 = xmm2[63:0]
    OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
    auto Result = _VExtractToGPR(16, 8, Src, 0);
    StoreResult_WithOpSize(Op, Op->Dest, Result, 8, -1);
  }
}

void OpDispatchBuilder::PADDQOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  uint8_t ElementSize = 8;
  switch (Op->OP) {
  case 0xD4: ElementSize = 8; break;
  case 0xFC: ElementSize = 1; break;
  case 0xFE: ElementSize = 4; break;
  default: LogMan::Msg::A("Unknown PADD op: 0x%04x", Op->OP); break;
  }

  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto ALUOp = _VAdd(Size, ElementSize, Dest, Src);
  StoreResult(Op, ALUOp, -1);
}

void OpDispatchBuilder::PSUBQOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  uint8_t ElementSize = 8;
  switch (Op->OP) {
  case 0xF8: ElementSize = 1; break;
  case 0xF9: ElementSize = 2; break;
  case 0xFA: ElementSize = 4; break;
  case 0xFB: ElementSize = 8; break;
  default: LogMan::Msg::A("Unknown PSUB op: 0x%04x", Op->OP); break;
  }

  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto ALUOp = _VSub(Size, ElementSize, Dest, Src);
  StoreResult(Op, ALUOp, -1);
}

template<size_t ElementSize>
void OpDispatchBuilder::PMINUOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto ALUOp = _VUMin(Size, ElementSize, Dest, Src);
  StoreResult(Op, ALUOp, -1);
}

template<size_t ElementSize>
void OpDispatchBuilder::PMAXUOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto ALUOp = _VUMax(Size, ElementSize, Dest, Src);
  StoreResult(Op, ALUOp, -1);
}

void OpDispatchBuilder::PMINSWOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto ALUOp = _VSMin(Size, 2, Dest, Src);
  StoreResult(Op, ALUOp, -1);
}

template<FEXCore::IR::IROps IROp, size_t ElementSize>
void OpDispatchBuilder::VectorALUOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto ALUOp = _VAdd(Size, ElementSize, Dest, Src);
  // Overwrite our IR's op type
  ALUOp.first->Header.Op = IROp;

  StoreResult(Op, ALUOp, -1);
}

template<FEXCore::IR::IROps IROp, size_t ElementSize>
void OpDispatchBuilder::VectorScalarALUOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src{};
  if (Op->Src1.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  }
  else {
    // Load a elementsize from memory and move it to a vector register
    Src = LoadSource_WithOpSize(Op, Op->Src1, ElementSize, Op->Flags, -1);
    switch (ElementSize) {
      case 4:
        Src = _SplatVector4(Src);
        break;
      case 8:
        Src = _SplatVector2(Src);
        break;
    }
  }

  // If OpSize == ElementSize then it only does the lower scalar op
  auto ALUOp = _VAdd(ElementSize, ElementSize, Dest, Src);
  // Overwrite our IR's op type
  ALUOp.first->Header.Op = IROp;

  // Insert the lower bits
  auto Result = _VInsElement(Size, ElementSize, 0, 0, Dest, ALUOp);

  StoreResult(Op, Result, -1);
}

template<FEXCore::IR::IROps IROp, size_t ElementSize, bool Scalar>
void OpDispatchBuilder::VectorUnaryOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  if (Scalar) {
    Size = ElementSize;
  }
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto ALUOp = _VFSqrt(Size, ElementSize, Src);
  // Overwrite our IR's op type
  ALUOp.first->Header.Op = IROp;

  // Insert the lower bits
  auto Result = _VInsElement(Size, ElementSize, 0, 0, Dest, ALUOp);

  StoreResult(Op, Result, -1);
}

void OpDispatchBuilder::MOVQOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  // This instruction is a bit special that if the destination is a register then it'll ZEXT the 64bit source to 128bit
  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, xmm[Op->Dest.TypeGPR.GPR - FEXCore::X86State::REG_XMM_0][0]), Src);
    auto Const = _Constant(0);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, xmm[Op->Dest.TypeGPR.GPR - FEXCore::X86State::REG_XMM_0][1]), Const);
  }
  else {
    // This is simple, just store the result
    StoreResult(Op, Src, -1);
  }
}

void OpDispatchBuilder::PMOVMSKBOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  OrderedNode *CurrentVal = _Constant(0);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  for (unsigned i = 0; i < Size; ++i) {
    // Extract the top bit of the element
    OrderedNode *Tmp = _Bfe(1, ((i + 1) * 8) - 1, Src);
    // Shift it to the correct location
    Tmp = _Lshl(Tmp, _Constant(i));

    // Or it with the current value
    CurrentVal = _Or(CurrentVal, Tmp);
  }
  StoreResult(Op, CurrentVal, -1);
}

void OpDispatchBuilder::PUNPCKLOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  uint8_t ElementSize = 8;
  switch (Op->OP) {
  case 0x60: ElementSize = 1; break;
  case 0x61: ElementSize = 2; break;
  case 0x62: ElementSize = 4; break;
  }

  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  auto ALUOp = _VZip(Size, ElementSize, Dest, Src);
  StoreResult(Op, ALUOp, -1);
}

void OpDispatchBuilder::PUNPCKHOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  uint8_t ElementSize = 8;
  switch (Op->OP) {
  case 0x68: ElementSize = 1; break;
  case 0x69: ElementSize = 2; break;
  case 0x6A: ElementSize = 4; break;
  case 0x6D: ElementSize = 8; break;
  }
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  auto ALUOp = _VZip2(Size, ElementSize, Dest, Src);
  StoreResult(Op, ALUOp, -1);
}

template<size_t ElementSize, bool Low>
void OpDispatchBuilder::PSHUFDOp(OpcodeArgs) {
  LogMan::Throw::A(ElementSize != 0, "What. No element size?");
  auto Size = GetSrcSize(Op);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  uint8_t Shuffle = Op->Src2.TypeLiteral.Literal;

  uint8_t NumElements = Size / ElementSize;
  if (ElementSize == 2) {
    NumElements /= 2;
  }

  uint8_t BaseElement = Low ? 0 : NumElements;

  auto Dest = Src;
  for (uint8_t Element = 0; Element < NumElements; ++Element) {
    Dest = _VInsElement(Size, ElementSize, BaseElement + Element, BaseElement + (Shuffle & 0b11), Dest, Src);
    Shuffle >>= 2;
  }

  StoreResult(Op, Dest, -1);
}

template<size_t ElementSize>
void OpDispatchBuilder::SHUFOp(OpcodeArgs) {
  LogMan::Throw::A(ElementSize != 0, "What. No element size?");
  auto Size = GetSrcSize(Op);
  OrderedNode *Src1 = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src2 = LoadSource(Op, Op->Src1, Op->Flags, -1);
  uint8_t Shuffle = Op->Src2.TypeLiteral.Literal;

  uint8_t NumElements = Size / ElementSize;

  auto Dest = Src1;
  std::array<OrderedNode*, 2> Srcs = {
    Src1, Src2
  };

  // [63:0]   = Src1[Selection]
  // [127:64] = Src2[Selection]
  for (uint8_t Element = 0; Element < NumElements; ++Element) {
    Dest = _VInsElement(Size, ElementSize, Element, Shuffle & 0b1, Dest, Srcs[Element]);
    Shuffle >>= 1;
  }

  StoreResult(Op, Dest, -1);
}
void OpDispatchBuilder::PCMPEQOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  uint8_t ElementSize = 4;
  switch (Op->OP) {
  case 0x74: ElementSize = 1; break;
  case 0x75: ElementSize = 2; break;
  case 0x76: ElementSize = 4; break;
  default: LogMan::Msg::A("Unknown ElementSize"); break;
  }
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  // This maps 1:1 to an AArch64 NEON Op
  auto ALUOp = _VCMPEQ(Size, ElementSize, Dest, Src);
  StoreResult(Op, ALUOp, -1);
}

template<size_t ElementSize>
void OpDispatchBuilder::PCMPGTOp(OpcodeArgs) {
  auto Size = GetSrcSize(Op);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  // This maps 1:1 to an AArch64 NEON Op
  auto ALUOp = _VCMPGT(Size, ElementSize, Dest, Src);
  StoreResult(Op, ALUOp, -1);
}

void OpDispatchBuilder::MOVDOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1,Op->Flags, -1);
  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR &&
      Op->Dest.TypeGPR.GPR >= FEXCore::X86State::REG_XMM_0) {
    // When destination is XMM then it is zext to 128bit
    uint64_t SrcSize = GetSrcSize(Op) * 8;
    while (SrcSize != 128) {
      Src = _Zext(SrcSize, Src);
      SrcSize *= 2;
    }
  }
  else {
    // Destination is GPR or mem
    // Extract from XMM first
    uint8_t ElementSize = 4;
    if (Op->Flags & X86Tables::DecodeFlags::FLAG_REX_WIDENING)
      ElementSize = 8;

    Src = _VExtractToGPR(GetSrcSize(Op), ElementSize, Src, 0);
  }

  StoreResult(Op, Op->Dest, Src, -1);
}

void OpDispatchBuilder::CMPXCHGOp(OpcodeArgs) {
// CMPXCHG ModRM, reg, {RAX}
// MemData = *ModRM.dest
// if (RAX == MemData)
//    modRM.dest = reg;
//    ZF = 1
// else
//    ZF = 0
// RAX = MemData
//
// CASL Xs, Xt, Xn
// MemData = *Xn
// if (MemData == Xs)
//    *Xn = Xt
// Xs = MemData

  auto Size = GetSrcSize(Op);
  // If this is a memory location then we want the pointer to it
  OrderedNode *Src1 = LoadSource(Op, Op->Dest, Op->Flags, -1, false);

  if (Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_FS_PREFIX) {
    Src1 = _Add(Src1, _LoadContext(8, offsetof(FEXCore::Core::CPUState, fs)));
  }
  else if (Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_GS_PREFIX) {
    Src1 = _Add(Src1, _LoadContext(8, offsetof(FEXCore::Core::CPUState, gs)));
  }

  // This is our source register
  OrderedNode *Src2 = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Src3 = _LoadContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]));
  // 0x80014000
  // 0x80064000
  // 0x80064000

  auto ZeroConst = _Constant(0);
  auto OneConst = _Constant(1);
  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    // If our destination is a GPR then this behaves differently
    // RAX = RAX == Op1 ? RAX : Op1
    // AKA if they match then don't touch RAX value
    // Otherwise set it to the rm operand
    OrderedNode *RAXResult = _Select(FEXCore::IR::COND_EQ,
      Src1, Src3,
      Src3, Src1);

    // Op1 = RAX == Op1 ? Op2 : Op1
    // If they match then set the rm operand to the input
    // else don't set the rm operand
    OrderedNode *DestResult = _Select(FEXCore::IR::COND_EQ,
        Src1, Src3,
        Src2, Src1);

    // ZF = RAX == Op1 ? 1 : 0
    // Result of compare
    OrderedNode *ZFResult = _Select(FEXCore::IR::COND_EQ,
      Src1, Src3,
      OneConst, ZeroConst);

    // Set ZF
    SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(ZFResult);
    if (Size < 4) {
      _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), RAXResult);
    }
    else {
      if (Size == 4) {
        RAXResult = _Zext(32, RAXResult);
      }
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), RAXResult);
    }

    // Store in to GPR Dest
    // Have to make sure this is after the result store in RAX for when Dest == RAX
    StoreResult(Op, DestResult, -1);
  }
  else {
    // DataSrc = *Src1
    // if (DataSrc == Src3) { *Src1 == Src2; } Src2 = DataSrc
    // This will write to memory! Careful!
    // Third operand must be a calculated guest memory address
    OrderedNode *CASResult = _CAS(Src3, Src2, Src1);

    // If our CASResult(OldMem value) is equal to our comparison
    // Then we managed to set the memory
    OrderedNode *ZFResult = _Select(FEXCore::IR::COND_EQ,
      CASResult, Src3,
      OneConst, ZeroConst);

    // RAX gets the result of the CAS op
    if (Size < 4) {
      _StoreContext(Size, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), CASResult);
    }
    else {
      if (Size == 4) {
        CASResult = _Zext(32, CASResult);
      }
      _StoreContext(8, offsetof(FEXCore::Core::CPUState, gregs[FEXCore::X86State::REG_RAX]), CASResult);
    }

    // Set ZF
    SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(ZFResult);
  }
}

OpDispatchBuilder::IRPair<IROp_CodeBlock> OpDispatchBuilder::CreateNewCodeBlock() {
  auto OldCursor = GetWriteCursor();
  SetWriteCursor(CodeBlocks.back());

  auto CodeNode = CreateCodeNode();

  auto NewNode = _Dummy();
  SetCodeNodeBegin(CodeNode, NewNode);

  auto EndBlock = _EndBlock(0);
  SetCodeNodeLast(CodeNode, EndBlock);

  if (CurrentCodeBlock) {
    LinkCodeBlocks(CurrentCodeBlock, CodeNode);
  }

  SetWriteCursor(OldCursor);

  return CodeNode;
}

void OpDispatchBuilder::SetCurrentCodeBlock(OrderedNode *Node) {
  CurrentCodeBlock = Node;
  LogMan::Throw::A(Node->Op(Data.Begin())->Op == OP_CODEBLOCK, "Node wasn't codeblock. It was '%s'", std::string(IR::GetName(Node->Op(Data.Begin())->Op)).c_str());
  SetWriteCursor(Node->Op(Data.Begin())->CW<IROp_CodeBlock>()->Begin.GetNode(ListData.Begin()));
}

void OpDispatchBuilder::CreateJumpBlocks(std::vector<FEXCore::Frontend::Decoder::DecodedBlocks> const *Blocks) {
  OrderedNode *PrevCodeBlock{};
  for (auto &Target : *Blocks) {
    auto CodeNode = CreateCodeNode();

    auto NewNode = _Dummy();
    SetCodeNodeBegin(CodeNode, NewNode);

    auto EndBlock = _EndBlock(0);
    SetCodeNodeLast(CodeNode, EndBlock);

    JumpTargets.try_emplace(Target.Entry, JumpTargetInfo{CodeNode, false});

    if (PrevCodeBlock) {
      LinkCodeBlocks(PrevCodeBlock, CodeNode);
    }

    PrevCodeBlock = CodeNode;
  }
}

void OpDispatchBuilder::BeginFunction(uint64_t RIP, std::vector<FEXCore::Frontend::Decoder::DecodedBlocks> const *Blocks) {
  Entry = RIP;
  auto IRHeader = _IRHeader(InvalidNode, RIP, 0);
  CreateJumpBlocks(Blocks);

  auto Block = GetNewJumpBlock(RIP);
  SetCurrentCodeBlock(Block);
  IRHeader.first->Blocks = Block->Wrapped(ListData.Begin());
}

void OpDispatchBuilder::Finalize() {
  // Node 0 is invalid node
  OrderedNode *RealNode = reinterpret_cast<OrderedNode*>(GetNode(1));
  FEXCore::IR::IROp_Header *IROp = RealNode->Op(Data.Begin());
  LogMan::Throw::A(IROp->Op == OP_IRHEADER, "First op in function must be our header");

  // Let's walk the jump blocks and see if we have handled every block target
  for (auto &Handler : JumpTargets) {
    if (Handler.second.HaveEmitted) continue;

    // We haven't emitted. Dump out to the dispatcher
    SetCurrentCodeBlock(Handler.second.BlockEntry);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, rip), _Constant(Handler.first));
    _ExitFunction();
  }
  CodeBlocks.clear();
}

void OpDispatchBuilder::ExitFunction() {
  _ExitFunction();
}

uint8_t OpDispatchBuilder::GetDstSize(FEXCore::X86Tables::DecodedOp Op) {
  constexpr std::array<uint8_t, 8> Sizes = {
    0, // Invalid DEF
    1,
    2,
    4,
    8,
    16,
    32,
    0, // Invalid DEF
  };

  uint32_t DstSizeFlag = FEXCore::X86Tables::DecodeFlags::GetSizeDstFlags(Op->Flags);
  uint8_t Size = Sizes[DstSizeFlag];
  LogMan::Throw::A(Size != 0, "Invalid destination size for op");
  return Size;
}

uint8_t OpDispatchBuilder::GetSrcSize(FEXCore::X86Tables::DecodedOp Op) {
  constexpr std::array<uint8_t, 8> Sizes = {
    0, // Invalid DEF
    1,
    2,
    4,
    8,
    16,
    32,
    0, // Invalid DEF
  };

  uint32_t SrcSizeFlag = FEXCore::X86Tables::DecodeFlags::GetSizeSrcFlags(Op->Flags);
  uint8_t Size = Sizes[SrcSizeFlag];
  LogMan::Throw::A(Size != 0, "Invalid destination size for op");
  return Size;
}

OrderedNode *OpDispatchBuilder::LoadSource_WithOpSize(FEXCore::X86Tables::DecodedOp const& Op, FEXCore::X86Tables::DecodedOperand const& Operand, uint8_t OpSize, uint32_t Flags, int8_t Align, bool LoadData, bool ForceLoad) {
  LogMan::Throw::A(Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR ||
          Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_LITERAL ||
          Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR_DIRECT ||
          Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR_INDIRECT ||
          Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_RIP_RELATIVE ||
          Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_SIB
        , "Unsupported Src type");

  OrderedNode *Src {nullptr};
  bool LoadableType = false;
  if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_LITERAL) {
    Src = _Constant(Operand.TypeLiteral.Size * 8, Operand.TypeLiteral.Literal);
  }
  else if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    if (Operand.TypeGPR.GPR >= FEXCore::X86State::REG_XMM_0) {
      Src = _LoadContext(OpSize, offsetof(FEXCore::Core::CPUState, xmm[Operand.TypeGPR.GPR - FEXCore::X86State::REG_XMM_0][Operand.TypeGPR.HighBits ? 1 : 0]));
    }
    else {
      Src = _LoadContext(OpSize, offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeGPR.GPR]) + (Operand.TypeGPR.HighBits ? 1 : 0));
    }
  }
  else if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR_DIRECT) {
    Src = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeGPR.GPR]));
    LoadableType = true;
  }
  else if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR_INDIRECT) {
    auto GPR = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeGPRIndirect.GPR]));
    auto Constant = _Constant(Operand.TypeGPRIndirect.Displacement);

		Src = _Add(GPR, Constant);

    LoadableType = true;
  }
  else if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_RIP_RELATIVE) {
    Src = _Constant(Operand.TypeRIPLiteral.Literal + Op->PC + Op->InstSize);
    LoadableType = true;
  }
  else if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_SIB) {
    OrderedNode *Tmp {};
    if (Operand.TypeSIB.Index != FEXCore::X86State::REG_INVALID) {
      Tmp = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeSIB.Index]));

      if (Operand.TypeSIB.Scale != 1) {
        auto Constant = _Constant(Operand.TypeSIB.Scale);
        Tmp = _Mul(Tmp, Constant);
      }
    }

    if (Operand.TypeSIB.Base != FEXCore::X86State::REG_INVALID) {
      auto GPR = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeSIB.Base]));
      if (Tmp != nullptr) {
        Tmp = _Add(Tmp, GPR);
      }
      else {
        Tmp = GPR;
      }
    }

    if (Operand.TypeSIB.Offset) {
      if (Tmp != nullptr) {
        Src = _Add(Tmp, _Constant(Operand.TypeSIB.Offset));
      }
      else {
        Src = _Constant(Operand.TypeSIB.Offset);
      }
    }
    else {
      if (Tmp != nullptr) {
        Src = Tmp;
      }
      else {
        Src = _Constant(0);
      }
    }

    LoadableType = true;
  }
  else {
    LogMan::Msg::A("Unknown Src Type: %d\n", Operand.TypeNone.Type);
  }

  if ((LoadableType && LoadData) || ForceLoad) {
    if (Flags & FEXCore::X86Tables::DecodeFlags::FLAG_FS_PREFIX) {
      Src = _Add(Src, _LoadContext(8, offsetof(FEXCore::Core::CPUState, fs)));
    }
    else if (Flags & FEXCore::X86Tables::DecodeFlags::FLAG_GS_PREFIX) {
      Src = _Add(Src, _LoadContext(8, offsetof(FEXCore::Core::CPUState, gs)));
    }

    Src = _LoadMem(OpSize, Src, Align == -1 ? OpSize : Align);
  }
  return Src;
}

OrderedNode *OpDispatchBuilder::LoadSource(FEXCore::X86Tables::DecodedOp const& Op, FEXCore::X86Tables::DecodedOperand const& Operand, uint32_t Flags, int8_t Align, bool LoadData, bool ForceLoad) {
  uint8_t OpSize = GetSrcSize(Op);
  return LoadSource_WithOpSize(Op, Operand, OpSize, Flags, Align, LoadData, ForceLoad);
}

void OpDispatchBuilder::StoreResult_WithOpSize(FEXCore::X86Tables::DecodedOp Op, FEXCore::X86Tables::DecodedOperand const& Operand, OrderedNode *const Src, uint8_t OpSize, int8_t Align) {
  LogMan::Throw::A((Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR ||
          Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_LITERAL ||
          Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR_DIRECT ||
          Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR_INDIRECT ||
          Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_RIP_RELATIVE ||
          Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_SIB
        ), "Unsupported Dest type");

  // 8Bit and 16bit destination types store their result without effecting the upper bits
  // 32bit ops ZEXT the result to 64bit
  OrderedNode *MemStoreDst {nullptr};
  bool MemStore = false;

  if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_LITERAL) {
    MemStoreDst = _Constant(Operand.TypeLiteral.Size * 8, Operand.TypeLiteral.Literal);
    MemStore = true; // Literals are ONLY hardcoded memory destinations
  }
  else if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    if (Operand.TypeGPR.GPR >= FEXCore::X86State::REG_XMM_0) {
      _StoreContext(Src, OpSize, offsetof(FEXCore::Core::CPUState, xmm[Operand.TypeGPR.GPR - FEXCore::X86State::REG_XMM_0][Operand.TypeGPR.HighBits ? 1 : 0]));
    }
    else {
      if (OpSize == 4) {
        LogMan::Throw::A(!Operand.TypeGPR.HighBits, "Can't handle 32bit store to high 8bit register");
        auto ZextOp = _Zext(Src, 32);

        _StoreContext(ZextOp, 8, offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeGPR.GPR]));
      }
      else {
        _StoreContext(Src, std::min(static_cast<uint8_t>(8), OpSize), offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeGPR.GPR]) + (Operand.TypeGPR.HighBits ? 1 : 0));
      }
    }
  }
  else if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR_DIRECT) {
    MemStoreDst = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeGPR.GPR]));
    MemStore = true;
  }
  else if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR_INDIRECT) {
    auto GPR = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeGPRIndirect.GPR]));
    auto Constant = _Constant(Operand.TypeGPRIndirect.Displacement);

    MemStoreDst = _Add(GPR, Constant);
    MemStore = true;
  }
  else if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_RIP_RELATIVE) {
    MemStoreDst = _Constant(Operand.TypeRIPLiteral.Literal + Op->PC + Op->InstSize);
    MemStore = true;
  }
  else if (Operand.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_SIB) {
    OrderedNode *Tmp {};
    if (Operand.TypeSIB.Index != FEXCore::X86State::REG_INVALID) {
      Tmp = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeSIB.Index]));

      if (Operand.TypeSIB.Scale != 1) {
        auto Constant = _Constant(Operand.TypeSIB.Scale);
        Tmp = _Mul(Tmp, Constant);
      }
    }

    if (Operand.TypeSIB.Base != FEXCore::X86State::REG_INVALID) {
      auto GPR = _LoadContext(8, offsetof(FEXCore::Core::CPUState, gregs[Operand.TypeSIB.Base]));

      if (Tmp != nullptr) {
        Tmp = _Add(Tmp, GPR);
      }
      else {
        Tmp = GPR;
      }
    }

    if (Operand.TypeSIB.Offset) {
      if (Tmp != nullptr) {
        MemStoreDst = _Add(Tmp, _Constant(Operand.TypeSIB.Offset));
      }
      else {
        MemStoreDst = _Constant(Operand.TypeSIB.Offset);
      }
    }
    else {
      if (Tmp != nullptr) {
        MemStoreDst = Tmp;
      }
      else {
        MemStoreDst = _Constant(0);
      }
    }

    MemStore = true;
  }

  if (MemStore) {
    if (Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_FS_PREFIX) {
      MemStoreDst = _Add(MemStoreDst, _LoadContext(8, offsetof(FEXCore::Core::CPUState, fs)));
    }
    else if (Op->Flags & FEXCore::X86Tables::DecodeFlags::FLAG_GS_PREFIX) {
      MemStoreDst = _Add(MemStoreDst, _LoadContext(8, offsetof(FEXCore::Core::CPUState, gs)));
    }

    if (OpSize == 10) {
      // For X87 extended doubles, split before storing
      auto Lower = _VExtractToGPR(16, 8, Src, 0);
      _StoreMem(8, MemStoreDst, Lower, Align);
      auto Upper = _VExtractToGPR(16, 8, Src, 1);
      auto DestAddr = _Add(MemStoreDst, _Constant(8));
      _StoreMem(2, DestAddr, Upper, std::min<uint8_t>(Align, 8));

    } else {
      _StoreMem(OpSize, MemStoreDst, Src, Align == -1 ? OpSize : Align);
    }
  }
}

void OpDispatchBuilder::StoreResult(FEXCore::X86Tables::DecodedOp Op, FEXCore::X86Tables::DecodedOperand const& Operand, OrderedNode *const Src, int8_t Align) {
  return StoreResult_WithOpSize(Op, Operand, Src, GetDstSize(Op), Align);
}

void OpDispatchBuilder::StoreResult(FEXCore::X86Tables::DecodedOp Op, OrderedNode *const Src, int8_t Align) {
  StoreResult(Op, Op->Dest, Src, Align);
}

OpDispatchBuilder::OpDispatchBuilder()
  : Data {8 * 1024 * 1024}
  , ListData {8 * 1024 * 1024} {
  ResetWorkingList();
}

void OpDispatchBuilder::ResetWorkingList() {
  Data.Reset();
  ListData.Reset();
  CodeBlocks.clear();
  JumpTargets.clear();
  BlockSetRIP = false;
  CurrentWriteCursor = nullptr;
  // This is necessary since we do "null" pointer checks
  InvalidNode = reinterpret_cast<OrderedNode*>(ListData.Allocate(sizeof(OrderedNode)));
  DecodeFailure = false;
  ShouldDump = false;
  CurrentCodeBlock = nullptr;
}

template<unsigned BitOffset>
void OpDispatchBuilder::SetRFLAG(OrderedNode *Value) {
  _StoreFlag(Value, BitOffset);
}
void OpDispatchBuilder::SetRFLAG(OrderedNode *Value, unsigned BitOffset) {
  _StoreFlag(Value, BitOffset);
}

OrderedNode *OpDispatchBuilder::GetRFLAG(unsigned BitOffset) {
  return _LoadFlag(BitOffset);
}
constexpr std::array<uint32_t, 17> FlagOffsets = {
  FEXCore::X86State::RFLAG_CF_LOC,
  FEXCore::X86State::RFLAG_PF_LOC,
  FEXCore::X86State::RFLAG_AF_LOC,
  FEXCore::X86State::RFLAG_ZF_LOC,
  FEXCore::X86State::RFLAG_SF_LOC,
  FEXCore::X86State::RFLAG_TF_LOC,
  FEXCore::X86State::RFLAG_IF_LOC,
  FEXCore::X86State::RFLAG_DF_LOC,
  FEXCore::X86State::RFLAG_OF_LOC,
  FEXCore::X86State::RFLAG_IOPL_LOC,
  FEXCore::X86State::RFLAG_NT_LOC,
  FEXCore::X86State::RFLAG_RF_LOC,
  FEXCore::X86State::RFLAG_VM_LOC,
  FEXCore::X86State::RFLAG_AC_LOC,
  FEXCore::X86State::RFLAG_VIF_LOC,
  FEXCore::X86State::RFLAG_VIP_LOC,
  FEXCore::X86State::RFLAG_ID_LOC,
};

void OpDispatchBuilder::SetPackedRFLAG(bool Lower8, OrderedNode *Src) {
  uint8_t NumFlags = FlagOffsets.size();
  if (Lower8) {
    NumFlags = 5;
  }
  auto OneConst = _Constant(1);
  for (int i = 0; i < NumFlags; ++i) {
    auto Tmp = _And(_Lshr(Src, _Constant(FlagOffsets[i])), OneConst);
    SetRFLAG(Tmp, FlagOffsets[i]);
  }
}

OrderedNode *OpDispatchBuilder::GetPackedRFLAG(bool Lower8) {
  OrderedNode *Original = _Constant(2);
  uint8_t NumFlags = FlagOffsets.size();
  if (Lower8) {
    NumFlags = 5;
  }

  for (int i = 0; i < NumFlags; ++i) {
    OrderedNode *Flag = _LoadFlag(FlagOffsets[i]);
    Flag = _Zext(32, Flag);
    Flag = _Lshl(Flag, _Constant(FlagOffsets[i]));
    Original = _Or(Original, Flag);
  }
  return Original;
}

void OpDispatchBuilder::GenerateFlags_ADC(FEXCore::X86Tables::DecodedOp Op, OrderedNode *Res, OrderedNode *Src1, OrderedNode *Src2, OrderedNode *CF) {
  auto Size = GetSrcSize(Op) * 8;
  // AF
  {
    OrderedNode *AFRes = _Xor(_Xor(Src1, Src2), Res);
    AFRes = _Bfe(1, 4, AFRes);
    SetRFLAG<FEXCore::X86State::RFLAG_AF_LOC>(AFRes);
  }

  // SF
  {
    auto SignBitConst = _Constant(GetSrcSize(Op) * 8 - 1);

    auto LshrOp = _Lshr(Res, SignBitConst);
    SetRFLAG<FEXCore::X86State::RFLAG_SF_LOC>(LshrOp);
  }

  // PF
  {
    auto PopCountOp = _Popcount(_And(Res, _Constant(0xFF)));

    auto XorOp = _Xor(PopCountOp, _Constant(1));
    SetRFLAG<FEXCore::X86State::RFLAG_PF_LOC>(XorOp);
  }

  // ZF
  {
    auto Dst8 = _Bfe(Size, 0, Res);

    auto SelectOp = _Select(FEXCore::IR::COND_EQ,
        Dst8, _Constant(0), _Constant(1), _Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(SelectOp);
  }

  // CF
  // Unsigned
  {
    auto Dst8 = _Bfe(Size, 0, Res);
    auto Src8 = _Bfe(Size, 0, Src2);

    auto SelectOpLT = _Select(FEXCore::IR::COND_ULT, Dst8, Src8, _Constant(1), _Constant(0));
    auto SelectOpLE = _Select(FEXCore::IR::COND_ULE, Dst8, Src8, _Constant(1), _Constant(0));
    auto SelectCF   = _Select(FEXCore::IR::COND_EQ, CF, _Constant(1), SelectOpLE, SelectOpLT);
    SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(SelectCF);
  }

  // OF
  // Signed
  {
    auto NegOne = _Constant(~0ULL);
    auto XorOp1 = _Xor(_Xor(Src1, Src2), NegOne);
    auto XorOp2 = _Xor(Res, Src1);
    OrderedNode *AndOp1 = _And(XorOp1, XorOp2);

    switch (Size) {
    case 8:
      AndOp1 = _Bfe(1, 7, AndOp1);
    break;
    case 16:
      AndOp1 = _Bfe(1, 15, AndOp1);
    break;
    case 32:
      AndOp1 = _Bfe(1, 31, AndOp1);
    break;
    case 64:
      AndOp1 = _Bfe(1, 63, AndOp1);
    break;
    default: LogMan::Msg::A("Unknown BFESize: %d", Size); break;
    }
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(AndOp1);
  }
}

void OpDispatchBuilder::GenerateFlags_SBB(FEXCore::X86Tables::DecodedOp Op, OrderedNode *Res, OrderedNode *Src1, OrderedNode *Src2, OrderedNode *CF) {
  // AF
  {
    OrderedNode *AFRes = _Xor(_Xor(Src1, Src2), Res);
    AFRes = _Bfe(1, 4, AFRes);
    SetRFLAG<FEXCore::X86State::RFLAG_AF_LOC>(AFRes);
  }

  // SF
  {
    auto SignBitConst = _Constant(GetSrcSize(Op) * 8 - 1);

    auto LshrOp = _Lshr(Res, SignBitConst);
    SetRFLAG<FEXCore::X86State::RFLAG_SF_LOC>(LshrOp);
  }

  // PF
  {
    auto PopCountOp = _Popcount(_And(Res, _Constant(0xFF)));

    auto XorOp = _Xor(PopCountOp, _Constant(1));
    SetRFLAG<FEXCore::X86State::RFLAG_PF_LOC>(XorOp);
  }

  // ZF
  {
    auto SelectOp = _Select(FEXCore::IR::COND_EQ,
        Res, _Constant(0), _Constant(1), _Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(SelectOp);
  }

  // CF
  // Unsigned
  {
    auto Dst8 = _Bfe(GetSrcSize(Op) * 8, 0, Res);
    auto Src8_1 = _Bfe(GetSrcSize(Op) * 8, 0, Src1);

    auto SelectOpLT = _Select(FEXCore::IR::COND_UGT, Dst8, Src8_1, _Constant(1), _Constant(0));
    auto SelectOpLE = _Select(FEXCore::IR::COND_UGE, Dst8, Src8_1, _Constant(1), _Constant(0));
    auto SelectCF   = _Select(FEXCore::IR::COND_EQ, CF, _Constant(1), SelectOpLE, SelectOpLT);
    SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(SelectCF);
  }

  // OF
  // Signed
  {
    auto XorOp1 = _Xor(Src1, Src2);
    auto XorOp2 = _Xor(Res, Src1);
    OrderedNode *AndOp1 = _And(XorOp1, XorOp2);

    switch (GetSrcSize(Op)) {
    case 1:
      AndOp1 = _Bfe(1, 7, AndOp1);
    break;
    case 2:
      AndOp1 = _Bfe(1, 15, AndOp1);
    break;
    case 4:
      AndOp1 = _Bfe(1, 31, AndOp1);
    break;
    case 8:
      AndOp1 = _Bfe(1, 63, AndOp1);
    break;
    default: LogMan::Msg::A("Unknown BFESize: %d", GetSrcSize(Op)); break;
    }
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(AndOp1);
  }
}

void OpDispatchBuilder::GenerateFlags_SUB(FEXCore::X86Tables::DecodedOp Op, OrderedNode *Res, OrderedNode *Src1, OrderedNode *Src2) {
  // AF
  {
    OrderedNode *AFRes = _Xor(_Xor(Src1, Src2), Res);
    AFRes = _Bfe(1, 4, AFRes);
    SetRFLAG<FEXCore::X86State::RFLAG_AF_LOC>(AFRes);
  }

  // SF
  {
    auto SignBitConst = _Constant(GetSrcSize(Op) * 8 - 1);

    auto LshrOp = _Lshr(Res, SignBitConst);
    SetRFLAG<FEXCore::X86State::RFLAG_SF_LOC>(LshrOp);
  }

  // PF
  {
    auto EightBitMask = _Constant(0xFF);
    auto PopCountOp = _Popcount(_And(Res, EightBitMask));
    auto XorOp = _Xor(PopCountOp, _Constant(1));
    SetRFLAG<FEXCore::X86State::RFLAG_PF_LOC>(XorOp);
  }

  // ZF
  {
    auto ZeroConst = _Constant(0);
    auto OneConst = _Constant(1);
    auto Bfe8 = _Bfe(GetSrcSize(Op) * 8, 0, Res);
    auto SelectOp = _Select(FEXCore::IR::COND_EQ,
        Bfe8, ZeroConst, OneConst, ZeroConst);
    SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(SelectOp);
  }

  // CF
  {
    auto ZeroConst = _Constant(0);
    auto OneConst = _Constant(1);

    auto SelectOp = _Select(FEXCore::IR::COND_ULT,
        Src1, Src2, OneConst, ZeroConst);

    SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(SelectOp);
  }
  // OF
  {
    auto XorOp1 = _Xor(Src1, Src2);
    auto XorOp2 = _Xor(Res, Src1);
    OrderedNode *FinalAnd = _And(XorOp1, XorOp2);

    switch (GetSrcSize(Op)) {
    case 1:
      FinalAnd = _Bfe(1, 7, FinalAnd);
    break;
    case 2:
      FinalAnd = _Bfe(1, 15, FinalAnd);
    break;
    case 4:
      FinalAnd = _Bfe(1, 31, FinalAnd);
    break;
    case 8:
      FinalAnd = _Bfe(1, 63, FinalAnd);
    break;
    default: LogMan::Msg::A("Unknown BFESize: %d", GetSrcSize(Op)); break;
    }
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(FinalAnd);
  }
}

void OpDispatchBuilder::GenerateFlags_ADD(FEXCore::X86Tables::DecodedOp Op, OrderedNode *Res, OrderedNode *Src1, OrderedNode *Src2) {
  // AF
  {
    OrderedNode *AFRes = _Xor(_Xor(Src1, Src2), Res);
    AFRes = _Bfe(1, 4, AFRes);
    SetRFLAG<FEXCore::X86State::RFLAG_AF_LOC>(AFRes);
  }

  // SF
  {
    auto SignBitConst = _Constant(GetSrcSize(Op) * 8 - 1);

    auto LshrOp = _Lshr(Res, SignBitConst);
    SetRFLAG<FEXCore::X86State::RFLAG_SF_LOC>(LshrOp);
  }

  // PF
  {
    auto EightBitMask = _Constant(0xFF);
    auto PopCountOp = _Popcount(_And(Res, EightBitMask));
    auto XorOp = _Xor(PopCountOp, _Constant(1));
    SetRFLAG<FEXCore::X86State::RFLAG_PF_LOC>(XorOp);
  }

  // ZF
  {
    auto SelectOp = _Select(FEXCore::IR::COND_EQ,
        Res, _Constant(0), _Constant(1), _Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(SelectOp);
  }
  // CF
  {
    auto Dst8 = _Bfe(GetSrcSize(Op) * 8, 0, Res);
    auto Src8 = _Bfe(GetSrcSize(Op) * 8, 0, Src2);

    auto SelectOp = _Select(FEXCore::IR::COND_ULT, Dst8, Src8, _Constant(1), _Constant(0));

    SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(SelectOp);
  }

  // OF
  {
    auto NegOne = _Constant(~0ULL);
    auto XorOp1 = _Xor(_Xor(Src1, Src2), NegOne);
    auto XorOp2 = _Xor(Res, Src1);

    OrderedNode *AndOp1 = _And(XorOp1, XorOp2);

    switch (GetSrcSize(Op)) {
    case 1:
      AndOp1 = _Bfe(1, 7, AndOp1);
    break;
    case 2:
      AndOp1 = _Bfe(1, 15, AndOp1);
    break;
    case 4:
      AndOp1 = _Bfe(1, 31, AndOp1);
    break;
    case 8:
      AndOp1 = _Bfe(1, 63, AndOp1);
    break;
    default: LogMan::Msg::A("Unknown BFESize: %d", GetSrcSize(Op)); break;
    }
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(AndOp1);
  }
}

void OpDispatchBuilder::GenerateFlags_MUL(FEXCore::X86Tables::DecodedOp Op, OrderedNode *Res, OrderedNode *High) {
  auto SignBitConst = _Constant(GetSrcSize(Op) * 8 - 1);

  // PF/AF/ZF/SF
  // Undefined
  {
    SetRFLAG<FEXCore::X86State::RFLAG_PF_LOC>(_Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_AF_LOC>(_Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(_Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_SF_LOC>(_Constant(0));
  }

  // CF/OF
  {
    // CF and OF are set if the result of the operation can't be fit in to the destination register
    // If the value can fit then the top bits will be zero

    auto SignBit = _Ashr(Res, SignBitConst);

    auto SelectOp = _Select(FEXCore::IR::COND_EQ, High, SignBit, _Constant(0), _Constant(1));

    SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(SelectOp);
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(SelectOp);
  }
}

void OpDispatchBuilder::GenerateFlags_UMUL(FEXCore::X86Tables::DecodedOp Op, OrderedNode *High) {
  // AF/SF/PF/ZF
  // Undefined
  {
    SetRFLAG<FEXCore::X86State::RFLAG_AF_LOC>(_Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_SF_LOC>(_Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_PF_LOC>(_Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(_Constant(0));
  }

  // CF/OF
  {
    // CF and OF are set if the result of the operation can't be fit in to the destination register
    // The result register will be all zero if it can't fit due to how multiplication behaves

    auto SelectOp = _Select(FEXCore::IR::COND_EQ, High, _Constant(0), _Constant(0), _Constant(1));

    SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(SelectOp);
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(SelectOp);
  }
}

void OpDispatchBuilder::GenerateFlags_Logical(FEXCore::X86Tables::DecodedOp Op, OrderedNode *Res, OrderedNode *Src1, OrderedNode *Src2) {
  // AF
  {
    // Undefined
    // Set to zero anyway
    SetRFLAG<FEXCore::X86State::RFLAG_AF_LOC>(_Constant(0));
  }

  // SF
  {
    auto SignBitConst = _Constant(GetSrcSize(Op) * 8 - 1);

    auto LshrOp = _Lshr(Res, SignBitConst);
    SetRFLAG<FEXCore::X86State::RFLAG_SF_LOC>(LshrOp);
  }

  // PF
  {
    auto EightBitMask = _Constant(0xFF);
    auto PopCountOp = _Popcount(_And(Res, EightBitMask));
    auto XorOp = _Xor(PopCountOp, _Constant(1));
    SetRFLAG<FEXCore::X86State::RFLAG_PF_LOC>(XorOp);
  }

  // ZF
  {
    auto SelectOp = _Select(FEXCore::IR::COND_EQ,
        Res, _Constant(0), _Constant(1), _Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(SelectOp);
  }

  // CF/OF
  {
    SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(_Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(_Constant(0));
  }
}

void OpDispatchBuilder::GenerateFlags_Shift(FEXCore::X86Tables::DecodedOp Op, OrderedNode *Res, OrderedNode *Src1, OrderedNode *Src2) {
  auto CmpResult = _Select(FEXCore::IR::COND_EQ, Src2, _Constant(0), _Constant(1), _Constant(0));
  auto CondJump = _CondJump(CmpResult);

  // Make sure to start a new block after ending this one
  auto JumpTarget = CreateNewCodeBlock();
  SetFalseJumpTarget(CondJump, JumpTarget);
  SetCurrentCodeBlock(JumpTarget);

  // AF
  {
    // Undefined
    // Set to zero anyway
    SetRFLAG<FEXCore::X86State::RFLAG_AF_LOC>(_Constant(0));
  }

  // SF
  {
    auto SignBitConst = _Constant(GetSrcSize(Op) * 8 - 1);

    auto LshrOp = _Lshr(Res, SignBitConst);
    SetRFLAG<FEXCore::X86State::RFLAG_SF_LOC>(LshrOp);
  }

  // PF
  {
    auto EightBitMask = _Constant(0xFF);
    auto PopCountOp = _Popcount(_And(Res, EightBitMask));
    auto XorOp = _Xor(PopCountOp, _Constant(1));
    SetRFLAG<FEXCore::X86State::RFLAG_PF_LOC>(XorOp);
  }

  // ZF
  {
    auto SelectOp = _Select(FEXCore::IR::COND_EQ,
        Res, _Constant(0), _Constant(1), _Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_ZF_LOC>(SelectOp);
  }

  // CF/OF
  {
    SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(_Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(_Constant(0));
  }

  auto Jump = _Jump();
  auto NextJumpTarget = CreateNewCodeBlock();
  SetJumpTarget(Jump, NextJumpTarget);
  SetTrueJumpTarget(CondJump, NextJumpTarget);
  SetCurrentCodeBlock(NextJumpTarget);
}

void OpDispatchBuilder::GenerateFlags_Rotate(FEXCore::X86Tables::DecodedOp Op, OrderedNode *Res, OrderedNode *Src1, OrderedNode *Src2) {
  // CF/OF
  // XXX: These are wrong
  {
    SetRFLAG<FEXCore::X86State::RFLAG_CF_LOC>(_Constant(0));
    SetRFLAG<FEXCore::X86State::RFLAG_OF_LOC>(_Constant(0));
  }
}

void OpDispatchBuilder::UnhandledOp(OpcodeArgs) {
  DecodeFailure = true;
}

void OpDispatchBuilder::MOVOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, 1);
  StoreResult(Op, Src, 1);
}

void OpDispatchBuilder::ALUOp(OpcodeArgs) {
  FEXCore::IR::IROps IROp;
  switch (Op->OP) {
  case 0x0:
  case 0x1:
  case 0x2:
  case 0x3:
  case 0x4:
  case 0x5:
    IROp = FEXCore::IR::IROps::OP_ADD;
  break;
  case 0x8:
  case 0x9:
  case 0xA:
  case 0xB:
  case 0xC:
  case 0xD:
    IROp = FEXCore::IR::IROps::OP_OR;
  break;
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x24:
  case 0x25:
    IROp = FEXCore::IR::IROps::OP_AND;
  break;
  case 0x28:
  case 0x29:
  case 0x2A:
  case 0x2B:
  case 0x2C:
  case 0x2D:
    IROp = FEXCore::IR::IROps::OP_SUB;
  break;
  case 0x30:
  case 0x31:
  case 0x32:
  case 0x33:
  case 0x34:
  case 0x35:
    IROp = FEXCore::IR::IROps::OP_XOR;
  break;
  default:
    IROp = FEXCore::IR::IROps::OP_LAST;
    LogMan::Msg::A("Unknown ALU Op: 0x%x", Op->OP);
  break;
  }

  // X86 basic ALU ops just do the operation between the destination and a single source
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  OrderedNode *Result{};
  OrderedNode *Dest{};
  if (DestIsLockedMem(Op)) {
    OrderedNode *DestMem = LoadSource(Op, Op->Dest, Op->Flags, -1, false);
    switch (IROp) {
      case FEXCore::IR::IROps::OP_ADD: {
        Dest = _AtomicFetchAdd(DestMem, Src, GetSrcSize(Op));
        Result = _Add(Dest, Src);
        break;
      }
      case FEXCore::IR::IROps::OP_SUB: {
        Dest = _AtomicFetchSub(DestMem, Src, GetSrcSize(Op));
        Result = _Sub(Dest, Src);
        break;
      }
      case FEXCore::IR::IROps::OP_OR: {
        Dest = _AtomicFetchOr(DestMem, Src, GetSrcSize(Op));
        Result = _Or(Dest, Src);
        break;
      }
      case FEXCore::IR::IROps::OP_AND: {
        Dest = _AtomicFetchAnd(DestMem, Src, GetSrcSize(Op));
        Result = _And(Dest, Src);
        break;
      }
      case FEXCore::IR::IROps::OP_XOR: {
        Dest = _AtomicFetchXor(DestMem, Src, GetSrcSize(Op));
        Result = _Xor(Dest, Src);
        break;
      }
      default: LogMan::Msg::A("Unknown Atomic IR Op: %d", IROp); break;
    }
  }
  else {
    Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

    auto ALUOp = _Add(Dest, Src);
    // Overwrite our IR's op type
    ALUOp.first->Header.Op = IROp;

    StoreResult(Op, ALUOp, -1);
    Result = ALUOp;
  }

  // Flags set
  {
    auto Size = GetSrcSize(Op) * 8;
    switch (IROp) {
    case FEXCore::IR::IROps::OP_ADD:
      GenerateFlags_ADD(Op, _Bfe(Size, 0, Result), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
    break;
    case FEXCore::IR::IROps::OP_SUB:
      GenerateFlags_SUB(Op, _Bfe(Size, 0, Result), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
    break;
    case FEXCore::IR::IROps::OP_MUL:
      GenerateFlags_MUL(Op, _Bfe(Size, 0, Result), _MulH(Dest, Src));
    break;
    case FEXCore::IR::IROps::OP_AND:
    case FEXCore::IR::IROps::OP_XOR:
    case FEXCore::IR::IROps::OP_OR: {
      GenerateFlags_Logical(Op, _Bfe(Size, 0, Result), _Bfe(Size, 0, Dest), _Bfe(Size, 0, Src));
    break;
    }
    default: break;
    }
  }
}

void OpDispatchBuilder::INTOp(OpcodeArgs) {
  uint8_t Reason{};
  uint8_t Literal{};
  bool setRIP = false;

  switch (Op->OP) {
  case 0xCD:
    Reason = 1;
    Literal = Op->Src1.TypeLiteral.Literal;
  break;
  case 0xCE:
    Reason = 2;
  break;
  case 0xF1:
    Reason = 3;
  break;
  case 0xF4: {
    Reason = 4;
    setRIP = true;
  break;
  }
  case 0x0B:
    Reason = 5;
  case 0xCC:
    Reason = 6;
    setRIP = true;
  break;
  break;
  }

  if (setRIP) {
    BlockSetRIP = setRIP;

    // We want to set RIP to the next instruction after HLT/INT3
    auto NewRIP = _Constant(Op->PC + Op->InstSize);
    _StoreContext(8, offsetof(FEXCore::Core::CPUState, rip), NewRIP);
  }

  if (Op->OP == 0xCE) { // Conditional to only break if Overflow == 1
    auto Flag = GetRFLAG(FEXCore::X86State::RFLAG_OF_LOC);

    // If condition doesn't hold then keep going
    auto CondJump = _CondJump(_Xor(Flag, _Constant(1)));
    auto FalseBlock = CreateNewCodeBlock();
    SetFalseJumpTarget(CondJump, FalseBlock);
    SetCurrentCodeBlock(FalseBlock);

    _Break(Reason, Literal);

    // Make sure to start a new block after ending this one
    auto JumpTarget = CreateNewCodeBlock();
    SetTrueJumpTarget(CondJump, JumpTarget);
    SetCurrentCodeBlock(JumpTarget);

  }
  else {
    _Break(Reason, Literal);
  }
}

template<size_t ElementSize>
void OpDispatchBuilder::PSRLD(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto Size = GetSrcSize(Op);

  auto Shift = _VUShr(Size, ElementSize, Dest, Src);
  StoreResult(Op, Shift, -1);
}

template<size_t ElementSize, bool Scalar>
void OpDispatchBuilder::PSLL(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto Size = GetDstSize(Op);

  OrderedNode *Result{};

  if (Scalar) {
    Result = _VUShlS(Size, ElementSize, Dest, Src);
  }
  else {
    Result = _VUShl(Size, ElementSize, Dest, Src);
  }

  StoreResult(Op, Result, -1);
}

void OpDispatchBuilder::PSRLDQ(OpcodeArgs) {
  LogMan::Throw::A(Op->Src1.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_LITERAL, "Src1 needs to be literal here");
  uint64_t Shift = Op->Src1.TypeLiteral.Literal;

  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto Size = GetDstSize(Op);

  auto Result = _VSRI(Size, 16, Dest, Shift);
  StoreResult(Op, Result, -1);
}

void OpDispatchBuilder::PSLLDQ(OpcodeArgs) {
  LogMan::Throw::A(Op->Src1.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_LITERAL, "Src1 needs to be literal here");
  uint64_t Shift = Op->Src1.TypeLiteral.Literal;

  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  auto Size = GetDstSize(Op);

  auto Result = _VSLI(Size, 16, Dest, Shift);
  StoreResult(Op, Result, -1);
}

void OpDispatchBuilder::MOVDDUPOp(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);
  OrderedNode *Res = _CreateVector2(Src, Src);
  StoreResult(Op, Res, -1);
}

template<size_t DstElementSize, bool Signed>
void OpDispatchBuilder::CVT(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  if (Op->Flags & X86Tables::DecodeFlags::FLAG_REX_WIDENING) {
    // Source is 64bit
    if (DstElementSize == 4) {
      Src = _Bfe(32, 0, Src);
    }
  }
  else {
    // Source is 32bit
    if (DstElementSize == 8) {
      if (Signed)
        Src = _Sext(32, Src);
      else
        Src = _Zext(32, Src);
    }
  }

  if (Signed)
    Src = _SCVTF(Src, DstElementSize);
  else
    Src = _UCVTF(Src, DstElementSize);

  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);

  Src = _VInsElement(16, DstElementSize, 0, 0, Dest, Src);

  StoreResult(Op, Src, -1);
}

template<size_t SrcElementSize, bool Signed>
void OpDispatchBuilder::FCVT(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  if (Signed)
    Src = _FCVTZS(Src, SrcElementSize);
  else
    Src = _FCVTZU(Src, SrcElementSize);

  StoreResult(Op, Src, -1);

}

template<size_t DstElementSize, size_t SrcElementSize>
void OpDispatchBuilder::FCVTF(OpcodeArgs) {
  OrderedNode *Dest = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  Src = _FCVTF(Src, DstElementSize, SrcElementSize);
  Src = _VInsElement(16, DstElementSize, 0, 0, Dest, Src);

  StoreResult(Op, Src, -1);
}

void OpDispatchBuilder::TZCNT(OpcodeArgs) {
  OrderedNode *Src = LoadSource(Op, Op->Src1, Op->Flags, -1);

  Src = _FindTrailingZeros(Src);
  StoreResult(Op, Src, -1);
}

void OpDispatchBuilder::MOVSSOp(OpcodeArgs) {
  if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR &&
      Op->Src1.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    // MOVSS xmm1, xmm2
    OrderedNode *Dest = LoadSource_WithOpSize(Op, Op->Dest, 16, Op->Flags, -1);
    OrderedNode *Src = LoadSource_WithOpSize(Op, Op->Src1, 16, Op->Flags, -1);
    auto Result = _VInsElement(16, 4, 0, 0, Dest, Src);
    StoreResult(Op, Result, -1);
  }
  else if (Op->Dest.TypeNone.Type == FEXCore::X86Tables::DecodedOperand::TYPE_GPR) {
    // MOVSS xmm1, mem32
    OrderedNode *Dest = LoadSource_WithOpSize(Op, Op->Dest, 16, Op->Flags, -1);
    OrderedNode *Src = LoadSource_WithOpSize(Op, Op->Src1, 4, Op->Flags, -1);
    Src = _VCastFromGPR(16, 4, Src);
    auto Result = _VInsElement(16, 8, 0, 0, Dest, Src);
    StoreResult(Op, Result, -1);
  }
  else {
    // MOVSS mem32, xmm1
    OrderedNode *Src = LoadSource_WithOpSize(Op, Op->Src1, 16, Op->Flags, -1);
    auto Result = _VExtractToGPR(16, 4, Src, 0);
    StoreResult_WithOpSize(Op, Op->Dest, Result, 4, -1);
  }
}


OrderedNode *OpDispatchBuilder::GetX87Top() {
  // Yes, we are storing 3 bits in a single flag register.
  // Deal with it
  return _LoadContext(1, offsetof(FEXCore::Core::CPUState, flags) + FEXCore::X86State::X87FLAG_TOP_LOC);
}

void OpDispatchBuilder::SetX87Top(OrderedNode *Value) {
  _StoreContext(Value, 1, offsetof(FEXCore::Core::CPUState, flags) + FEXCore::X86State::X87FLAG_TOP_LOC);
}

template<size_t width>
void OpDispatchBuilder::FLD(OpcodeArgs) {
  // Update TOP
  auto orig_top = GetX87Top();
  auto top = _And(_Sub(orig_top, _Constant(1)), _Constant(7));
  SetX87Top(top);

  size_t read_width = (width == 80) ? 16 : width / 8;

  // Read from memory
  auto data = LoadSource_WithOpSize(Op, Op->Src1, read_width, Op->Flags, -1);

  OrderedNode *converted;

  // Convert to 80bit float
  if (width == 32 || width == 64) {
    _Zext(32, data);
    if (width == 32)
      data = _Zext(32, data);

    constexpr size_t mantissa_bits = (width == 32) ? 23 : 52;
    constexpr size_t sign_bits = width - (mantissa_bits + 1);

    uint64_t sign_mask = (width == 32) ? 0x80000000 : 0x8000000000000000;
    uint64_t exponent_mask = (width == 32) ? 0x7F800000 : 0x7FE0000000000000;
    uint64_t lower_mask = (width == 32) ? 0x007FFFFF : 0x001FFFFFFFFFFFFF;
    auto sign = _Lshr(_And(data, _Constant(sign_mask)), _Constant(width - 16));
    auto exponent = _Lshr(_And(data, _Constant(exponent_mask)), _Constant(mantissa_bits));
    auto lower = _Lshl(_And(data, _Constant(lower_mask)), _Constant(63 - mantissa_bits));

    // XXX: Need to handle NaN/Infinities
    constexpr size_t exponent_zero = (1 << (sign_bits-1));
    auto adjusted_exponent = _Add(exponent, _Constant(0x4000 - exponent_zero));
    auto upper = _Or(sign, adjusted_exponent);

    // XXX: Need to support decoding of denormals
    auto intergerBit = _Constant(1ULL << 63);

    converted = _VCastFromGPR(16, 8, _Or(intergerBit, lower));
    converted = _VInsElement(16, 8, 1, 0, converted, _VCastFromGPR(16, 8, upper));
  }
  else if (width == 80) {
    // TODO
  }
  // Write to ST[TOP]
  _StoreContextIndexed(converted, top, 16, offsetof(FEXCore::Core::CPUState, mm[0][0]), 16);
  //_StoreContext(converted, 16, offsetof(FEXCore::Core::CPUState, mm[7][0]));
}

template<size_t width, bool pop>
void OpDispatchBuilder::FST(OpcodeArgs) {
  auto orig_top = GetX87Top();
  if (width == 80) {
    auto data = _LoadContextIndexed(orig_top, 16, offsetof(FEXCore::Core::CPUState, mm[0][0]), 16);
    StoreResult_WithOpSize(Op, Op->Dest, data, 10, 1);
  }

  // TODO: Other widths

  if (pop) {
    auto top = _And(_Add(orig_top, _Constant(1)), _Constant(7));
    SetX87Top(top);
  }
}

void OpDispatchBuilder::FADD(OpcodeArgs) {
  auto top = GetX87Top();
  OrderedNode* arg;

  auto mask = _Constant(7);

  if (Op->Src1.TypeNone.Type != 0) {
    // Memory arg
    arg = LoadSource(Op, Op->Src1, Op->Flags, -1);
  } else {
    // Implicit arg
    auto offset = _Constant(Op->OP & 7);
    arg = _And(_Add(top, offset), mask);
  }

  auto a = _LoadContextIndexed(top, 16, offsetof(FEXCore::Core::CPUState, mm[0][0]), 16);
  auto b = _LoadContextIndexed(arg, 16, offsetof(FEXCore::Core::CPUState, mm[0][0]), 16);

  // _StoreContext(a, 16, offsetof(FEXCore::Core::CPUState, mm[0][0]));
  // _StoreContext(b, 16, offsetof(FEXCore::Core::CPUState, mm[1][0]));

  auto ExponentMask = _Constant(0x7FFF);

  //auto result = _F80Add(a, b);
  // TODO: Handle sign and negative additions.

  // TODO: handle NANs (and other weird numbers?)

  auto a_Exponent = _And(_VExtractToGPR(16, 8, a, 1), ExponentMask);
  auto b_Exponent = _And(_VExtractToGPR(16, 8, b, 1), ExponentMask);
  auto shift = _Sub(a_Exponent, b_Exponent);

  auto zero = _Constant(0);

  auto ExponentLarger  = _Select(COND_ULT, shift, zero, a_Exponent, b_Exponent);

  auto a_Mantissa = _VExtractToGPR(16, 8, a, 0);
  auto b_Mantissa = _VExtractToGPR(16, 8, b, 0);
  auto MantissaLarger  = _Select(COND_ULT, shift, zero, a_Mantissa, b_Mantissa);
  auto MantissaSmaller = _Select(COND_ULT, shift, zero, b_Mantissa, a_Mantissa);

  auto invertedShift   = _Select(COND_ULT, shift, zero, _Neg(shift), shift);
  auto MantissaSmallerShifted = _Lshr(MantissaSmaller, invertedShift);

  auto MantissaSummed = _Add(MantissaLarger, MantissaSmallerShifted);

  auto one = _Constant(1);
  // Hacky way to detect overflow and adjust
  auto ExponentAdjusted = _Select(COND_ULT, MantissaLarger, MantissaSummed, ExponentLarger, _Add(ExponentLarger, one));
  auto MantissaShifted = _Or(_Constant(1ULL << 63), _Lshr(MantissaSummed, one));
  auto MantissaAdjusted = _Select(COND_ULT, MantissaLarger, MantissaSummed, MantissaSummed, MantissaShifted);

  // TODO: Rounding, Infinities, exceptions, precision, tags?

  auto lower = _VCastFromGPR(16, 8, MantissaAdjusted);
  auto upper = _VCastFromGPR(16, 8, ExponentAdjusted);

  auto result = _VInsElement(16, 8, 1, 0, lower, upper);

  if ((Op->TableInfo->Flags & X86Tables::InstFlags::FLAGS_POP) != 0) {
    top = _And(_Add(top, one), mask);
    SetX87Top(top);
  }

  // Write to ST[TOP]
  _StoreContextIndexed(result, top, 16, offsetof(FEXCore::Core::CPUState, mm[0][0]), 16);
}

void OpDispatchBuilder::FXSaveOp(OpcodeArgs) {
  OrderedNode *Mem = LoadSource(Op, Op->Dest, Op->Flags, -1, false);

  // Saves 512bytes to the memory location provided
  // Header changes depending on if REX.W is set or not
  if (Op->Flags & X86Tables::DecodeFlags::FLAG_REX_WIDENING) {
    // BYTE | 0 1 | 2 3 | 4   | 5     | 6 7 | 8 9 | a b | c d | e f |
    // ------------------------------------------
    //   00 | FCW | FSW | FTW | <R>   | FOP | FIP                   |
    //   16 | FDP                           | MXCSR     | MXCSR_MASK|
  }
  else {
    // BYTE | 0 1 | 2 3 | 4   | 5     | 6 7 | 8 9 | a b | c d | e f |
    // ------------------------------------------
    //   00 | FCW | FSW | FTW | <R>   | FOP | FIP[31:0] | FCS | <R> |
    //   16 | FDP[31:0] | FDS         | <R> | MXCSR     | MXCSR_MASK|
  }

  // BYTE | 0 1 | 2 3 | 4   | 5     | 6 7 | 8 9 | a b | c d | e f |
  // ------------------------------------------
  //   32 | ST0/MM0                             | <R>
  //   48 | ST1/MM1                             | <R>
  //   64 | ST2/MM2                             | <R>
  //   80 | ST3/MM3                             | <R>
  //   96 | ST4/MM4                             | <R>
  //  112 | ST5/MM5                             | <R>
  //  128 | ST6/MM6                             | <R>
  //  144 | ST7/MM7                             | <R>
  //  160 | XMM0
  //  173 | XMM1
  //  192 | XMM2
  //  208 | XMM3
  //  224 | XMM4
  //  240 | XMM5
  //  256 | XMM6
  //  272 | XMM7
  //  288 | XMM8
  //  304 | XMM9
  //  320 | XMM10
  //  336 | XMM11
  //  352 | XMM12
  //  368 | XMM13
  //  384 | XMM14
  //  400 | XMM15
  //  416 | <R>
  //  432 | <R>
  //  448 | <R>
  //  464 | Available
  //  480 | Available
  //  496 | Available
  // FCW: x87 FPU control word
  // FSW: x87 FPU status word
  // FTW: x87 FPU Tag word (Abridged)
  // FOP: x87 FPU opcode. Lower 11 bits of the opcode
  // FIP: x87 FPU instructyion pointer offset
  // FCS: x87 FPU instruction pointer selector. If CPUID_0000_0007_0000_00000:EBX[bit 13] = 1 then this is deprecated and stores as 0
  // FDP: x87 FPU instruction operand (data) pointer offset
  // FDS: x87 FPU instruction operand (data) pointer selector. Same deprecation as FCS
  // MXCSR: If OSFXSR bit in CR4 is not set then this may not be saved
  // MXCSR_MASK: Mask for writes to the MXCSR register
  // If OSFXSR bit in CR4 is not set than FXSAVE /may/ not save the XMM registers
  // This is implementation dependent
  for (unsigned i = 0; i < 8; ++i) {
    OrderedNode *MMReg = _LoadContext(16, offsetof(FEXCore::Core::CPUState, mm[i]));
    OrderedNode *MemLocation = _Add(Mem, _Constant(i * 16 + 32));

    _StoreMem(16, MemLocation, MMReg, 16);
  }
  for (unsigned i = 0; i < 16; ++i) {
    OrderedNode *XMMReg = _LoadContext(16, offsetof(FEXCore::Core::CPUState, xmm[i]));
    OrderedNode *MemLocation = _Add(Mem, _Constant(i * 16 + 160));

    _StoreMem(16, MemLocation, XMMReg, 16);
  }
}

void OpDispatchBuilder::FXRStoreOp(OpcodeArgs) {
  OrderedNode *Mem = LoadSource(Op, Op->Src1, Op->Flags, -1, false);
  for (unsigned i = 0; i < 8; ++i) {
    OrderedNode *MemLocation = _Add(Mem, _Constant(i * 16 + 32));
    auto MMReg = _LoadMem(16, MemLocation, 16);
    _StoreContext(16, offsetof(FEXCore::Core::CPUState, mm[i]), MMReg);
  }
  for (unsigned i = 0; i < 16; ++i) {
    OrderedNode *MemLocation = _Add(Mem, _Constant(i * 16 + 160));
    auto XMMReg = _LoadMem(16, MemLocation, 16);
    _StoreContext(16, offsetof(FEXCore::Core::CPUState, xmm[i]), XMMReg);
  }
}

void OpDispatchBuilder::PAlignrOp(OpcodeArgs) {
  OrderedNode *Src1 = LoadSource(Op, Op->Dest, Op->Flags, -1);
  OrderedNode *Src2 = LoadSource(Op, Op->Src1, Op->Flags, -1);

  uint8_t Index = Op->Src2.TypeLiteral.Literal;
  OrderedNode *Res = _VExtr(GetDstSize(Op), 1, Src1, Src2, Index);
  StoreResult(Op, Res, -1);
}

void OpDispatchBuilder::UnimplementedOp(OpcodeArgs) {
  // We don't actually support this instruction
  // Multiblock may hit it though
  _Break(0, 0);
}

#undef OpcodeArgs

void OpDispatchBuilder::ReplaceAllUsesWithInclusive(OrderedNode *Node, OrderedNode *NewNode, IR::NodeWrapperIterator After, IR::NodeWrapperIterator End) {
  uintptr_t ListBegin = ListData.Begin();
  uintptr_t DataBegin = Data.Begin();

  while (After != End) {
    OrderedNodeWrapper *WrapperOp = After();
    OrderedNode *RealNode = WrapperOp->GetNode(ListBegin);
    FEXCore::IR::IROp_Header *IROp = RealNode->Op(DataBegin);

    uint8_t NumArgs = IR::GetArgs(IROp->Op);
    for (uint8_t i = 0; i < NumArgs; ++i) {
      if (IROp->Args[i].ID() == Node->Wrapped(ListBegin).ID()) {
        Node->RemoveUse();
        NewNode->AddUse();
        IROp->Args[i].NodeOffset = NewNode->Wrapped(ListBegin).NodeOffset;
      }
    }

    ++After;
  }
}

void OpDispatchBuilder::RemoveArgUses(OrderedNode *Node) {
  uintptr_t ListBegin = ListData.Begin();
  uintptr_t DataBegin = Data.Begin();

  FEXCore::IR::IROp_Header *IROp = Node->Op(DataBegin);

  uint8_t NumArgs = IR::GetArgs(IROp->Op);
  for (uint8_t i = 0; i < NumArgs; ++i) {
    auto ArgNode = IROp->Args[i].GetNode(ListBegin);
    ArgNode->RemoveUse();
  }
}

void OpDispatchBuilder::Remove(OrderedNode *Node) {
  RemoveArgUses(Node);

  Node->Unlink(ListData.Begin());
}

void InstallOpcodeHandlers() {
  const std::vector<std::tuple<uint8_t, uint8_t, X86Tables::OpDispatchPtr>> BaseOpTable = {
    // Instructions
    {0x00, 6, &OpDispatchBuilder::ALUOp},
    {0x08, 6, &OpDispatchBuilder::ALUOp},
    {0x10, 6, &OpDispatchBuilder::ADCOp},
    {0x18, 6, &OpDispatchBuilder::SBBOp},
    {0x20, 6, &OpDispatchBuilder::ALUOp},
    {0x28, 6, &OpDispatchBuilder::ALUOp},
    {0x30, 6, &OpDispatchBuilder::ALUOp},
    {0x38, 6, &OpDispatchBuilder::CMPOp},
    {0x50, 8, &OpDispatchBuilder::PUSHOp},
    {0x58, 8, &OpDispatchBuilder::POPOp},
    {0x68, 1, &OpDispatchBuilder::PUSHOp},
    {0x6A, 1, &OpDispatchBuilder::PUSHOp},

    {0x63, 1, &OpDispatchBuilder::MOVSXDOp},
    {0x69, 1, &OpDispatchBuilder::IMUL2SrcOp},
    {0x6B, 1, &OpDispatchBuilder::IMUL2SrcOp},
    {0x70, 16, &OpDispatchBuilder::CondJUMPOp},
    {0x84, 2, &OpDispatchBuilder::TESTOp},
    {0x86, 2, &OpDispatchBuilder::XCHGOp},
    {0x88, 1, &OpDispatchBuilder::MOVOp},
    {0x89, 1, &OpDispatchBuilder::MOVOp},
    // XXX: Causes LLVM to hang?
    {0x8A, 1, &OpDispatchBuilder::MOVOp},
    {0x8B, 1, &OpDispatchBuilder::MOVOp},

    {0x8D, 1, &OpDispatchBuilder::LEAOp},
    {0x90, 8, &OpDispatchBuilder::XCHGOp},

    {0x98, 1, &OpDispatchBuilder::CDQOp},
    {0x99, 1, &OpDispatchBuilder::CQOOp},
    {0x9E, 1, &OpDispatchBuilder::SAHFOp},
    {0x9F, 1, &OpDispatchBuilder::LAHFOp},
    {0xA0, 4, &OpDispatchBuilder::MOVOffsetOp},
    {0xA4, 2, &OpDispatchBuilder::MOVSOp},
    // XXX: Causes issues with ld.so
    {0xA6, 2, &OpDispatchBuilder::CMPSOp},
    {0xA8, 2, &OpDispatchBuilder::TESTOp},
    {0xAA, 2, &OpDispatchBuilder::STOSOp},
    {0xAE, 2, &OpDispatchBuilder::SCASOp},
    {0xB0, 8, &OpDispatchBuilder::MOVOp},
    {0xB8, 8, &OpDispatchBuilder::MOVOp},
    {0xC2, 2, &OpDispatchBuilder::RETOp},
    {0xC9, 1, &OpDispatchBuilder::LEAVEOp},
    {0xCC, 3, &OpDispatchBuilder::INTOp},
    {0xE8, 1, &OpDispatchBuilder::CALLOp},
    {0xE9, 1, &OpDispatchBuilder::JUMPOp},
    {0xEB, 1, &OpDispatchBuilder::JUMPOp},
    {0xF1, 1, &OpDispatchBuilder::INTOp},
    {0xF4, 1, &OpDispatchBuilder::INTOp},
    {0xF5, 1, &OpDispatchBuilder::FLAGControlOp},
    {0xF8, 2, &OpDispatchBuilder::FLAGControlOp},
    {0xFC, 2, &OpDispatchBuilder::FLAGControlOp},
  };

  const std::vector<std::tuple<uint8_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> TwoByteOpTable = {
     // Instructions
     {0x00, 1, nullptr}, // GROUP 6
     {0x01, 1, nullptr}, // GROUP 7
     {0x05, 1, &OpDispatchBuilder::SyscallOp},
     {0x0B, 1, &OpDispatchBuilder::INTOp},
     {0x0D, 1, nullptr}, // GROUP P
     {0x18, 1, nullptr}, // GROUP 16

     {0x19, 7, &OpDispatchBuilder::NOPOp}, // NOP with ModRM

     {0x31, 1, &OpDispatchBuilder::RDTSCOp},

     {0x40, 16, &OpDispatchBuilder::CMOVOp},
     {0x6E, 1, &OpDispatchBuilder::UnhandledOp}, // MOVD
     {0x7E, 1, &OpDispatchBuilder::UnhandledOp}, // MOVD
     {0x80, 16, &OpDispatchBuilder::CondJUMPOp}, // XXX: Fails to fixup some jumps
     {0x90, 16, &OpDispatchBuilder::SETccOp}, // XXX: Causes some unit tests to fail due to flags being incorrect
     {0xA2, 1, &OpDispatchBuilder::CPUIDOp},
     {0xA3, 1, &OpDispatchBuilder::BTOp}, // BT
     {0xA4, 2, &OpDispatchBuilder::SHLDOp},
     {0xAB, 1, &OpDispatchBuilder::BTSOp},
     {0xAC, 2, &OpDispatchBuilder::SHRDOp},
     {0xAF, 1, &OpDispatchBuilder::IMUL1SrcOp}, // XXX: Causes issues with LLVM JIT
     {0xB0, 2, &OpDispatchBuilder::CMPXCHGOp}, // CMPXCHG
     {0xB3, 1, &OpDispatchBuilder::BTROp},
     {0xB6, 2, &OpDispatchBuilder::MOVZXOp},
     {0xBB, 1, &OpDispatchBuilder::BTCOp},
     {0xBC, 1, &OpDispatchBuilder::BSFOp}, // BSF
     {0xBD, 1, &OpDispatchBuilder::BSROp}, // BSF
     // XXX: Broken on LLVM?
     {0xBE, 2, &OpDispatchBuilder::MOVSXOp},
     {0xC0, 2, &OpDispatchBuilder::XADDOp},
     {0xC8, 8, &OpDispatchBuilder::BSWAPOp},

     // SSE
     // XXX: Broken on LLVM?
     {0x10, 2, &OpDispatchBuilder::MOVUPSOp},
     {0x16, 1, &OpDispatchBuilder::MOVLHPSOp},
     {0x17, 1, &OpDispatchBuilder::MOVUPSOp},
     {0x28, 2, &OpDispatchBuilder::MOVUPSOp},
     {0x54, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VAND, 16>},
     {0x56, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VOR, 16>},
     {0x57, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VXOR, 16>},
     {0x58, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFADD, 4>},
     {0x59, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFMUL, 4>},
     {0x5C, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFSUB, 4>},
     {0x5D, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFMIN, 4>},
     {0x5E, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFDIV, 4>},
     {0x5F, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFMAX, 4>},
     {0x60, 3, &OpDispatchBuilder::PUNPCKLOp},
     {0x64, 1, &OpDispatchBuilder::PCMPGTOp<1>},
     {0x65, 1, &OpDispatchBuilder::PCMPGTOp<2>},
     {0x66, 1, &OpDispatchBuilder::PCMPGTOp<4>},
     {0x68, 3, &OpDispatchBuilder::UnhandledOp},
     {0x6C, 1, &OpDispatchBuilder::UnhandledOp},
     {0x71, 1, nullptr}, // GROUP 12
     {0x72, 1, nullptr}, // GROUP 13
     {0x73, 1, nullptr}, // GROUP 14

     {0x74, 3, &OpDispatchBuilder::PCMPEQOp},
     {0xAE, 1, nullptr}, // GROUP 15
     {0xB9, 1, nullptr}, // GROUP 10
     {0xBA, 1, nullptr}, // GROUP 8
     {0xC7, 1, nullptr}, // GROUP 9

     {0xD4, 1, &OpDispatchBuilder::PADDQOp},
     {0xD6, 1, &OpDispatchBuilder::MOVQOp},
     {0xD7, 1, &OpDispatchBuilder::PMOVMSKBOp},
     // XXX: Untested
     {0xDA, 1, &OpDispatchBuilder::PMINUOp<1>},
     {0xDE, 1, &OpDispatchBuilder::PMAXUOp<1>},
     {0xEA, 1, &OpDispatchBuilder::PMINSWOp},

     {0xEB, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VOR, 8>},
     {0xEF, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VXOR, 8>},
     {0xF8, 4, &OpDispatchBuilder::PSUBQOp},
     {0xFE, 1, &OpDispatchBuilder::PADDQOp},
  };

   const std::vector<std::tuple<uint16_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> PrimaryGroupOpTable = {
 #define OPD(group, prefix, Reg) (((group - FEXCore::X86Tables::TYPE_GROUP_1) << 6) | (prefix) << 3 | (Reg))
    // GROUP 1
    // XXX: Something in this group causing bad syscall when commented out
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 0), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 1), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 2), 1, &OpDispatchBuilder::ADCOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 3), 1, &OpDispatchBuilder::SBBOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 4), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 5), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 6), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x80), 7), 1, &OpDispatchBuilder::CMPOp}, // CMP

    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 0), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 1), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 2), 1, &OpDispatchBuilder::ADCOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 3), 1, &OpDispatchBuilder::SBBOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 4), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 5), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 6), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x81), 7), 1, &OpDispatchBuilder::CMPOp},

    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 0), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 1), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 2), 1, &OpDispatchBuilder::ADCOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 3), 1, &OpDispatchBuilder::SBBOp}, // Unit tests find this setting flags incorrectly
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 4), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 5), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 6), 1, &OpDispatchBuilder::SecondaryALUOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_1, OpToIndex(0x83), 7), 1, &OpDispatchBuilder::CMPOp},

    // GROUP 2
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xC0), 0), 1, &OpDispatchBuilder::ROLOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xC0), 1), 1, &OpDispatchBuilder::ROROp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xC0), 4), 1, &OpDispatchBuilder::SHLOp<false>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xC0), 5), 1, &OpDispatchBuilder::SHROp<false>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xC0), 7), 1, &OpDispatchBuilder::ASHROp}, // SAR

    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xC1), 0), 1, &OpDispatchBuilder::ROLOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xC1), 1), 1, &OpDispatchBuilder::ROROp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xC1), 4), 1, &OpDispatchBuilder::SHLOp<false>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xC1), 5), 1, &OpDispatchBuilder::SHROp<false>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xC1), 7), 1, &OpDispatchBuilder::ASHROp}, // SAR

    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD0), 0), 1, &OpDispatchBuilder::ROLOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD0), 1), 1, &OpDispatchBuilder::ROROp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD0), 4), 1, &OpDispatchBuilder::SHLOp<true>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD0), 5), 1, &OpDispatchBuilder::SHROp<true>}, // 1Bit SHR
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD0), 7), 1, &OpDispatchBuilder::ASHROp}, // SAR

    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD1), 0), 1, &OpDispatchBuilder::ROLOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD1), 1), 1, &OpDispatchBuilder::ROROp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD1), 4), 1, &OpDispatchBuilder::SHLOp<true>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD1), 5), 1, &OpDispatchBuilder::SHROp<true>}, // 1Bit SHR
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD1), 7), 1, &OpDispatchBuilder::ASHROp}, // SAR

    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD2), 0), 1, &OpDispatchBuilder::ROLOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD2), 1), 1, &OpDispatchBuilder::ROROp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD2), 4), 1, &OpDispatchBuilder::SHLOp<false>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD2), 5), 1, &OpDispatchBuilder::SHROp<false>}, // SHR by CL
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD2), 7), 1, &OpDispatchBuilder::ASHROp}, // SAR

    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD3), 0), 1, &OpDispatchBuilder::ROLOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD3), 1), 1, &OpDispatchBuilder::ROROp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD3), 4), 1, &OpDispatchBuilder::SHLOp<false>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD3), 5), 1, &OpDispatchBuilder::SHROp<false>}, // SHR by CL
    {OPD(FEXCore::X86Tables::TYPE_GROUP_2, OpToIndex(0xD3), 7), 1, &OpDispatchBuilder::ASHROp}, // SAR

    // GROUP 3
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF6), 0), 1, &OpDispatchBuilder::TESTOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF6), 2), 1, &OpDispatchBuilder::NOTOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF6), 3), 1, &OpDispatchBuilder::NEGOp}, // NEG
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF6), 4), 1, &OpDispatchBuilder::MULOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF6), 5), 1, &OpDispatchBuilder::IMULOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF6), 6), 1, &OpDispatchBuilder::DIVOp}, // DIV
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF6), 7), 1, &OpDispatchBuilder::IDIVOp}, // IDIV

    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF7), 0), 1, &OpDispatchBuilder::TESTOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF7), 2), 1, &OpDispatchBuilder::NOTOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF7), 3), 1, &OpDispatchBuilder::NEGOp}, // NEG

    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF7), 4), 1, &OpDispatchBuilder::MULOp}, // MUL
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF7), 5), 1, &OpDispatchBuilder::IMULOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF7), 6), 1, &OpDispatchBuilder::DIVOp}, // DIV
    {OPD(FEXCore::X86Tables::TYPE_GROUP_3, OpToIndex(0xF7), 7), 1, &OpDispatchBuilder::IDIVOp}, // IDIV

    // GROUP 4
    {OPD(FEXCore::X86Tables::TYPE_GROUP_4, OpToIndex(0xFE), 0), 1, &OpDispatchBuilder::INCOp}, // INC
    {OPD(FEXCore::X86Tables::TYPE_GROUP_4, OpToIndex(0xFE), 1), 1, &OpDispatchBuilder::DECOp}, // DEC

    // GROUP 5
    {OPD(FEXCore::X86Tables::TYPE_GROUP_5, OpToIndex(0xFF), 0), 1, &OpDispatchBuilder::INCOp}, // INC
    {OPD(FEXCore::X86Tables::TYPE_GROUP_5, OpToIndex(0xFF), 1), 1, &OpDispatchBuilder::DECOp}, // DEC
    {OPD(FEXCore::X86Tables::TYPE_GROUP_5, OpToIndex(0xFF), 2), 1, &OpDispatchBuilder::CALLAbsoluteOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_5, OpToIndex(0xFF), 4), 1, &OpDispatchBuilder::JUMPAbsoluteOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_5, OpToIndex(0xFF), 6), 1, &OpDispatchBuilder::PUSHOp},

    // GROUP 11
    {OPD(FEXCore::X86Tables::TYPE_GROUP_11, OpToIndex(0xC6), 0), 1, &OpDispatchBuilder::MOVOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_11, OpToIndex(0xC7), 0), 1, &OpDispatchBuilder::MOVOp},
 #undef OPD
   };

  const std::vector<std::tuple<uint8_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> RepModOpTable = {
    {0x10, 2, &OpDispatchBuilder::MOVSSOp},
    {0x19, 7, &OpDispatchBuilder::NOPOp},
    {0x2A, 1, &OpDispatchBuilder::CVT<4, true>},
    {0x2C, 1, &OpDispatchBuilder::FCVT<4, true>},
    {0x51, 1, &OpDispatchBuilder::VectorUnaryOp<IR::OP_VFSQRT, 4, true>},
    {0x52, 1, &OpDispatchBuilder::VectorUnaryOp<IR::OP_VFRSQRT, 4, true>},
    {0x58, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFADD, 4>},
    {0x59, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFMUL, 4>},
    {0x5A, 1, &OpDispatchBuilder::FCVTF<8, 4>},
    {0x5C, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFSUB, 4>},
    {0x5D, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFMIN, 4>},
    {0x5E, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFDIV, 4>},
    {0x5F, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFMAX, 4>},
    {0x6F, 1, &OpDispatchBuilder::MOVUPSOp},
    {0x7E, 1, &OpDispatchBuilder::MOVQOp},
    {0x7F, 1, &OpDispatchBuilder::MOVUPSOp},
    {0xBC, 2, &OpDispatchBuilder::TZCNT},
  };

  const std::vector<std::tuple<uint8_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> RepNEModOpTable = {
    {0x10, 2, &OpDispatchBuilder::MOVSDOp},
    {0x12, 1, &OpDispatchBuilder::MOVDDUPOp},
    {0x19, 7, &OpDispatchBuilder::NOPOp},
    {0x2A, 1, &OpDispatchBuilder::CVT<8, true>},
    {0x2C, 1, &OpDispatchBuilder::FCVT<8, true>},
    {0x5A, 1, &OpDispatchBuilder::FCVTF<4, 8>},
    {0x51, 1, &OpDispatchBuilder::VectorUnaryOp<IR::OP_VFSQRT, 8, true>},
    //x52 = Invalid
    {0x58, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFADD, 8>},
    {0x59, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFMUL, 8>},
    {0x5C, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFSUB, 8>},
    {0x5D, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFMIN, 8>},
    {0x5E, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFDIV, 8>},
    {0x5F, 1, &OpDispatchBuilder::VectorScalarALUOp<IR::OP_VFMAX, 8>},
    {0x70, 1, &OpDispatchBuilder::PSHUFDOp<2, true>},
    {0xF0, 1, &OpDispatchBuilder::UnimplementedOp},
  };

  const std::vector<std::tuple<uint8_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> OpSizeModOpTable = {
    {0x12, 2, &OpDispatchBuilder::MOVOp},
    {0x16, 2, &OpDispatchBuilder::MOVHPDOp},
    {0x19, 7, &OpDispatchBuilder::NOPOp},
    {0x28, 2, &OpDispatchBuilder::MOVAPSOp},

    {0x40, 16, &OpDispatchBuilder::CMOVOp},
    {0x54, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VAND, 16>},
    {0x56, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VOR, 16>},
    {0x57, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VXOR, 16>},
    {0x58, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFADD, 8>},
    {0x59, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFMUL, 8>},
    {0x5C, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFSUB, 8>},
    {0x5D, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFMIN, 8>},
    {0x5E, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFDIV, 8>},
    {0x5F, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VFMAX, 8>},
    {0x60, 3, &OpDispatchBuilder::PUNPCKLOp},
    {0x64, 1, &OpDispatchBuilder::PCMPGTOp<1>},
    {0x65, 1, &OpDispatchBuilder::PCMPGTOp<2>},
    {0x66, 1, &OpDispatchBuilder::PCMPGTOp<4>},
    {0x68, 3, &OpDispatchBuilder::PUNPCKHOp},
    {0x6C, 1, &OpDispatchBuilder::PUNPCKLOp},
    {0x6D, 1, &OpDispatchBuilder::PUNPCKHOp},
    {0x6E, 1, &OpDispatchBuilder::MOVDOp},
    {0x6F, 1, &OpDispatchBuilder::MOVUPSOp},
    {0x70, 1, &OpDispatchBuilder::PSHUFDOp<4, true>},

    //  XXX: Causing IR interpreter some problems
    {0x74, 3, &OpDispatchBuilder::PCMPEQOp},
    {0x78, 1, nullptr}, // GROUP 17
    {0x7E, 1, &OpDispatchBuilder::MOVDOp},
    {0x7F, 1, &OpDispatchBuilder::MOVUPSOp},
    {0xC6, 1, &OpDispatchBuilder::SHUFOp<8>},

    {0xD4, 1, &OpDispatchBuilder::PADDQOp},
    // XXX: Causes LLVM to crash if commented out?
    {0xD6, 1, &OpDispatchBuilder::MOVQOp},
    {0xD7, 1, &OpDispatchBuilder::PMOVMSKBOp}, // PMOVMSKB
    // XXX: Untested
    {0xDA, 1, &OpDispatchBuilder::PMINUOp<1>},
    {0xDB, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VAND, 16>},
    {0xDE, 1, &OpDispatchBuilder::PMAXUOp<1>},
    {0xE7, 1, &OpDispatchBuilder::UnimplementedOp},
    {0xEA, 1, &OpDispatchBuilder::PMINSWOp},

    {0xEB, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VOR, 16>},

    {0xEF, 1, &OpDispatchBuilder::VectorALUOp<IR::OP_VXOR, 16>},
    {0xF2, 1, &OpDispatchBuilder::PSLL<4, true>},
    {0xF3, 1, &OpDispatchBuilder::PSLL<8, true>},
    {0xF8, 4, &OpDispatchBuilder::PSUBQOp},
    {0xFE, 1, &OpDispatchBuilder::PADDQOp},
  };

constexpr uint16_t PF_NONE = 0;
constexpr uint16_t PF_F3 = 1;
constexpr uint16_t PF_66 = 2;
constexpr uint16_t PF_F2 = 3;
#define OPD(group, prefix, Reg) (((group - FEXCore::X86Tables::TYPE_GROUP_6) << 5) | (prefix) << 3 | (Reg))
  const std::vector<std::tuple<uint16_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> SecondaryExtensionOpTable = {
    // GROUP 8
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_NONE, 4), 1, &OpDispatchBuilder::BTOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_F3, 4), 1, &OpDispatchBuilder::BTOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_66, 4), 1, &OpDispatchBuilder::BTOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_F2, 4), 1, &OpDispatchBuilder::BTOp},

    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_NONE, 5), 1, &OpDispatchBuilder::BTSOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_F3, 5), 1, &OpDispatchBuilder::BTSOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_66, 5), 1, &OpDispatchBuilder::BTSOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_F2, 5), 1, &OpDispatchBuilder::BTSOp},

    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_NONE, 6), 1, &OpDispatchBuilder::BTROp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_F3, 6), 1, &OpDispatchBuilder::BTROp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_66, 6), 1, &OpDispatchBuilder::BTROp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_F2, 6), 1, &OpDispatchBuilder::BTROp},

    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_NONE, 7), 1, &OpDispatchBuilder::BTCOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_F3, 7), 1, &OpDispatchBuilder::BTCOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_66, 7), 1, &OpDispatchBuilder::BTCOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_8, PF_F2, 7), 1, &OpDispatchBuilder::BTCOp},

    // GROUP 13
    {OPD(FEXCore::X86Tables::TYPE_GROUP_13, PF_NONE, 2), 1, &OpDispatchBuilder::PSRLD<4>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_13, PF_NONE, 6), 1, &OpDispatchBuilder::PSLL<4, true>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_13, PF_66, 2), 1, &OpDispatchBuilder::PSRLD<4>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_13, PF_66, 6), 1, &OpDispatchBuilder::PSLL<4, true>},

    // GROUP 14
    {OPD(FEXCore::X86Tables::TYPE_GROUP_14, PF_NONE, 2), 1, &OpDispatchBuilder::PSRLD<4>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_14, PF_NONE, 6), 1, &OpDispatchBuilder::PSLL<8, true>},

    {OPD(FEXCore::X86Tables::TYPE_GROUP_14, PF_66, 2), 1, &OpDispatchBuilder::PSRLD<4>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_14, PF_66, 6), 1, &OpDispatchBuilder::PSLL<8, true>},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_14, PF_66, 3), 1, &OpDispatchBuilder::PSRLDQ},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_14, PF_66, 7), 1, &OpDispatchBuilder::PSLLDQ},

    // GROUP 15
    {OPD(FEXCore::X86Tables::TYPE_GROUP_15, PF_NONE, 0), 1, &OpDispatchBuilder::FXSaveOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_15, PF_NONE, 1), 1, &OpDispatchBuilder::FXRStoreOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_15, PF_NONE, 5), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_15, PF_NONE, 7), 1, &OpDispatchBuilder::UnimplementedOp},

    {OPD(FEXCore::X86Tables::TYPE_GROUP_15, PF_F3, 5), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_15, PF_F3, 6), 1, &OpDispatchBuilder::UnimplementedOp},

    // GROUP 16
    {OPD(FEXCore::X86Tables::TYPE_GROUP_16, PF_NONE, 0), 8, &OpDispatchBuilder::NOPOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_16, PF_F3, 0), 8, &OpDispatchBuilder::NOPOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_16, PF_66, 0), 8, &OpDispatchBuilder::NOPOp},
    {OPD(FEXCore::X86Tables::TYPE_GROUP_16, PF_F2, 0), 8, &OpDispatchBuilder::NOPOp},
  };
#undef OPD

  const std::vector<std::tuple<uint8_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> SecondaryModRMExtensionOpTable = {
    // REG /2
    {((1 << 3) | 0), 1, &OpDispatchBuilder::UnimplementedOp},
  };
#define OPDReg(op, reg) (((op - 0xD8) << 8) | (reg << 3))
#define OPD(op, modrmop) (((op - 0xD8) << 8) | modrmop)
  const std::vector<std::tuple<uint16_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> X87OpTable = {
    {OPDReg(0xD9, 0) | 0x00, 8, &OpDispatchBuilder::FLD<32>},
    {OPDReg(0xD9, 0) | 0x40, 8, &OpDispatchBuilder::FLD<32>},
    {OPDReg(0xD9, 0) | 0x80, 8, &OpDispatchBuilder::FLD<32>},

    {OPDReg(0xD9, 5) | 0x00, 8, &OpDispatchBuilder::NOPOp}, // XXX: stubbed FLDCW
    {OPDReg(0xD9, 5) | 0x40, 8, &OpDispatchBuilder::NOPOp}, // XXX: stubbed FLDCW
    {OPDReg(0xD9, 5) | 0x80, 8, &OpDispatchBuilder::NOPOp}, // XXX: stubbed FLDCW

    {OPDReg(0xD9, 7) | 0x00, 8, &OpDispatchBuilder::NOPOp}, // XXX: stubbed FNSTCW
    {OPDReg(0xD9, 7) | 0x40, 8, &OpDispatchBuilder::NOPOp}, // XXX: stubbed FNSTCW
    {OPDReg(0xD9, 7) | 0x80, 8, &OpDispatchBuilder::NOPOp}, // XXX: stubbed FNSTCW

    {OPDReg(0xDB, 7) | 0x00, 8, &OpDispatchBuilder::FST<80, true>},
    {OPDReg(0xDB, 7) | 0x40, 8, &OpDispatchBuilder::FST<80, true>},
    {OPDReg(0xDB, 7) | 0x80, 8, &OpDispatchBuilder::FST<80, true>},

    {OPDReg(0xDD, 0) | 0x00, 8, &OpDispatchBuilder::FLD<64>},
    {OPDReg(0xDD, 0) | 0x40, 8, &OpDispatchBuilder::FLD<64>},
    {OPDReg(0xDD, 0) | 0x80, 8, &OpDispatchBuilder::FLD<64>},

    {OPD(0xDE, 0xC0), 8, &OpDispatchBuilder::FADD}
  };
#undef OPD
#undef OPDReg

#define OPD(prefix, opcode) ((prefix << 8) | opcode)
  constexpr uint16_t PF_38_NONE = 0;
  constexpr uint16_t PF_38_66   = 1;
  constexpr uint16_t PF_38_F2   = 2;
  const std::vector<std::tuple<uint16_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> H0F38Table = {
    {OPD(PF_38_66,   0x00), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(PF_38_66,   0x3B), 1, &OpDispatchBuilder::UnimplementedOp},
  };

#undef OPD

#define OPD(REX, prefix, opcode) ((REX << 9) | (prefix << 8) | opcode)
#define PF_3A_NONE 0
#define PF_3A_66   1
  const std::vector<std::tuple<uint16_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> H0F3ATable = {
    {OPD(0, PF_3A_66,   0x0F), 1, &OpDispatchBuilder::PAlignrOp},
    {OPD(1, PF_3A_66,   0x0F), 1, &OpDispatchBuilder::PAlignrOp},
  };
#undef PF_3A_NONE
#undef PF_3A_66

#undef OPD


#define OPD(map_select, pp, opcode) (((map_select - 1) << 10) | (pp << 8) | (opcode))
  const std::vector<std::tuple<uint16_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> VEXTable = {
    {OPD(1, 0b01, 0x6E), 1, &OpDispatchBuilder::UnimplementedOp},

    {OPD(1, 0b01, 0x6F), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(1, 0b10, 0x6F), 1, &OpDispatchBuilder::UnimplementedOp},

    {OPD(1, 0b01, 0x74), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(1, 0b01, 0x75), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(1, 0b01, 0x76), 1, &OpDispatchBuilder::UnimplementedOp},

    {OPD(1, 0b00, 0x77), 1, &OpDispatchBuilder::UnimplementedOp},

    {OPD(1, 0b01, 0x7E), 1, &OpDispatchBuilder::UnimplementedOp},

    {OPD(1, 0b01, 0x7F), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(1, 0b10, 0x7F), 1, &OpDispatchBuilder::UnimplementedOp},

    {OPD(1, 0b01, 0xD7), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(1, 0b01, 0xEB), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(1, 0b01, 0xEF), 1, &OpDispatchBuilder::UnimplementedOp},

    {OPD(2, 0b01, 0x3B), 1, &OpDispatchBuilder::UnimplementedOp},

    {OPD(2, 0b01, 0x58), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(2, 0b01, 0x59), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(2, 0b01, 0x5A), 1, &OpDispatchBuilder::UnimplementedOp},

    {OPD(2, 0b01, 0x78), 1, &OpDispatchBuilder::UnimplementedOp},
    {OPD(2, 0b01, 0x79), 1, &OpDispatchBuilder::UnimplementedOp},
  };
#undef OPD

  const std::vector<std::tuple<uint8_t, uint8_t, FEXCore::X86Tables::OpDispatchPtr>> EVEXTable = {
    {0x10, 1, &OpDispatchBuilder::UnimplementedOp},
    {0x11, 1, &OpDispatchBuilder::UnimplementedOp},
    {0x59, 1, &OpDispatchBuilder::UnimplementedOp},
    {0x7F, 1, &OpDispatchBuilder::UnimplementedOp},
  };

  uint64_t NumInsts{};
  auto InstallToTable = [&NumInsts](auto& FinalTable, auto& LocalTable) {
    for (auto Op : LocalTable) {
      auto OpNum = std::get<0>(Op);
      auto Dispatcher = std::get<2>(Op);
      for (uint8_t i = 0; i < std::get<1>(Op); ++i) {
        LogMan::Throw::A(FinalTable[OpNum + i].OpcodeDispatcher == nullptr, "Duplicate Entry");
        FinalTable[OpNum + i].OpcodeDispatcher = Dispatcher;
        if (Dispatcher)
          ++NumInsts;
      }
    }
  };

  [[maybe_unused]] auto CheckTable = [](auto& FinalTable) {
    for (size_t i = 0; i < FinalTable.size(); ++i) {
      auto const &Op = FinalTable.at(i);

      if (Op.Type != X86Tables::TYPE_INST) continue; // Invalid op, we don't care
      if (Op.OpcodeDispatcher == nullptr) {
        LogMan::Msg::D("Op: 0x%lx %s didn't have an OpDispatcher", i, Op.Name);
      }
    }
  };

  InstallToTable(FEXCore::X86Tables::BaseOps, BaseOpTable);
  InstallToTable(FEXCore::X86Tables::SecondBaseOps, TwoByteOpTable);
  InstallToTable(FEXCore::X86Tables::PrimaryInstGroupOps, PrimaryGroupOpTable);

  InstallToTable(FEXCore::X86Tables::RepModOps, RepModOpTable);
  InstallToTable(FEXCore::X86Tables::RepNEModOps, RepNEModOpTable);
  InstallToTable(FEXCore::X86Tables::OpSizeModOps, OpSizeModOpTable);
  InstallToTable(FEXCore::X86Tables::SecondInstGroupOps, SecondaryExtensionOpTable);

  InstallToTable(FEXCore::X86Tables::SecondModRMTableOps, SecondaryModRMExtensionOpTable);

  InstallToTable(FEXCore::X86Tables::X87Ops, X87OpTable);

  InstallToTable(FEXCore::X86Tables::H0F38TableOps, H0F38Table);
  InstallToTable(FEXCore::X86Tables::H0F3ATableOps, H0F3ATable);
  InstallToTable(FEXCore::X86Tables::VEXTableOps, VEXTable);
  InstallToTable(FEXCore::X86Tables::EVEXTableOps, EVEXTable);

  // Useful for debugging
  // CheckTable(FEXCore::X86Tables::BaseOps);
  printf("We installed %ld instructions to the tables\n", NumInsts);
}

}
