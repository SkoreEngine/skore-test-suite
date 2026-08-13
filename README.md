# skore-test-suite

Integration and standalone engine tests for [skore](https://github.com/SkoreEngine/skore) (`v2`).

Unit tests stay in the engine (`sk-tests.exe` — foundation + plugins). This repo holds the cases that used to live under `skore/tests/` (compression conformance, Vulkan / UI capture / fixtures / physics scene / MSDF smoke).

Harness code (`ui_capture_harness`, text-screenshot, fixture loaders, integration `main.c`) stays in `skore/tests/` so later task runners can reuse it. Vulkan capture writes PNGs; there is no grok-vision grader.

## 1. Get the engine

```bash
git submodule update --init --recursive
git -C thirdparty/skore checkout v2
```

Or point at a local checkout:

```bash
cmake -S . -B build -G Ninja -DSKORE_DIR=../skore
```

If `../skore` exists, CMake uses it automatically.

## 2. Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

This also builds engine `sk-tests` (unit host) next to the integration binaries in `build/bin`.

## 3. Run

```bash
# everything (unit host + compression + integration)
ctest --test-dir build --output-on-failure

# unit host (foundation + plugins) — also built here
./build/bin/sk-tests

# compression surface (header compile-check + mapping table)
./build/bin/sk-compression-conformance

# integration only (Vulkan / UI capture / text screenshots)
ctest --test-dir build -L integration --output-on-failure
# or:
./scripts/run-integration-tests.sh
./scripts/run-ui-integration-tests.sh

# skip integration under ctest
cmake -E env SK_RUN_INTEGRATION=0 ctest --test-dir build --output-on-failure
```

About 2 minutes to configure + build if the engine is already warm; first configure is longer (engine + thirdparty).
