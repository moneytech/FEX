#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

namespace FEXCore {
class JITSymbols final {
public:
  JITSymbols();
  ~JITSymbols();
  void Register(void *HostAddr, uint64_t GuestAddr, uint32_t CodeSize);
  void Register(void *HostAddr, uint32_t CodeSize, std::string const &Name);

private:
  FILE* fp{};
};
}
