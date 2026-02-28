#!/bin/bash
# Generate LLVM source-based coverage report (line + branch).
# Usage: ./scripts/coverage.sh [--open] [--agent] [--save-baseline] [--quiet-ok]
#
# Modes (only one agent-side mode should be used at a time):
#   --agent          Full table + uncovered lines (existing agent mode)
#   --save-baseline  Run coverage, save TOTAL line%/branch% to .coverage-baseline
#   --quiet-ok       Run coverage, compare to .coverage-baseline; 1 line if OK,
#                    full table + uncovered lines if regression >1%
#   --open           Generate and optionally open HTML report (human mode)

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/coverage"
PROFILES_DIR="${BUILD_DIR}/profiles"
MERGED="${BUILD_DIR}/merged.profdata"
REPORT_DIR="${BUILD_DIR}/coverage-report"
SELF_TEST="${BUILD_DIR}/self_test"
BASELINE_FILE="${ROOT_DIR}/.coverage-baseline"

MODE_AGENT=false
MODE_OPEN=false
MODE_SAVE_BASELINE=false
MODE_QUIET_OK=false
for arg in "$@"; do
    case "${arg}" in
        --agent)          MODE_AGENT=true ;;
        --open)           MODE_OPEN=true ;;
        --save-baseline)  MODE_SAVE_BASELINE=true ;;
        --quiet-ok)       MODE_QUIET_OK=true ;;
        *)                echo "Unknown flag: ${arg}" >&2; exit 1 ;;
    esac
done

if [[ "$(uname -s)" == "Darwin" ]]; then
    LLVM_PROFDATA=(xcrun llvm-profdata)
    LLVM_COV=(xcrun llvm-cov)
    OPEN_CMD=(open)
else
    if ! command -v llvm-profdata >/dev/null 2>&1; then
        echo "llvm-profdata not found in PATH" >&2
        exit 127
    fi
    if ! command -v llvm-cov >/dev/null 2>&1; then
        echo "llvm-cov not found in PATH" >&2
        exit 127
    fi
    LLVM_PROFDATA=(llvm-profdata)
    LLVM_COV=(llvm-cov)

    if command -v xdg-open >/dev/null 2>&1; then
        OPEN_CMD=(xdg-open)
    else
        OPEN_CMD=()
    fi
fi

cmake --preset coverage -S "${ROOT_DIR}"
cmake --build --preset coverage

rm -rf "${PROFILES_DIR}" && mkdir -p "${PROFILES_DIR}"

# All tests discovered via CTest
LLVM_PROFILE_FILE="${PROFILES_DIR}/self_test_%p.profraw" \
    ctest --preset coverage

"${LLVM_PROFDATA[@]}" merge --sparse \
    --output="${MERGED}" "${PROFILES_DIR}"/*.profraw

# Print uncovered lines to stdout (shared by --agent and --quiet-ok regression path)
print_uncovered() {
    "${LLVM_COV[@]}" export --format=lcov \
        --instr-profile="${MERGED}" \
        --ignore-filename-regex="(build/|/usr/)" \
        "${SELF_TEST}" \
    | awk -v root="${ROOT_DIR}/" '
        /^SF:/ {
            file = substr($0, 4)
            sub("^" root, "", file)
            delete uncov
            n = 0
        }
        /^DA:/ {
            split(substr($0, 4), parts, ",")
            if (parts[2] + 0 == 0) {
                uncov[++n] = parts[1] + 0
            }
        }
        /^end_of_record/ {
            if (n == 0) next
            result = ""
            start = uncov[1]; prev = start
            for (i = 2; i <= n; i++) {
                if (uncov[i] == prev + 1) {
                    prev = uncov[i]
                } else {
                    result = result (result == "" ? "" : ", ")
                    result = result (start == prev ? start : start "-" prev)
                    start = uncov[i]; prev = start
                }
            }
            result = result (result == "" ? "" : ", ")
            result = result (start == prev ? start : start "-" prev)
            print file ": " result
        }
    '
}

# Generate text report
REPORT=$("${LLVM_COV[@]}" report \
    --instr-profile="${MERGED}" \
    --ignore-filename-regex="(build/|/usr/)" \
    "${SELF_TEST}")

# Extract TOTAL line% and branch% from report
# llvm-cov report TOTAL columns (left to right): TOTAL Regions MissedRegions RegionPct
#   Functions MissedFunctions FunctionPct Lines MissedLines LinePct Branches MissedBranches BranchPct
LINE_PCT=$(echo "${REPORT}" | awk '/^TOTAL/ { gsub(/%/, ""); print $(NF-3) }')
BRANCH_PCT=$(echo "${REPORT}" | awk '/^TOTAL/ { gsub(/%/, ""); print $NF }')

# --save-baseline: write totals and exit
if [[ "${MODE_SAVE_BASELINE}" == true ]]; then
    echo "line=${LINE_PCT}" > "${BASELINE_FILE}"
    echo "branch=${BRANCH_PCT}" >> "${BASELINE_FILE}"
    echo "coverage-baseline: saved (line: ${LINE_PCT}%, branch: ${BRANCH_PCT}%)"
    exit 0
fi

# --quiet-ok: compare to baseline; 1 line on success, full report on regression
if [[ "${MODE_QUIET_OK}" == true ]]; then
    if [[ ! -f "${BASELINE_FILE}" ]]; then
        echo "coverage-check: no baseline found — run 'just coverage-baseline' first" >&2
        exit 1
    fi
    BASE_LINE=$(grep '^line=' "${BASELINE_FILE}" | cut -d= -f2)
    BASE_BRANCH=$(grep '^branch=' "${BASELINE_FILE}" | cut -d= -f2)
    REGRESSION=$(awk -v cur_l="${LINE_PCT}" -v base_l="${BASE_LINE}" \
                     -v cur_b="${BRANCH_PCT}" -v base_b="${BASE_BRANCH}" '
        BEGIN {
            line_drop   = base_l + 0 - (cur_l + 0)
            branch_drop = base_b + 0 - (cur_b + 0)
            print (line_drop > 1.0 || branch_drop > 1.0) ? "yes" : "no"
        }')
    if [[ "${REGRESSION}" == "no" ]]; then
        echo "coverage-check: OK (line: ${LINE_PCT}%, branch: ${BRANCH_PCT}%)"
        exit 0
    fi
    echo "coverage-check: REGRESSION (line: ${LINE_PCT}% [was ${BASE_LINE}%], branch: ${BRANCH_PCT}% [was ${BASE_BRANCH}%])"
    echo ""
    echo "${REPORT}"
    echo ""
    echo "=== Uncovered Lines ==="
    print_uncovered
    exit 1
fi

# --open: generate HTML report
if [[ "${MODE_OPEN}" == true ]]; then
    "${LLVM_COV[@]}" show \
        --format=html \
        --instr-profile="${MERGED}" \
        --show-branches=count \
        --show-line-counts-or-regions \
        --output-dir="${REPORT_DIR}" \
        --ignore-filename-regex="(build/|/usr/)" \
        "${SELF_TEST}"
fi

echo ""
echo "${REPORT}"

if [[ "${MODE_AGENT}" == true ]]; then
    echo ""
    echo "=== Uncovered Lines ==="
    print_uncovered
fi

if [[ "${MODE_OPEN}" == true ]]; then
    echo ""
    echo "HTML report: ${REPORT_DIR}/index.html"
    if [[ "${#OPEN_CMD[@]}" -gt 0 ]]; then
        "${OPEN_CMD[@]}" "${REPORT_DIR}/index.html"
    fi
fi
