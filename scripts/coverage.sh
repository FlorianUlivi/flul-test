#!/bin/bash
# Generate LLVM source-based coverage report (line + branch).
# Usage: ./scripts/coverage.sh [--open] [--agent]

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/coverage"
PROFILES_DIR="${BUILD_DIR}/profiles"
MERGED="${BUILD_DIR}/merged.profdata"
REPORT_DIR="${BUILD_DIR}/coverage-report"
SELF_TEST="${BUILD_DIR}/self_test"

MODE_AGENT=false
MODE_OPEN=false
for arg in "$@"; do
    case "${arg}" in
        --agent) MODE_AGENT=true ;;
        --open)  MODE_OPEN=true ;;
        *)       echo "Unknown flag: ${arg}" >&2; exit 1 ;;
    esac
done

cmake --preset coverage -S "${ROOT_DIR}"
cmake --build --preset coverage

rm -rf "${PROFILES_DIR}" && mkdir -p "${PROFILES_DIR}"

# All tests discovered via CTest
LLVM_PROFILE_FILE="${PROFILES_DIR}/self_test_%p.profraw" \
    ctest --preset coverage

xcrun llvm-profdata merge --sparse \
    --output="${MERGED}" "${PROFILES_DIR}"/*.profraw

if [[ "${MODE_AGENT}" == false ]]; then
    xcrun llvm-cov show \
        --format=html \
        --instr-profile="${MERGED}" \
        --show-branches=count \
        --show-line-counts-or-regions \
        --output-dir="${REPORT_DIR}" \
        --ignore-filename-regex="(build/|/usr/)" \
        "${SELF_TEST}"
fi

echo ""
xcrun llvm-cov report \
    --instr-profile="${MERGED}" \
    --ignore-filename-regex="(build/|/usr/)" \
    "${SELF_TEST}"

if [[ "${MODE_AGENT}" == true ]]; then
    echo ""
    echo "=== Uncovered Lines ==="
    xcrun llvm-cov export --format=lcov \
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
            # collapse consecutive lines into ranges
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
fi

if [[ "${MODE_AGENT}" == false ]]; then
    echo ""
    echo "HTML report: ${REPORT_DIR}/index.html"
    [[ "${MODE_OPEN}" == true ]] && open "${REPORT_DIR}/index.html"
fi
