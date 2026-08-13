#!/usr/bin/env bash
# Run UI capture / dock / screenshot suites (no grok-vision grades).
# Invokes the binaries directly (not gated). Default `ctest` also runs
# integration; use this script for a focused UI subset.
#
# Usage (from skore-test-suite root, after configure):
#   ./scripts/run-ui-integration-tests.sh
#   BUILD_DIR=build ./scripts/run-ui-integration-tests.sh
#   ./scripts/run-ui-integration-tests.sh --no-build
#   SUITES=dock,text-screenshot ./scripts/run-ui-integration-tests.sh
#
# Suites (comma-separated via SUITES, default: all):
#   interaction  — behavioural engine suite (plugin) + integration tab/menu
#   dock         — docking public-API e2e (integration/ui_dock.c)
#   authoring    — UI test authoring helpers
#   text-screenshot — APX-268 deterministic MSDF text PNG suite
#
# Artifacts: PNGs under $SK_TEST_ARTIFACT_DIR (default: $BUILD_DIR/test-artifacts).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
SUITES="${SUITES:-interaction,dock,authoring,text-screenshot}"
DO_BUILD=1
EXTRA_ARGS=()

usage() {
  sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage ;;
    --no-build) DO_BUILD=0; shift ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --suites) SUITES="$2"; shift 2 ;;
    *) EXTRA_ARGS+=("$1"); shift ;;
  esac
done

# Resolve BUILD_DIR to an absolute path so artifact writes work when cwd is bin/.
if [[ "${BUILD_DIR}" != /* ]]; then
  BUILD_DIR="${ROOT}/${BUILD_DIR}"
fi

BIN_DIR="${BUILD_DIR}/bin"
INTEGRATION_BIN="${BIN_DIR}/sk-integration-tests"
UNIT_BIN="${BIN_DIR}/sk-tests"
if [[ -n "${SK_TEST_ARTIFACT_DIR:-}" ]]; then
  ARTIFACT_DIR="${SK_TEST_ARTIFACT_DIR}"
  if [[ "${ARTIFACT_DIR}" != /* ]]; then
    ARTIFACT_DIR="${ROOT}/${ARTIFACT_DIR}"
  fi
else
  ARTIFACT_DIR="${BUILD_DIR}/test-artifacts"
fi

# Headless Vulkan when no GPU / ICD is configured (Linux Mesa lavapipe).
if [[ -z "${VK_ICD_FILENAMES:-}" ]]; then
  for icd in \
    /usr/share/vulkan/icd.d/lvp_icd.json \
    /usr/share/vulkan/icd.d/lvp_icd.x86_64.json; do
    if [[ -f "$icd" ]]; then
      export VK_ICD_FILENAMES="$icd"
      break
    fi
  done
fi

export SK_TEST_ARTIFACT_DIR="${ARTIFACT_DIR}"
mkdir -p "${ARTIFACT_DIR}"

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "error: build dir '${BUILD_DIR}' missing; configure first:" >&2
  echo "  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON" >&2
  exit 1
fi

if [[ "${DO_BUILD}" -eq 1 ]]; then
  echo "==> Building sk-integration-tests + sk-tests (${BUILD_DIR})"
  cmake --build "${BUILD_DIR}" --target sk-integration-tests sk-tests --parallel
fi

if [[ ! -x "${INTEGRATION_BIN}" ]]; then
  echo "error: ${INTEGRATION_BIN} not found; build sk-integration-tests first" >&2
  exit 1
fi

integration_tokens=()
plugin_tokens=()
IFS=',' read -r -a suite_list <<< "${SUITES}"
for raw in "${suite_list[@]}"; do
  s="$(echo "$raw" | tr '[:upper:]' '[:lower:]' | tr -d '[:space:]')"
  case "$s" in
    "" ) ;;
    interaction|ix|engine)
      integration_tokens+=("ui_ix_*")
      plugin_tokens+=("ui_author_ix_*")
      ;;
    dock|docking|ui-dock|ui_dock)
      integration_tokens+=("ui_dock_*")
      ;;
    authoring|author)
      integration_tokens+=("ui_author_*")
      plugin_tokens+=("ui_author_*")
      ;;
    text-screenshot|textshot|screenshot)
      integration_tokens+=("ui_text_screenshot_*")
      ;;
    all)
      integration_tokens+=(
        "ui_ix_*"
        "ui_author_*"
        "ui_dock_*"
        "ui_text_screenshot_*"
      )
      plugin_tokens+=("ui_author_ix_*")
      ;;
    *)
      integration_tokens+=("$s")
      ;;
  esac
done

dedup_join() {
  local seen="|" out="" t
  for t in "$@"; do
    [[ -z "$t" ]] && continue
    case "$seen" in
      *"|$t|"*) continue ;;
    esac
    seen="${seen}${t}|"
    if [[ -z "$out" ]]; then
      out="$t"
    else
      out="${out},${t}"
    fi
  done
  printf '%s' "$out"
}

INTEGRATION_FILTER="$(dedup_join "${integration_tokens[@]}")"
PLUGIN_FILTER="$(dedup_join "${plugin_tokens[@]}")"

if [[ -z "${INTEGRATION_FILTER}" && -z "${PLUGIN_FILTER}" ]]; then
  echo "error: no suites selected (SUITES=${SUITES})" >&2
  exit 1
fi

echo "==> UI integration suites"
echo "    BUILD_DIR=${BUILD_DIR}"
echo "    SUITES=${SUITES}"
echo "    SK_TEST_ARTIFACT_DIR=${SK_TEST_ARTIFACT_DIR}"
if [[ -n "${VK_ICD_FILENAMES:-}" ]]; then
  echo "    VK_ICD_FILENAMES=${VK_ICD_FILENAMES}"
else
  echo "    VK_ICD_FILENAMES=(unset — GPU capture tests may IGNORE without an ICD)"
fi

status=0

if [[ -n "${INTEGRATION_FILTER}" ]]; then
  echo ""
  echo "==> sk-integration-tests  SK_TEST_FILTER=${INTEGRATION_FILTER}"
  set +e
  (
    cd "${BIN_DIR}"
    export SK_TEST_FILTER="${INTEGRATION_FILTER}"
    # shellcheck disable=SC2086
    ./sk-integration-tests ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
  )
  rc=$?
  set -e
  if [[ $rc -ne 0 ]]; then
    echo "sk-integration-tests failed (exit ${rc})" >&2
    status=$rc
  fi
fi

if [[ -n "${PLUGIN_FILTER}" ]]; then
  if [[ ! -x "${UNIT_BIN}" ]]; then
    echo "error: ${UNIT_BIN} not found; build sk-tests for the interaction suite" >&2
    exit 1
  fi
  echo ""
  echo "==> sk-tests (plugin interaction)  SK_TEST_FILTER=${PLUGIN_FILTER}"
  set +e
  (
    cd "${BIN_DIR}"
    export SK_TEST_FILTER="${PLUGIN_FILTER}"
    ./sk-tests
  )
  rc=$?
  set -e
  if [[ $rc -ne 0 ]]; then
    echo "sk-tests (interaction) failed (exit ${rc})" >&2
    status=$rc
  fi
fi

echo ""
echo "==> Artifacts under ${ARTIFACT_DIR}"
if command -v find >/dev/null 2>&1; then
  find "${ARTIFACT_DIR}" -type f -name '*.png' 2>/dev/null | sort | head -n 40 || true
fi

if [[ $status -ne 0 ]]; then
  echo "UI integration suites FAILED (exit ${status})" >&2
  exit "$status"
fi
echo "UI integration suites OK"
exit 0
