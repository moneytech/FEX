#include "Interface/Context/Context.h"
#include "Interface/Core/Core.h"
#include "Interface/Core/LookupCache.h"
#include <sys/mman.h>

namespace FEXCore {
LookupCache::LookupCache(FEXCore::Context::Context *CTX)
  : ctx {CTX} {

  // Block cache ends up looking like this
  // PageMemoryMap[VirtualMemoryRegion >> 12]
  //       |
  //       v
  // PageMemory[Memory & (VIRTUAL_PAGE_SIZE - 1)]
  //       |
  //       v
  // Pointer to Code
  //
  // Allocate a region of memory that we can use to back our block pointers
  // We need one pointer per page of virtual memory
  // At 64GB of virtual memory this will allocate 128MB of virtual memory space
  PagePointer = reinterpret_cast<uintptr_t>(mmap(nullptr, ctx->Config.VirtualMemSize / 4096 * 8, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

  // Allocate our memory backing our pages
  // We need 32KB per guest page (One pointer per byte)
  // XXX: We can drop down to 16KB if we store 4byte offsets from the code base
  // We currently limit to 128MB of real memory for caching for the total cache size.
  // Can end up being inefficient if we compile a small number of blocks per page
  PageMemory = reinterpret_cast<uintptr_t>(mmap(nullptr, CODE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  LogMan::Throw::A(PageMemory != -1ULL, "Failed to allocate page memory");

  // L1 Cache
  L1Pointer = reinterpret_cast<uintptr_t>(mmap(nullptr, L1_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  LogMan::Throw::A(L1Pointer != -1ULL, "Failed to allocate L1Pointer");

  VirtualMemSize = ctx->Config.VirtualMemSize;
}

LookupCache::~LookupCache() {
  munmap(reinterpret_cast<void*>(PagePointer), ctx->Config.VirtualMemSize / 4096 * 8);
  munmap(reinterpret_cast<void*>(PageMemory), CODE_SIZE);
  munmap(reinterpret_cast<void*>(L1Pointer), L1_SIZE);
}

void LookupCache::HintUsedRange(uint64_t Address, uint64_t Size) {
  // Tell the kernel we will definitely need [Address, Address+Size) mapped for the page pointer
  // Page Pointer is allocated per page, so shift by page size
  Address >>= 12;
  Size >>= 12;
  madvise(reinterpret_cast<void*>(PagePointer + Address), Size, MADV_WILLNEED);
}

void LookupCache::ClearL2Cache() {
  // Clear out the page memory
  madvise(reinterpret_cast<void*>(PagePointer), ctx->Config.VirtualMemSize / 4096 * 8, MADV_DONTNEED);
  madvise(reinterpret_cast<void*>(PageMemory), CODE_SIZE, MADV_DONTNEED);
  AllocateOffset = 0;
}

void LookupCache::ClearCache() {
  // Clear L1
  madvise(reinterpret_cast<void*>(L1Pointer), L1_SIZE, MADV_DONTNEED);
  // Clear L2
  ClearL2Cache();
  // All code is gone, remove links
  BlockLinks.clear();
  // All code is gone, clear the block list
  BlockList.clear();
}

}

