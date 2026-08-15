#!/usr/bin/env bash
# APX-361: headless widget automation against the skore goal branch.
#
# Builds (unless --no-build) and runs:
#   1. sk-widget-auto-tests          (unit harness, no GPU)
#   2. sk-tests --filter=ui_author_* (plugin SK_UI_TEST, includes 19 families)
#   3. the 19-family CTest           (sk-widget-family-automation)
#
# Integration / lavapipe PNG suites are NOT run here (and are not added to
# the default unit path). Use scripts/run-ui-integration-tests.sh for those.
#
# Usage (from skore-test-suite root, after configure pointing at the goal
# branch: feature/review-necessary-widgets-for-skore-edito):
#   ./scripts/run-widget-automation.sh
#   BUILD_DIR=build ./scripts/run-widget-automation.sh --no-build
#
# Point the engine at the goal branch first:
#   git -C ../skore checkout feature/review-necessary-widgets-for-skore-edito
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSKORE_DIR=../skore

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
DO_BUILD=1

usage() {
  sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage ;;
    --no-build) DO_BUILD=0; shift ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    *) echo "error: unknown argument '$1'" >&2; usage ;;
  esac
done

if [[ "${BUILD_DIR}" != /* ]]; then
  BUILD_DIR="${ROOT}/${BUILD_DIR}"
fi

BIN_DIR="${BUILD_DIR}/bin"
UNIT_BIN="${BIN_DIR}/sk-tests"
AUTO_BIN="${BIN_DIR}/sk-widget-auto-tests"
# Trailing '*' is a prefix match only (no mid-token glob).
FAMILY_FILTER="ui_author_button_family_*,ui_author_text_family_*,ui_author_checkbox_radio_*,ui_author_input_text_family_*,ui_author_slider_drag_family_*,ui_author_combo_family_*,ui_author_tree_family_*,ui_author_table_family_*,ui_author_tab_bar_family_*,ui_author_menu_family_*,ui_author_popup_modal_family_*,ui_author_child_window_layout_family_*,ui_author_color_family_*,ui_author_image_family_*,ui_author_content_item_family_*,ui_author_drag_drop_family_*,ui_author_selectable_family_*,ui_author_separator_layout_family_*,ui_author_tooltip_family_*"
AUTHOR_FILTER="ui_author_*"

# 19 families: 18 *_family_tests.c plus checkbox_radio_tests.c
FAMILIES=(
  button text checkbox_radio input_text slider_drag combo tree table tab_bar
  menu popup_modal child_window_layout color image content_item drag_drop
  selectable separator_layout tooltip
)

if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "error: build dir '${BUILD_DIR}' missing; configure first:" >&2
  echo "  git -C ../skore checkout feature/review-necessary-widgets-for-skore-edito" >&2
  echo "  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSKORE_DIR=../skore" >&2
  exit 1
fi

if [[ "${DO_BUILD}" -eq 1 ]]; then
  echo "==> Building sk-widget-auto-tests + sk-tests (${BUILD_DIR})"
  cmake --build "${BUILD_DIR}" --target sk-widget-auto-tests sk-tests --parallel
fi

if [[ ! -x "${AUTO_BIN}" ]]; then
  echo "error: ${AUTO_BIN} not found; build sk-widget-auto-tests first" >&2
  exit 1
fi
if [[ ! -x "${UNIT_BIN}" ]]; then
  echo "error: ${UNIT_BIN} not found; build sk-tests first" >&2
  exit 1
fi

echo "==> Widget automation (headless, no GPU)"
echo "    BUILD_DIR=${BUILD_DIR}"
echo "    FAMILY_FILTER=${FAMILY_FILTER}"
echo "    AUTHOR_FILTER=${AUTHOR_FILTER}"

status=0

run_bin() {
  local label="$1"
  shift
  echo ""
  echo "==> ${label}"
  echo "    $*"
  set +e
  (
    cd "${BIN_DIR}"
    "$@"
  )
  local rc=$?
  set -e
  echo "    exit=${rc}"
  if [[ $rc -ne 0 ]]; then
    echo "${label} failed (exit ${rc})" >&2
    status=$rc
  fi
  return 0
}

run_bin "sk-widget-auto-tests" ./sk-widget-auto-tests

echo ""
echo "==> sk-tests --list ${FAMILY_FILTER} (expect all 19 families)"
set +e
list_out="$(
  cd "${BIN_DIR}"
  ./sk-tests --list --filter="${FAMILY_FILTER}"
)"
list_rc=$?
set -e
printf '%s\n' "${list_out}"
if [[ $list_rc -ne 0 ]]; then
  echo "sk-tests --list failed (exit ${list_rc})" >&2
  status=$list_rc
fi

missing=()
for fam in "${FAMILIES[@]}"; do
  if ! printf '%s\n' "${list_out}" | grep -E -q "ui_author_${fam}(_|$)"; then
    missing+=("${fam}")
  fi
done
listed="$(printf '%s\n' "${list_out}" | grep -c '^ui_author_' || true)"
echo "    listed_ui_author=${listed}  families_expected=19  families_missing=${#missing[@]}"
if [[ ${#missing[@]} -ne 0 ]]; then
  echo "error: missing family scenarios: ${missing[*]}" >&2
  echo "    checkout feature/review-necessary-widgets-for-skore-edito (not v2)" >&2
  status=1
fi

run_bin "sk-tests family filter" env SK_TEST_FILTER="${FAMILY_FILTER}" ./sk-tests
run_bin "sk-tests SK_TEST_FILTER=${AUTHOR_FILTER}" env SK_TEST_FILTER="${AUTHOR_FILTER}" ./sk-tests

echo ""
echo "==> ctest -R 'sk-widget-auto-tests|sk-widget-family-automation'"
set +e
ctest --test-dir "${BUILD_DIR}" -R 'sk-widget-auto-tests|sk-widget-family-automation' --output-on-failure
ctest_rc=$?
set -e
echo "    ctest_exit=${ctest_rc}"
if [[ $ctest_rc -ne 0 ]]; then
  status=$ctest_rc
fi

if [[ $status -ne 0 ]]; then
  echo "Widget automation FAILED (exit ${status})" >&2
  exit "$status"
fi
echo "Widget automation OK"
exit 0
