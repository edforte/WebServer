# webserv

This is when you finally understand why URLs start with HTTP

https://datatracker.ietf.org/doc/html/rfc1945

## Development

### Code Formatting

This repository uses `clang-format` for consistent code style. To format all C++ files in a pull request, simply comment `/format` on the PR. The GitHub Action will automatically format the code and push the changes to your branch.


## Building (preferred: CMake)

The project uses **CMake** as the source of truth. Prefer using CMake for local development and CI; it supports out-of-source builds, parallel compilation, and running tests.

Recommended out-of-source workflow:

```bash
# Configure the build directory in Release mode
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build (parallel)
cmake --build build -- -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure -j$(nproc)
```

One-line configure + build:

```bash
mkdir -p build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -- -j$(nproc)
```

For a Debug build, set `-DCMAKE_BUILD_TYPE=Debug`.

If you prefer to use the older `cmake ..` style from inside the `build/` directory, that still works:

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j$(nproc)
```


### Makefile (fallback for non-CMake users)

A top-level `Makefile` is generated from CMake and committed to the repository so users without CMake can still build the project. This Makefile is treated as a fallback; CMake is recommended for development.

Basic `Makefile` targets (fallback):

```bash
make        # Build the project
make clean  # Remove build artifacts
make fclean # Remove all generated files
make re     # Rebuild from scratch
```

For detailed configuration documentation, see [docs/CONFIGURATION.md](docs/CONFIGURATION.md).

### Regenerating the Makefile

The Makefile is generated from `CMakeLists.txt` using the `generate-makefile` target. Automatic regeneration is supported when CMake is present.

Automatic regeneration: the generated `Makefile` embeds a small stamp-based rule so `make` will automatically regenerate the top-level `Makefile` when important CMake files change (for example `CMakeLists.txt`, `Makefile.in`, and other `*.cmake` files). The stamp file used is `build/.cmake_stamp`.

**Note:** Automatic regeneration only works if CMake is installed and available in your environment. If CMake is not available, a warning is displayed but the build continues using the committed Makefile.

Manual regeneration options:

```bash
# Run the CMake generator directly
cmake -B build
cmake --build build --target generate-makefile

# Or use the convenience make target (embedded in the generated `Makefile`):
make regenerate        # alias for regenerate-stamp
make regenerate-stamp  # force regeneration via the stamp target
```

After regeneration, if you want to keep the generated `Makefile` in the repo, commit the updated file.


### Build System Files

- `CMakeLists.txt`: Source of truth for the build system
- `Makefile`: Auto-generated from `CMakeLists.txt` (committed for users without CMake)
- `cmake/generate_makefile.cmake`: Script that generates the `Makefile` from `CMakeLists.txt`
- `cmake/Makefile.in`: Template used to generate the top-level `Makefile`

The Makefile is automatically generated from `CMakeLists.txt`, so there's no risk of them getting out of sync as long as CMake is available for regeneration.

## Running

Run the server with a configuration file:

```bash
./webserv [config_file]
```

Set the log level at runtime:
```bash
./webserv -l:0           # Use default config with DEBUG level
./webserv -l:1 my.conf   # Use custom config with INFO level
```

Log levels: 0=DEBUG, 1=INFO, 2=ERROR

If no config file is specified, the default is `conf/default.conf`.
