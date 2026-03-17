#include "../mmio_runtime_test_suite.hpp"

namespace {

struct CortexMConfig {
  static constexpr std::uintptr_t kSpiCrAddress = 0x20001000u;
  static constexpr std::uintptr_t kSpiMrAddress = 0x20001004u;
  static constexpr std::uintptr_t kSpiSrAddress = 0x20001008u;
  static constexpr std::uintptr_t kAccessStatusAddress = 0x2000100Cu;
  static constexpr std::uintptr_t kAccessCommandAddress = 0x20001010u;
  static constexpr std::uintptr_t kAccessLatchAddress = 0x20001014u;
  static constexpr std::uintptr_t kAccessZeroAddress = 0x20001018u;
  static constexpr std::uintptr_t kAliasAddress = 0x20002000u;
  static constexpr const char* kBanner = "Running MMIO++ QEMU target tests\n";
  static constexpr const char* kSuccess = "MMIO++ QEMU target tests passed\n";
};

}  // namespace

extern "C" int mmiocpp_target_main() {
  return mmiocpp::targets::runtime_tests::run<CortexMConfig>();
}
