#!/usr/bin/env bash
set -euo pipefail

# Resolve the project directory so this script works from any directory.
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "$project_dir"

if (( $# > 1 )); then
  printf 'Usage: bash %s [Debug|Release]\n' "$0" >&2
  exit 2
fi

preset="${1:-Debug}"
case "$preset" in
  Debug|Release) ;;
  *)
    printf 'Unknown build preset: %s (expected Debug or Release)\n' "$preset" >&2
    exit 2
    ;;
esac

cmake --preset "$preset"
cmake --build --preset "$preset" --parallel

printf '\nFirmware: %s/build/%s/nucleo_arcade.elf\n' "$project_dir" "$preset"
