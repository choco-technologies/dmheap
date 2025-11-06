# =====================================================================
#               DMOD HEAP CMake Configuration
# =====================================================================
cmake_minimum_required(VERSION 3.10)

# ======================================================================
#               DMOD Heap
# ======================================================================
project(dmheap
    VERSION 1.0
    DESCRIPTION "DMOD Heap Memory Manager"
    LANGUAGES C CXX)

set(DMHEAP_DONT_IMPLEMENT_DMOD_API OFF CACHE BOOL "Do not implement DMOD API in dmheap library")


# ======================================================================
#               Fetch DMOD repository
# ======================================================================
include(FetchContent)

# Only fetch and build dmod if it's not already available as a target
if(NOT TARGET dmod_inc)
    FetchContent_Declare(
        dmod
        GIT_REPOSITORY https://github.com/choco-technologies/dmod.git
        GIT_TAG        develop
    )

    # ======================================================================
    #               DMOD Configuration
    # ======================================================================
    set(DMOD_MODE "DMOD_SYSTEM" CACHE STRING "DMOD build mode")
    set(DMOD_BUILD_TESTS OFF CACHE BOOL "Build tests")
    set(DMOD_BUILD_EXAMPLES OFF CACHE BOOL "Build examples")
    set(DMOD_BUILD_TOOLS OFF CACHE BOOL "Build tools")
    set(DMOD_BUILD_TEMPLATES OFF CACHE BOOL "Build templates")
    
    # Pass coverage flags to DMOD if enabled
    if(ENABLE_COVERAGE)
        set(DMOD_ENABLE_COVERAGE ON CACHE BOOL "Enable DMOD coverage")
    endif()

    FetchContent_MakeAvailable(dmod)
    set(DMOD_DIR ${dmod_SOURCE_DIR} CACHE PATH "DMOD source directory" FORCE)
else()
    message(STATUS "dmod target already exists, skipping FetchContent")
endif()

# ======================================================================
#               DMOD Heap Library
# ======================================================================
set(MODULE_NAME dmheap)
add_library(${MODULE_NAME} STATIC
    src/dmheap.c
)

target_compile_definitions(${MODULE_NAME} 
    PRIVATE 
        $<$<BOOL:${DMHEAP_DONT_IMPLEMENT_DMOD_API}>:DMHEAP_DONT_IMPLEMENT_DMOD_API>
        DMHEAP_VERSION="${PROJECT_VERSION}"
)

target_include_directories(${MODULE_NAME} 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)   

target_link_libraries(${MODULE_NAME} 
    PRIVATE    
        dmod_inc
    )

create_library_makefile(${MODULE_NAME})

# ======================================================================
#               Tests
# ======================================================================
option(DMHEAP_BUILD_TESTS "Build tests" OFF)

if(DMHEAP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()