#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
test_dir="$(mktemp -d /tmp/nucleo-game-host.XXXXXX)"
trap 'rm -rf -- "$test_dir"' EXIT
cd -- "$project_dir"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
    -Ifirmware/App/Game/Inc -Ifirmware/App/Services/Inc \
    Tests/Host/test_game_display.c \
    firmware/App/Game/Src/game.c \
    firmware/App/Game/Src/game_model.c \
    firmware/App/Game/Src/game_scene.c \
    firmware/App/Game/Src/game_sprite.c \
    -o "$test_dir/test_game_display"
"$test_dir/test_game_display" "$@"

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fno-pie -no-pie \
    -ITests/Host/fakes \
    -isystem firmware/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
    -Ifirmware/Platform/Inc -Ifirmware/Board/Inc -Ifirmware/App/Services/Inc \
    -include Tests/Host/fakes/hardware.h \
    Tests/Host/test_interrupt_io.c \
    firmware/Platform/Src/cpu.c firmware/Platform/Src/timebase.c firmware/Platform/Src/io_wait.c \
    firmware/Platform/Src/adc.c firmware/Platform/Src/spi.c firmware/Platform/Src/i2c.c \
    firmware/Platform/Src/uart.c firmware/App/Services/Src/joystick_service.c \
    -o "$test_dir/test_interrupt_io"
timeout 10s "$test_dir/test_interrupt_io"

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fno-pie -no-pie -DHOST_RTOS_IO \
    -ITests/Host/fakes/rtos -ITests/Host/fakes -Ifirmware/RTOS/Inc \
    -isystem firmware/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
    -Ifirmware/Platform/Inc -Ifirmware/Board/Inc -Ifirmware/App/Services/Inc \
    -include Tests/Host/fakes/hardware.h \
    Tests/Host/test_interrupt_io.c \
    firmware/Platform/Src/cpu.c firmware/Platform/Src/timebase.c \
    firmware/RTOS/Src/io_wait_rtos.c \
    firmware/Platform/Src/adc.c firmware/Platform/Src/spi.c firmware/Platform/Src/i2c.c \
    firmware/Platform/Src/uart.c firmware/App/Services/Src/joystick_service.c \
    -o "$test_dir/test_rtos_io"
timeout 10s "$test_dir/test_rtos_io"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
    -Ifirmware/App/Game/Inc -Ifirmware/App/Services/Inc -Ifirmware/Devices/Inc -Ifirmware/Platform/Inc \
    Tests/Host/test_arcade.c firmware/App/Game/Src/console_model.c \
    firmware/App/Game/Src/mini_games.c firmware/App/Game/Src/console_scene.c \
    firmware/App/Game/Src/ui_font.c firmware/App/Game/Src/game_model.c \
    firmware/App/Game/Src/game_scene.c firmware/App/Game/Src/game_sprite.c \
    firmware/Devices/Src/oled.c -o "$test_dir/test_arcade"
timeout 60s "$test_dir/test_arcade" "$@"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
    -ITests/Host/fakes/rtos -ITests/Host/fakes -Ifirmware/RTOS/Inc -Ifirmware/RTOS/Private \
    -isystem firmware/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
    -Ifirmware/Platform/Inc -Ifirmware/Board/Inc \
    -Ifirmware/App/Services/Inc -Ifirmware/App/Game/Inc -Ifirmware/Devices/Inc \
    Tests/Host/test_rtos_tasks.c \
    firmware/RTOS/Src/app_rtos.c firmware/RTOS/Src/input_task.c \
    firmware/RTOS/Src/game_task.c firmware/RTOS/Src/display_task.c \
    firmware/RTOS/Src/logger_task.c firmware/RTOS/Src/oled_task.c \
    firmware/App/Game/Src/console_model.c firmware/App/Game/Src/mini_games.c \
    firmware/App/Game/Src/console_scene.c firmware/App/Game/Src/ui_font.c \
    firmware/App/Game/Src/game_model.c \
    firmware/App/Game/Src/game_scene.c firmware/App/Game/Src/game_sprite.c \
    -o "$test_dir/test_rtos_tasks"
timeout 10s "$test_dir/test_rtos_tasks"
