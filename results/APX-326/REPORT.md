# APX-326 — full skore-test-suite run

**Verdict: GREEN.** Follow-up APX-327 can close as a no-op. No product fixes and no harness/assertion fixes are needed from this run.

CTest: **8/8 passed, 0 failed, 0 skipped** in 18.09s.
Unity (inside `sk-tests`): **852 ran, 0 failed, 0 ignored**.
Unity (inside `sk-integration-tests`): **60 ran, 0 failed, 0 ignored**.
Failing test names: **none**.

## What was run

This repo's CI (`.github/workflows/ci.yml`) configures one platform:

| Item | Value |
| --- | --- |
| OS / arch | Linux x86_64 (Ubuntu 24.04, kernel 6.8.0-137-generic) |
| Compiler | GCC 13.3.0 |
| Generator | Ninja 1.11.1 + CMake 3.30.5 |
| Config | `CMAKE_BUILD_TYPE=Debug`, `BUILD_TESTING=ON`, `SK_ENABLE_CLANG_TIDY=OFF` |
| Engine | sibling `../skore` at **v2** `5c7a3da1b84ffb5fd862f108e21353078c227e7a` |
| GPU | Mesa lavapipe (`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`) |
| Integration gate | `SK_RUN_INTEGRATION=1` |

That is the only platform/config this repo already uses. Windows, macOS, and Release are **not** wired in `skore-test-suite` CI (those live in the engine repo). They were not executed here.

Command (after configure + `cmake --build build --parallel`, 478 targets, success):

```bash
ctest --test-dir build --output-on-failure --verbose
```

`sk-isolate-tests` and `sk-repro-cycles` are built but not registered with CTest, so they are outside the configured suite.

## CTest results

| # | Name | Label | Time | Result |
| --- | --- | --- | --- | --- |
| 1 | `sk-compression-conformance` | unit | 0.00s | PASS — 8 mappings, 3 declared intentional gaps |
| 2 | `sk-physics-scene` | physics | 0.40s | PASS — `RESULT: PASS` (stack settle, kinematic rider, character stairs, bit-identical repeat) |
| 3 | `sk-msdf-atlas-smoke` | unit | 0.07s | PASS — 117 glyphs, 64x64 atlas |
| 4 | `msdf-atlas-c-smoke` | (unlabeled) | 0.30s | PASS — all checks passed |
| 5 | `sk-no-statics-guard` | unit | 0.79s | PASS — 121 allowlisted statics, 0 accessor hits |
| 6 | `sk-tests` | unit | 11.30s | PASS — 852 / 0 / 0 (host + 10 plugins) |
| 7 | `sk-integration-tests` | integration | 3.29s | PASS — 60 / 0 / 0 (Vulkan / UI / dock / fixtures) |
| 8 | `sk-text-screenshot` | integration; ui | 1.94s | PASS — 5 MSDF samples byte-deterministic |

## `sk-tests` breakdown

| Registry | Ran | Failed | Ignored |
| --- | --- | --- | --- |
| host (foundation + editor) | 392 | 0 | 0 |
| `sk-render-device.so` | 8 | 0 | 0 |
| `sk-vulkan-render-device.so` | 13 | 0 | 0 |
| `sk-test-render-device.so` | 11 | 0 | 0 |
| `sk-profiler.so` | 43 | 0 | 0 |
| `sk-ui.so` | 168 | 0 | 0 |
| `sk-jolt.so` | 43 | 0 | 0 |
| `sk-entities.so` | 92 | 0 | 0 |
| `sk-platform-window.so` | 13 | 0 | 0 |
| `sk-render-graph.so` | 60 | 0 | 0 |
| `sk-dxc-compiler.so` | 9 | 0 | 0 |
| **TOTAL** | **852** | **0** | **0** |

Name inventories: `sk-tests-names.txt`, `sk-integration-tests-names.txt`.

## Failures and classification

No CTest failures. No Unity failures. No ignored Unity cases.

Notes that are **not** failures (do not open product or harness tasks for these):

1. `sk-tests` prints `SKIP: integration tests (SK_RUN_INTEGRATION=0)`. The unit host does not run the integration registry; those 60 cases ran in `sk-integration-tests`.
2. `sk-ui` logs `[ERROR] [ui-image-structure] ... FAIL` while testing the CPU image-assert helpers with **intentionally wrong** expected values. Unity still reports `168 Tests 0 Failures` for that plugin.
3. One-time `mimalloc` warning in `sk-physics-scene` about falling back from aligned OS allocation. Scene still `RESULT: PASS`.
4. `clay_adapter` WARN about Clay absolute-position mapping during text-screenshot capture. Verify still passed.

## Artifacts in this folder

| File | Contents |
| --- | --- |
| `results.json` | Machine-readable summary for APX-327 |
| `ctest-summary.log` | Condensed CTest / Unity totals and physics/MSDF outcomes |
| `ctest-full.junit.xml` | CTest JUnit (8 testcases, 0 failures) |
| `sk-tests-names.txt` | Host + plugin test names from `sk-tests --list` |
| `sk-integration-tests-names.txt` | Integration case names from `sk-integration-tests --list` |

## Follow-up

**APX-327 (Triage suite failures and open or implement fix tasks): no-op.** Suite is green against the current v2 pin. Do not start product or harness changes from this report.
