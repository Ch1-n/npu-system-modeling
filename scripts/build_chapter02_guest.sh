#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT_DIR="${REPO_ROOT}/build/chapter-02/guest"
BUILDER="${REPO_ROOT}/tools/build_chapter02_guest.py"

mkdir -p "${OUT_DIR}"
python3 "${BUILDER}" "${OUT_DIR}/npu_add"

printf 'Built %s\n' "${OUT_DIR}/npu_add"
