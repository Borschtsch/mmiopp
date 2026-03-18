#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif
#endif

#include "access_example_registers.hpp"
#include "spi_example_registers.hpp"
#include "stm32f429/i2s_driver.hpp"
#include "stm32f429/spi_registers.hpp"
#include "stm32f429/spi_driver.hpp"

namespace {

static_assert(std::is_same_v<decltype(SPI_CR::SPIEN::MASK | SPI_CR::SWRST::MASK),
                             mmio::BitMask<SPI_CR>>,
              "field-mask composition should stay a typed mask");
static_assert(std::is_same_v<std::remove_cv_t<decltype(SPI_CR::MASK)>, mmio::BitMask<SPI_CR>>,
              "register full mask should stay a typed mask");
static_assert(std::is_same_v<decltype(SPI_CR::SPIEN::ENABLE | SPI_CR::SWRST::RESET),
                             mmio::RegValue<SPI_CR>>,
              "bit-field value composition should stay a register value");
static_assert(std::is_same_v<decltype(SPI_MR::MSTR::MASTER | SPI_MR::CSAAT::KEEP_ASSERTED),
                             mmio::RegValue<SPI_MR>>,
              "bit-field composition on the same register should stay a register value");
static_assert(std::is_same_v<decltype(SPI_MR::MSTR::MASTER | SPI_MR::DLY::value(7)),
                             mmio::AssignValue<SPI_MR>>,
              "bit/value composition should become an assign value");
static_assert(std::is_same_v<decltype(SPI_MR::DLY::value(7) | SPI_MR::MSTR::MASTER),
                             mmio::AssignValue<SPI_MR>>,
              "value/bit composition should become an assign value");
static_assert(std::is_same_v<decltype(SPI_MR::PCS::value(2) | SPI_MR::DLY::value(7)),
                             mmio::AssignValue<SPI_MR>>,
              "value/value composition should stay an assign value");
static_assert(std::is_same_v<decltype(SPI_SR::RDRF::READY | SPI_SR::OVRES::OVERRUN),
                             mmio::RegValue<SPI_SR>>,
              "status-bit composition should stay a register value");
static_assert(std::is_same_v<decltype(stm32f429::i2s::CR2::TXDMAEN::ENABLED |
                                      stm32f429::i2s::CR2::ERRIE::ENABLED),
                             mmio::RegValue<stm32f429::i2s::CR2>>,
              "STM32 I2S CR2 interrupt and DMA bits should compose as a register value");
static_assert(std::is_same_v<decltype(stm32f429::i2s::I2SCFGR::MODE_MASTER_RX |
                                      stm32f429::i2s::I2SCFGR::STANDARD_PCM_LONG),
                             mmio::AssignValue<stm32f429::i2s::I2SCFGR>>,
              "STM32 I2S mode and standard combinations should compose as an assign value");
static_assert(std::is_same_v<decltype(stm32f429::i2s::I2SPR::MCLKOUTPUT_ENABLE |
                                      stm32f429::i2s::I2SPR::I2SDIV::value(12)),
                             mmio::AssignValue<stm32f429::i2s::I2SPR>>,
              "STM32 I2S prescaler composition should stay an assign value");
static_assert(std::is_same_v<decltype(stm32f429::spi::CR1::MSTR::MASTER |
                                      stm32f429::spi::CR1::BR::DIV_16),
                             mmio::AssignValue<stm32f429::spi::CR1>>,
              "STM32 SPI control fields should compose as an assign value");
static_assert(std::is_same_v<decltype(stm32f429::spi::CR2::TXEIE::ENABLED |
                                      stm32f429::spi::CR2::ERRIE::ENABLED),
                             mmio::RegValue<stm32f429::spi::CR2>>,
              "STM32 SPI interrupt bits should compose as a register value");
static_assert(std::is_same_v<stm32f429::SPI1::CR1,
                             stm32f429::spi::CR1::Instance<stm32f429::SPI1_BASE + 0x00u>>,
              "SPI1 CR1 alias should point at the correct register address");
static_assert(std::is_same_v<stm32f429::SPI2::I2SCFGR,
                             stm32f429::i2s::I2SCFGR::Instance<stm32f429::SPI2_BASE + 0x1Cu>>,
              "SPI2 I2SCFGR alias should point at the correct register address");
static_assert(std::is_same_v<stm32f429::I2S2ext::DR,
                             stm32f429::spi::DR::Instance<stm32f429::I2S2EXT_BASE + 0x0Cu>>,
              "I2S2ext DR alias should point at the correct register address");
static_assert(std::is_default_constructible_v<stm32f429::drivers::SpiDriver<stm32f429::SPI1>>,
              "SPI driver should be default constructible");
static_assert(std::is_default_constructible_v<stm32f429::drivers::I2sDriver<stm32f429::SPI2>>,
              "Simplex I2S driver should be default constructible");
static_assert(
    std::is_default_constructible_v<stm32f429::drivers::I2sDriver<stm32f429::SPI2, stm32f429::I2S2ext>>,
    "Full-duplex I2S driver should be default constructible");
// Access-restricted fields are compile-checked through the positive examples
// below and the compile-fail matrix, so the tests do not depend on internal
// usage-bit details.

void spiTxComplete(void*) {}
void spiRxComplete(void*) {}
void spiTransferComplete(void*) {}
void spiErrorCallback(void*, stm32f429::drivers::SpiError) {}
void i2sTxComplete(void*) {}
void i2sRxComplete(void*) {}
void i2sTransferComplete(void*) {}
void i2sErrorCallback(void*, stm32f429::drivers::I2sError) {}

#if defined(_WIN32)
constexpr std::uintptr_t kHostMmioBaseAddress = 0x4D4D0000u;
constexpr std::size_t kHostMmioWindowSize = 0x1000u;
constexpr std::uintptr_t kHostSpiCrAddress = kHostMmioBaseAddress + 0x00u;
constexpr std::uintptr_t kHostSpiMrAddress = kHostMmioBaseAddress + 0x04u;
constexpr std::uintptr_t kHostSpiSrAddress = kHostMmioBaseAddress + 0x08u;
constexpr std::uintptr_t kHostAccessStatusAddress = kHostMmioBaseAddress + 0x0Cu;
constexpr std::uintptr_t kHostAccessCommandAddress = kHostMmioBaseAddress + 0x10u;
constexpr std::uintptr_t kHostAccessLatchAddress = kHostMmioBaseAddress + 0x14u;
constexpr std::uintptr_t kHostAccessZeroAddress = kHostMmioBaseAddress + 0x18u;

constexpr std::uint32_t bit(unsigned offset) noexcept {
  return static_cast<std::uint32_t>(1u << offset);
}

constexpr std::uint32_t encode(unsigned offset, std::uint32_t value) noexcept {
  return static_cast<std::uint32_t>(value << offset);
}

class ScopedHostMmioWindow {
 public:
  ScopedHostMmioWindow() {
    base_ = ::VirtualAlloc(reinterpret_cast<void*>(kHostMmioBaseAddress),
                           static_cast<SIZE_T>(kHostMmioWindowSize),
                           MEM_RESERVE | MEM_COMMIT,
                           PAGE_READWRITE);
    if (base_ != reinterpret_cast<void*>(kHostMmioBaseAddress)) {
      if (base_ != nullptr) {
        ::VirtualFree(base_, 0, MEM_RELEASE);
      }
      base_ = nullptr;
    }
  }

  ~ScopedHostMmioWindow() {
    if (base_ != nullptr) {
      ::VirtualFree(base_, 0, MEM_RELEASE);
    }
  }

  ScopedHostMmioWindow(const ScopedHostMmioWindow&) = delete;
  ScopedHostMmioWindow& operator=(const ScopedHostMmioWindow&) = delete;

  bool mapped() const { return base_ != nullptr; }

 private:
  void* base_ = nullptr;
};
#endif

// These functions are intentionally never executed on the host. They exist to
// compile-check the positive API surface without dereferencing MMIO addresses in
// a desktop process. Runtime behavior is covered by the QEMU target tests.
void compileCheckedExamples() {
  SPI_CR::Instance<0x1000u> spiCr;
  SPI_MR::Instance<0x1004u> spiMr;
  SPI_SR::Instance<0x1008u> spiSr;
  SPI_CR spiCrShadow;
  SPI_MR spiMrShadow;

  const auto controlFlags = SPI_CR::SPIEN::ENABLE | SPI_CR::SWRST::RESET | SPI_CR::CMD::value(2);
  const auto modeConfig = SPI_MR::MSTR::MASTER | SPI_MR::DLY::value(5);
  const auto chipSelectConfig = SPI_MR::PCS::value(2) | SPI_MR::DLY::value(7);
  const auto clearSpiEnable = ~SPI_CR::SPIEN::MASK;
  const SPI_CR spiCrFromValue = SPI_CR::SPIEN::ENABLE | SPI_CR::CMD::value(3);

  spiCr = controlFlags;
  spiCr |= SPI_CR::SPIEN::ENABLE;
  spiCr &= clearSpiEnable;
  spiCr ^= SPI_CR::SWRST::MASK;
  spiCr.set<SPI_CR::CMD>(1);

  spiMr = modeConfig;
  spiMr = chipSelectConfig;
  spiMr.set<SPI_MR::MSTR::MASTER>();
  spiMr.set(SPI_MR::MSTR::MASTER | SPI_MR::DLY::value(7));
  spiMr.set<SPI_MR::DLY>(7);

  const auto command = spiCr.get<SPI_CR::CMD>();
  const auto delay = spiMr.get<SPI_MR::DLY>();
  const bool isMaster = (spiMr & SPI_MR::MSTR::MASTER);
  const bool keepsChipSelect = (spiMr & SPI_MR::CSAAT::KEEP_ASSERTED);
  const bool receiveReady = (spiSr & SPI_SR::RDRF::READY);
  const bool transmitReady = (spiSr & SPI_SR::TDRE::READY);
  const bool noOverrun = (spiSr & SPI_SR::OVRES::OK);
  const SPI_CR spiCrCopy = spiCr;
  const SPI_MR spiMrCopy = spiMr;

  spiCrShadow = spiCr;
  spiCrShadow |= SPI_CR::SPIEN::ENABLE;
  spiCrShadow &= ~SPI_CR::SWRST::MASK;
  spiCrShadow ^= SPI_CR::SWRST::MASK;
  spiCrShadow.set<SPI_CR::CMD>(1);

  spiMrShadow = spiMr;
  spiMrShadow.set(SPI_MR::MSTR::MASTER | SPI_MR::DLY::value(4));
  spiMrShadow.set<SPI_MR::PCS>(3);

  const auto shadowCommand = spiCrShadow.get<SPI_CR::CMD>();
  const auto shadowPcs = spiMrShadow.get<SPI_MR::PCS>();
  const bool shadowEnabled = (spiCrShadow & SPI_CR::SPIEN::ENABLE);

  spiCr = spiCrShadow;
  spiMr = spiMrShadow;
  (void)command;
  (void)delay;
  (void)isMaster;
  (void)keepsChipSelect;
  (void)receiveReady;
  (void)transmitReady;
  (void)noOverrun;
  (void)spiCrFromValue;
  (void)spiCrCopy;
  (void)spiMrCopy;
  (void)shadowCommand;
  (void)shadowPcs;
  (void)shadowEnabled;
}

void compileCheckedAccessExamples() {
  ACCESS_STATUS::Instance<0x1010u> accessStatus;
  ACCESS_COMMAND::Instance<0x1014u> accessCommand;
  ACCESS_LATCH::Instance<0x1018u> accessLatch;
  ACCESS_ZERO::Instance<0x101Cu> accessZero;
  ACCESS_STATUS accessStatusShadow;
  ACCESS_COMMAND accessCommandShadow;
  ACCESS_LATCH accessLatchShadow;
  ACCESS_ZERO accessZeroShadow;

  accessStatus = ACCESS_STATUS::OVERRUN::CLEAR | ACCESS_STATUS::FRAME::CLEAR;
  accessStatus.set<ACCESS_STATUS::FRAME::CLEAR>();

  accessCommand = ACCESS_COMMAND::START::TRIGGER | ACCESS_COMMAND::COUNT::value(3);
  accessCommand.set(ACCESS_COMMAND::STOP::TRIGGER | ACCESS_COMMAND::COUNT::value(1));
  accessCommand.set(ACCESS_COMMAND::COUNT::value(7));
  accessCommand.set<ACCESS_COMMAND::START::TRIGGER>();

  accessLatch = ACCESS_LATCH::ENABLED::SET | ACCESS_LATCH::CHANNEL::SET;
  accessLatch.set<ACCESS_LATCH::ENABLED::SET>();

  accessZero = ACCESS_ZERO::STICKY::CLEAR_LATCH | ACCESS_ZERO::ARMED::FORCE_SET;

  const bool ready = (accessStatus & ACCESS_STATUS::READY::ASSERTED);
  const bool overrunDetected = (accessStatus & ACCESS_STATUS::OVERRUN::DETECTED);
  const auto count = accessStatus.get<ACCESS_STATUS::COUNT>();
  const bool latchOn = (accessLatch & ACCESS_LATCH::ENABLED::ON);
  const bool stickySet = (accessZero & ACCESS_ZERO::STICKY::SET);

  accessStatusShadow = accessStatus;
  accessStatusShadow = ACCESS_STATUS::OVERRUN::CLEAR | ACCESS_STATUS::FRAME::CLEAR;
  const bool shadowReady = (accessStatusShadow & ACCESS_STATUS::READY::ASSERTED);
  const auto shadowCount = accessStatusShadow.get<ACCESS_STATUS::COUNT>();
  accessStatus = accessStatusShadow;

  accessCommandShadow = ACCESS_COMMAND::START::TRIGGER | ACCESS_COMMAND::COUNT::value(3);
  accessCommandShadow.set(ACCESS_COMMAND::STOP::TRIGGER | ACCESS_COMMAND::COUNT::value(1));
  accessCommandShadow.set<ACCESS_COMMAND::START::TRIGGER>();
  accessCommand = accessCommandShadow;

  accessLatchShadow = ACCESS_LATCH::ENABLED::SET | ACCESS_LATCH::CHANNEL::SET;
  const bool shadowLatchOn = (accessLatchShadow & ACCESS_LATCH::ENABLED::ON);
  accessLatch = accessLatchShadow;

  accessZeroShadow = ACCESS_ZERO::STICKY::CLEAR_LATCH | ACCESS_ZERO::ARMED::FORCE_SET;
  const bool shadowStickySet = (accessZeroShadow & ACCESS_ZERO::STICKY::SET);
  accessZero = accessZeroShadow;
  (void)ready;
  (void)overrunDetected;
  (void)count;
  (void)latchOn;
  (void)stickySet;
  (void)shadowReady;
  (void)shadowCount;
  (void)shadowLatchOn;
  (void)shadowStickySet;
}

void compileCheckedStm32f429RegisterExamples() {
  stm32f429::SPI1::CR1 spi1Cr1;
  stm32f429::SPI1::CR2 spi1Cr2;
  stm32f429::SPI1::SR spi1Sr;
  stm32f429::SPI1::DR spi1Dr;
  stm32f429::SPI1::CRCPR spi1Crcpr;
  stm32f429::SPI1::RXCRCR spi1RxCrc;
  stm32f429::SPI1::TXCRCR spi1TxCrc;
  stm32f429::SPI2::CR2 spi2Cr2;
  stm32f429::SPI2::SR spi2Sr;
  stm32f429::SPI2::DR spi2Dr;
  stm32f429::SPI2::I2SCFGR spi2Cfg;
  stm32f429::SPI2::I2SPR spi2Prescaler;
  stm32f429::I2S2ext::DR i2s2ExtDr;

  const auto spiControl = stm32f429::spi::CR1::MSTR::MASTER |
                          stm32f429::spi::CR1::BR::DIV_16 |
                          stm32f429::spi::CR1::CPOL::HIGH |
                          stm32f429::spi::CR1::CPHA::SECOND_EDGE |
                          stm32f429::spi::CR1::SSM::SOFTWARE |
                          stm32f429::spi::CR1::SSI::HIGH;
  const auto spiInterrupts = stm32f429::spi::CR2::ERRIE::ENABLED |
                             stm32f429::spi::CR2::RXNEIE::ENABLED |
                             stm32f429::spi::CR2::TXEIE::ENABLED;
  const auto dmaInterruptConfig =
      stm32f429::i2s::CR2::TXDMAEN::ENABLED | stm32f429::i2s::CR2::RXDMAEN::ENABLED |
      stm32f429::i2s::CR2::ERRIE::ENABLED;
  const auto formatConfig = stm32f429::i2s::I2SCFGR::I2SMOD::I2S_MODE |
                            stm32f429::i2s::I2SCFGR::MODE_MASTER_TX |
                            stm32f429::i2s::I2SCFGR::STANDARD_PHILIPS |
                            stm32f429::i2s::I2SCFGR::DATAFORMAT_16B |
                            stm32f429::i2s::I2SCFGR::CPOL_LOW;
  const auto pcmLongConfig = stm32f429::i2s::I2SCFGR::I2SE::ENABLED |
                             stm32f429::i2s::I2SCFGR::MODE_MASTER_RX |
                             stm32f429::i2s::I2SCFGR::STANDARD_PCM_LONG |
                             stm32f429::i2s::I2SCFGR::DATAFORMAT_32B;
  const auto prescalerConfig = stm32f429::i2s::I2SPR::MCLKOUTPUT_ENABLE |
                               stm32f429::i2s::I2SPR::ODD::EVEN |
                               stm32f429::i2s::I2SPR::I2SDIV::value(12);

  spi1Cr1 = spiControl;
  spi1Cr1 |= stm32f429::spi::CR1::SPE::ENABLED;
  spi1Cr1 &= ~stm32f429::spi::CR1::SPE::MASK;
  spi1Cr1 ^= stm32f429::spi::CR1::CRCEN::MASK;

  spi1Cr2 = spiInterrupts;
  spi1Cr2 |= stm32f429::spi::CR2::SSOE::ENABLED;
  spi1Cr2 &= ~stm32f429::spi::CR2::ERRIE::MASK;

  spi1Dr.set<stm32f429::spi::DR::DATA>(0xA55Au);
  spi1Crcpr.set<stm32f429::spi::CRCPR::POLYNOMIAL>(7u);

  const auto txData = spi1Dr.get<stm32f429::spi::DR::DATA>();
  const auto polynomial = spi1Crcpr.get<stm32f429::spi::CRCPR::POLYNOMIAL>();
  const auto rxCrc = spi1RxCrc.get<stm32f429::spi::RXCRCR::VALUE>();
  const auto txCrc = spi1TxCrc.get<stm32f429::spi::TXCRCR::VALUE>();
  const bool txEmpty = (spi1Sr & stm32f429::spi::SR::TXE::EMPTY);
  const bool rxNotEmpty = (spi1Sr & stm32f429::spi::SR::RXNE::NOT_EMPTY);

  spi2Cr2 = dmaInterruptConfig;
  spi2Cr2 |= stm32f429::i2s::CR2::TXEIE::ENABLED;
  spi2Cr2 &= ~stm32f429::i2s::CR2::ERRIE::MASK;

  spi2Cfg = formatConfig;
  spi2Cfg.set(pcmLongConfig);
  spi2Cfg.set<stm32f429::i2s::I2SCFGR::DATLEN>(2);
  spi2Cfg |= stm32f429::i2s::I2SCFGR::I2SE::ENABLED;
  spi2Cfg &= ~stm32f429::i2s::I2SCFGR::I2SE::MASK;
  spi2Cfg ^= stm32f429::i2s::I2SCFGR::I2SE::MASK;

  spi2Prescaler = prescalerConfig;
  spi2Prescaler.set<stm32f429::i2s::I2SPR::I2SDIV>(9);

  spi2Dr.set<stm32f429::i2s::DR::DATA>(0x55AAu);
  i2s2ExtDr.set<stm32f429::i2s::DR::DATA>(0xAA55u);

  const auto data = spi2Dr.get<stm32f429::i2s::DR::DATA>();
  const auto divider = spi2Prescaler.get<stm32f429::i2s::I2SPR::I2SDIV>();
  const auto dataLength = spi2Cfg.get<stm32f429::i2s::I2SCFGR::DATLEN>();
  const bool i2sMode = (spi2Cfg & stm32f429::i2s::I2SCFGR::I2SMOD::I2S_MODE);
  const bool masterRx = (spi2Cfg & stm32f429::i2s::I2SCFGR::MODE_MASTER_RX);
  const bool receiveNotEmpty = (spi2Sr & stm32f429::i2s::SR::RXNE::NOT_EMPTY);
  const bool busy = (spi2Sr & stm32f429::i2s::SR::BSY::BUSY);
  (void)txData;
  (void)polynomial;
  (void)rxCrc;
  (void)txCrc;
  (void)txEmpty;
  (void)rxNotEmpty;
  (void)data;
  (void)divider;
  (void)dataLength;
  (void)i2sMode;
  (void)masterRx;
  (void)receiveNotEmpty;
  (void)busy;
}

void compileCheckedStm32f429DriverExamples() {
  using namespace stm32f429::drivers;

  SpiDriver<stm32f429::SPI1> spi1;
  SpiConfig spiConfig{};
  spiConfig.role = SpiRole::master;
  spiConfig.phase = SpiClockPhase::secondEdge;
  spiConfig.polarity = SpiClockPolarity::high;
  spiConfig.bitOrder = SpiBitOrder::msbFirst;
  spiConfig.frameSize = SpiFrameSize::bits16;
  spiConfig.frameFormat = SpiFrameFormat::motorola;
  spiConfig.busMode = SpiBusMode::fullDuplex;
  spiConfig.baudDivider = SpiBaudDivider::div16;
  spiConfig.softwareSlaveManagement = true;
  spiConfig.internalSlaveSelectHigh = true;
  spiConfig.nssOutputEnable = false;
  spiConfig.crcEnable = false;
  spiConfig.crcPolynomial = 7u;
  spiConfig.enableAfterConfigure = true;
  spi1.setCallbacks({nullptr, &spiTxComplete, &spiRxComplete, &spiTransferComplete,
                      &spiErrorCallback});

  std::uint16_t spiTx[4] = {0x1111u, 0x2222u, 0x3333u, 0x4444u};
  std::uint16_t spiRx[4] = {};

  const auto spiConfigure = spi1.configure(spiConfig);
  const auto spiState = spi1.state();
  const auto spiErrors = spi1.error();
  const bool spiReady = spi1.ready();
  const bool spiEnabled = spi1.enabled();
  const bool spiBusy = spi1.busyFlag();
  const bool spiTxReady = spi1.txReady();
  const bool spiRxReady = spi1.rxReady();
  const auto spiTxPoll = spi1.transmitPolling(spiTx, 4u);
  const auto spiRxPoll = spi1.receivePolling(spiRx, 4u);
  const auto spiTransferPoll = spi1.transferPolling(spiTx, spiRx, 4u);
  const auto spiTxIt = spi1.startTransmitInterrupt(spiTx, 4u);
  const auto spiRxIt = spi1.startReceiveInterrupt(spiRx, 4u);
  const auto spiTransferIt = spi1.startTransferInterrupt(spiTx, spiRx, 4u);
  spi1.clearError();
  spi1.clearOverrun();
  spi1.flushReceive();
  spi1.enable();
  spi1.disable();
  spi1.onInterrupt();
  spi1.cancelInterrupt();

  I2sDriver<stm32f429::SPI2> i2s2;
  I2sConfig i2sSimplexConfig{};
  i2sSimplexConfig.mode = I2sMode::masterTx;
  i2sSimplexConfig.standard = I2sStandard::philips;
  i2sSimplexConfig.dataFormat = I2sDataFormat::bits16;
  i2sSimplexConfig.clockPolarity = I2sClockPolarity::low;
  i2sSimplexConfig.masterClock = I2sMasterClock::enabled;
  i2sSimplexConfig.prescaler = {12u, false};
  i2sSimplexConfig.enableAfterConfigure = true;
  i2s2.setCallbacks({nullptr, &i2sTxComplete, &i2sRxComplete, &i2sTransferComplete,
                      &i2sErrorCallback});

  std::uint16_t i2sTx[4] = {1u, 2u, 3u, 4u};
  std::uint16_t i2sRx[4] = {};

  const auto i2sSimplexConfigure = i2s2.configure(i2sSimplexConfig);
  const auto i2sSimplexState = i2s2.state();
  const auto i2sSimplexErrors = i2s2.error();
  const bool i2sSimplexReady = i2s2.ready();
  const bool i2sSimplexEnabled = i2s2.enabled();
  const bool i2sSimplexBusy = i2s2.busyFlag();
  const bool i2sSimplexTxReady = i2s2.txReady();
  const bool i2sSimplexRxReady = i2s2.rxReady();
  const auto i2sTxPoll = i2s2.transmitPolling(i2sTx, 4u);
  const auto i2sRxPoll = i2s2.receivePolling(i2sRx, 4u);
  const auto i2sTxIt = i2s2.startTransmitInterrupt(i2sTx, 4u);
  const auto i2sRxIt = i2s2.startReceiveInterrupt(i2sRx, 4u);
  i2s2.clearError();
  i2s2.clearOverrun();
  i2s2.clearUnderrun();
  i2s2.flushReceive();
  i2s2.enable();
  i2s2.disable();
  i2s2.onInterrupt();
  i2s2.cancelInterrupt();

  I2sDriver<stm32f429::SPI3, stm32f429::I2S3ext> i2s3;
  I2sConfig i2sDuplexConfig{};
  i2sDuplexConfig.mode = I2sMode::masterTx;
  i2sDuplexConfig.standard = I2sStandard::pcmLong;
  i2sDuplexConfig.dataFormat = I2sDataFormat::bits32;
  i2sDuplexConfig.clockPolarity = I2sClockPolarity::high;
  i2sDuplexConfig.masterClock = I2sMasterClock::enabled;
  i2sDuplexConfig.prescaler = {4u, true};
  i2sDuplexConfig.enableAfterConfigure = false;
  i2s3.setCallbacks({nullptr, &i2sTxComplete, &i2sRxComplete, &i2sTransferComplete,
                      &i2sErrorCallback});

  const auto i2sDuplexConfigure = i2s3.configure(i2sDuplexConfig);
  const auto i2sDuplexPoll = i2s3.transmitReceivePolling(i2sTx, i2sRx, 4u);
  const auto i2sDuplexIt = i2s3.startTransmitReceiveInterrupt(i2sTx, i2sRx, 4u);
  i2s3.onInterrupt();
  i2s3.cancelInterrupt();

  (void)spiConfigure;
  (void)spiState;
  (void)spiErrors;
  (void)spiReady;
  (void)spiEnabled;
  (void)spiBusy;
  (void)spiTxReady;
  (void)spiRxReady;
  (void)spiTxPoll;
  (void)spiRxPoll;
  (void)spiTransferPoll;
  (void)spiTxIt;
  (void)spiRxIt;
  (void)spiTransferIt;
  (void)i2sSimplexConfigure;
  (void)i2sSimplexState;
  (void)i2sSimplexErrors;
  (void)i2sSimplexReady;
  (void)i2sSimplexEnabled;
  (void)i2sSimplexBusy;
  (void)i2sSimplexTxReady;
  (void)i2sSimplexRxReady;
  (void)i2sTxPoll;
  (void)i2sRxPoll;
  (void)i2sTxIt;
  (void)i2sRxIt;
  (void)i2sDuplexConfigure;
  (void)i2sDuplexPoll;
  (void)i2sDuplexIt;
}

int runBoundRegisterRuntimeChecks() {
#if !defined(_WIN32)
  return 0;
#else
  int failures = 0;

  auto expect = [&failures](bool condition) {
    if (!condition) {
      ++failures;
    }
  };

  auto rawWord = [](std::uintptr_t address) -> volatile std::uint32_t& {
    return *reinterpret_cast<volatile std::uint32_t*>(address);
  };

  auto readWord = [&rawWord](std::uintptr_t address) -> std::uint32_t { return rawWord(address); };
  auto writeWord = [&rawWord](std::uintptr_t address, std::uint32_t value) { rawWord(address) = value; };

  ScopedHostMmioWindow mmioWindow;
  if (!mmioWindow.mapped()) {
    return 1;
  }

  auto clearWindow = [&]() {
    writeWord(kHostSpiCrAddress, 0u);
    writeWord(kHostSpiMrAddress, 0u);
    writeWord(kHostSpiSrAddress, 0u);
    writeWord(kHostAccessStatusAddress, 0u);
    writeWord(kHostAccessCommandAddress, 0u);
    writeWord(kHostAccessLatchAddress, 0u);
    writeWord(kHostAccessZeroAddress, 0u);
  };

  SPI_CR::Instance<kHostSpiCrAddress> spiCr;
  SPI_MR::Instance<kHostSpiMrAddress> spiMr;
  SPI_SR::Instance<kHostSpiSrAddress> spiSr;
  ACCESS_STATUS::Instance<kHostAccessStatusAddress> accessStatus;
  ACCESS_COMMAND::Instance<kHostAccessCommandAddress> accessCommand;
  ACCESS_LATCH::Instance<kHostAccessLatchAddress> accessLatch;
  ACCESS_ZERO::Instance<kHostAccessZeroAddress> accessZero;

  clearWindow();

  spiCr.set(SPI_CR::SPIEN::ENABLE);
  expect(readWord(kHostSpiCrAddress) == bit(0));
  expect(spiCr & SPI_CR::SPIEN::ENABLE);

  expect(spiCr.get<SPI_CR::SPIEN>() == 1u);
  spiCr.set<SPI_CR::SWRST::RESET>();
  expect(readWord(kHostSpiCrAddress) == bit(7));

  spiCr |= SPI_CR::SPIEN::ENABLE;
  expect(readWord(kHostSpiCrAddress) == (bit(0) | bit(7)));

  SPI_CR snappedControl = spiCr;
  expect(snappedControl & SPI_CR::SPIEN::ENABLE);
  expect(snappedControl & SPI_CR::SWRST::RESET);
  snappedControl.set<SPI_CR::CMD>(2u);
  spiCr = snappedControl;
  expect(readWord(kHostSpiCrAddress) == (bit(0) | bit(7) | encode(8, 2)));

  spiCr ^= SPI_CR::SWRST::MASK;
  expect(readWord(kHostSpiCrAddress) == (bit(0) | encode(8, 2)));

  clearWindow();

  spiMr.set(SPI_MR::PCS::value(1));
  expect(readWord(kHostSpiMrAddress) == encode(4, 1));
  expect(spiMr & SPI_MR::PCS::value(1));

  spiCr = SPI_CR::SPIEN::ENABLE | SPI_CR::SWRST::RESET | SPI_CR::CMD::value(2);
  expect(readWord(kHostSpiCrAddress) == (bit(0) | bit(7) | encode(8, 2)));
  expect(spiCr & SPI_CR::SPIEN::ENABLE);
  expect(spiCr.get<SPI_CR::CMD>() == 2u);

  spiCr &= ~SPI_CR::SWRST::MASK;
  expect(readWord(kHostSpiCrAddress) == (bit(0) | encode(8, 2)));

  spiCr.set<SPI_CR::CMD>(1u);
  expect(readWord(kHostSpiCrAddress) == (bit(0) | encode(8, 1)));
  expect(spiCr & SPI_CR::SWRST::IDLE);

  spiMr = SPI_MR::MSTR::MASTER | SPI_MR::CSAAT::KEEP_ASSERTED |
          SPI_MR::PCS::value(2) | SPI_MR::DLY::value(7);
  expect(readWord(kHostSpiMrAddress) == (bit(0) | bit(3) | encode(4, 2) | encode(8, 7)));
  expect(spiMr & SPI_MR::MSTR::MASTER);
  expect(spiMr.get<SPI_MR::PCS>() == 2u);
  expect(static_cast<std::uint32_t>(spiMr.get<SPI_MR::DLY>()) == 7u);

  spiMr &= ~SPI_MR::CSAAT::MASK;
  expect(readWord(kHostSpiMrAddress) == (bit(0) | encode(4, 2) | encode(8, 7)));
  expect(spiMr & SPI_MR::CSAAT::RELEASE);

  writeWord(kHostSpiSrAddress, bit(0) | bit(1) | bit(3));
  expect(spiSr & SPI_SR::RDRF::READY);
  expect(spiSr & SPI_SR::TDRE::READY);
  expect(spiSr & SPI_SR::OVRES::OVERRUN);

  writeWord(kHostAccessStatusAddress, bit(0) | encode(4, 5) | bit(8));
  expect(accessStatus & ACCESS_STATUS::READY::ASSERTED);
  expect(accessStatus.get<ACCESS_STATUS::COUNT>() == 5u);
  expect(accessStatus & ACCESS_STATUS::OVERRUN::DETECTED);
  expect(accessStatus & ACCESS_STATUS::FRAME::OK);

  accessCommand = ACCESS_COMMAND::START::TRIGGER | ACCESS_COMMAND::COUNT::value(3);
  expect(readWord(kHostAccessCommandAddress) == (bit(0) | encode(8, 3)));

  accessLatch = ACCESS_LATCH::ENABLED::SET | ACCESS_LATCH::CHANNEL::SET;
  expect(readWord(kHostAccessLatchAddress) == (bit(0) | bit(1)));

  accessZero = ACCESS_ZERO::STICKY::CLEAR_LATCH | ACCESS_ZERO::ARMED::FORCE_SET;
  expect(readWord(kHostAccessZeroAddress) == 0u);

  return failures;
#endif
}

int runShadowRegisterRuntimeChecks() {
  int failures = 0;

  auto expect = [&failures](bool condition) {
    if (!condition) {
      ++failures;
    }
  };

  SPI_CR controlShadow;
  SPI_CR externalControlShadow = SPI_CR::SPIEN::ENABLE | SPI_CR::CMD::value(3);
  SPI_CR committedControlShadow;
  SPI_MR modeShadow = SPI_MR::MSTR::MASTER | SPI_MR::DLY::value(5);
  stm32f429::spi::CR1 spiConfigShadow;

  expect(controlShadow & SPI_CR::SPIEN::DISABLE);
  expect(controlShadow.get<SPI_CR::CMD>() == 0u);

  controlShadow = externalControlShadow;
  expect(controlShadow & SPI_CR::SPIEN::ENABLE);
  expect(controlShadow.get<SPI_CR::CMD>() == 3u);

  controlShadow |= SPI_CR::SWRST::RESET;
  controlShadow &= ~SPI_CR::SPIEN::MASK;
  controlShadow.set<SPI_CR::CMD>(1u);
  expect(controlShadow & SPI_CR::SWRST::RESET);
  expect(controlShadow & SPI_CR::SPIEN::DISABLE);
  expect(controlShadow.get<SPI_CR::CMD>() == 1u);

  committedControlShadow = controlShadow;
  expect(committedControlShadow & SPI_CR::SWRST::RESET);
  expect(committedControlShadow.get<SPI_CR::CMD>() == 1u);
  controlShadow ^= SPI_CR::SWRST::MASK;
  expect(controlShadow & SPI_CR::SWRST::IDLE);
  controlShadow ^= SPI_CR::SWRST::MASK;
  expect(controlShadow & SPI_CR::SWRST::RESET);

  SPI_CR encodedControlShadow;
  encodedControlShadow.set<SPI_CR::SPIEN::ENABLE>();
  expect(encodedControlShadow & SPI_CR::SPIEN::ENABLE);

  SPI_MR reverseMixedModeShadow = SPI_MR::PCS::value(1) | SPI_MR::MSTR::MASTER;
  expect(reverseMixedModeShadow & SPI_MR::MSTR::MASTER);
  expect(reverseMixedModeShadow.get<SPI_MR::PCS>() == 1u);

  const auto combinedControlMask = SPI_CR::SPIEN::MASK | SPI_CR::SWRST::MASK;
  SPI_CR composedControlShadow = SPI_CR::SPIEN::ENABLE | SPI_CR::SWRST::RESET;
  expect(composedControlShadow & SPI_CR::SPIEN::ENABLE);
  expect(composedControlShadow & SPI_CR::SWRST::RESET);
  composedControlShadow &= ~combinedControlMask;
  expect(composedControlShadow & SPI_CR::SPIEN::DISABLE);
  expect(composedControlShadow & SPI_CR::SWRST::IDLE);

  SPI_MR composedModeShadow = SPI_MR::MSTR::MASTER | SPI_MR::PCS::value(2) | SPI_MR::DLY::value(7);
  expect(composedModeShadow & SPI_MR::MSTR::MASTER);
  expect(composedModeShadow.get<SPI_MR::PCS>() == 2u);
  expect(static_cast<std::uint32_t>(composedModeShadow.get<SPI_MR::DLY>()) == 7u);

  stm32f429::spi::CR2 interruptShadow =
      stm32f429::spi::CR2::TXEIE::ENABLED | stm32f429::spi::CR2::ERRIE::ENABLED;
  const auto primaryInterruptMask = (interruptShadow & stm32f429::spi::CR2::TXEIE::ENABLED)
                                      ? stm32f429::spi::CR2::TXEIE::MASK
                                      : stm32f429::spi::CR2::ERRIE::MASK;
  const auto secondaryInterruptMask = (interruptShadow & stm32f429::spi::CR2::ERRIE::ENABLED)
                                        ? stm32f429::spi::CR2::ERRIE::MASK
                                        : stm32f429::spi::CR2::TXEIE::MASK;
  interruptShadow &= ~(primaryInterruptMask | secondaryInterruptMask);
  expect(interruptShadow & stm32f429::spi::CR2::TXEIE::DISABLED);
  expect(interruptShadow & stm32f429::spi::CR2::ERRIE::DISABLED);


  modeShadow.set(SPI_MR::MSTR::MASTER | SPI_MR::PCS::value(2) | SPI_MR::DLY::value(7));
  modeShadow &= ~SPI_MR::CSAAT::MASK;
  expect(modeShadow & SPI_MR::MSTR::MASTER);
  expect(modeShadow & SPI_MR::CSAAT::RELEASE);
  expect(modeShadow.get<SPI_MR::PCS>() == 2u);
  expect(static_cast<std::uint32_t>(modeShadow.get<SPI_MR::DLY>()) == 7u);

  spiConfigShadow = stm32f429::spi::CR1::MSTR::MASTER | stm32f429::spi::CR1::BR::DIV_16;
  spiConfigShadow |= stm32f429::spi::CR1::SPE::ENABLED;
  spiConfigShadow &= ~stm32f429::spi::CR1::CRCEN::MASK;
  expect(spiConfigShadow & stm32f429::spi::CR1::MSTR::MASTER);
  expect(spiConfigShadow & stm32f429::spi::CR1::SPE::ENABLED);
  expect(spiConfigShadow.get<stm32f429::spi::CR1::BR>() == 3u);
  expect(spiConfigShadow & stm32f429::spi::CR1::CRCEN::DISABLED);

  return failures;
}

}  // namespace

int main() {
  (void)&compileCheckedExamples;
  (void)&compileCheckedAccessExamples;
  (void)&compileCheckedStm32f429RegisterExamples;
  (void)&compileCheckedStm32f429DriverExamples;
  return runBoundRegisterRuntimeChecks() + runShadowRegisterRuntimeChecks();
}
