#include "Tests/LinuxSyscalls/Syscalls.h"
#include "Tests/LinuxSyscalls/x64/Syscalls.h"
#include "Tests/LinuxSyscalls/x32/Syscalls.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/shm.h>

namespace FEX::HLE {
  void RegisterSHM() {
    REGISTER_SYSCALL_IMPL(shmget, [](FEXCore::Core::InternalThreadState *Thread, key_t key, size_t size, int shmflg) -> uint64_t {
      uint64_t Result = shmget(key, size, shmflg);
      SYSCALL_ERRNO();
    });

    REGISTER_SYSCALL_IMPL(shmctl, [](FEXCore::Core::InternalThreadState *Thread, int shmid, int cmd, struct shmid_ds *buf) -> uint64_t {
      uint64_t Result = ::shmctl(shmid, cmd, buf);
      SYSCALL_ERRNO();
    });

    REGISTER_SYSCALL_IMPL(shmdt, [](FEXCore::Core::InternalThreadState *Thread, const void *shmaddr) -> uint64_t {
      uint64_t Result = ::shmdt(shmaddr);
      SYSCALL_ERRNO();
    });
  }
}
