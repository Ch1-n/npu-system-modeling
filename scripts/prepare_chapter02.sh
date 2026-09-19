#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
GEM5_ROOT="${REPO_ROOT}/third_party/gem5"
GEM5_BIN="${GEM5_ROOT}/build/RISCV_OPT/gem5.opt"
CONFIG_SCRIPT="${REPO_ROOT}/examples/chapter-01/gem5-systemc-host/config/guest_se.py"
GUEST_BIN="${REPO_ROOT}/build/chapter-02/guest/npu_add"
OUTPUT_DIR="${REPO_ROOT}/build/chapter-02"

if [ ! -x "${GEM5_BIN}" ]; then
    printf 'Run scripts/build_gem5.sh first.\n' >&2
    exit 1
fi

"${REPO_ROOT}/scripts/build_chapter02_guest.sh"
mkdir -p "${OUTPUT_DIR}"

"${GEM5_BIN}" \
    --outdir="${OUTPUT_DIR}" \
    --dump-config="config.ini" \
    "${CONFIG_SCRIPT}" --binary "${GUEST_BIN}" --generate-only

printf 'Prepared %s\n' "${OUTPUT_DIR}/config.ini"
