# =====================================================================
#               DMOD memory Tool Module
# =====================================================================
# Standalone project: builds as an Application-type DMOD module, which needs
# dmod fetched in DMOD_MODULE mode. This is intentionally separate from
# dmheap's own top-level CMakeLists.txt, which fetches dmod in DMOD_SYSTEM
# mode to build the dmheap library itself - the two modes can't coexist in a
# single CMake configure. Build this the same way you would build dmell or
# one of its command modules (see the top-level dmheap README.md).
cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED DMOD_MODULE_VERSION)
    set(DMOD_MODULE_VERSION "1.0" CACHE STRING "DMOD module version")
endif()

# ======================================================================
#               Fetch DMOD repository
# ======================================================================
include(FetchContent)
FetchContent_Declare(
    dmod
    GIT_REPOSITORY https://github.com/choco-technologies/dmod.git
    GIT_TAG        develop
)

set(DMOD_MODE "DMOD_MODULE" CACHE STRING "DMOD build mode")
set(DMOD_BUILD_TESTS OFF CACHE BOOL "Build tests")
set(DMOD_BUILD_EXAMPLES OFF CACHE BOOL "Build examples")
set(DMOD_BUILD_TOOLS OFF CACHE BOOL "Build tools")
set(DMOD_BUILD_TEMPLATES OFF CACHE BOOL "Build templates")

FetchContent_MakeAvailable(dmod)
set(DMOD_DIR ${dmod_SOURCE_DIR} CACHE PATH "DMOD source directory")

project(memory
    VERSION ${DMOD_MODULE_VERSION}
    DESCRIPTION "dmheap allocator inspection tool"
    LANGUAGES C CXX)

include(${DMOD_DIR}/paths.cmake)
dmod_setup_external_module()

# Module name
set(MEMORY_MODULE_NAME memory)

# Author
set(DMOD_AUTHOR_NAME Patryk Kubiak)

# Stack size for the memory module - the module-summary and fragmentation
# reports are allocated on the heap (not the stack), so this only needs to
# cover normal call/printf overhead.
set(DMOD_STACK_SIZE 2048)

# Path to the DMR file for this module - setting this before dmod_add_executable
# makes the build automatically produce a ready-to-publish release package
# (via mkdmrpkg) under build/packages/${MEMORY_MODULE_NAME}.zip
set(DMOD_DMR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/memory.dmr)

# Create the module executable
dmod_add_executable(${MEMORY_MODULE_NAME} ${DMOD_MODULE_VERSION}
    memory.c
)

# dmheap.h is only needed for its type/API declarations here - the real
# implementation is resolved at runtime against the kernel's own dmheap
# instance, so this just adds the include directory (no library linkage).
target_include_directories(${MEMORY_MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
