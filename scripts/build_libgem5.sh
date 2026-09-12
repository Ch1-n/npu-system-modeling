#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
GEM5_ROOT="${REPO_ROOT}/third_party/gem5"
VENV_ROOT="${REPO_ROOT}/.venv/gem5"
SCONS="${VENV_ROOT}/bin/scons"
PYTHON="${VENV_ROOT}/bin/python"
JOBS="${JOBS:-4}"

if [ ! -x "${SCONS}" ]; then
    printf 'Run scripts/setup_gem5.sh first.\n' >&2
    exit 1
fi

case "$(uname -s)" in
    Darwin) LIB_TARGET="build/RISCV_LIB/libgem5_opt.dylib" ;;
    Linux) LIB_TARGET="build/RISCV_LIB/libgem5_opt.so" ;;
    *) printf 'Unsupported host OS.\n' >&2; exit 1 ;;
esac

if [ -z "${PYTHON_CONFIG:-}" ]; then
    python_base=$("${PYTHON}" -c 'import sys; print(sys.base_prefix)')
    python_version=$("${PYTHON}" -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
    for candidate in \
        "${python_base}/bin/python${python_version}-config" \
        "${python_base}/bin/python3-config" \
        "${python_base}/bin/python-config"; do
        if [ -x "${candidate}" ]; then
            PYTHON_CONFIG="${candidate}"
            break
        fi
    done
fi

if [ -z "${PYTHON_CONFIG:-}" ]; then
    printf 'Unable to locate python-config for %s.\n' "${PYTHON}" >&2
    exit 1
fi

cd "${GEM5_ROOT}"
env PYTHON_CONFIG="${PYTHON_CONFIG}" \
    "${SCONS}" setconfig build/RISCV_LIB \
        BUILD_ISA=y USE_RISCV_ISA=y USE_SYSTEMC=n
env PYTHON_CONFIG="${PYTHON_CONFIG}" \
    "${SCONS}" --with-cxx-config --without-python --without-tcmalloc \
        --duplicate-sources "${LIB_TARGET}" -j"${JOBS}"
