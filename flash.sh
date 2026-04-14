#!/usr/bin/env bash
# Compila el firmware y lo sube a la RP2040 (Zero u otra con UF2).
# Uso:
#   ./flash.sh              # compila y, si puede, flashea con picotool
#   ./flash.sh --build-only # solo compila
#
# Flasheo: pon la placa en modo BOOTSEL (USB) y ejecuta este script;
# si picotool no está, copia manualmente el .uf2 que indica al final.

set -euo pipefail

# CMake/Ninja del extension pack de Pico si no están en PATH
if ! command -v cmake >/dev/null 2>&1; then
  shopt -s nullglob
  for d in "${HOME}/.pico-sdk/cmake/"*/bin; do
    [[ -x "${d}/cmake" ]] && PATH="${d}:${PATH}" && break
  done
  shopt -u nullglob
fi
if ! command -v ninja >/dev/null 2>&1; then
  shopt -s nullglob
  for n in "${HOME}/.pico-sdk/ninja/"*/ninja; do
    [[ -x "$n" ]] && PATH="$(dirname "$n"):${PATH}" && break
  done
  shopt -u nullglob
fi
if ! command -v picotool >/dev/null 2>&1; then
  shopt -s nullglob
  for p in "${HOME}/.pico-sdk/picotool/"*/picotool/picotool; do
    [[ -x "$p" ]] && PATH="$(dirname "$p"):${PATH}" && break
  done
  shopt -u nullglob
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${ROOT}/build"
UF2="${BUILD}/CardKB-USB.uf2"

BUILD_ONLY=0
for arg in "$@"; do
  case "$arg" in
    --build-only) BUILD_ONLY=1 ;;
  esac
done

if [[ -z "${PICO_SDK_PATH:-}" ]]; then
  if [[ -f "${HOME}/.pico-sdk/sdk/2.2.0/pico_sdk_init.cmake" ]]; then
    export PICO_SDK_PATH="${HOME}/.pico-sdk/sdk/2.2.0"
  elif [[ -f "${HOME}/pico/pico-sdk/pico_sdk_init.cmake" ]]; then
    export PICO_SDK_PATH="${HOME}/pico/pico-sdk"
  else
    echo "Falta PICO_SDK_PATH. Ejemplo:" >&2
    echo "  export PICO_SDK_PATH=\$HOME/pico/pico-sdk" >&2
    exit 1
  fi
fi
echo "PICO_SDK_PATH=${PICO_SDK_PATH}"

cmake -S "${ROOT}" -B "${BUILD}" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD}" -j"$(nproc 2>/dev/null || echo 4)"

echo ""
echo "Firmware: ${UF2}"
ls -la "${UF2}"

if [[ "${BUILD_ONLY}" -eq 1 ]]; then
  exit 0
fi

if command -v picotool >/dev/null 2>&1; then
  echo ""
  echo "Intentando flashear con picotool (placa en modo BOOTSEL por USB)..."
  if picotool load "${UF2}" 2>/dev/null; then
    echo "Listo. Reiniciando..."
    picotool reboot 2>/dev/null || true
    exit 0
  fi
  echo "picotool no pudo cargar (¿BOOTSEL? ¿permisos udev?). Copia el UF2 a mano."
else
  echo ""
  echo "picotool no está en PATH. Instálalo o copia el .uf2:"
  echo "  1) Mantén BOOTSEL al enchufar USB (o pulsa reset+BOOTSEL)."
  echo "  2) Arrastra ${UF2} a la unidad RPI-RP2."
fi
