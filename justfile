set shell := ["bash", "-euo", "pipefail", "-c"]

default:
    @just --list

build preset="debug":
    #!/usr/bin/env bash
    set -euo pipefail
    ROOT="$(git rev-parse --show-toplevel)"
    CACHE="${ROOT}/build/{{preset}}/CMakeCache.txt"
    if [[ ! -f "${CACHE}" ]]; then
        cmake --preset {{preset}} -S "${ROOT}" 2>&1
    fi
    OUTPUT="$(cmake --build --preset {{preset}} 2>&1)"
    STATUS=$?
    if [[ ${STATUS} -ne 0 ]]; then
        echo "${OUTPUT}"
        exit ${STATUS}
    fi

fmt:
    #!/usr/bin/env bash
    set -euo pipefail
    FILES=()
    while IFS= read -r f; do
        [[ -n "${f}" ]] && [[ -f "${f}" ]] && FILES+=("${f}")
    done < <(
        git status --porcelain \
        | awk '{print $2}' \
        | grep -E '\.(cpp|hpp)$' \
        | sort -u
    )
    if [[ ${#FILES[@]} -eq 0 ]]; then
        exit 0
    fi
    CHANGED=()
    for f in "${FILES[@]}"; do
        if ! clang-format --dry-run --Werror "${f}" > /dev/null 2>&1; then
            clang-format -i "${f}"
            CHANGED+=("${f}")
        fi
    done
    if [[ ${#CHANGED[@]} -gt 0 ]]; then
        echo "fmt: reformatted ${CHANGED[*]}"
    fi

lint: (build)
    #!/usr/bin/env bash
    set -euo pipefail
    ROOT="$(git rev-parse --show-toplevel)"
    DB="${ROOT}/build/debug/compile_commands.json"
    if [[ ! -f "${DB}" ]]; then
        echo "lint: compile_commands.json not found — run 'just build' first" >&2
        exit 1
    fi
    FILES=()
    while IFS= read -r f; do
        FULL="${ROOT}/${f}"
        [[ -n "${f}" ]] && [[ -f "${FULL}" ]] && FILES+=("${FULL}")
    done < <(
        git status --porcelain \
        | awk '{print $2}' \
        | grep -E '\.(cpp|hpp)$' \
        | sort -u
    )
    if [[ ${#FILES[@]} -eq 0 ]]; then
        exit 0
    fi
    "${ROOT}/scripts/clang-tidy.sh" -p "${ROOT}/build/debug" "${FILES[@]}"

test preset="debug": (build preset)
    #!/usr/bin/env bash
    set -euo pipefail
    OUTPUT="$(ctest --preset {{preset}} --output-on-failure 2>&1)"
    STATUS=$?
    if [[ ${STATUS} -ne 0 ]]; then
        echo "${OUTPUT}"
        exit ${STATUS}
    fi
    SUMMARY=$(echo "${OUTPUT}" | grep -E "% tests passed" | tail -1 || true)
    echo "test: ${SUMMARY:-passed}"

check: (build) fmt lint (test)

coverage-baseline:
    @./scripts/coverage.sh --agent --save-baseline

coverage-check:
    @./scripts/coverage.sh --agent --quiet-ok
