#!/usr/bin/env bash

set -euo pipefail

GEM5_VERSION="v25.1.0.1"
SCONS_VERSION="4.10.1"
REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
GEM5_ROOT="${REPO_ROOT}/third_party/gem5"
VENV_ROOT="${REPO_ROOT}/.venv/gem5"

select_python() {
    if [ -n "${PYTHON_BIN:-}" ]; then
        printf '%s\n' "${PYTHON_BIN}"
        return
    fi

    for candidate in python3.13 python3.12 python3; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            command -v "${candidate}"
            return
        fi
    done
    return 1
}

mkdir -p "${REPO_ROOT}/third_party"
if [ ! -f "${GEM5_ROOT}/SConstruct" ]; then
    archive="${TMPDIR:-/tmp}/gem5-${GEM5_VERSION}.tar.gz"
    extracted="${REPO_ROOT}/third_party/gem5-${GEM5_VERSION#v}"
    curl -L --fail --retry 3 \
        -o "${archive}" \
        "https://codeload.github.com/gem5/gem5/tar.gz/refs/tags/${GEM5_VERSION}"
    tar -xzf "${archive}" -C "${REPO_ROOT}/third_party"
    mv "${extracted}" "${GEM5_ROOT}"
fi

printf 'Existing gem5 trees are reused; verify their version and local patches before claiming a clean build.\n'

PYTHON=$(select_python) || {
    printf 'Install Python 3.12 or 3.13.\n' >&2
    exit 1
}
"${PYTHON}" -c 'import sys; assert sys.version_info[:2] in ((3, 12), (3, 13)), "Use Python 3.12 or 3.13"'

if [ ! -x "${VENV_ROOT}/bin/python" ]; then
    "${PYTHON}" -m venv "${VENV_ROOT}"
fi
"${VENV_ROOT}/bin/python" -c 'import sys; assert sys.version_info[:2] in ((3, 12), (3, 13)), "Existing venv must use Python 3.12 or 3.13"'
"${VENV_ROOT}/bin/python" -m pip install --upgrade pip "scons==${SCONS_VERSION}"

printf 'gem5 source: %s\n' "${GEM5_ROOT}"
printf 'Python: %s\n' "$("${VENV_ROOT}/bin/python" --version)"
printf 'SCons: %s\n' "$("${VENV_ROOT}/bin/scons" --version | sed -n '1p')"
