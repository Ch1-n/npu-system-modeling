#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
GEM5_ROOT="${REPO_ROOT}/third_party/gem5"
PATCH_FILE="${REPO_ROOT}/examples/chapter-02/patches/gem5-custom-0-npu-add.patch"

if [ ! -f "${GEM5_ROOT}/SConstruct" ]; then
    printf 'Missing gem5 source tree at %s. Run scripts/setup_gem5.sh first.\n' \
        "${GEM5_ROOT}" >&2
    exit 1
fi

DECODER="${GEM5_ROOT}/src/arch/riscv/isa/decoder.isa"

if rg -q "Teaching-only NPU instructions" "${DECODER}"; then
    printf 'Chapter 2 gem5 patch is already applied.\n'
elif patch --dry-run -p1 -d "${GEM5_ROOT}" < "${PATCH_FILE}" >/dev/null 2>&1; then
    patch -p1 -d "${GEM5_ROOT}" < "${PATCH_FILE}"
    printf 'Applied chapter 2 gem5 patch.\n'
else
    printf 'Cannot apply chapter 2 gem5 patch; inspect the gem5 version and local changes.\n' >&2
    exit 1
fi
