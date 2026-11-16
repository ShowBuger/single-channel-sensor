# Third-Party Libraries

## FreeRTOS Kernel

- **Version**: V11.1.0
- **Source**: https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/tag/V11.1.0
- **License**: MIT
- **Integration Date**: 2026-11-16
- **Location**: `FreeRTOS-Kernel/`

### Installation

Download and extract FreeRTOS Kernel V11.1.0:

```bash
cd firmware/libs
curl -L -o freertos.zip https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/V11.1.0.zip
unzip freertos.zip
mv FreeRTOS-Kernel-11.1.0 FreeRTOS-Kernel
rm freertos.zip
```

The kernel source files are not committed to this repository. Each developer must download them separately following the above steps.

### Configuration

FreeRTOS configuration is board-specific:
- H7: `src/port/stm32h7/FreeRTOSConfig.h`
- L4: `src/port/stm32l4/FreeRTOSConfig.h`
- F1: `src/port/stm32f1/FreeRTOSConfig.h`

### Port Files

ARM Cortex-M port files used:
- H7 (Cortex-M7): `portable/GCC/ARM_CM7/r0p1/`
- L4 (Cortex-M4F): `portable/GCC/ARM_CM4F/`
- F1 (Cortex-M3): `portable/GCC/ARM_CM3/`

Memory management: `portable/MemMang/heap_4.c`

## STM32 HAL Drivers

HAL drivers are checked into this repository under version control.

- STM32H7xx HAL Driver
- STM32L4xx HAL Driver
- STM32F1xx HAL Driver (legacy StdPeriph also present for F1)
- CMSIS

## Notes

- FreeRTOS upgrade requires a separate commit with explicit version change documentation
- Lock kernel version to ensure reproducible builds across development team
- Test all three MCU ports after any FreeRTOS version change
