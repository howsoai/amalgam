# Amalgam&reg;

An LLM-ready, tree-structured language for safe, sandboxed code generation, manipulation, and advanced information-theoretic inference. 

**Table of Contents**

1. [Introduction](#introduction)
1. [Programming in Amalgam](#programming-in-amalgam)
    * [IDE Syntax Highlighting](#ide-syntax-highlighting)
    * [IDE Debugging](#ide-debugging)
1. [Amalgam Interpreter](#amalgam-interpreter)
    * [Build Matrix](#build-matrix)
    * [Dev/local Builds](#devlocal-builds)
    * [Runtime Requirements](#runtime-requirements)
    * [Usage](#usage)
1. [History and Roadmap](#history-and-roadmap)
1. [License](#license)
1. [Contributing](#contributing)

## Introduction

Amalgam&reg; is an LLM-ready, tree-structured language for safe, sandboxed code generation, manipulation, and advanced information-theoretic inference.  Unlike traditional languages that prioritize developer shorthand, Amalgam focuses on code-data symmetry and semantic consistency.

### Key Features
  - LLM-Ready By Design: By intentionally omitting a macro system, Amalgam ensures that visual similarity equals functional identity.  This structure allows LLMs to perform precise code generation, "patching" (identifying the exact difference between two scripts), and merging operations with high reliability.
  - Robust Sandboxing: Built with an inherent focus on sandboxing, Amalgam provides a robust permission and execution constraints system.  This makes it ideal for executing AI-generated code where safety, CPU limits, and memory constraints are paramount.  It has been hardened by a over a decade of evolutionary fuzz testing.
  - Universal Data/Code Unity: In Amalgam, there is no distinction between a "class" and an "instance" or "data" and "code".  Everything is code.  Amalgam allows for sophisticated querying of capabilities and easy reproduction of behaviors through copying and modification.
  - Rich Querying System: Subdividing code into "entities", Amalgam supports fast similarity search of code and data, as well as a rich information theoretic foundation for determining similarity.

### Design Philosophy

While influenced by LISP (prefix notation) and Smalltalk (object-oriented capabilities and embedded environment), Amalgam intentionally excludes a macro system.  This ensures that similar code is semantically identical.  By removing context-dependent transformations, it ensures that if two snippets look alike, they behave identically—a critical requirement for automated inference and "patching" via `(call (difference x y) {_ z})`.

Amalgam supports genetic programming and intentional loose typing to facilitate flexible execution, while its underlying tree structure maintains the integrity of the logic.  It is optimized for functional-style coding though supports procedural and object oriented programming.

### Example Uses

Amalgam is used in production systems driving the [Howso Engine](https://github.com/howsoai/howso-engine-py) allowing for rich data and code to be used and executed during inference, as well as evolutionary programming systems like [Evolver](https://github.com/howsoai/evolver).  Code can be called natively via Python via the [amalgam-lang-py](https://github.com/howsoai/amalgam-lang-py) package.

## Programming in Amalgam

See the [Amalgam beginner's guide](docs/beginner_guide.md) to get started.

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

The Amalgam interpreter is written conforming to the C++ 17 standard so theoretically should be compilable on virtually any modern system.

### Build Matrix

An interpreter application and shared library (dll/so/dylib) are built for each release. A versioned tarball is created for each target platform in the build matrix:

| Platform                     | Variants <sup>1</sup>               | Automated Testing  | Notes |
|------------------------------|-------------------------------------|:------------------:|-------|
| Windows amd64                | MT, ST, OMP, MT-NoAVX               | :heavy_check_mark: | |
| Linux amd64                  | MT, ST, OMP, MT-NoAVX, ST-PGC, AFMI | :heavy_check_mark: | ST-PGC and AFMI are for testing only, not packaged for release |
| Linux arm64: 8.2-a+simd+rcpc | MT, ST, OMP                         | :heavy_check_mark: | Tested with [qemu](https://www.qemu.org/) |
| Linux arm64: 8-a+simd        | ST                                  | :heavy_check_mark: | Tested with [qemu](https://www.qemu.org/) |
| macOS arm64: 8.4-a+simd      | MT, ST, OMP                         | :heavy_check_mark: | M1 and newer supported (amd64 NoAVX also tested w/ emulation) |
| WASM 64-bit                  | ST                                  | :heavy_check_mark: | Built on linux using emscripten, headless test with node:18 + jest |

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
		* Can be built with single-threaded or multi-threaded
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
    * AFMI
        * Stands for Amalgam fast memory integrity, which performs memory instrumentation to catch a variety of potential bugs
        * Incurs a small performance penalty so can be used in realistic workflows for debugging

#### Build Tools

Pre-built binaries use CMake+Ninja for CI/CD. See [PR workflow](.github/workflows/create-pr-build.yml) for automated build steps.

Though Amalgam is intended to support any C++17 compliant compiler, the current specific tool and OS versions used are:

* CMake 3.30
* Ninja 1.10
* Windows:
    * Visual Studio 2026 v145
* Linux:
    * Ubuntu 20.04, gcc-10
* macOS (Darwin):
    * macOS 13, AppleClang 15.0
* WASM:
    * Ubuntu 20.04, emscripten 3.1.67

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

* glibc 2.31 or later
* Arch: amd64 or arm64
    * Specific arm64 builds: `armv8-a+simd` & `armv8.2-a+simd+rcpc`

##### macOS (Darwin)

* macOS 12 or higher
* Arch: arm64
    * Specific arm64 builds: `armv8.4-a+simd` (M1 or later)

##### WASM

WASM support is still experimental.

* No specific runtime requirements at this time

## Dev/local Builds

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

Automation uses the CMake generated build system (ninja), but Visual Studio or VSCode are the best options for local development.  A helper script [open-in-vs.bat](open-in-vs.bat) is provided to set-up the CLI with VS build tools and open the IDE.  On Windows, it can be used via:

1. Default (no args) : Visual Studio solution (CMake generated, "amd64-windows-vs" preset)
    * `open-in-vs.bat`
1. vs_cmake : Visual Studio directory (load from directory with CMake file)
    * `open-in-vs.bat vs_cmake`
1. vscode : VSCode directory (load from directory with CMake file)
    * `open-in-vs.bat vscode`
1. vs_static : Visual Studio solution (local static non-CMake generated: [Amalgam.sln](Amalgam.sln))
    * `open-in-vs.bat vs_static`

Note: on Windows, some issues have been found with using the CMake generated VS solutions and the native CMake support in Visual Studio and VSCode.  If the developer experience is unstable, it is recommended that the `vs_static` build be used instead of the CMake generated build.

#### Build Customizations

Some specific build customizations are important to note. These customizations can be altered in the main [CMake file](CMakeLists.txt#L1):

* [Compiler options](build/cmake/global_compiler_flags.cmake)
* [arm64 arch](build/cmake/global_compiler_flags.cmake#L90)
* [amd64 AVX intrinsics](build/cmake/global_compiler_flags.cmake#L126)
* [Custom testing](build/cmake/create_tests.cmake)

### Debugging

For debugging the C++ code, example launch files are provided in [launch.json](docs/launch.example.json) (VSCode) and [launch.vs.json](docs/launch.vs.example.json) (Visual Studio) when opening a folder as a CMake project.

Remote debugging for linux is supported in both IDEs.

### Unit Tests

Unit tests can be run via an amalgam executable via the `--validate-amalgam` paramater.  No additional files are required as the majority of the tests are run from the built-in documentation.
```bash
./amalgam-mt --validate-amalgam
```

## Usage

Running the binary without any parameters is typical of many interpreters and yields a read-execute-print loop.  Help is available from within:

```bash
./amalgam-mt
```

Help for command line parameters can be obtained via the startard `--help` parameter:
```bash
./amalgam-mt --help
```

To run an Amalgam script:

```bash
./amalgam-mt test.amlg
```

 If the Amalgam file begins with a shebang (#!) followed by the path to the executable binary, which, coincidentally is the syntax for a private label, the script can be run as:

 ```bash
 ./test.amlg
 ```

## History and Roadmap

Initial development on Amalgam began in 2011. It was first offered as a commercial product in 2014 at Hazardous Software Inc. and was open sourced in September 2023 by Howso Incorporated (formerly known as Diveplane Corporation, a company spun out of Hazardous Software Inc.).  Amalgam takes inspiration from many programming languages, but the largest influences are LISP, Scheme, Haskell, Perl, Smalltalk, and Python.

The Amalgam interpreter does not currently have rich support for linking C libraries into the language, but that is planned for future functionality.  The roadmap also includes eventually disolving the query opcodes in favor of efficient specialized code paths using other opcodes such as `map` and `filter`.

## License

[License](LICENSE.txt)

## Contributing

[Contributing](CONTRIBUTING.md)
