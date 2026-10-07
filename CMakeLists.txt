cmake_minimum_required(VERSION 3.16)
project(robohub-esp32 C ASM)

# ---------------------------------------------------------------------------
# Locate robohub-common
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
endif()

if(NOT ROBOHUB_COMMON_DIR)
  if(EXISTS "/home/miraur/projects/robohub-common")
    set(ROBOHUB_COMMON_DIR "/home/miraur/projects/robohub-common")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../robohub-common")
    get_filename_component(ROBOHUB_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../robohub-common" ABSOLUTE)
  elseif(DEFINED ENV{ROBOHUB_COMMON_DIR})
    set(ROBOHUB_COMMON_DIR "$ENV{ROBOHUB_COMMON_DIR}")
  else()
    message(FATAL_ERROR "robohub-common not found. Set ROBOHUB_COMMON_DIR or place it beside this project.")
  endif()
endif()
message(STATUS "[robohub-esp32] robohub-common at ${ROBOHUB_COMMON_DIR}")

# Added only when this platform *is* the project. Built as a dependency — the
# usual case — the generated project adds robohub-common itself, right after
# this directory, and adding it here too would be two targets of one name.
# ESP-IDF's `project()` makes this directory look top-level either way, so the
# question is asked of the source directory rather than of that flag.
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  add_subdirectory("${ROBOHUB_COMMON_DIR}" robohub-common)
endif()

# ---------------------------------------------------------------------------
# ESP-IDF
# ---------------------------------------------------------------------------
# A bare-metal build of this board is ESP-IDF's build. The board's prelude
# located the framework before `project()` and made this an IDF project; the
# variable it set says so, and its targets are what the rest is written in.
if(NOT DEFINED IDF_PATH)
  if(DEFINED ENV{IDF_PATH})
    set(IDF_PATH "$ENV{IDF_PATH}")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/esp-idf")
    set(IDF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/esp-idf")
  endif()
endif()

set(ROBOHUB_ESP_SDK_AVAILABLE FALSE)
if(IDF_PATH AND EXISTS "${IDF_PATH}")
  message(STATUS "[robohub-esp32] ESP-IDF at ${IDF_PATH}")
  set(ROBOHUB_ESP_SDK_AVAILABLE TRUE)
else()
  message(STATUS "[robohub-esp32] ESP-IDF not configured; host builds and tests will work, bare-metal builds will not link")
endif()

# ---------------------------------------------------------------------------
# Host vs bare-metal
# ---------------------------------------------------------------------------
set(ROBOHUB_ESP_HOSTED OFF)
if(PROJECT_IS_TOP_LEVEL AND RHO_BUILD_HOST_TESTS AND NOT RHO_TARGET_IS_BAREMETAL)
  set(ROBOHUB_ESP_HOSTED ON)
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(ROBOHUB_ESP_SOURCES
    src/platform/esp_console.c
    src/platform/esp_fault.c
    src/platform/esp_platform.c
    src/platform/esp_tick.c
)

if(RHO_ROBO_CONFIG AND EXISTS "${RHO_ROBO_CONFIG}")
  include("${RHO_ROBO_CONFIG}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/robo-config.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/robo-config.cmake")
endif()

macro(robohub_esp_add_guarded symbol)
  cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
  if(RHO_CONFIG_${symbol})
    list(APPEND ROBOHUB_ESP_SOURCES ${ARG_SOURCES})
  endif()
endmacro()

robohub_esp_add_guarded(RH_HAL_GPIO SOURCES src/hal/esp_gpio.c)
robohub_esp_add_guarded(RH_HAL_I2C  SOURCES src/hal/esp_i2c.c)
robohub_esp_add_guarded(RH_HAL_SPI  SOURCES src/hal/esp_spi.c)
robohub_esp_add_guarded(RH_HAL_UART SOURCES src/hal/esp_uart.c)
robohub_esp_add_guarded(RH_HAL_TIMER SOURCES src/hal/esp_timer.c)
robohub_esp_add_guarded(RH_HAL_RS485 SOURCES src/hal/esp_rs485.c)

# The board's own support: its pins, its LED, its registration. Compiled here,
# into the platform library, because the common library calls the registration
# this provides and a symbol a program's component defines is a symbol the
# common library was linked past.
list(APPEND ROBOHUB_ESP_SOURCES boards/lolin32-lite/bsp/bsp.c)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(robohub-esp32 STATIC ${ROBOHUB_ESP_SOURCES})

target_include_directories(robohub-esp32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/drivers
    ${ROBOHUB_COMMON_DIR}/include
    ${ROBOHUB_COMMON_DIR}/include/robohub
    ${ROBOHUB_COMMON_DIR}/include/robohub/hal
    ${ROBOHUB_COMMON_DIR}/include/robohub/drivers
)

target_link_libraries(robohub-esp32 PUBLIC robohub-common)

if(ROBOHUB_ESP_SDK_AVAILABLE)
  # `RH_ESP_IDF` is the name the sources branch on.
  target_compile_definitions(robohub-esp32 PRIVATE RH_ESP_IDF=1)

  # This library's own sources include ESP-IDF's headers — its system headers,
  # its FreeRTOS, its per-peripheral drivers — and it cannot get them from the
  # component system the way a component would, because this is a library the
  # generated project links rather than a component ESP-IDF discovers. Its
  # headers are named directly, and the configuration ESP-IDF generates is
  # found in the build tree it writes it to.
  file(GLOB ROBOHUB_IDF_INCLUDE_DIRS
    "${IDF_PATH}/components/*/include"
    "${IDF_PATH}/components/*/*/include"
    "${IDF_PATH}/components/*/*/*/include"
  )
  # A component tree holds headers for more than the part this board carries:
  # the `linux` component is a host's libc and shadowing the real one with it is
  # how `sys/cdefs.h` comes to include a header that is not there. The ones for
  # other hosts and other architectures are left out.
  list(FILTER ROBOHUB_IDF_INCLUDE_DIRS EXCLUDE REGEX "/components/linux/")
  list(FILTER ROBOHUB_IDF_INCLUDE_DIRS EXCLUDE REGEX "/components/riscv/")
  list(FILTER ROBOHUB_IDF_INCLUDE_DIRS EXCLUDE REGEX "/components/linux-")
  target_include_directories(robohub-esp32 PRIVATE
    ${ROBOHUB_IDF_INCLUDE_DIRS}
    "${CMAKE_BINARY_DIR}/config"
    "${CMAKE_BINARY_DIR}/esp-idf/config"
  )
endif()

target_compile_features(robohub-esp32 PUBLIC c_std_11)

# ---------------------------------------------------------------------------
# Host tests
# ---------------------------------------------------------------------------
if(ROBOHUB_ESP_HOSTED)
  message(STATUS "[robohub-esp32] registering host unit tests")
  enable_testing()
  add_executable(robohub-esp32-test-error tests/test_error.c)
  target_link_libraries(robohub-esp32-test-error PRIVATE robohub-esp32)
  add_test(NAME robohub-esp32-test-error COMMAND robohub-esp32-test-error)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(FILES module.yml DESTINATION share/robohub-esp32)
install(DIRECTORY include/ DESTINATION include)
install(TARGETS robohub-esp32
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
