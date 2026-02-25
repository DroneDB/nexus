# libnexus

**Fork of [cnr-isti-vclab/nexus](https://github.com/cnr-isti-vclab/nexus)** — restructured as a **C++ static library** (`libnexus`) for programmatic creation and compression of batched multiresolution 3D models.

All command-line executables (`nxsbuild`, `nxscompress`, `nxsview`, `nxsedit`) and Qt dependencies have been removed. The library exposes a clean C API for integration into any C/C++ application.

## Entry Points

The public API is defined in [`src/common/nxs.h`](src/common/nxs.h):

```c
#include "common/nxs.h"

// Simple build: input mesh → output .nxs or .nxz
NXSErr nexusBuild(const char *input,
                  const char *output,
                  char *errorMessage,
                  int errorMessageSize);

// Extended build with full control over all parameters
NXSErr nexusBuildEx(const char *input,
                    const char *output,
                    const NexusBuildOptions &options,
                    char *errorMessage,
                    int errorMessageSize);
```

**Return codes** (`NXSErr`):
| Value | Meaning |
|-------|---------|
| `NXSERR_NONE` (0) | Success |
| `NXSERR_EXCEPTION` (1) | Internal error |
| `NXSERR_INVALID_INPUT` (2) | Invalid input file or parameters |

**`NexusBuildOptions`** controls geometry build (node size, scaling, texture quality, point cloud mode, normals/colors/texcoords toggling) and compression (corto/meco codec, quantization bits for coordinates, normals, colors, textures).

If the output filename ends with `.nxz`, the library automatically performs compression after the build step.

## Building

### Prerequisites

- CMake ≥ 3.16
- C++17 compiler
- [vcpkg](https://github.com/microsoft/vcpkg) with the following packages:
  `zlib`, `libpng`, `libjpeg-turbo`, `stb`, `plog`, `bshoshany-thread-pool`
- [VCGLib](https://github.com/cnr-isti-vclab/vcglib) (auto-detected from `../vcglib` or pass `-DVCGDIR=...`)

### Windows (PowerShell)

```powershell
./build.ps1                 # Release build
./build.ps1 -WithTests      # Include unit tests
./build.ps1 -Debug           # Debug build
```

### Linux / macOS

```bash
./build.sh                  # Release build
./build.sh --with-tests     # Include unit tests
./build.sh --debug          # Debug build
```

### Manual CMake

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON

cmake --build build --config Release
```

Output: `build/src/Release/libnexus.lib` (Windows) or `build/src/liblibnexus.a` (Linux/macOS).

## Tests

Unit tests use [Google Test](https://github.com/google/googletest) and require additional vcpkg packages: `gtest`, `cpr`, `minizip-ng`.

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build -C Release --output-on-failure
```

## Project Structure

```
src/
  common/        Core library: file I/O, image, data structures, nxs API
  nxsbuild/      Mesh loading, KD-tree, multiresolution builder
  nxsedit/       Extraction / compression
  nxszip/        Low-level mesh encoding/decoding (corto/meco)
  corto/         Corto compression library (submodule)
  cmake/         CMake packaging helpers
tests/           Google Test unit tests
```

## Original Project

Based on [Nexus](http://vcg.isti.cnr.it/nexus/) by [Visual Computing Laboratory](http://vcg.isti.cnr.it) - ISTI - CNR.

### Publications

[Multiresolution and fast decompression for optimal web-based rendering](http://vcg.isti.cnr.it/Publications/2016/PD16/FastDec_Ponchio.pdf)
Federico Ponchio, Matteo Dellepiane
Graphical Models, Volume 88, pp. 1-11, November 2016

[Fast decompression for web-based view-dependent 3D rendering](http://vcg.isti.cnr.it/Publications/2015/PD15/Ponchio_Compressed.pdf)
Federico Ponchio, Matteo Dellepiane
Web3D 2015. Proceedings of the 20th International Conference on 3D Web Technology, pp. 199-207, June 2015

[Multiresolution structures for interactive visualization of very large 3D datasets](http://vcg.isti.cnr.it/~ponchio/download/ponchio_phd.pdf)
Federico Ponchio, PhD Thesis

## License

GPL — see [src/LICENSE](src/LICENSE).



