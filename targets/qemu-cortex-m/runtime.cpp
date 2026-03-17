#include "semihosting.hpp"

extern "C" [[noreturn]] void mmiocpp_target_exit(int status) {
  mmiocpp::qemu::semihosting::exit(status);
}

extern "C" [[noreturn]] void Default_Handler() {
  mmiocpp::qemu::semihosting::write0("Unhandled exception in QEMU target test\n");
  mmiocpp::qemu::semihosting::exit(1);
}