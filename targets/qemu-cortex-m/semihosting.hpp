#ifndef MMIOCPP_QEMU_CORTEX_M_SEMIHOSTING_HPP
#define MMIOCPP_QEMU_CORTEX_M_SEMIHOSTING_HPP

#include <cstdint>

namespace mmiocpp::qemu::semihosting {

constexpr std::uint32_t kSysWrite0 = 0x04u;
constexpr std::uint32_t kSysExitExtended = 0x20u;
constexpr std::uint32_t kApplicationExit = 0x20026u;

inline std::uintptr_t call(std::uint32_t operation, const void* argument) {
  register std::uintptr_t r0 __asm("r0") = operation;
  register std::uintptr_t r1 __asm("r1") = reinterpret_cast<std::uintptr_t>(argument);
  __asm volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory");
  return r0;
}

inline void write0(const char* text) {
  (void)call(kSysWrite0, text);
}

[[noreturn]] inline void exit(int status) {
  const std::uint32_t block[2] = {kApplicationExit, static_cast<std::uint32_t>(status)};
  (void)call(kSysExitExtended, block);
  while (true) {
  }
}

}  // namespace mmiocpp::qemu::semihosting

#endif  // MMIOCPP_QEMU_CORTEX_M_SEMIHOSTING_HPP