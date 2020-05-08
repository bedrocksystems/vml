# vml

Virtual Machine Libraries.

[![pipeline status](https://gitlab.com/bedrocksystems/vml/badges/master/pipeline.svg)](https://gitlab.com/bedrocksystems/vml/commits/master)

## Purpose

The goal of this repo is to provide simple, robust and efficient libraries that can be used to build
a virtual execution environment on various platforms. It will provide virtual hardware pieces that can
be put together to create a Virtual Machine or emulation engine.

## Documentation

https://bedrocksystems.gitlab.io/vml

It can also be generated by running:
```sh
make doc
```

## Building

### Requirements
- make
- gcc or clang (with binutils or llvm equivalents)
- doxygen (if building the documentation)

### Default configuration: POSIX+x86_64 host system assumed
```sh
make
```

This will compile a default set of libraries along with examples.

### Supported configurations

VML supports various combinations of platforms and architecture. At the moment, we can pick from:

- x86_64 and aarch64 as the architecture (controlled by the TARGET environment variable)
- POSIX

For example:
```sh
make PLATFORM=posix TARGET=aarch64
```

will build for a POSIX system running under an aarch64 architecture. Of course, when cross-compiling, a
cross-compilation toolchain (and the appropriate libc,STL) should be provided.

## Libraries

Cross-platform:
- devices/pl011: Virtual UART PL011
- devices/firmware: Virtual Firmware functions (PSCI 1.0 supported)
- devices/gic: Virtual interrupt controller on ARM platforms (v2 and v3 supported)
- devices/msr: Virtual System Registers on ARM
- devices/simple_as: Static memory address space representation of a guest
- devices/timer: Virtual timers on ARM
- devices/vbus: Virtual bus, acts as an access point to the virtual address space (device and memory)
- devices/virtio: virtio implementation for para-virtualised drivers
- devices/virtio_net: a virtual network card based on virtio
- devices/virtio_console: a virtual serial device baseed on virtio
- arch/arch_api: Abstract API that should be implemented by all architecture libraries
- arch/arch_x86_64: Concrete implementation of the architecture API for x86_64
- arch/arch_aarch64: Concrete implementation of the architecture API for Aarch64 (ARM 64-bit)
- config/vmm_debug: Debugging facilities
- vcpu/cpu_model: Abstract CPU representation
- vcpu/vcpu_roundup: API used to stop, resume VCPUs

Platform libraries (provides the functions that the libs are expecting from the platform):
- posix

Note that some libraries are self-contained (if we omit the usage of the platform lib) and some other
libraries have dependencies on libraries present in this repo.

When building for POSIX, no external dependencies are required at the moment.

## Porting libraries to a new architecture or platform

Libraries are platform and architecture agnostic. To port them to a new architecture or platform,
a new library in arch/ or platform/ should be added.
