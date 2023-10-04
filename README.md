# Amalgam&trade;

**Table of Contents**

1. [Introduction](#introduction)
1. [Programming in Amalgam](#programming-in-amalgam)
    * [IDE Syntax Highlighting](#ide-syntax-highlighting)
    * [IDE Debugging](#ide-debugging)
1. [Amalgam Interpreter](#amalgam-interpreter)
    * [Recommended System Specs](#recommended-system-specs)
    * [Pre-built Binaries](#pre-built-binaries)
    * [Dev/local Builds](#devlocal-builds)
    * [Usage](#usage)
1. [Contributing](#contributing)

## Introduction

Amalgam&trade; is a domain specific language ([DSL](https://en.wikipedia.org/wiki/Domain-specific_language)) developed primarily for [genetic programming](https://en.wikipedia.org/wiki/Generic_programming) and [instance based machine learning](https://en.wikipedia.org/wiki/Instance-based_learning), but also for simulation, agent based modeling, data storage and retrieval, the mathematics of probability theory and information theory, and game content and AI. The language format is somewhat LISP-like in that it uses parenthesized list format with prefix notation and is geared toward functional programming, where there is a one-to-one mapping between the code and the corresponding parse tree.

Whereas virtually all practical programming languages are primarily designed for some combination of programmer productivity and computational performance, Amalgam prioritizes code matching and merging, as well as a deep equivalence of code and data. Amalgam uses _entities_ to store code and data, with a rich query system to find entities by their _labels_. The language uses a variable stack, but all attributes and methods are stored directly as labels in entities. There is no separate class versus instance, but entities can be used as prototypes to be copied and modified. Though code and data are represented as trees from the root of each entity, graphs in code and data structures are permitted and are flattened to code using special references. Further, instead of failing early when there is an error, Amalgam supports genetic programming and code mixing by being extremely weakly typed, and attempts to find a way to execute code no matter whether types match or not.

Amalgam takes inspiration from many programming languages, but those with the largest influence are LISP, Scheme, Haskell, Perl, Smalltalk, and Python. Despite being much like LISP, there is deliberately no macro system. This is to make sure that code is semantically similar whenever the code is similar, regardless of context. It makes it easy to find the difference between x and y as an executable patch, and then apply that patch to z as `(call (difference x y) (assoc _ z))`, or semantically mix blocks of code a and b as `(mix a b)`. Amalgam is not a purely functional language. It has imperative and object oriented capabilities, but is primarily optimized for functional programming with relatively few opcodes that are functionally flexible based on parameters to maximize flexibility with code mixing and matching.

As Amalgam was designed with genetic programming in mind, there is alway a chance that an evolved program ends up consuming more CPU or memory resources than desired, or may attempt to affect the system outside of the interpreter. For these reasons, there are many strict sandboxing aspects of the language with optional constraints on access, CPU, and memory. The interpreter also has a permissions system, with only the root entity having access to system commands (though it can give other entities root permissions), and entities cannot have any effect on containing entities unless the container offers an executable label to contained entities.

The Amalgam interpreter was designed to be used a standalone interpreter and to build functionality for other programming languages and environments. It does not currently have rich support for linking C libraries into the language, but that is planned for future functionality.

Initial development on Amalgam began in 2011. It was first offered as a commercial product in 2014 at Hazardous Software Inc. and was open sourced in September 2023 by Howso Incorporated (formerly known as Diveplane Corporation, a company spun out of Hazardous Software Inc.).

When referencing the language: 'Amalgam', 'amalgam', 'amalgam-lang', and 'amalgam language' are used interchangeably with **Amalgam** being preferred. When referencing the interpreter: 'Amalgam interpreter', 'interpreter', 'Amalgam app', and 'Amalgam lib' are used interchangeably.

### Programming in Amalgam

See the [Amalgam beginner's guide](AMALGAM-BEGINNER-GUIDE.md) to get started.

Full Amalgam language usage documentation is located in the [Amalgam Language Reference](https://howsoai.github.io/amalgam).

Further examples can be found in the [examples](examples/README.md) directory.

The primary file extensions consist of:
* `.amlg` - Amalgam script
* `.mdam` - Amalgam metadata, primarily just current random seed
* `.caml` - compressed Amalgam for fast storage and loading, that may contain many entities.

### IDE Syntax Highlighting

Syntax highlighting is provided as a plugin for 2 major vendors:

* [VSCode Plugin](https://github.com/howsoai/amalgam-ide-support-vscode)
* [Notepad++ Plugin](https://github.com/howsoai/amalgam-ide-support-npp)

Since debugging Amalgam code is only supported in VSCode, it is recommended that VSCode be the main IDE for writing and debugging Amalgam code.

### IDE Debugging

Debugging Amalgam is supported through the [VSCode Plugin](https://github.com/howsoai/amalgam-ide-support-vscode).

## Amalgam Interpreter

The Amalgam interpreter is written in C++ and uses the newest standards to create a fast, cross-platform experience when running Amalgam code.

### Recommended System Specs

At least 8 physical cores and 16GB of RAM.

Although the interpreter itself can run on very few system resources, the above recommendation is based on the typical type of workloads that are compute and memory intensive.

### Pre-built Binaries

Pre-built binaries are provided for specific target systems. They are as statically linked as possible without overly complicating the build.

#### Build Matrix

An interpreter application and shared library (dll/so/dylib) are built for each release. A versioned tarball is created for each target platform in the build matrix:

| Platform                     | Variants <sup>1</sup>         | Automated Testing  | Notes |
|------------------------------|-------------------------------|:------------------:|-------|
| Windows amd64                | MT, ST, OMP, MT-NoAVX         | :heavy_check_mark: |       |
| Linux amd64                  | MT, ST, OMP, MT-NoAVX, ST-PGC | :heavy_check_mark: | ST-PGC is for testing only, not packaged for release |
| Linux arm64: 8.2-a+simd+rcpc | MT, ST, OMP                   | :heavy_check_mark: | Tested with [qemu](https://www.qemu.org/) |
| Linux arm64: 8-a+simd        | ST                            | :heavy_check_mark: | Tested with [qemu](https://www.qemu.org/) |
| macOS amd64                  | MT, ST, OMP, MT-NoAVX         | :heavy_check_mark: | Only **MT-NoAVX** tested currently |
| macOS arm64: 8.4-a+simd      | MT, ST, OMP                   | :x:                | Manually tested, M1 and newer supported |
| WASM 64-bit                  | ST                            | :x:                | Built on linux using emscripten. Planned testing: headless test with node + jest |

* <sup>1</sup> Variant meanings:
    * MT
        * Multi-threaded
        * Binary postfix: '-mt'
        * Interpreter uses threads for parallel operations to increase throughput at the cost of some overhead
    * ST
        * Single-threaded
        * Binary postfix: '-st'
        * Interpreter does not use multiple threads for parallel operations
    * OMP
        * [OpenMP](https://en.wikipedia.org/wiki/OpenMP)
        * Binary postfix: '-omp'
        * Interpreter uses OpenMP threading internally to minimize latency of query operations
    * MT-NoAVX
        * Multi-threaded but without AVX intrinsics
        * Binary postfix: '-mt-noavx'
        * Interpreter uses threads for parallel operations to increase throughput at the cost of some overhead but without AVX intrinsics
        * Useful in emulators, virtualized environments, and older hardware that don't have AVX support
        * amd64 only, as AVX intrinsics are only applicable to variants of this architecture
    * ST-PGC
        * Single-threaded with pedantic garbage collection (PGC)
        * Binary postfix: '-st-pgc'
        * Interpreter does not use any threads and performs garbage collection at every operation
        * Very slow by nature, intended only be used for verification during testing or debugging

#### Build Tools

Pre-built binaries use CMake+Ninja for CI/CD. See [PR workflow](.github/workflows/create-pr-build.yml) for automated build steps.

Though Amalgam is intended to support any C++17 compliant compiler, the current specific tool and OS versions used are:

* CMake 3.23
* Ninja 1.11
* Windows:
    * Visual Studio 2022 v143
* Linux:
    * Ubuntu 20.04, gcc-10
* macOS (Darwin):
    * macOS 12, AppleClang 14.0
* WASM:
    * Ubuntu 20.04, emscripten 3.1.32

#### Runtime Requirements

Running the pre-built interpreter has specific runtime requirements per platform:

##### All

* 64-bit OS

##### amd64

* [AVX2](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions#Advanced_Vector_Extensions_2) intrinsics
    * A "no-avx" variant of the multi-threading binary is built on amd64 platforms for environments without AVX2 intrinsics.
    * See [Dev/local Builds](#devlocal-builds) for compiling with other intrinsics (i.e., AVX512)

##### Windows

* OS:
    * Microsoft Windows 10 or later
    * Microsoft Windows Server 2016 or later
* Arch: amd64

##### Linux

* glibc 2.29 or later
* Arch: amd64 or arm64
    * Specific arm64 builds: `armv8-a+simd` & `armv8.2-a+simd+rcpc`

##### macOS (Darwin)

* macOS 12 or higher
* Arch: amd64 or arm64
    * Specific arm64 builds: `armv8.4-a+simd` (M1 or later)

##### WASM

WASM support is still experimental.

* No specific runtime requirements at this time

### Dev/local Builds

Dev and local builds can be either run using a CLI or IDE.

#### CLI

The workflow for building the interpreter on all platforms is fairly straightforward. [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) are used to define all settings in the [CMake presets file](CMakePresets.json).

Example for release build on linux amd64:

```bash
PRESET="amd64-release-linux"
cmake --preset $PRESET # configure/generate (./out/build)
cmake --build --preset $PRESET # build
cmake --build --preset $PRESET --target test # test
cmake --build --preset $PRESET --target install # install (./out/install)
cmake --build --preset $PRESET --target package # package (./out/package)
```

The above performs a local "build install". For specifying a custom location, run install with an install prefix. Depending on permissions, admin access (admin elevated prompt on Windows, sud/su on linux/macos) might be needed:

```bash
cmake -DCMAKE_INSTALL_PREFIX="/path/to/install/location" --build --preset $PRESET --target install
```

Depending on the platform, not all tests will run successfully out of the box, especially when cross compiling. For those cases (i.e., arm64 on Mac M1 or AVX2/AVX512), the tests that are runnable on the specific platform can be included/excluded by running CTest directly (not through CMake, like above):

```bash
ctest --preset $PRESET --label-exclude 'advanced_intrinsics'
```

To see all available test labels:

```bash
ctest --preset $PRESET --print-labels
```

All CTest run options can be on the [CMake website](https://cmake.org/cmake/help/latest/manual/ctest.1.html#run-tests).

#### IDE

Automation uses the CMake generated build system (ninja), but Visual Studio or VSCode are the best options for local development. In general, VScode is recommended as it provides the most uniform developer experience across platforms.

For the best C++ developer experience, Visual Studio on Windows is the ideal development environment (no paid features needed, VS Community edition works). A helper script [open-in-vs.bat](open-in-vs.bat) is provided to set-up the CLI with VS build tools and open the IDE. It can be used to open multiple variants of the Windows build:

1. Default (no args) : Visual Studio solution (CMake generated, "amd64-windows-vs" preset)
    * `open-in-vs.bat`
1. vs_cmake : Visual Studio directory (load from directory with CMake file)
    * `open-in-vs.bat vs_cmake`
1. vscode : VSCode directory (load from directory with CMake file)
    * `open-in-vs.bat vscode`
1. vs_static : Visual Studio solution (local static non-CMake generated: [Amalgam.sln](Amalgam.sln))
    * `open-in-vs.bat vs_static`

Note: on Windows, some issues have been found with using the CMake generated VS solutions and the native CMake support in Visual Studio and VSCode. If the developer experience is unstable, it is recommended that the `vs_static` build be used instead of the CMake generated build. It is planned to (eventually) deprecate this static VS solution when CMake support in VS becomes more stable.

#### Build Customizations

Some specific build customizations are important to note. These customizations can be altered in the main [CMake file](CMakeLists.txt#L1):

* [Compiler options](build/cmake/global_compiler_flags.cmake)
* [arm64 arch](build/cmake/global_compiler_flags.cmake#L90)
* [amd64 AVX intrinsics](build/cmake/global_compiler_flags.cmake#L126)
* [Custom testing](build/cmake/create_tests.cmake)

### Debugging

For debugging the C++ code, example launch files are provided in [launch.json](docs/launch.example.json) (VSCode) and [launch.vs.json](docs/launch.vs.example.json) (Visual Studio) when opening a folder as a CMake project.

Remote debugging for linux is supported in both IDEs.

### Usage

Given an Amalgam interpreter, usage is similar to other popular interpreters.

Basic usage description and CLI options can be retrieved by running the binary without any parameters:

```bash
./amalgam-mt
```

Run an Amalgam script:

```bash
./amalgam-mt test.amlg
```

 If the Amalgam file begins with a shebang (#!) followed by the path to the executable binary, which, coincidentally is the syntax for a private label, the script can be run as:

 ```bash
 ./test.amlg
 ```

## License

[License](LICENSE.txt)

## Contributing

[Contributing](CONTRIBUTING.md)
