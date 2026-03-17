#ifndef MMIOCPP_RUNTIME_TEST_SUITE_HPP
#define MMIOCPP_RUNTIME_TEST_SUITE_HPP

#include <cstdint>

#include "semihosting.hpp"
#include "access_example_registers.hpp"
#include "spi_example_registers.hpp"

namespace mmiocpp::targets::runtime_tests {

struct SPI_CR_ALIAS : mmio::Register<SPI_CR_ALIAS> {
  struct SPIEN : mmio::BitField<SPI_CR_ALIAS, 0, 1> {
    static constexpr auto DISABLE = SPIEN::value(0);
    static constexpr auto ENABLE = SPIEN::value(1);
  };

  struct SWRST : mmio::BitField<SPI_CR_ALIAS, 7, 1> {
    static constexpr auto IDLE = SWRST::value(0);
    static constexpr auto RESET = SWRST::value(1);
  };

  struct CMD : mmio::ValueField<SPI_CR_ALIAS, 8, 2> {};
};

template <typename Config>
class Suite {
 public:
  static int run() {
    g_failures = 0;
    mmiocpp::qemu::semihosting::write0(Config::kBanner);
    test_address_targeting_and_exact_raw_writes();
    test_direct_raw_writes_decode_through_api();
    test_logical_operations_and_readback();
    test_local_register_copies();
    test_hardware_driven_status_bits();
    test_access_policy_register_protocols();
    test_alias_views_share_the_same_address();

    if (g_failures == 0) {
      mmiocpp::qemu::semihosting::write0(Config::kSuccess);
    }
    return g_failures == 0 ? 0 : 1;
  }

 private:
  inline static int g_failures = 0;

  static constexpr std::uint32_t bit(unsigned offset) noexcept {
    return static_cast<std::uint32_t>(1u << offset);
  }

  static constexpr std::uint32_t encode(unsigned offset, std::uint32_t value) noexcept {
    return static_cast<std::uint32_t>(value << offset);
  }

  static constexpr std::uint32_t kSpiCrEnableReset = bit(0) | bit(7);
  static constexpr std::uint32_t kSpiCrEnableResetCmd2 = kSpiCrEnableReset | encode(8, 2);
  static constexpr std::uint32_t kSpiMrMasterKeepAssertedPcs2Dly7 =
      bit(0) | bit(3) | encode(4, 2) | encode(8, 7);
  static constexpr std::uint32_t kSpiSrReadyBits = bit(0) | bit(1);
  static constexpr std::uint32_t kSpiSrOverrunBit = bit(3);
  static constexpr std::uint32_t kAccessStatusReadyCount5 = bit(0) | encode(4, 5);
  static constexpr std::uint32_t kAccessStatusErrorBits = bit(8) | bit(9);
  static constexpr std::uint32_t kAccessCommandStartCount3 = bit(0) | encode(8, 3);
  static constexpr std::uint32_t kAccessLatchSetBits = bit(0) | bit(1);

  static volatile std::uint32_t& raw_word(std::uintptr_t address) {
    return *reinterpret_cast<volatile std::uint32_t*>(address);
  }

  static std::uint32_t read_word(std::uintptr_t address) {
    return raw_word(address);
  }

  static void write_word(std::uintptr_t address, std::uint32_t value) {
    raw_word(address) = value;
  }

  static void report_failure(const char* message) {
    mmiocpp::qemu::semihosting::write0("FAIL: ");
    mmiocpp::qemu::semihosting::write0(message);
    mmiocpp::qemu::semihosting::write0("\n");
    ++g_failures;
  }

  static void expect(bool condition, const char* message) {
    if (!condition) {
      report_failure(message);
    }
  }

  template <typename T, typename U>
  static void expect_equal(T actual, U expected, const char* message) {
    if (actual != expected) {
      report_failure(message);
    }
  }

  static void expect_word(std::uintptr_t address, std::uint32_t expected, const char* message) {
    expect_equal(read_word(address), expected, message);
  }

  static void clear_register_window() {
    write_word(Config::kSpiCrAddress, 0u);
    write_word(Config::kSpiMrAddress, 0u);
    write_word(Config::kSpiSrAddress, 0u);
    write_word(Config::kAliasAddress, 0u);
    write_word(Config::kAccessStatusAddress, 0u);
    write_word(Config::kAccessCommandAddress, 0u);
    write_word(Config::kAccessLatchAddress, 0u);
    write_word(Config::kAccessZeroAddress, 0u);
  }

  static void test_address_targeting_and_exact_raw_writes() {
    SPI_CR::Instance<Config::kSpiCrAddress> spiCr;
    SPI_MR::Instance<Config::kSpiMrAddress> spiMr;

    write_word(Config::kSpiCrAddress, 0xAAAAAAAAu);
    write_word(Config::kSpiMrAddress, 0xBBBBBBBBu);
    write_word(Config::kSpiSrAddress, 0xCCCCCCCCu);

    spiCr = SPI_CR::SPIEN::ENABLE | SPI_CR::SWRST::RESET;
    expect_word(Config::kSpiCrAddress, kSpiCrEnableReset,
                "whole-register writes should encode the expected raw control bits");
    expect_word(Config::kSpiMrAddress, 0xBBBBBBBBu,
                "whole-register writes should not touch neighboring register addresses");
    expect_word(Config::kSpiSrAddress, 0xCCCCCCCCu,
                "whole-register writes should not disturb the status register address");

    spiMr = SPI_MR::MSTR::MASTER | SPI_MR::PCS::value(2) | SPI_MR::DLY::value(7);
    expect_word(Config::kSpiMrAddress, bit(0) | encode(4, 2) | encode(8, 7),
                "value composition should write the exact encoded raw mode word");
    expect_word(Config::kSpiCrAddress, kSpiCrEnableReset,
                "writing the mode register should leave the control register address unchanged");
  }

  static void test_direct_raw_writes_decode_through_api() {
    SPI_CR::Instance<Config::kSpiCrAddress> spiCr;
    SPI_MR::Instance<Config::kSpiMrAddress> spiMr;
    SPI_SR::Instance<Config::kSpiSrAddress> spiSr;

    clear_register_window();
    write_word(Config::kSpiCrAddress, kSpiCrEnableResetCmd2);
    write_word(Config::kSpiMrAddress, kSpiMrMasterKeepAssertedPcs2Dly7);
    write_word(Config::kSpiSrAddress, kSpiSrReadyBits | kSpiSrOverrunBit);

    expect(spiCr & SPI_CR::SPIEN::ENABLE,
           "typed predicates should decode enable from a direct raw write");
    expect(spiCr & SPI_CR::SWRST::RESET,
           "typed predicates should decode reset from a direct raw write");
    expect_equal(spiCr.template get<SPI_CR::CMD>(), 2u,
                 "typed get should decode the command field from a direct raw write");

    expect(spiMr & SPI_MR::MSTR::MASTER,
           "typed predicates should decode master mode from a direct raw write");
    expect(spiMr & SPI_MR::CSAAT::KEEP_ASSERTED,
           "typed predicates should decode chip-select hold from a direct raw write");
    expect_equal(spiMr.template get<SPI_MR::PCS>(), 2u,
                 "typed get should decode the PCS field from a direct raw write");
    expect_equal(static_cast<std::uint32_t>(spiMr.template get<SPI_MR::DLY>()), 7u,
                 "typed get should decode the delay field from a direct raw write");

    expect(spiSr & SPI_SR::RDRF::READY,
           "status predicates should decode a hardware-set receive-ready bit");
    expect(spiSr & SPI_SR::TDRE::READY,
           "status predicates should decode a hardware-set transmit-ready bit");
    expect(spiSr & SPI_SR::OVRES::OVERRUN,
           "status predicates should decode a hardware-set overrun bit");
  }

  static void test_logical_operations_and_readback() {
    SPI_CR::Instance<Config::kSpiCrAddress> spiCr;
    SPI_MR::Instance<Config::kSpiMrAddress> spiMr;

    clear_register_window();

    spiCr = SPI_CR::SPIEN::ENABLE | SPI_CR::SWRST::RESET | SPI_CR::CMD::value(2);
    expect_word(Config::kSpiCrAddress, kSpiCrEnableResetCmd2,
                "mixed whole-register writes should produce the expected raw control word");
    expect_equal(spiCr.template get<SPI_CR::CMD>(), 2u,
                 "typed get should read back the command field after a write");

    spiCr &= ~SPI_CR::SWRST::MASK;
    expect_word(Config::kSpiCrAddress, bit(0) | encode(8, 2),
                "clear-by-mask should only clear the targeted control bit");

    spiCr ^= SPI_CR::SPIEN::MASK;
    expect_word(Config::kSpiCrAddress, encode(8, 2),
                "toggle-by-mask should only flip the targeted control bit");

    spiCr.template set<SPI_CR::CMD>(1u);
    expect_word(Config::kSpiCrAddress, encode(8, 1),
                "masked field writes should replace only the targeted command bits");

    spiMr = SPI_MR::MSTR::MASTER | SPI_MR::CSAAT::KEEP_ASSERTED |
            SPI_MR::PCS::value(2) | SPI_MR::DLY::value(7);
    expect_word(Config::kSpiMrAddress, kSpiMrMasterKeepAssertedPcs2Dly7,
                "whole-register mode writes should produce the expected raw word");
    expect(spiMr & SPI_MR::MSTR::MASTER,
           "predicate reads should see the written master state");
    expect_equal(spiMr.template get<SPI_MR::PCS>(), 2u,
                 "typed get should read back the written PCS field");
    expect_equal(static_cast<std::uint32_t>(spiMr.template get<SPI_MR::DLY>()), 7u,
                 "typed get should read back the written delay field");

    spiMr &= ~SPI_MR::MSTR::MASK;
    expect_word(Config::kSpiMrAddress, bit(3) | encode(4, 2) | encode(8, 7),
                "clear-by-mask should preserve neighboring mode fields");
    expect(spiMr & SPI_MR::MSTR::SLAVE,
           "clearing the master bit should read back as slave mode");

    spiMr ^= SPI_MR::CSAAT::MASK;
    expect_word(Config::kSpiMrAddress, encode(4, 2) | encode(8, 7),
                "toggle-by-mask should flip only the chip-select field bit");
    expect(spiMr & SPI_MR::CSAAT::RELEASE,
           "toggling the CSAAT bit should read back as release mode");

    spiMr.template set<SPI_MR::DLY>(3u);
    expect_word(Config::kSpiMrAddress, encode(4, 2) | encode(8, 3),
                "masked value-field writes should preserve unrelated mode fields");
    expect_equal(static_cast<std::uint32_t>(spiMr.template get<SPI_MR::DLY>()), 3u,
                 "typed get should read back the updated delay field");

    spiMr.set(SPI_MR::MSTR::MASTER | SPI_MR::DLY::value(5));
    expect_word(Config::kSpiMrAddress, bit(0) | encode(8, 5),
                "whole-register set(value) should overwrite the raw register word");
    expect_equal(spiMr.template get<SPI_MR::PCS>(), 0u,
                 "whole-register set(value) should clear fields not present in the encoded value");
  }

  static void test_local_register_copies() {
    SPI_CR::Instance<Config::kSpiCrAddress> spiCr;
    SPI_CR spiCrLocal;

    clear_register_window();

    expect(spiCrLocal & SPI_CR::SPIEN::DISABLE,
           "default local register objects should start with cleared bits");
    expect_equal(spiCrLocal.template get<SPI_CR::CMD>(), 0u,
                 "default local register objects should start with zero field values");

    write_word(Config::kSpiCrAddress, kSpiCrEnableResetCmd2);
    SPI_CR spiCrCopy = spiCr;
    expect(spiCrCopy & SPI_CR::SPIEN::ENABLE,
           "copying from a bound register should snapshot the enable bit");
    expect(spiCrCopy & SPI_CR::SWRST::RESET,
           "copying from a bound register should snapshot the reset bit");
    expect_equal(spiCrCopy.template get<SPI_CR::CMD>(), 2u,
                 "copying from a bound register should snapshot value fields");

    spiCrCopy &= ~SPI_CR::SWRST::MASK;
    spiCrCopy.set<SPI_CR::CMD>(1u);
    spiCr = spiCrCopy;
    expect_word(Config::kSpiCrAddress, bit(0) | encode(8, 1),
                "assigning a local register object back to MMIO should write its full raw value");

    spiCrLocal = SPI_CR::SPIEN::ENABLE | SPI_CR::CMD::value(3);
    spiCrLocal |= SPI_CR::SWRST::RESET;
    expect(spiCrLocal & SPI_CR::SWRST::RESET,
           "local register objects should support bit-style updates");
    expect_equal(spiCrLocal.template get<SPI_CR::CMD>(), 3u,
                 "local register objects should support typed field reads after updates");

    spiCr = spiCrLocal;
    expect_word(Config::kSpiCrAddress, bit(0) | bit(7) | encode(8, 3),
                "writing a locally composed register object should commit the expected raw word");
  }

  static void test_hardware_driven_status_bits() {
    SPI_SR::Instance<Config::kSpiSrAddress> spiSr;

    clear_register_window();
    write_word(Config::kSpiCrAddress, 0x5A5A5A5Au);
    write_word(Config::kSpiMrAddress, 0xA5A5A5A5u);

    write_word(Config::kSpiSrAddress, 0u);
    expect(spiSr & SPI_SR::RDRF::EMPTY,
           "zero status words should read back as receive-empty");
    expect(spiSr & SPI_SR::TDRE::BUSY,
           "zero status words should read back as transmit-busy");
    expect(spiSr & SPI_SR::OVRES::OK,
           "zero status words should read back as no overrun");

    write_word(Config::kSpiSrAddress, kSpiSrReadyBits);
    expect(spiSr & SPI_SR::RDRF::READY,
           "direct raw writes should simulate hardware setting receive-ready");
    expect(spiSr & SPI_SR::TDRE::READY,
           "direct raw writes should simulate hardware setting transmit-ready");
    expect(spiSr & SPI_SR::OVRES::OK,
           "direct raw writes should leave unrelated status bits clear");

    write_word(Config::kSpiSrAddress, kSpiSrReadyBits | kSpiSrOverrunBit);
    expect(spiSr & SPI_SR::OVRES::OVERRUN,
           "direct raw writes should simulate hardware raising overrun");

    write_word(Config::kSpiSrAddress, bit(1));
    expect(spiSr & SPI_SR::RDRF::EMPTY,
           "hardware-cleared status bits should read back immediately");
    expect(spiSr & SPI_SR::TDRE::READY,
           "status reads should preserve other hardware-driven bits");
    expect(spiSr & SPI_SR::OVRES::OK,
           "hardware-cleared overrun should read back as OK");

    expect_word(Config::kSpiCrAddress, 0x5A5A5A5Au,
                "hardware status updates should not touch the control register address");
    expect_word(Config::kSpiMrAddress, 0xA5A5A5A5u,
                "hardware status updates should not touch the mode register address");
  }

  static void test_access_policy_register_protocols() {
    ACCESS_STATUS::Instance<Config::kAccessStatusAddress> accessStatus;
    ACCESS_COMMAND::Instance<Config::kAccessCommandAddress> accessCommand;
    ACCESS_LATCH::Instance<Config::kAccessLatchAddress> accessLatch;
    ACCESS_ZERO::Instance<Config::kAccessZeroAddress> accessZero;

    clear_register_window();
    write_word(Config::kAccessStatusAddress, kAccessStatusReadyCount5 | bit(8));
    write_word(Config::kAccessLatchAddress, bit(0));
    write_word(Config::kAccessZeroAddress, bit(0) | bit(1));

    expect(accessStatus & ACCESS_STATUS::READY::ASSERTED,
           "read-only fields should decode hardware-provided states");
    expect_equal(accessStatus.template get<ACCESS_STATUS::COUNT>(), 5u,
                 "read-only value fields should decode raw counter bits");
    expect(accessStatus & ACCESS_STATUS::OVERRUN::DETECTED,
           "W1C fields should expose readable status states");
    expect(accessStatus & ACCESS_STATUS::FRAME::OK,
           "W1C fields should still report clear status when the raw bit is zero");

    accessStatus = ACCESS_STATUS::OVERRUN::CLEAR | ACCESS_STATUS::FRAME::CLEAR;
    expect_word(Config::kAccessStatusAddress, kAccessStatusErrorBits,
                "W1C actions should place one bits on the bus for the targeted status fields");

    accessCommand = ACCESS_COMMAND::START::TRIGGER | ACCESS_COMMAND::COUNT::value(3);
    expect_word(Config::kAccessCommandAddress, kAccessCommandStartCount3,
                "write-only command fields should still encode the expected raw command word");

    accessCommand.set(ACCESS_COMMAND::COUNT::value(7));
    expect_word(Config::kAccessCommandAddress, encode(8, 7),
                "write-only value-field writes should emit the expected raw payload bits");

    accessCommand.template set<ACCESS_COMMAND::START::TRIGGER>();
    expect_word(Config::kAccessCommandAddress, bit(0),
                "template set<value> should also work for write-only command actions");

    expect(accessLatch & ACCESS_LATCH::ENABLED::ON,
           "W1S-backed fields should still expose readable latched states");
    expect(accessLatch & ACCESS_LATCH::CHANNEL::OFF,
           "W1S-backed fields should decode clear states when the raw bit is zero");

    accessLatch = ACCESS_LATCH::ENABLED::SET | ACCESS_LATCH::CHANNEL::SET;
    expect_word(Config::kAccessLatchAddress, kAccessLatchSetBits,
                "W1S actions should place one bits on the bus for the targeted latch fields");

    expect(accessZero & ACCESS_ZERO::STICKY::SET,
           "W0C-backed fields should still expose readable asserted states");
    expect(accessZero & ACCESS_ZERO::ARMED::SET,
           "W0S-backed fields should still expose readable asserted states");

    accessZero = ACCESS_ZERO::STICKY::CLEAR_LATCH | ACCESS_ZERO::ARMED::FORCE_SET;
    expect_word(Config::kAccessZeroAddress, 0u,
                "W0C/W0S actions should emit zero bits on the bus for the targeted fields");
  }

  static void test_alias_views_share_the_same_address() {
    SPI_CR::Instance<Config::kAliasAddress> spiCr;
    typename SPI_CR_ALIAS::template Instance<Config::kAliasAddress> alias;

    clear_register_window();
    write_word(Config::kAliasAddress, kSpiCrEnableResetCmd2);

    expect(alias & SPI_CR_ALIAS::SPIEN::ENABLE,
           "an alias register definition should decode direct raw writes at the same address");
    expect_equal(alias.template get<SPI_CR_ALIAS::CMD>(), 2u,
                 "an alias register definition should decode value fields from the same raw address");

    spiCr &= ~SPI_CR::SWRST::MASK;
    expect(alias & SPI_CR_ALIAS::SWRST::IDLE,
           "clearing bits through one view should update every alias of that address");
    expect_word(Config::kAliasAddress, bit(0) | encode(8, 2),
                "alias operations should update the exact shared raw word");
  }
};

template <typename Config>
int run() {
  return Suite<Config>::run();
}

}  // namespace mmiocpp::targets::runtime_tests

#endif  // MMIOCPP_RUNTIME_TEST_SUITE_HPP
