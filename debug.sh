#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
elf_path="${project_dir}/firmware/build/Debug/nucleo_arcade.elf"
openocd_log="${TMPDIR:-/tmp}/nucleo_arcade-openocd.log"

if ! command -v openocd >/dev/null 2>&1; then
    printf '%s\n' "OpenOCD is missing. Install OpenOCD and an Arm GDB toolchain."
    exit 127
fi

if command -v gdb-multiarch >/dev/null 2>&1; then
    gdb_command="gdb-multiarch"
elif command -v arm-none-eabi-gdb >/dev/null 2>&1; then
    gdb_command="arm-none-eabi-gdb"
else
    printf '%s\n' "GDB is missing. Install an Arm GNU toolchain with GDB."
    exit 127
fi

bash "${project_dir}/build.sh"

openocd -f interface/stlink.cfg -c "transport select hla_swd" -f target/stm32f4x.cfg \
    >"${openocd_log}" 2>&1 &
openocd_pid=$!

cleanup()
{
    kill "${openocd_pid}" 2>/dev/null || true
    wait "${openocd_pid}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

sleep 1
if ! kill -0 "${openocd_pid}" 2>/dev/null; then
    cat "${openocd_log}" >&2
    wait "${openocd_pid}" || true
    exit 1
fi

"${gdb_command}" -q -tui "${elf_path}" \
    -ex 'set pagination off' \
    -ex 'set print pretty on' \
    -ex 'target extended-remote localhost:3333' \
    -ex 'monitor reset halt' \
    -ex 'load' \
    -ex 'monitor reset halt' \
    -ex 'break EXTI15_10_IRQHandler' \
    -ex 'continue'
