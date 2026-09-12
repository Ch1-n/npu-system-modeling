#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
GEM5_ROOT="${REPO_ROOT}/third_party/gem5"
GEM5_BIN="${GEM5_ROOT}/build/RISCV_OPT/gem5.opt"
HELLO_BIN="${GEM5_ROOT}/tests/test-progs/hello/bin/riscv/linux/hello"
CONFIG_SCRIPT="${REPO_ROOT}/examples/chapter-01/gem5-systemc-host/config/guest_se.py"
OUTPUT_DIR="${REPO_ROOT}/build/chapter-01"

if [ ! -x "${GEM5_BIN}" ]; then
    printf 'Run scripts/build_gem5.sh first.\n' >&2
    exit 1
fi
if [ ! -x "${HELLO_BIN}" ]; then
    printf 'Missing RISC-V hello binary at %s.\n' "${HELLO_BIN}" >&2
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"

"${GEM5_BIN}" \
    --outdir="${OUTPUT_DIR}/smoke" \
    "${CONFIG_SCRIPT}" --binary "${HELLO_BIN}" \
    >"${OUTPUT_DIR}/smoke.log" 2>&1

if ! grep -q 'Hello world!' "${OUTPUT_DIR}/smoke.log"; then
    cat "${OUTPUT_DIR}/smoke.log"
    printf 'gem5 RISC-V smoke test failed.\n' >&2
    exit 1
fi

"${GEM5_BIN}" \
    --outdir="${OUTPUT_DIR}" \
    --dump-config="config.ini" \
    "${CONFIG_SCRIPT}" --binary "${HELLO_BIN}" --generate-only \
    >"${OUTPUT_DIR}/config-generation.log" 2>&1

printf 'PASS gem5 Hello World\n'
printf 'Generated %s\n' "${OUTPUT_DIR}/config.ini"
