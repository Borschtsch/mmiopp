#include "../mmio_runtime_test_suite.hpp"

namespace {

struct CortexR5Config {
  static constexpr std::uintptr_t kSpiCrAddress = 0x00100000u;
  static constexpr std::uintptr_t kSpiMrAddress = 0x00100004u;
  static constexpr std::uintptr_t kSpiSrAddress = 0x00100008u;
  static constexpr std::uintptr_t kAccessStatusAddress = 0x0010000Cu;
  static constexpr std::uintptr_t kAccessCommandAddress = 0x00100010u;
  static constexpr std::uintptr_t kAccessLatchAddress = 0x00100014u;
  static constexpr std::uintptr_t kAccessZeroAddress = 0x00100018u;
  static constexpr std::uintptr_t kAliasAddress = 0x00101000u;
  static constexpr const char* kBanner = "Running MMIO++ QEMU Cortex-R5 build-target tests\n";
  static constexpr const char* kSuccess = "MMIO++ QEMU Cortex-R5 build-target tests passed\n";
};

}  // namespace

extern "C" int mmiocpp_target_main() {
  return mmiocpp::targets::runtime_tests::run<CortexR5Config>();
}
