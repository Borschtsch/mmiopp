#include "spi_example_registers.hpp"

namespace {

// The demo stays as a compile-checked usage example. Host-side builds do not
// execute direct MMIO writes; real runtime behavior is covered by the QEMU
// target tests.
void demoSequence() {
  SPI_CR::Instance<0xFFFE0000u> spiCr;
  SPI_MR::Instance<0xFFFE0004u> spiMr;

  // Direct MMIO register instances stay the normal path for on-target access.
  spiCr = SPI_CR::SPIEN::ENABLE | SPI_CR::SWRST::RESET;
  spiCr.set<SPI_CR::CMD>(1);

  spiCr |= SPI_CR::SPIEN::ENABLE;
  spiCr &= ~SPI_CR::SWRST::MASK;
  spiCr ^= SPI_CR::SPIEN::MASK;

  spiMr = SPI_MR::MSTR::MASTER | SPI_MR::DLY::value(7);
  spiMr.set<SPI_MR::MSTR::MASTER>();
  spiMr.set(SPI_MR::MSTR::MASTER | SPI_MR::DLY::value(7));
  spiMr.set<SPI_MR::PCS>(2);
  spiMr.set<SPI_MR::DLY>(7);
  spiMr |= SPI_MR::MSTR::MASTER;
  spiMr &= ~SPI_MR::MSTR::MASK;

  const bool isSlave = (spiMr & SPI_MR::MSTR::SLAVE);

  // The same register definitions also work as plain local shadow values when
  // several updates should be staged before one final commit.
  SPI_CR controlShadow = spiCr;
  SPI_MR modeShadow = spiMr;

  controlShadow |= SPI_CR::SWRST::RESET;
  modeShadow.set<SPI_MR::MSTR::MASTER>();
  modeShadow.set<SPI_MR::PCS>(1);

  spiCr = controlShadow;
  spiMr = modeShadow;

  const bool isMaster = (spiMr & SPI_MR::MSTR::MASTER);
  const bool releasesChipSelect = (modeShadow & SPI_MR::CSAAT::RELEASE);
  (void)isMaster;
  (void)isSlave;
  (void)releasesChipSelect;
}

}  // namespace

int main() {
  (void)&demoSequence;
  return 0;
}
