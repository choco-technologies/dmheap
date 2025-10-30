# DMHEAP Tests

This directory contains comprehensive tests for the dmheap memory manager library.

## Test Structure

### Unit Tests (`test_dmheap_unit.c`)
Unit tests focus on testing individual functions and features of dmheap:
- Initialization tests
- Module registration/unregistration
- Basic allocation (malloc, free)
- Aligned allocation
- Reallocation
- Memory concatenation
- Edge cases and error handling
- Fragmentation handling
- Stress testing

### Module Tests (`test_dmheap_module.c`)
Module tests simulate real-world usage scenarios:
- FileSystem simulation (file metadata and data allocation)
- Network buffer pool management
- Multi-module concurrent usage
- Dynamic data structures (linked lists)
- Memory reuse patterns
- Alignment requirements for various data types
- Heap exhaustion and recovery
- Mixed allocation sizes

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
./tests/test_dmheap_unit
./tests/test_dmheap_module
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

# View coverage report
# The HTML report will be in build/coverage/html/index.html
```

The coverage report will show:
- Line coverage percentage
- Branch coverage
- Function coverage
- Detailed file-by-file coverage information

## Performance Benchmarking

The module tests include performance benchmarking that measures:
- `malloc/free` operations
- `aligned_alloc` operations
- `realloc` operations

Benchmark results are automatically saved to `build/benchmark_results.json` in JSON format for tracking performance over time.

Example JSON output:
```json
{
  "test_suite": "dmheap_module_tests",
  "timestamp": 1234567890,
  "iterations": 10000,
  "results": {
    "malloc_free_ms": 45.123,
    "malloc_free_per_op_us": 4.512,
    "aligned_alloc_ms": 52.345,
    "aligned_alloc_per_op_us": 5.234,
    "realloc_ms": 67.890,
    "realloc_per_op_us": 6.789
  }
}
```

## Test Coverage Goals

The test suite aims for:
- **>80% line coverage** ✓
- **>70% branch coverage**
- **>90% function coverage**

## Expected Test Results

All tests should pass on a properly functioning dmheap implementation. If tests fail:

1. Check the error messages for details
2. Review the specific test case that failed
3. Use coverage reports to identify untested code paths
4. Debug using the detailed test output

## Continuous Integration

These tests are designed to be run in CI/CD pipelines. The JSON benchmark output allows tracking performance regressions over time.

## Adding New Tests

To add new tests:

1. Add test functions to the appropriate file:
   - Unit tests → `test_dmheap_unit.c`
   - Module/integration tests → `test_dmheap_module.c`

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

4. Rebuild and run tests to verify

## Test Macros

- `ASSERT_TEST(condition, message)` - Asserts a condition and tracks pass/fail
- `reset_heap()` - Reinitializes the test heap for a clean test environment

## Troubleshooting

### Tests crash or hang
- Check heap size is sufficient for the test
- Verify dmheap is properly initialized before use
- Look for memory corruption issues

### Coverage report not generated
- Ensure `lcov` and `genhtml` are installed
- Use GCC or Clang compiler
- Build with `-DENABLE_COVERAGE=ON`

### Benchmark results not created
- Ensure the `build` directory exists
- Check write permissions
- Verify tests complete successfully
