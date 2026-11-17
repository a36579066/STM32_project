# Nucleo Arcade

A three-game console for the **NUCLEO-F446RE**, with an SPI ST7789 TFT for gameplay and a 128 x 64 I2C OLED for game selection, status, and pause controls.

The `rtos` branch runs **COIN DASH, SNAKE, and BREAKOUT** with FreeRTOS tasks, interrupts, and DMA. GPIO, UART, SPI, and ADC/timer/DMA drivers use CMSIS register access; STM32 HAL handles startup initialization, clock/power configuration, and the HAL tick.

## Features

- **COIN DASH:** three lives, crates and spikes to jump over, drones to walk under, coin combos, shields, increasing difficulty, and a best score retained across rounds in RAM.
- **Arcade hub:** select games with the stick and A. MENU pauses any game; choose Resume, Restart, or Game List. B cancels a pause menu or returns from a game-over screen. Best scores are retained per game in RAM.
- **SNAKE / BREAKOUT:** grid movement, food and self/wall collisions; paddle and ball physics, destructible bricks, three lives, and increasing levels. B holds boost in Snake and slow motion in Breakout.
- **OLED:** SSD1306 by default, configurable SH1106, ACK probes at 7-bit addresses `0x3C` and `0x3D`, changed-page uploads, and independent recovery. TFT selection remains available if the OLED fails.
- **Joystick input:** TIM3 triggers a two-channel ADC1 scan at 100 Hz; circular DMA captures X/Y pairs. Startup center calibration, a dead zone, button debounce, and retained press events handle noisy input and short presses.
- **TFT rendering:** 320 x 240 landscape, rotated 90 degrees clockwise. COIN DASH recomposes changed regions with a 2,560-byte RGB565 staging buffer. The hub and mini games identify dirty tiles before composing them into a 1,024-byte buffer.
- **Interrupt-driven I/O:** SPI pixel transfers use DMA, single-byte transfers use RXNE interrupts, and UART TX uses a 1,024-byte ring buffer with TXE/TC interrupts.
- **FreeRTOS:** Kernel V11.3.0 with the GCC Cortex-M4F port. Input, Game, Display, Logger, and OLED tasks and all queues are statically allocated; no RTOS heap is used.
- **CPU waiting:** task notifications block on SPI/I2C DMA completion and UART backpressure; delays block tasks, and the idle hook uses `WFI`. Bootstrap waits retain `WFI`. A 1 ms SysTick serves HAL, driver timeouts, and FreeRTOS.
- **Host verification:** the actual game, renderer, and peripheral drivers are exercised with a framebuffer and simulated hardware registers.

## Hardware and wiring

Required hardware: a NUCLEO-F446RE, an ST7789 SPI TFT compatible with the configured 320 x 240 layout, a KY-023 joystick, three momentary buttons (A, B, MENU), and an optional four-pin 128 x 64 I2C OLED. The joystick's SW switch can serve as A.

| Connection | MCU pin | Nucleo label / interface |
|---|---|---|
| TFT SCK | PA5 | D13 |
| TFT MOSI | PA7 | D11 |
| TFT CS | PB6 | D10 |
| TFT D/C | PC7 | D9 |
| TFT RESET | PA9 | D8 |
| Joystick VRx | PA0 | A0 / ADC1_IN0 |
| Joystick VRy | PA1 | A1 / ADC1_IN1 |
| A / joystick SW | PB0 | A3, input with internal pull-up |
| B button | PB5 | D4, input with internal pull-up |
| MENU button | PA8 | D7, input with internal pull-up |
| OLED SCL / SDA | PB8 / PB9 | D15 / D14, I2C1 AF4, open drain |
| USART2 TX / RX | PA2 / PA3 | D1 / D0, ST-LINK Virtual COM Port |
| Onboard B1 button | PC13 | EXTI press-event logging |

Power the KY-023 from **3V3**, including modules whose supply pin is labeled `+5V`, and connect all grounds. Connect each button between its signal pin and **GND**; pressing it pulls the input low. Connect the OLED to 3V3 and GND when supported by its module; I2C pull-ups must use 3.3 V. TFT supply and backlight connections depend on the module. MCU signal levels are 3.3 V.

PA5 is shared with the onboard LD2 LED; the display configuration assigns it to SPI1 SCK. Pin definitions are centralized in [board_pins.h](firmware/Board/Private/board_pins.h).

## Build

Use Linux or Ubuntu/WSL with Bash and these tools on `PATH`:

- Arm GNU toolchain: `arm-none-eabi-gcc`, binutils, and Newlib with nano specs.
- CMake 3.22 or newer and Ninja.
- A host C11 compiler (`cc`) and GNU coreutils for host tests; Python 3 for exporting previews.

From the repository root:

```bash
bash build.sh          # Debug: -O0 plus -O2 rendering, debug information
bash build.sh Release  # Release: -Os
```

| Configuration | Output |
|---|---|
| Debug | `firmware/build/Debug/nucleo_arcade.elf` |
| Release | `firmware/build/Release/nucleo_arcade.elf` |

The scripts configure, compile, and link the firmware. Program the resulting ELF separately using ST-LINK/SWD.

## Flash and play

1. Connect the board through its ST-LINK USB connector. In STM32CubeProgrammer, select ST-LINK/SWD and program either ELF above.
2. Open the ST-LINK Virtual COM Port at **115200 baud, 8 data bits, no parity, 1 stop bit (8N1)**.
3. Reset the board with the joystick centered and the game button released. Calibration collects 32 samples over approximately 320 ms.
4. At the game catalog, move the stick up/down and press A to launch a game.
5. Press MENU during gameplay for Resume, Restart, or Game List. Move the stick and press A; B or MENU resumes. The OLED mirrors this menu and shows live scores during play.

The OLED controller cannot be identified from its ACK. The default is SSD1306. If UART reports `OLED ready` but the image is blank or shifted, change `APP_OLED_CONTROLLER` to `OLED_SH1106` in [oled_config.h](firmware/App/Services/Inc/oled_config.h), rebuild, and flash.

COIN DASH controls remain:

| Input / event | Action |
|---|---|
| Joystick left / right | Move the robot |
| Game button while playing | Jump; release before pressing again |
| Crates / spikes | Jump over them |
| Drones | Stay on the ground and pass underneath |
| Every 10 coins | Gain one shield, shown by the HUD icon |
| All three lives lost | Press the game button to retry |

Launching from the catalog or retrying consumes the press without jumping. A press within 150 ms before landing can queue the next jump. Combos reach x5, levels increase every 12 seconds up to level 9, and the best score resets on power-up or MCU reset.

UART prints `JOY` readings and A/B/MENU press/release events (`BTN PB0`, `BTN B PB5`, `BTN MENU PA8`). Successful display initialization includes:

```text
TFT game ready: 320x240 landscape
ARCADE HUB ready: stick up/down, A select, MENU pause, B back
OLED ready: I2C1 address 0x3C SSD1306 bus=100000 Hz
```

If ST-LINK is detected but reports `No STM32 target found`, use **Under Reset** with hardware reset.

## Clock configuration

The current setup uses HSI as the PLL input:

```text
HSI 16 MHz / 8 * 180 / 2 = SYSCLK 180 MHz (M=8, N=180, P=2)
SYSCLK / 1 = HCLK 180 MHz
HCLK / 4   = PCLK1 45 MHz
HCLK / 2   = PCLK2 90 MHz
```

USART2 uses PCLK1; SPI1 uses PCLK2/8 for an **11.25 MHz SCK**. ADC uses PCLK2/4 for **22.5 MHz**. With the default timer clock selection, TIM3 receives 90 MHz; `PSC=89` and `ARR=9999` produce the **100 Hz** joystick trigger.

I2C1 also uses PCLK1=45 MHz. At 100 kHz standard mode, `CR2.FREQ=45`, `CCR=225`, and `TRISE=46`. OLED TX uses **DMA1 Stream 6 Channel 1**, with event/error interrupts for address ACK and the final byte/STOP.

Clock and power settings are in [SystemClock_Config()](firmware/Core/Src/main.c). The 180 MHz setup enables Scale 1 and OverDrive. Peripheral drivers and their supplied clock values must agree with the configured clock tree.

## Firmware layout

| Location | Responsibility |
|---|---|
| `firmware/Core/` | Startup, `main()`, system clocks, and interrupt entry points |
| `firmware/Platform/` | Register-level GPIO, ADC/timer/DMA, SPI, I2C, UART, SysTick, and CPU sleep |
| `firmware/Board/` | Nucleo pin assignments and board configuration |
| `firmware/Devices/` | ST7789 and SSD1306/SH1106 controller commands and pixel transfers |
| `firmware/App/Services/` | Display API and joystick calibration, normalization, and debounce |
| `firmware/App/Game/` | Hardware-independent game model, sprites, and region rendering |
| `firmware/App/Src/app.c` | Board/RTOS bootstrap and EXTI event delegation |
| `firmware/RTOS/` | Static task/queue setup, task bodies, I/O notification adapter, and RTOS hooks |
| `firmware/Middlewares/FreeRTOS-Kernel/` | Pinned upstream kernel and Cortex-M4F port |
| `firmware/Drivers/` | CMSIS and the included STM32 HAL components |
| `Tests/Host/` | Game/display regressions and simulated peripheral tests |

Input follows `TIM3 TRGO -> ADC1 -> DMA2 Stream 4 -> Joystick Service -> Input task -> Game task`. SPI1 uses DMA2 Stream 0 for RX and Stream 3 for TX, both on Channel 3; ADC1 uses Stream 4 on Channel 0.

| Task | Priority | Responsibility |
|---|---:|---|
| Input | 4 | Calibrate the stick, publish snapshots every 10 ms, and queue button/joystick diagnostics |
| Display | 2 | Own TFT/SPI access and render the latest queued game state |
| Game | 3 | Own the catalog/pause state and three game models; publish TFT frames and separate OLED status |
| Logger | 1 | Format queued events and write UART output |
| OLED | 1 | Own I2C1, probe/init the controller, and upload changed pages every 200 ms |

Game has higher priority than Display so pixel composition cannot delay physics updates. The Game task waits for both input calibration and display initialization. Frame queues keep the latest snapshot; Display restores pixels using the last frame actually drawn. Display calls remain synchronous for their caller, while RTOS notifications allow other tasks to run during transfers. Application startup and I/O use separate task notification indexes.

## Verification

Run host tests from the repository root:

```bash
bash Tests/Host/run.sh
```

Coverage includes jump buffering, button edges, movement boundaries, collision/shield/retry logic, extended gameplay, and pixel comparisons between incremental and full redraws. Peripheral tests run against both the original wait backend and the RTOS adapter, covering DMA, coherent input snapshots, debounce, SPI completion/error/timeout handling, UART queues, and completion-before-block races. Task API doubles exercise the real five task bodies, startup barrier, frame handoff, UART diagnostics, display failure handling, and OLED failure/retry without stopping gameplay. Arcade tests also cover menu navigation, pause timing, mini-game physics, OLED protocol/page caching, and skipped-frame pixel comparisons.

Host tests use simulated registers. On-board checks are needed for analog readings, physical SPI signals, frame rate, input latency, and CPU power consumption. The game uses a 20 ms scheduling target; actual frame rate depends on rendering and transfer time.

## License

Original project-authored content is licensed under the [MIT License](LICENSE). Vendor components retain their license files in their firmware directories. STM32 and Nucleo names identify compatible hardware; this project is independent of STMicroelectronics.
