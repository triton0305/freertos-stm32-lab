cmake_minimum_required(VERSION 3.22)
project(stm32-freertos-lab LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Completely standalone: no equipment-controller path or production sources.
# Pinned upstream commits are downloaded by CMake on first configure.
include(FetchContent)
find_package(Git REQUIRED)
function(verify_pin source expected)
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" rev-parse HEAD
    OUTPUT_VARIABLE actual OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE result)
  if(NOT result EQUAL 0 OR NOT actual STREQUAL expected)
    message(FATAL_ERROR "Dependency version mismatch at ${source}: expected ${expected}, got ${actual}. Use the pinned official checkout; remove only this dependency's build/_deps cache and reconfigure if download was interrupted.")
  endif()
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" diff --quiet HEAD RESULT_VARIABLE dirty)
  if(NOT dirty EQUAL 0)
    message(FATAL_ERROR "Modified dependency at ${source}; restore the pinned source before building.")
  endif()
endfunction()
FetchContent_Declare(cmsis_5
  GIT_REPOSITORY https://github.com/ARM-software/CMSIS_5.git
  GIT_TAG 2b7495b8535bdcb306dac29b9ded4cfb679d7e5c)
FetchContent_Declare(cmsis_device_f4
  GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis-device-f4.git
  GIT_TAG 3b64b7b0b546cefe84f13c65508126eea58b97d8)
FetchContent_Declare(stm32_hal
  GIT_REPOSITORY https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git
  GIT_TAG c2e1406d7ea4b73aa42b98ddeb75a8670b1a5a16)
# Fetch source trees without executing any upstream CMakeLists.txt.
foreach(dep cmsis_5 cmsis_device_f4 stm32_hal)
  FetchContent_GetProperties(${dep})
  if(NOT ${dep}_POPULATED)
    FetchContent_Populate(${dep})
  endif()
endforeach()
verify_pin("${cmsis_5_SOURCE_DIR}" 2b7495b8535bdcb306dac29b9ded4cfb679d7e5c)
verify_pin("${cmsis_device_f4_SOURCE_DIR}" 3b64b7b0b546cefe84f13c65508126eea58b97d8)
verify_pin("${stm32_hal_SOURCE_DIR}" c2e1406d7ea4b73aa42b98ddeb75a8670b1a5a16)

add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Core")
set(FREERTOS_PORT GCC_ARM_CM4F CACHE STRING "FreeRTOS GCC Cortex-M4F port")
set(FREERTOS_HEAP 4 CACHE STRING "FreeRTOS heap_4")
if(FREERTOS_KERNEL_SOURCE_DIR)
  verify_pin("${FREERTOS_KERNEL_SOURCE_DIR}" 3a22924e0a9ddbbc8b0758881c33b3422a5cc20d)
  add_subdirectory("${FREERTOS_KERNEL_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/freertos-build")
else()
  FetchContent_Declare(freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG 3a22924e0a9ddbbc8b0758881c33b3422a5cc20d)
  FetchContent_MakeAvailable(freertos_kernel)
  verify_pin("${freertos_kernel_SOURCE_DIR}" 3a22924e0a9ddbbc8b0758881c33b3422a5cc20d)
endif()

set(HAL "${stm32_hal_SOURCE_DIR}/Src")
add_executable(stm32-freertos-lab.elf
  Core/board.c App/rtos_lab.c
  "${cmsis_device_f4_SOURCE_DIR}/Source/Templates/gcc/startup_stm32f411xe.s"
  "${cmsis_device_f4_SOURCE_DIR}/Source/Templates/system_stm32f4xx.c"
  "${HAL}/stm32f4xx_hal.c"
  "${HAL}/stm32f4xx_hal_rcc.c"
  "${HAL}/stm32f4xx_hal_rcc_ex.c"
  "${HAL}/stm32f4xx_hal_flash.c"
  "${HAL}/stm32f4xx_hal_flash_ex.c"
  "${HAL}/stm32f4xx_hal_gpio.c"
  "${HAL}/stm32f4xx_hal_uart.c"
  "${HAL}/stm32f4xx_hal_cortex.c"
  "${HAL}/stm32f4xx_hal_pwr.c"
  "${HAL}/stm32f4xx_hal_pwr_ex.c"
)
target_include_directories(stm32-freertos-lab.elf PRIVATE
  Core App
  "${stm32_hal_SOURCE_DIR}/Inc"
  "${stm32_hal_SOURCE_DIR}/Inc/Legacy"
  "${cmsis_device_f4_SOURCE_DIR}/Include"
  "${cmsis_5_SOURCE_DIR}/CMSIS/Core/Include")
target_compile_definitions(stm32-freertos-lab.elf PRIVATE USE_HAL_DRIVER STM32F411xE)
target_link_libraries(stm32-freertos-lab.elf PRIVATE freertos_kernel m)
target_link_options(stm32-freertos-lab.elf PRIVATE
  "-T${CMAKE_CURRENT_SOURCE_DIR}/Core/STM32F411xx_FLASH.ld"
  "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/stm32-freertos-lab.map,--gc-sections,--print-memory-usage"
  "--specs=nano.specs" "--specs=nosys.specs")
add_custom_command(TARGET stm32-freertos-lab.elf POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:stm32-freertos-lab.elf>
          "${CMAKE_CURRENT_BINARY_DIR}/stm32-freertos-lab.bin"
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:stm32-freertos-lab.elf>
  VERBATIM)
