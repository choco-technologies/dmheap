# DMHEAP Tests

This directory contains comprehensive tests for the dmheap memory manager library.

## Test Structure

### Unit Tests (`test_dmheap_unit.c`) ✓
Comprehensive unit tests covering all dmheap functions:
- Initialization tests (valid/invalid parameters)
- Module registration/unregistration
- Basic allocation (malloc, free)
- Aligned allocation
- Reallocation
- Memory concatenation
- Edge cases and error handling
- Fragmentation handling
- Stress testing
- Performance benchmarking

**Status**: Fully functional, passes all tests

### Module Tests (`test_dmheap_module.c`) ✓
Integration tests simulating real-world usage:
- FileSystem simulation (file metadata and data allocation)
- Network buffer pool management
- Multi-module concurrent usage
- Dynamic data structures (linked lists)
- Memory reuse patterns
- Alignment requirements for various data types
- Heap exhaustion and recovery
- Mixed allocation sizes
- Performance benchmarking with JSON output

**Status**: Fully functional, passes all tests

### Simple Test (`test_simple.c`) ✓
Basic functional test that validates core dmheap functionality:
- Initialization
- Basic allocation and deallocation
- Memory write/read verification
- Simple pass/fail reporting

**Status**: Fully functional, passes all tests

## Building and Running Tests

### Building Tests

From the repository root:

```bash
# Create build directory
mkdir -p build
cd build

# Configure with tests enabled
cmake ..

# Build
make

# Run all tests
ctest --output-on-failure
```

### Running Individual Tests

```bash
# From the build directory
./tests/test_dmheap_unit    # Unit tests
./tests/test_dmheap_module  # Module/integration tests
./tests/test_simple         # Simple functional test
```

### Building with Coverage

To generate code coverage reports:

```bash
# Configure with coverage enabled
cd build
cmake -DENABLE_COVERAGE=ON ..

# Build
make

# Run tests and generate coverage report
make coverage

# View coverage summary in terminal output
# HTML report will be in build/coverage/html/index.html (if lcov succeeds)
```

**Current Coverage**: With all three test suites running, the coverage is significantly improved. To see exact coverage numbers, run `make coverage` after building with `-DENABLE_COVERAGE=ON`.

## Bug Fixes

During test development, the following bugs were discovered and fixed:

**Bug 1**: `dmheap_init()` did not reset `module_list` to NULL when reinitializing the heap. This caused memory corruption when tests reused the heap buffer with `reset_heap()`.

**Fix**: Added `g_dmheap_context.module_list = NULL;` in `dmheap_init()` (line 447 of src/dmheap.c).

**Bug 2**: Infinite loop in block management when manipulating free lists. The `block->next` pointer could incorrectly point to itself, causing infinite loops.

**Fix**: Added `block_set_next()` helper function with assertion `DMOD_ASSERT(block != next)` to prevent self-referencing blocks. Fixed order of operations in `_aligned_alloc` and `create_module` to remove blocks from free list before splitting.

## Performance Benchmarking

Tests include performance benchmarking that measures:
- `malloc/free` operations
- `aligned_alloc` operations
- `realloc` operations

Benchmark results are saved to `build/benchmark_results.json` in JSON format for tracking performance over time.

Example JSON output:
```json
{
  "test_suite": "dmheap_tests",
  "timestamp": 1234567890,
  "iterations": 100,
  "malloc_free_ms": 1.234,
  "malloc_free_per_op_us": 12.34
}
```

## Test Execution Notes

- All three test suites complete successfully ✓
- Unit tests: ~0.00 seconds ✓
- Module tests: ~0.00 seconds ✓
- Simple test: <0.01 seconds ✓
- All test files compile successfully ✓
- Bug fixes for module_list initialization and infinite loop included ✓

## Adding New Tests

To add more tests:

1. Add test functions to the appropriate file:
   - Unit tests → `test_dmheap_unit.c`
   - Integration tests → `test_dmheap_module.c`
   - Simple validations → `test_simple.c`

2. Follow the existing test pattern:
   ```c
   static void test_new_feature(void) {
       printf("\n=== Testing New Feature ===\n");
       reset_heap();
       
       // Test code here
       ASSERT_TEST(condition, "Test description");
       
       printf("[INFO] New feature test completed\n");
   }
   ```

3. Call the test function from `main()`

4. Rebuild and run tests

## Troubleshooting

### Build errors
- Ensure all dependencies are available (DMOD library is fetched automatically via CMake)
- Clean the build directory and rebuild: `rm -rf build && mkdir build && cd build && cmake .. && make`

### Coverage report not generated
- Ensure `lcov` and `genhtml` are installed: `sudo apt-get install lcov`
- Use GCC or Clang compiler
- Build with `-DENABLE_COVERAGE=ON`
- Run tests with `ctest` or manually before generating coverage
- Check for `.gcda` files in build directory after running tests
