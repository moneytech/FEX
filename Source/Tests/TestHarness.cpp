#include "Common/ArgumentLoader.h"
#include "HarnessHelpers.h"

#include <FEXCore/Config/Config.h>
#include <FEXCore/Core/CodeLoader.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/Core/CoreState.h>
#include <FEXCore/Core/CPUBackend.h>
#include <FEXCore/Core/X86Enums.h>
#include <FEXCore/HLE/SyscallHandler.h>
#include <FEXCore/Debug/InternalThreadState.h>
#include <FEXCore/Utils/LogManager.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

void MsgHandler(LogMan::DebugLevels Level, char const *Message) {
  const char *CharLevel{nullptr};

  switch (Level) {
  case LogMan::NONE:
    CharLevel = "NONE";
    break;
  case LogMan::ASSERT:
    CharLevel = "ASSERT";
    break;
  case LogMan::ERROR:
    CharLevel = "ERROR";
    break;
  case LogMan::DEBUG:
    CharLevel = "DEBUG";
    break;
  case LogMan::INFO:
    CharLevel = "Info";
    break;
  default:
    CharLevel = "???";
    break;
  }
  printf("[%s] %s\n", CharLevel, Message);
}

void AssertHandler(char const *Message) {
  printf("[ASSERT] %s\n", Message);
}

int main(int argc, char **argv) {
  LogMan::Throw::InstallHandler(AssertHandler);
  LogMan::Msg::InstallHandler(MsgHandler);

  FEXCore::Config::Initialize();
  FEXCore::Config::AddLayer(std::make_unique<FEX::ArgLoader::ArgLoader>(argc, argv));
  FEXCore::Config::Load();

  auto Args = FEX::ArgLoader::Get();

  LogMan::Throw::A(Args.size() > 1, "Not enough arguments");

  FEXCore::Context::InitializeStaticTables();
  auto CTX1 = FEXCore::Context::CreateNewContext();

  auto CTX2 = FEXCore::Context::CreateNewContext();

  FEXCore::Config::SetConfig(CTX1, FEXCore::Config::CONFIG_DEFAULTCORE, FEXCore::Config::CONFIG_CUSTOM);
  FEXCore::Config::SetConfig(CTX1, FEXCore::Config::CONFIG_SINGLESTEP, 1);
  FEXCore::Config::SetConfig(CTX1, FEXCore::Config::CONFIG_MAXBLOCKINST, 1);

  FEXCore::Config::SetConfig(CTX2, FEXCore::Config::CONFIG_DEFAULTCORE, FEXCore::Config::CONFIG_INTERPRETER);
  FEXCore::Config::SetConfig(CTX2, FEXCore::Config::CONFIG_SINGLESTEP, 1);
  FEXCore::Config::SetConfig(CTX2, FEXCore::Config::CONFIG_MAXBLOCKINST, 1);

  FEXCore::Context::InitializeContext(CTX1);
  FEXCore::Context::InitializeContext(CTX2);

  FEX::HarnessHelper::HarnessCodeLoader Loader{Args[0], Args[1].c_str()};

  bool Result1 = FEXCore::Context::InitCore(CTX1, &Loader);
  bool Result2 = FEXCore::Context::InitCore(CTX2, &Loader);

 if (!Result1 || !Result2)
   return 2;

 while (FEXCore::Context::RunUntilExit(CTX1) == FEXCore::Context::ExitReason::EXIT_DEBUG);
 LogMan::Msg::I("Running Core2");
  while (FEXCore::Context::RunUntilExit(CTX2) == FEXCore::Context::ExitReason::EXIT_DEBUG);

  FEXCore::Core::CPUState State1;
  FEXCore::Core::CPUState State2;

  FEXCore::Context::GetCPUState(CTX1, &State1);
  FEXCore::Context::GetCPUState(CTX2, &State2);

  bool Passed = Loader.CompareStates(&State1, &State2);
  LogMan::Msg::I("Passed? %s\n", Passed ? "Yes" : "No");

  FEXCore::Context::DestroyContext(CTX1);
  FEXCore::Context::DestroyContext(CTX2);

  return Passed ? 0 : 1;
}

