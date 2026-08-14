# APX-331 — verify recorded suite results and confirm triage closure

Audit only. Did not re-run the suite. Did not re-do APX-325 or APX-326.
Opened **no** follow-up tasks.

**suite green, pin correct**

## (1) Recorded suite results — commit `945dd9b` (APX-326)

Source: `results/APX-326/` (`REPORT.md`, `results.json`, `ctest-summary.log`, `ctest-full.junit.xml`).

| Layer | Ran | Passed | Failed | Skipped / ignored |
| --- | ---: | ---: | ---: | ---: |
| CTest targets | 8 | 8 | 0 | 0 |
| Unity `sk-tests` | 852 | 852 | 0 | 0 |
| Unity `sk-integration-tests` | 60 | 60 | 0 | 0 |

CTest line: `100% tests passed, 0 tests failed out of 8` in 18.09s (Linux Debug, GCC 13.3, lavapipe).

All eight CTest names passed: `sk-compression-conformance`, `sk-physics-scene`, `sk-msdf-atlas-smoke`, `msdf-atlas-c-smoke`, `sk-no-statics-guard`, `sk-tests`, `sk-integration-tests`, `sk-text-screenshot`.

Failing test names: **none**.

The run is complete (not partial, aborted, or inconclusive). Notes already classified as non-failures (intentional `ui-image-structure` helper FAIL logs, `SK_RUN_INTEGRATION=0` skip inside the unit host, mimalloc / clay warnings) stay closed.

## (2) Submodule pin — commit `624d1ef` (APX-325)

`git ls-tree 624d1ef thirdparty/skore` vs live `origin/v2` after `git fetch origin v2` in the sibling `skore` checkout:

| Ref | SHA |
| --- | --- |
| gitlink at `624d1ef` (`thirdparty/skore`) | `5c7a3da1b84ffb5fd862f108e21353078c227e7a` |
| `origin/v2` tip (after fetch) | `5c7a3da1b84ffb5fd862f108e21353078c227e7a` |

They **match**. `v2` has **not** advanced since the pin. HEAD of this branch still carries the same gitlink.

## (3) Closure

suite green, pin correct

No product defects and no harness/path/assertion issues. Existing merge-safe lessons (do not copy a v2 TU onto the goal branch; keep Windows/clang-tidy and path-separator assertions merge-safe; PR CI builds the merge into v2) do not trigger new work.
