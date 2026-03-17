#include "semihosting.hpp"

extern "C" [[noreturn]] void mmiocpp_target_exit(int status) {
  mmiocpp::qemu::semihosting::exit(status);
}