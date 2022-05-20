# Simple Level Zero Samples

[![build](https://github.com/bashbaug/simple-level-zero-samples/workflows/build/badge.svg?branch=main)](https://github.com/bashbaug/simple-level-zero-samples/actions?query=workflow%3Abuild+branch%3Amain)

This repo contains simple Level Zero samples that demonstrate how to build
Level Zero applications using only the open-source headers and libs.
All samples have been tested on Windows and Linux.

## Code Structure

```
README.md               This file
LICENSE                 License information
CMakeLists.txt          Top-level CMakefile
external/               External Projects (headers and libs)
include/                Include Files
samples/                Samples
```

## How to Build the Samples

The samples require the following external dependencies:

Level Zero Headers and Loader:

    git clone https://github.com/oneapi-src/level-zero external/level-zero

After satisfying the external dependencies create build files using CMake.  For example:

    mkdir build && cd build
    cmake ..

Then, build with the generated build files.

## License

These samples are licensed under the [MIT License](LICENSE).

Notes:
* The samples use [popl](https://github.com/badaix/popl) for its options
parsing, which is licensed under the MIT License.
