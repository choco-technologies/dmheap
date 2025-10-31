# =====================================================================
#               DMHEAP Tests
# =====================================================================
cmake_minimum_required(VERSION 3.10)

# Enable testing
enable_testing()

# =====================================================================
#               Test: Unit Tests
# =====================================================================
add_executable(test_dmheap_unit test_dmheap_unit.c)
target_link_libraries(test_dmheap_unit 
    PRIVATE 
        dmheap
        dmod_system
        dmod_common
        dmod_fastlz
        dmod_inc
)
target_include_directories(test_dmheap_unit
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Temporarily disabled due to hanging issue - needs investigation
# add_test(NAME unit_tests COMMAND test_dmheap_unit)

# =====================================================================
#               Test: Module Tests
# =====================================================================
add_executable(test_dmheap_module test_dmheap_module.c)
target_link_libraries(test_dmheap_module 
    PRIVATE 
        dmheap
        dmod_system
        dmod_common
        dmod_fastlz
        dmod_inc
)
target_include_directories(test_dmheap_module
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Temporarily disabled due to hanging issue - needs investigation
# add_test(NAME module_tests COMMAND test_dmheap_module)

# =====================================================================
#               Test: Simple Test
# =====================================================================
add_executable(test_simple test_simple.c)
target_link_libraries(test_simple 
    PRIVATE 
        dmheap
        dmod_system
        dmod_common
        dmod_fastlz
        dmod_inc
)
target_include_directories(test_simple
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME dmheap_unit   COMMAND test_dmheap_unit)
add_test(NAME dmheap_module COMMAND test_dmheap_module)
add_test(NAME simple_test   COMMAND test_simple)

# =====================================================================
#               Coverage Support (optional)
# =====================================================================
option(ENABLE_COVERAGE "Enable code coverage" OFF)

if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Enabling code coverage")
        
        # Add coverage flags to the dmheap library
        target_compile_options(dmheap PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(dmheap PRIVATE --coverage)
        
        # Add coverage flags to test executables
        target_compile_options(test_dmheap_unit PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(test_dmheap_unit PRIVATE --coverage)
        
        target_compile_options(test_dmheap_module PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(test_dmheap_module PRIVATE --coverage)
        
        target_compile_options(test_simple PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(test_simple PRIVATE --coverage)
        
        # Add custom target for generating coverage report
        find_program(GCOV gcov)
        find_program(LCOV lcov)
        find_program(GENHTML genhtml)
        
        if(LCOV AND GENHTML)
            add_custom_target(coverage
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
                COMMAND ${LCOV} --directory . --zerocounters
                COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                COMMAND ${LCOV} --directory . --capture --output-file ${CMAKE_BINARY_DIR}/coverage/coverage.info
                COMMAND ${LCOV} --remove ${CMAKE_BINARY_DIR}/coverage/coverage.info '/usr/*' '*/build/_deps/*' --output-file ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
                COMMAND ${GENHTML} ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info --output-directory ${CMAKE_BINARY_DIR}/coverage/html
                COMMAND ${LCOV} --list ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating code coverage report"
            )
            message(STATUS "Coverage target available. Run 'make coverage' to generate report.")
        else()
            message(WARNING "lcov/genhtml not found. Coverage report generation will not be available.")
        endif()
    else()
        message(WARNING "Code coverage is only supported with GCC or Clang")
    endif()
endif()
