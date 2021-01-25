#include "Tests/LinuxSyscalls/Syscalls.h"
#include "Tests/LinuxSyscalls/x64/Syscalls.h"
#include <FEXCore/Core/Context.h>
#include <FEXCore/Debug/InternalThreadState.h>

#include <sys/mman.h>
#include <sys/shm.h>

namespace FEX::HLE::x64 {
  void RegisterMemory() {
    REGISTER_SYSCALL_IMPL_X64(munmap, [](FEXCore::Core::InternalThreadState *Thread, void *addr, size_t length) -> uint64_t {
      uint64_t Result = ::munmap(addr, length);
      if (Result != -1) {
        FEXCore::Context::FlushCodeRange(Thread, (uintptr_t)addr, (uintptr_t)addr + length);
      }
      SYSCALL_ERRNO();
    });

    REGISTER_SYSCALL_IMPL_X64(mmap, [](FEXCore::Core::InternalThreadState *Thread, void *addr, size_t length, int prot, int flags, int fd, off_t offset) -> uint64_t {
      uint64_t Result = reinterpret_cast<uint64_t>(::mmap(addr, length, prot, flags, fd, offset));
      if (Result != -1) {
        FEXCore::Context::FlushCodeRange(Thread, (uintptr_t)Result, (uintptr_t)Result + length);
      }
      SYSCALL_ERRNO();
    });

    REGISTER_SYSCALL_IMPL_X64(mremap, [](FEXCore::Core::InternalThreadState *Thread, void *old_address, size_t old_size, size_t new_size, int flags, void *new_address) -> uint64_t {
      uint64_t Result = reinterpret_cast<uint64_t>(::mremap(old_address, old_size, new_size, flags, new_address));
      SYSCALL_ERRNO();
    });

    REGISTER_SYSCALL_IMPL_X64(mprotect, [](FEXCore::Core::InternalThreadState *Thread, void *addr, size_t len, int prot) -> uint64_t {
      uint64_t Result = ::mprotect(addr, len, prot);
      if (Result != -1 && prot & PROT_EXEC) {
        FEXCore::Context::FlushCodeRange(Thread, (uintptr_t)addr, (uintptr_t)addr + len);
      }
      SYSCALL_ERRNO();
    });

    REGISTER_SYSCALL_IMPL_X64(mlockall, [](FEXCore::Core::InternalThreadState *Thread, int flags) -> uint64_t {
      uint64_t Result = ::mlockall(flags);
      SYSCALL_ERRNO();
    });

    REGISTER_SYSCALL_IMPL_X64(munlockall, [](FEXCore::Core::InternalThreadState *Thread) -> uint64_t {
      uint64_t Result = ::munlockall();
      SYSCALL_ERRNO();
    });

    REGISTER_SYSCALL_IMPL_X64(shmat, [](FEXCore::Core::InternalThreadState *Thread, int shmid, const void *shmaddr, int shmflg) -> uint64_t {
      uint64_t Result = reinterpret_cast<uint64_t>(shmat(shmid, shmaddr, shmflg));
      SYSCALL_ERRNO();
    });
  }
}
