# What makes this board's build ESP-IDF's build.
#
# Included from the generated toolchain.cmake, before `project()`, which is
# where the framework has to be located and its toolchain file chosen. From
# there ESP-IDF owns everything an ESP32 image needs: the second-stage
# bootloader, the partition table, the linker fragments for each memory, and
# the app image build.
#
# The IDF is vendored beside this platform, through the vendor config the
# board declares, so the version a board was built against is the version it
# builds with.
#
# @project RoboHub Platform
# @license MIT

get_filename_component(IDF_PATH "${CMAKE_CURRENT_LIST_DIR}/../../vendor/esp-idf" ABSOLUTE)
set(IDF_PATH "${IDF_PATH}" CACHE PATH "the ESP-IDF this platform vendors")
set(ENV{IDF_PATH} "${IDF_PATH}")

# The tools ESP-IDF installed live under one prefix, and its own Python
# environment is where its build scripts and the component manager live. The
# build is run through that interpreter rather than the system one, which has
# neither; the environment is found rather than named, so a machine whose IDF
# version differs still builds.
set(ENV{IDF_TOOLS_PATH} "$ENV{HOME}/.espressif")
file(GLOB ROBOHUB_IDF_PYTHON_ENVS "$ENV{HOME}/.espressif/python_env/idf*_py*_env")
if(ROBOHUB_IDF_PYTHON_ENVS)
    list(GET ROBOHUB_IDF_PYTHON_ENVS 0 ROBOHUB_IDF_PYTHON_ENV)
    set(ENV{IDF_PYTHON_ENV_PATH} "${ROBOHUB_IDF_PYTHON_ENV}")
    set(ENV{PYTHON} "${ROBOHUB_IDF_PYTHON_ENV}/bin/python")
    message(STATUS "[robohub-esp32] ESP-IDF python env at ${ROBOHUB_IDF_PYTHON_ENV}")
else()
    message(WARNING "[robohub-esp32] no ESP-IDF python environment found under ~/.espressif; run the vendored IDF's install.sh esp32")
endif()

# ESP-IDF builds Xtensa code from C, C++ and assembly, so the generated build
# is told to enable all three once the compiler below is known.
set(RHO_PROGRAM_LANGUAGES "C;CXX;ASM")

# The part this board carries. ESP-IDF is one framework for a family of chips,
# and which one is being built for is named rather than derived.
set(IDF_TARGET "esp32" CACHE STRING "the chip this board carries")
set(ENV{IDF_TARGET} "esp32")

# The chip's toolchain file, which must be chosen before `project()`: this is
# where the compiler, its flags and the per-memory linker fragments come from.
# `IDF_TOOLCHAIN` is the kind — gcc — and not this file, which is what ESP-IDF
# compares against what the build was configured with.
set(IDF_TOOLCHAIN "gcc" CACHE STRING "the toolchain kind ESP-IDF builds with")
set(CMAKE_TOOLCHAIN_FILE "${IDF_PATH}/tools/cmake/toolchain-esp32.cmake" CACHE FILEPATH "the ESP32 toolchain")

# ESP-IDF's own project(): including it before `project()` is how a project
# becomes an IDF project rather than one beside it.
include(${IDF_PATH}/tools/cmake/project.cmake)
