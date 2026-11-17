cmake_minimum_required(VERSION 3.22)
enable_language(C ASM)

set(CMSIS_DEVICE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Device/ST/STM32F4xx)
set(CMSIS_CORE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Include)
set(HAL_DRIVER_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver)

set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Inc
    ${HAL_DRIVER_DIR}/Inc
    ${HAL_DRIVER_DIR}/Inc/Legacy
    ${CMSIS_DEVICE_DIR}/Include
    ${CMSIS_CORE_DIR}
)

add_library(stm32cubemx INTERFACE)
target_compile_definitions(stm32cubemx INTERFACE
    USE_HAL_DRIVER
    STM32F446xx
    $<$<CONFIG:Debug>:DEBUG>
)
target_include_directories(stm32cubemx SYSTEM INTERFACE ${MX_Include_Dirs})

add_library(STM32_Drivers OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/system_stm32f4xx.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_cortex.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_dma.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_dma_ex.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_exti.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_flash.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_flash_ex.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_flash_ramfunc.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_gpio.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_pwr.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_pwr_ex.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_rcc.c
    ${HAL_DRIVER_DIR}/Src/stm32f4xx_hal_rcc_ex.c
)
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f4xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f4xx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32f446xx.s
)

target_link_libraries(${CMAKE_PROJECT_NAME} STM32_Drivers)
