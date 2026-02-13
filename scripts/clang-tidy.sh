#!/bin/bash
# Wrapper for clang-tidy that adds macOS SDK path when needed and falls back to
# versioned clang-tidy binaries on Linux.

if [[ "$(uname -s)" == "Darwin" ]]; then
	exec clang-tidy "$@" \
		--extra-arg=-isysroot \
		--extra-arg="$(xcrun --show-sdk-path)"
fi

if command -v clang-tidy >/dev/null 2>&1; then
	exec clang-tidy "$@"
fi

echo "clang-tidy not found in PATH" >&2
exit 127
