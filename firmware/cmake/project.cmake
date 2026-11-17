# Keep explicit source lists here; cmake/stm32cubemx belongs to CubeMX.
# Include directories follow dependencies instead of leaking into every module.

add_library(console_cmsis INTERFACE)
target_compile_definitions(console_cmsis INTERFACE STM32F446xx)
target_include_directories(console_cmsis SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
)

add_library(console_platform STATIC
    ${PROJECT_SOURCE_DIR}/Platform/Src/cpu.c
    ${PROJECT_SOURCE_DIR}/Platform/Src/gpio.c
    ${PROJECT_SOURCE_DIR}/Platform/Src/adc.c
    ${PROJECT_SOURCE_DIR}/Platform/Src/spi.c
    ${PROJECT_SOURCE_DIR}/Platform/Src/uart.c
    ${PROJECT_SOURCE_DIR}/Platform/Src/i2c.c
    ${PROJECT_SOURCE_DIR}/Platform/Src/timebase.c
    ${PROJECT_SOURCE_DIR}/RTOS/Src/io_wait_rtos.c
)
target_include_directories(console_platform PUBLIC
    ${PROJECT_SOURCE_DIR}/Platform/Inc
)
target_link_libraries(console_platform PUBLIC console_cmsis stm32cubemx console_rtos)

add_library(console_devices STATIC
    ${PROJECT_SOURCE_DIR}/Devices/Src/st7789.c
    ${PROJECT_SOURCE_DIR}/Devices/Src/oled.c
)
target_include_directories(console_devices PUBLIC
    ${PROJECT_SOURCE_DIR}/Devices/Inc
)
target_link_libraries(console_devices PUBLIC console_platform)

add_library(console_board STATIC
    ${PROJECT_SOURCE_DIR}/Board/Src/board.c
)
target_include_directories(console_board
    PUBLIC ${PROJECT_SOURCE_DIR}/Board/Inc
    PRIVATE ${PROJECT_SOURCE_DIR}/Board/Private
)
target_link_libraries(console_board PRIVATE console_platform)

add_library(console_services STATIC
    ${PROJECT_SOURCE_DIR}/App/Services/Src/display_service.c
    ${PROJECT_SOURCE_DIR}/App/Services/Src/joystick_service.c
)
target_include_directories(console_services PUBLIC
    ${PROJECT_SOURCE_DIR}/App/Services/Inc
)
target_link_libraries(console_services PUBLIC
    console_board console_platform console_devices
)

add_library(console_game_render STATIC
    ${PROJECT_SOURCE_DIR}/App/Game/Src/game_scene.c
    ${PROJECT_SOURCE_DIR}/App/Game/Src/game_sprite.c
    ${PROJECT_SOURCE_DIR}/App/Game/Src/ui_font.c
    ${PROJECT_SOURCE_DIR}/App/Game/Src/console_scene.c
)
target_include_directories(console_game_render PUBLIC
    ${PROJECT_SOURCE_DIR}/App/Game/Inc
)
target_link_libraries(console_game_render PUBLIC console_services)
# Keep pixel composition responsive even when flashing the Debug build.
# Symbols remain enabled; stepping optimized rendering code can skip locals.
target_compile_options(console_game_render PRIVATE $<$<CONFIG:Debug>:-O2>)

add_library(console_game STATIC
    ${PROJECT_SOURCE_DIR}/App/Game/Src/game.c
    ${PROJECT_SOURCE_DIR}/App/Game/Src/game_model.c
    ${PROJECT_SOURCE_DIR}/App/Game/Src/console_model.c
    ${PROJECT_SOURCE_DIR}/App/Game/Src/mini_games.c
)
target_include_directories(console_game PUBLIC
    ${PROJECT_SOURCE_DIR}/App/Game/Inc
)
target_link_libraries(console_game PUBLIC console_game_render)

set(FREERTOS_SOURCE_DIR ${PROJECT_SOURCE_DIR}/Middlewares/FreeRTOS-Kernel/Source)
add_library(console_rtos STATIC
    ${FREERTOS_SOURCE_DIR}/tasks.c
    ${FREERTOS_SOURCE_DIR}/queue.c
    ${FREERTOS_SOURCE_DIR}/list.c
    ${FREERTOS_SOURCE_DIR}/portable/GCC/ARM_CM4F/port.c
)
target_include_directories(console_rtos PUBLIC
    ${FREERTOS_SOURCE_DIR}/include
    ${FREERTOS_SOURCE_DIR}/portable/GCC/ARM_CM4F
    ${PROJECT_SOURCE_DIR}/RTOS/Inc
)
target_link_libraries(console_rtos PUBLIC console_cmsis stm32cubemx)

add_library(console_app STATIC
    ${PROJECT_SOURCE_DIR}/App/Src/app.c
    ${PROJECT_SOURCE_DIR}/RTOS/Src/app_rtos.c
    ${PROJECT_SOURCE_DIR}/RTOS/Src/display_task.c
    ${PROJECT_SOURCE_DIR}/RTOS/Src/game_task.c
    ${PROJECT_SOURCE_DIR}/RTOS/Src/input_task.c
    ${PROJECT_SOURCE_DIR}/RTOS/Src/logger_task.c
    ${PROJECT_SOURCE_DIR}/RTOS/Src/oled_task.c
)
target_include_directories(console_app PUBLIC
    ${PROJECT_SOURCE_DIR}/App/Inc
    ${PROJECT_SOURCE_DIR}/RTOS/Inc
    PRIVATE ${PROJECT_SOURCE_DIR}/RTOS/Private
)
target_link_libraries(console_app PRIVATE
    console_board console_platform console_game console_rtos
)

foreach(module IN ITEMS console_platform console_devices console_board
                        console_services console_game_render console_game console_rtos console_app)
    target_compile_options(${module} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

# CubeMX uses the plain signature for the executable; keep it consistent.
# Game logic is built independently of the CMSIS register-level implementation.
target_link_libraries(${CMAKE_PROJECT_NAME}
    console_app console_platform console_game console_game_render console_rtos
)
