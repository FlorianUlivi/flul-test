#!/bin/bash
# Wrapper for clang-tidy that adds macOS SDK path for Homebrew LLVM compatibility

exec clang-tidy "$@" --extra-arg=-isysroot --extra-arg="$(xcrun --show-sdk-path)"
