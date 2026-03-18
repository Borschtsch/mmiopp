# MMIO++

[![Host Tests](assets/metrics/host-tests.svg)](assets/metrics/summary.md)
[![Compile-Fail Cases](assets/metrics/compile-fail.svg)](assets/metrics/summary.md)
[![Target Suites](assets/metrics/target-suites.svg)](assets/metrics/summary.md)
[![Core Line Coverage](assets/metrics/core-line-coverage.svg)](assets/metrics/summary.md)
[![Core Branch Coverage](assets/metrics/core-branch-coverage.svg)](assets/metrics/summary.md)

Latest main-branch CI signals are published in [assets/metrics/summary.md](assets/metrics/summary.md).

MMIO++ is a C++ memory-mapped I/O framework for developers who already know the feel of C register macros and want to keep that direct style without keeping the usual hazards.

The goal is not to turn register access into a heavyweight abstraction. The goal is to let register code still look familiar:

- define registers and fields close to the hardware layout
- compose named field values the way embedded developers already think about them
- reject cross-register mistakes and mask/value mixups at compile time
- keep the generated code suitable for real MMIO work

In practice, MMIO++ is meant to feel like the better-behaved C++ version of the macro-based register code many embedded teams already write by hand.

## Why MMIO++

Classic C register macros are concise, but they also make it easy to:

- combine values from unrelated registers
- confuse field masks with encoded field values
- write raw integers where only valid field encodings should be allowed
- lose intent in long chains of shifts and bitwise operators

MMIO++ keeps the familiar register-programming shape while using C++ types to stop those mistakes earlier.

## Familiar Register Style

Register definitions stay close to the way register maps are documented:

```cpp
struct SPI_CR : mmio::Register<SPI_CR> {
  struct SPIEN : mmio::BitField<SPI_CR, 0, 1> {
    static constexpr auto DISABLE = value(0);
    static constexpr auto ENABLE = value(1);
  };

  struct SWRST : mmio::BitField<SPI_CR, 7, 1> {
    static constexpr auto IDLE = value(0);
    static constexpr auto RESET = value(1);
  };

  struct DLY : mmio::ValueField<SPI_CR, 8, 2, std::uint8_t> {};
};
```

The public API separates the two concepts that C macro code often blurs together:

- `FIELD::VALUE_NAME` is an encoded field value used for writes, predicates, and value composition.
- `FIELD::MASK` is the automatically derived bit mask used for clear and toggle operations.
- Numeric value fields use `FIELD::value(x)`.

That makes the call sites read like register code, but with stricter rules:

```cpp
SPI_CR::Instance<0xFFFE0000u> spiCr;
SPI_MR::Instance<0xFFFE0004u> spiMr;

spiCr = SPI_CR::SPIEN::ENABLE | SPI_CR::SWRST::RESET;
spiCr |= SPI_CR::SPIEN::ENABLE;
spiCr &= ~SPI_CR::SWRST::MASK;

spiMr.set<SPI_MR::MSTR::MASTER>();
spiMr.set(SPI_MR::MSTR::MASTER | SPI_MR::DLY::value(7));
spiMr.set<SPI_MR::DLY>(7);

const bool isMaster = spiMr & SPI_MR::MSTR::MASTER;
```

The same register definition type also works as a shadow register value:

```cpp
SPI_MR modeShadow = SPI_MR::MSTR::MASTER | SPI_MR::DLY::value(7);
modeShadow.set<SPI_MR::PCS>(2);

SPI_MR::Instance<0xFFFE0004u> spiMr;
spiMr = modeShadow;
```

That pattern is useful when a register image belongs to an external device, when you want to stage several field updates locally, or when you want to snapshot one live register and commit a modified copy back later.

## Performance

MMIO++ is designed to stay zero-overhead in the usual embedded sense:

- MMIO-bound register objects still compile down to direct volatile loads and stores.
- Shadow registers are plain non-volatile local values, so the compiler can fold several updates together before one final commit.
- The framework uses compile-time types to reject misuse instead of runtime checks.

For the longer project story and the API rationale beyond the quickstart, see [docs/index.html](docs/index.html).

## Project Layout

- `include/mmio.hpp`: core public header only.
- `examples/`: example register maps and driver code built on top of the core header.
- `mmio_demo.cpp`: small usage example built through the normal host workflow.
- `tests/mmio_tests.cpp`: host-side positive API and behavior checks.
- `tests/compile_fail/*.cpp`: compile-fail coverage for misuse cases.
- `targets/qemu-cortex-m/`: Cortex-M3 QEMU target harness and runtime tests.
- `targets/qemu-cortex-r5/`: Cortex-R5 QEMU target harness and runtime tests.
- `CMakePresets.json`: canonical Windows entrypoints for build and test workflows.
- `scripts/bootstrap.ps1`: Windows tooling bootstrap.

## Windows Tooling

Install the expected Windows toolchain layout with:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1
```

The bootstrap installs CMake, the Arm GNU Toolchain, QEMU, and the repo-local WinLibs fallback that the committed presets use.

## Canonical Windows Commands

Use the committed CMake workflow presets as the primary entrypoints:

```powershell
cmake --workflow --preset host
cmake --workflow --preset host-test
cmake --workflow --preset qemu-m3-build
cmake --workflow --preset qemu-m3-run
cmake --workflow --preset qemu-m3-test
cmake --workflow --preset qemu-r5-build
cmake --workflow --preset qemu-r5-run
cmake --workflow --preset qemu-r5-test
```

What they do:

- `host`: configure and build the normal Windows host targets
- `host-test`: configure, build, and run the host test suite
- `qemu-m3-build`: build the Cortex-M3 QEMU target
- `qemu-m3-run`: build and run the Cortex-M3 QEMU runtime test target
- `qemu-m3-test`: build and run the Cortex-M3 CTest flow
- `qemu-r5-build`: build the Cortex-R5 QEMU target
- `qemu-r5-run`: build and run the Cortex-R5 QEMU runtime test target
- `qemu-r5-test`: build and run the Cortex-R5 CTest flow

## What The Tests Cover

The Windows host workflow checks the public API shape, positive usage paths, and compile-fail misuse cases.

The QEMU workflows exercise the code against target CPU address spaces rather than relying only on a desktop shim, so register behavior is validated in a more realistic environment.
