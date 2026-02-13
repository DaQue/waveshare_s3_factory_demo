#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DB_PATH="${BUILD_DIR}/compile_commands.json"
CLANG_DB_DIR="${BUILD_DIR}/clang-tidy"
CLANG_DB_PATH="${CLANG_DB_DIR}/compile_commands.json"

if [[ ! -f "${DB_PATH}" ]]; then
    echo "Missing ${DB_PATH}."
    echo "Build once first (for example: source <esp-idf>/export.sh && idf.py build)."
    exit 1
fi

if command -v clang-tidy >/dev/null 2>&1; then
    CLANG_TIDY_BIN="$(command -v clang-tidy)"
else
    CLANG_TIDY_BIN="$(
        find "${HOME}/.espressif/tools/esp-clang" -type f -name clang-tidy 2>/dev/null \
            | sort \
            | tail -n 1
    )"
fi

if [[ -z "${CLANG_TIDY_BIN:-}" || ! -x "${CLANG_TIDY_BIN}" ]]; then
    echo "clang-tidy not found."
    echo "Install Espressif clang tools, then retry."
    exit 1
fi

cd "${ROOT_DIR}"

# ESP-IDF adds GCC-only flags that clang-tidy cannot parse; strip known offenders.
mkdir -p "${CLANG_DB_DIR}"
jq '
    map(
        if has("command") then
            .command |= (
                gsub(" -fno-shrink-wrap"; "") |
                gsub(" -fno-tree-switch-conversion"; "") |
                gsub(" -fstrict-volatile-bitfields"; "") |
                gsub(" -mdisable-hardware-atomics"; "") |
                gsub(" -Werror=all"; "") |
                gsub(" -Werror "; " ") |
                sub(" -Werror$"; "")
            )
        else
            .
        end
    )
' "${DB_PATH}" > "${CLANG_DB_PATH}"

XTENSA_GPP="$(
    find "${HOME}/.espressif/tools/xtensa-esp-elf" -type f -name 'xtensa-esp32s3-elf-g++' 2>/dev/null \
        | sort \
        | tail -n 1
)"

if [[ $# -eq 0 ]]; then
    mapfile -d '' FILES < <(find main -type f \( -name '*.c' -o -name '*.cpp' \) -print0)
else
    FILES=("$@")
fi

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "No C/C++ files matched."
    exit 0
fi

echo "Using: ${CLANG_TIDY_BIN}"
echo "Database: ${CLANG_DB_PATH}"

EXTRA_ARGS=()
if [[ -n "${XTENSA_GPP}" ]]; then
    XTENSA_BIN_DIR="$(dirname "${XTENSA_GPP}")"
    XTENSA_TOOL_ROOT="$(cd "${XTENSA_BIN_DIR}/.." && pwd)"
    XTENSA_SYSROOT="${XTENSA_TOOL_ROOT}/xtensa-esp-elf"
    GCC_VER_DIR="$(
        find "${XTENSA_TOOL_ROOT}/lib/gcc/xtensa-esp-elf" -mindepth 1 -maxdepth 1 -type d 2>/dev/null \
            | sort \
            | tail -n 1
    )"
    GCC_VER="$(basename "${GCC_VER_DIR}")"

    EXTRA_ARGS+=("--extra-arg=--target=xtensa-esp32s3-elf")
    EXTRA_ARGS+=("--extra-arg=--sysroot=${XTENSA_SYSROOT}")

    if [[ -n "${GCC_VER_DIR}" && -d "${GCC_VER_DIR}" ]]; then
        EXTRA_ARGS+=("--extra-arg=-isystem${XTENSA_SYSROOT}/include")
        EXTRA_ARGS+=("--extra-arg=-isystem${XTENSA_SYSROOT}/include/c++/${GCC_VER}")
        EXTRA_ARGS+=("--extra-arg=-isystem${XTENSA_SYSROOT}/include/c++/${GCC_VER}/xtensa-esp-elf")
        EXTRA_ARGS+=("--extra-arg=-isystem${XTENSA_SYSROOT}/include/c++/${GCC_VER}/backward")
        EXTRA_ARGS+=("--extra-arg=-isystem${GCC_VER_DIR}/include")
        EXTRA_ARGS+=("--extra-arg=-isystem${GCC_VER_DIR}/include-fixed")
    fi
fi

"${CLANG_TIDY_BIN}" -p "${CLANG_DB_DIR}" "${EXTRA_ARGS[@]}" "${FILES[@]}"
