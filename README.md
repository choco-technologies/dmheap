# dmheap - DMOD Heap Memory Manager

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A sophisticated, module-aware heap memory manager designed specifically for the **DMOD (Dynamic Modules)** framework. dmheap provides dynamic memory allocation with module tracking, alignment support, and efficient memory management for embedded systems.

## Table of Contents

- [Overview](#overview)
- [What is DMOD?](#what-is-dmod)
- [What is dmheap?](#what-is-dmheap)
- [Architecture](#architecture)
- [Features](#features)
- [Building](#building)
- [Testing](#testing)
- [Usage](#usage)
- [Integration Example](#integration-example)
- [API Reference](#api-reference)
- [Configuration Options](#configuration-options)
- [Contributing](#contributing)
- [License](#license)

## Overview

dmheap is a custom heap memory allocator that integrates seamlessly with the DMOD dynamic module system. It provides module-aware memory management, allowing you to track which modules own which memory blocks, automatically clean up module memory, and efficiently manage memory in embedded systems with limited resources.

## What is DMOD?

**DMOD (Dynamic Modules)** is a library that enables dynamic loading and unloading of modules in embedded systems at runtime. It allows you to:

- **Dynamically load modules**: Load functionality from `.dmf` files without recompiling
- **Manage dependencies**: Automatically handle module dependencies
- **Inter-module communication**: Modules can communicate via a common API
- **Resource management**: Efficiently manage system resources
- **Safe updates**: Update individual modules without affecting the entire system

DMOD provides a modular architecture that makes embedded systems more flexible, maintainable, and easier to extend. For more information, visit the [DMOD repository](https://github.com/choco-technologies/dmod).

## What is dmheap?

**dmheap** is a heap memory manager specifically designed to work with DMOD. It provides:

- **Module-aware allocation**: Track which module owns each memory allocation
- **Automatic cleanup**: When a module is unloaded, all its allocations are freed automatically
- **Alignment support**: Allocate memory with specific alignment requirements (critical for embedded systems)
- **Fragmentation management**: Tools to concatenate free blocks and reduce fragmentation
- **Static buffer management**: Operates on a pre-allocated buffer (no reliance on system malloc)
- **Thread-safe operations**: Uses DMOD's critical section mechanisms

dmheap can optionally provide implementations for DMOD's core memory allocation API (`Dmod_MallocEx`, `Dmod_FreeEx`, etc.), making it a complete memory management solution for DMOD-based systems.

## Architecture

### Memory Block Structure

dmheap manages memory using a linked-list based approach with two main lists:

```
┌─────────────────────────────────────────────────┐
│            dmheap Context                       │
├─────────────────────────────────────────────────┤
│  - heap_start: void*                            │
│  - heap_size: size_t                            │
│  - alignment: size_t                            │
│  - free_list: block_t*  ───────┐               │
│  - used_list: block_t*  ───┐   │               │
│  - module_list: module_t*   │   │               │
└────────────────────────────┼───┼───────────────┘
                             │   │
                ┌────────────┘   └──────────────┐
                ▼                                ▼
          ┌──────────┐                    ┌──────────┐
          │ Used     │                    │ Free     │
          │ Block 1  │──►Block 2──►...    │ Block 1  │──►Block 2
          └──────────┘                    └──────────┘
```

Each memory block contains:
- **next**: Pointer to the next block in the list
- **address**: Pointer to the usable memory
- **size**: Size of the usable memory
- **owner**: Pointer to the owning module (for tracking)

### Module Tracking

dmheap maintains a list of registered modules. Each allocation can be associated with a module, enabling:

1. **Automatic cleanup**: When `dmheap_unregister_module()` is called, all memory allocated by that module is automatically freed
2. **Memory tracking**: Know which module is using how much memory
3. **Safe module unloading**: Prevent memory leaks when modules are unloaded

### Alignment Handling

dmheap supports custom alignment for allocations, which is crucial for:
- DMA operations requiring aligned buffers
- SIMD operations (SSE, NEON, etc.)
- Hardware-specific alignment requirements
- Performance optimization

## Features

- ✅ **Module-aware allocations**: Track ownership of memory by modules
- ✅ **Automatic module cleanup**: Free all module memory on unregister
- ✅ **Custom alignment support**: Allocate memory with specific alignment
- ✅ **Memory reallocation**: Efficient realloc implementation
- ✅ **Fragmentation management**: Concatenate free blocks to reduce fragmentation
- ✅ **Static buffer operation**: No dependency on system malloc/free
- ✅ **Thread-safe**: Uses DMOD critical sections for synchronization
- ✅ **Comprehensive logging**: Integration with DMOD logging system
- ✅ **Zero external dependencies**: Only requires DMOD framework
- ✅ **Optional DMOD API implementation**: Can provide standard DMOD memory functions

## Building

### Prerequisites

- **CMake**: Version 3.10 or higher
- **C Compiler**: GCC or compatible
- **Make**: For Makefile-based builds (optional)

### Using CMake

```bash
# Clone the repository
git clone https://github.com/choco-technologies/dmheap.git
cd dmheap

# Create build directory
mkdir build
cd build

# Configure
cmake ..

# Build
make

# Run tests
ctest --verbose
```

### Build Options

You can customize the build with these CMake options:

```bash
# Disable DMOD API implementation (see Configuration Options below)
cmake -DDMHEAP_DONT_IMPLEMENT_DMOD_API=ON ..

# Disable tests
cmake -DBUILD_TESTS=OFF ..

# Change DMOD mode
cmake -DDMOD_MODE=DMOD_EMBEDDED ..
```

### Using Makefile

dmheap also supports traditional Makefile builds:

```bash
# Build the library
make

# The library will be created as libdmheap.a
```

The Makefile is automatically generated by CMake and includes necessary DMOD paths.

## Testing

dmheap includes comprehensive test suites:

### Test Suites

1. **dmheap_unit**: Unit tests for core functionality
   - Initialization
   - Module registration
   - Basic allocation/deallocation
   - Aligned allocations
   - Reallocation
   - Fragmentation handling
   - Stress tests

2. **dmheap_module**: Module-level integration tests
   - FileSystem simulation
   - Network buffer pool
   - Multi-module usage
   - Dynamic data structures
   - Memory reuse patterns
   - Alignment requirements

3. **simple_test**: Basic smoke tests

### Running Tests

```bash
# Run all tests
cd build
ctest

# Run with verbose output
ctest --verbose

# Run specific test
./tests/test_dmheap_unit
./tests/test_dmheap_module
./tests/test_simple
```

### Example Test Output

```
=== Simple DMHEAP Test ===
[INFO] dmheap: Initialized with buffer 0x7ffd... of size 65536.
Init: PASS
Is Initialized: PASS
Malloc: PASS
Write: PASS
Free: PASS

All simple tests completed!
```

## Usage

### Basic Usage

```c
#include "dmheap.h"
#include <string.h>

// 1. Define your heap buffer
#define HEAP_SIZE (64 * 1024)  // 64KB
static char heap_buffer[HEAP_SIZE];

int main(void) {
    // 2. Initialize the heap
    bool success = dmheap_init(heap_buffer, HEAP_SIZE, 8);  // 8-byte alignment
    if (!success) {
        // Handle initialization failure
        return -1;
    }
    
    // 3. Allocate memory
    void* ptr = dmheap_malloc(256, "main");
    if (ptr != NULL) {
        // Use the memory
        memset(ptr, 0, 256);
        
        // 4. Free memory
        dmheap_free(ptr, false);
    }
    
    return 0;
}
```

### Module-Aware Usage

```c
#include "dmheap.h"

void module_example(void) {
    // Register a module
    dmheap_register_module("my_module");
    
    // Allocate memory for this module
    void* data1 = dmheap_malloc(512, "my_module");
    void* data2 = dmheap_malloc(1024, "my_module");
    
    // Use the memory...
    
    // When done with the module, unregister it
    // This automatically frees ALL memory allocated by "my_module"
    dmheap_unregister_module("my_module");
    // data1 and data2 are now freed automatically!
}
```

### Aligned Allocation

```c
#include "dmheap.h"
#include <assert.h>
#include <stdint.h>

void aligned_example(void) {
    // Allocate 1KB with 64-byte alignment (for DMA, SIMD, etc.)
    void* aligned_buffer = dmheap_aligned_alloc(64, 1024, "dma_module");
    
    // Verify alignment
    assert((uintptr_t)aligned_buffer % 64 == 0);
    
    // Use the buffer...
    
    // Free when done
    dmheap_free(aligned_buffer, true);  // true = concatenate free blocks
}
```

### Memory Reallocation

```c
#include "dmheap.h"

void realloc_example(void) {
    // Initial allocation
    void* buffer = dmheap_malloc(100, "data_processor");
    memcpy(buffer, "Hello", 6);
    
    // Need more space
    buffer = dmheap_realloc(buffer, 200, "data_processor");
    // Original data is preserved, now have 200 bytes
    
    // Need less space
    buffer = dmheap_realloc(buffer, 50, "data_processor");
    // Data preserved, memory reduced
    
    dmheap_free(buffer, false);
}
```

### Fragmentation Management

```c
#include "dmheap.h"

void fragmentation_example(void) {
    // Allocate and free in a pattern that creates fragmentation
    void* a = dmheap_malloc(100, "test");
    void* b = dmheap_malloc(100, "test");
    void* c = dmheap_malloc(100, "test");
    
    dmheap_free(b, false);  // Creates a hole in the middle
    
    // Manually concatenate all free blocks to reduce fragmentation
    dmheap_concatenate_free_blocks();
    
    // Or use concatenate parameter when freeing
    dmheap_free(a, true);  // Automatically tries to merge with adjacent free blocks
    dmheap_free(c, true);
}
```

## Integration Example

Here's a complete example of integrating dmheap into a DMOD-based embedded project:

### Project Structure

```
my_embedded_project/
├── CMakeLists.txt
├── main.c
├── modules/
│   ├── sensor_module/
│   └── network_module/
└── lib/
    └── dmheap/  (this repository)
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(my_embedded_project C)

# Add dmheap as a subdirectory
add_subdirectory(lib/dmheap)

# Your main executable
add_executable(my_app main.c)

# Link with dmheap
target_link_libraries(my_app PRIVATE dmheap)
```

### main.c

```c
#include "dmheap.h"
#include "dmod.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Define heap for the entire system
#define SYSTEM_HEAP_SIZE (256 * 1024)  // 256KB
static char system_heap[SYSTEM_HEAP_SIZE];

// Simulated sensor module
void sensor_module_init(void) {
    dmheap_register_module("sensor");
    
    // Allocate sensor data buffer
    typedef struct {
        float temperature;
        float humidity;
        uint32_t timestamp;
    } sensor_data_t;
    
    sensor_data_t* data = dmheap_malloc(sizeof(sensor_data_t), "sensor");
    if (data != NULL) {
        data->temperature = 25.5f;
        data->humidity = 60.0f;
        data->timestamp = 12345;
        
        printf("Sensor module initialized with data at %p\n", data);
        // Note: In real code, you'd store this pointer for later use
    }
}

void sensor_module_cleanup(void) {
    // Automatically frees all sensor module allocations
    dmheap_unregister_module("sensor");
    printf("Sensor module cleaned up\n");
}

// Simulated network module
void network_module_init(void) {
    dmheap_register_module("network");
    
    // Allocate network buffers (need DMA alignment)
    #define NUM_NET_BUFFERS 10
    #define NET_BUFFER_SIZE 1500  // MTU size
    
    for (int i = 0; i < NUM_NET_BUFFERS; i++) {
        void* buffer = dmheap_aligned_alloc(64, NET_BUFFER_SIZE, "network");
        if (buffer == NULL) {
            printf("Failed to allocate network buffer %d\n", i);
            break;
        }
        // In real code, you'd store these pointers in a buffer pool
    }
    
    printf("Network module initialized with %d buffers\n", NUM_NET_BUFFERS);
}

void network_module_cleanup(void) {
    // Automatically frees all network module allocations
    dmheap_unregister_module("network");
    printf("Network module cleaned up\n");
}

int main(void) {
    printf("=== Embedded System with dmheap ===\n\n");
    
    // Initialize heap for the entire system
    if (!dmheap_init(system_heap, SYSTEM_HEAP_SIZE, 8)) {
        printf("ERROR: Failed to initialize heap\n");
        return -1;
    }
    printf("Heap initialized: %d KB\n\n", SYSTEM_HEAP_SIZE / 1024);
    
    // Initialize modules
    sensor_module_init();
    network_module_init();
    
    printf("\n--- Modules running ---\n");
    printf("Both modules are using heap memory...\n");
    
    // Simulate running for a while
    printf("\n--- Shutting down sensor module ---\n");
    sensor_module_cleanup();
    
    printf("\n--- Network module still running ---\n");
    printf("Network module continues to function...\n");
    
    // Clean up everything
    printf("\n--- System shutdown ---\n");
    network_module_cleanup();
    
    printf("\nAll modules cleaned up successfully!\n");
    
    return 0;
}
```

### Build and Run

```bash
mkdir build && cd build
cmake ..
make
./my_app
```

### Expected Output

```
=== Embedded System with dmheap ===

Heap initialized: 256 KB

[INFO] dmheap: Initialized with buffer 0x... of size 262144.
[INFO] dmheap: Module sensor registered successfully.
Sensor module initialized with data at 0x...
[INFO] dmheap: Module network registered successfully.
Network module initialized with 10 buffers

--- Modules running ---
Both modules are using heap memory...

--- Shutting down sensor module ---
[INFO] dmheap: Module sensor unregistered successfully.
Sensor module cleaned up

--- Network module still running ---
Network module continues to function...

--- System shutdown ---
[INFO] dmheap: Module network unregistered successfully.
Network module cleaned up

All modules cleaned up successfully!
```

## API Reference

### Initialization

#### `dmheap_init`

```c
bool dmheap_init(void* buffer, size_t size, size_t alignment);
```

Initialize the heap with a given buffer and size.

- **Parameters:**
  - `buffer`: Pointer to the memory buffer to be used as heap
  - `size`: Size of the memory buffer in bytes
  - `alignment`: Default alignment for allocations (must be power of 2)
- **Returns:** `true` if initialization is successful, `false` otherwise
- **Thread-safe:** Yes

#### `dmheap_is_initialized`

```c
bool dmheap_is_initialized(void);
```

Check if the heap is initialized.

- **Returns:** `true` if initialized, `false` otherwise
- **Thread-safe:** Yes

### Module Management

#### `dmheap_register_module`

```c
bool dmheap_register_module(const char* module_name);
```

Register a module with the heap for memory tracking.

- **Parameters:**
  - `module_name`: Name of the module to register (max length: `DMOD_MAX_MODULE_NAME_LENGTH`)
- **Returns:** `true` if registration is successful, `false` otherwise
- **Thread-safe:** Yes

#### `dmheap_unregister_module`

```c
void dmheap_unregister_module(const char* module_name);
```

Unregister a module from the heap and automatically free all its allocations.

- **Parameters:**
  - `module_name`: Name of the module to unregister
- **Thread-safe:** Yes
- **Note:** This automatically frees ALL memory allocated by this module

### Memory Allocation

#### `dmheap_malloc`

```c
void* dmheap_malloc(size_t size, const char* module_name);
```

Allocate memory from the heap.

- **Parameters:**
  - `size`: Size of memory to allocate in bytes
  - `module_name`: Name of the module requesting allocation (for tracking, can be NULL)
- **Returns:** Pointer to the allocated memory, or NULL if allocation fails
- **Thread-safe:** Yes
- **Alignment:** Uses default alignment specified during `dmheap_init`

#### `dmheap_aligned_alloc`

```c
void* dmheap_aligned_alloc(size_t alignment, size_t size, const char* module_name);
```

Allocate aligned memory from the heap.

- **Parameters:**
  - `alignment`: Alignment requirement (must be power of 2)
  - `size`: Size of memory to allocate in bytes
  - `module_name`: Name of the module requesting allocation (for tracking, can be NULL)
- **Returns:** Pointer to the allocated memory (aligned to `alignment`), or NULL if allocation fails
- **Thread-safe:** Yes
- **Use cases:** DMA buffers, SIMD operations, hardware requirements

#### `dmheap_realloc`

```c
void* dmheap_realloc(void* ptr, size_t size, const char* module_name);
```

Reallocate memory from the heap.

- **Parameters:**
  - `ptr`: Pointer to previously allocated memory (can be NULL)
  - `size`: New size of memory to allocate in bytes
  - `module_name`: Name of the module requesting reallocation (for logging)
- **Returns:** Pointer to the reallocated memory, or NULL if reallocation fails
- **Thread-safe:** Yes
- **Behavior:**
  - If `ptr` is NULL, behaves like `dmheap_malloc`
  - If `size` is smaller, shrinks the allocation
  - If `size` is larger, allocates new memory and copies data
  - Original data is preserved up to the minimum of old and new sizes

#### `dmheap_free`

```c
void dmheap_free(void* ptr, bool concatenate);
```

Free memory back to the heap.

- **Parameters:**
  - `ptr`: Pointer to the memory to free
  - `concatenate`: If `true`, attempt to merge adjacent free blocks to reduce fragmentation
- **Thread-safe:** Yes
- **Note:** Safe to call with NULL pointer (no-op)

### Fragmentation Management

#### `dmheap_concatenate_free_blocks`

```c
void dmheap_concatenate_free_blocks(void);
```

Manually concatenate all adjacent free blocks in the heap to reduce fragmentation.

- **Thread-safe:** Yes
- **Use case:** Call periodically or when you need to allocate large contiguous blocks

## Configuration Options

### `DMHEAP_DONT_IMPLEMENT_DMOD_API`

This is a CMake option and preprocessor definition that controls whether dmheap provides implementations for DMOD's standard memory allocation API.

#### What it does:

When **NOT defined** (default behavior):
- dmheap implements these DMOD API functions:
  - `Dmod_MallocEx` → calls `dmheap_aligned_alloc`
  - `Dmod_ReallocEx` → calls `dmheap_realloc`
  - `Dmod_AlignedMallocEx` → calls `dmheap_aligned_alloc`
  - `Dmod_FreeEx` → calls `dmheap_free`
  - `Dmod_FreeModule` → calls `dmheap_unregister_module`

When **defined**:
- dmheap does NOT implement these functions
- You must provide your own implementations elsewhere in your project
- dmheap only provides its own `dmheap_*` functions

#### Why use it?

**Use `DMHEAP_DONT_IMPLEMENT_DMOD_API=ON` when:**

1. **You have another memory allocator**: If you want to use a different allocator for DMOD's memory functions
2. **Custom memory management**: You need custom implementations of the DMOD memory API
3. **Multiple memory heaps**: You're using dmheap for specific purposes but have another allocator for general use
4. **Static linking conflicts**: You're linking multiple libraries that might provide DMOD API implementations

**Use default (OFF) when:**

1. **Simple integration**: You want dmheap to handle all DMOD memory allocation
2. **Single allocator**: dmheap is your only memory allocator
3. **Quick setup**: You want minimal configuration

#### Example usage:

**CMake:**
```bash
# Enable the option
cmake -DDMHEAP_DONT_IMPLEMENT_DMOD_API=ON ..

# Or in CMakeLists.txt
set(DMHEAP_DONT_IMPLEMENT_DMOD_API ON CACHE BOOL "Don't implement DMOD API")
```

**Source code:**
```c
// If you want to provide your own implementations
#define DMHEAP_DONT_IMPLEMENT_DMOD_API
#include "dmheap.h"

// Now you must implement these yourself:
DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void*, _MallocEx, (size_t Size, const char* ModuleName)) {
    // Your custom implementation
    return my_custom_malloc(Size);
}

// ... implement other DMOD memory functions
```

#### Implementation details:

In `dmheap.c`, the DMOD API implementations are conditionally compiled:

```c
#ifndef DMHEAP_DONT_IMPLEMENT_DMOD_API
DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void*, _MallocEx, ( size_t Size, const char* ModuleName ))
{
    return dmheap_aligned_alloc( g_dmheap_context.alignment, Size, ModuleName );
}

// ... other DMOD API implementations
#endif // DMHEAP_DONT_IMPLEMENT_DMOD_API
```

This allows you to:
- Use dmheap as a standalone heap manager with its own API
- Integrate with DMOD without conflicts
- Provide custom DMOD memory implementations if needed

## Contributing

Contributions are welcome! Please feel free to submit issues, fork the repository, and create pull requests.

### Development Setup

1. Fork the repository
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/dmheap.git`
3. Create a feature branch: `git checkout -b feature/my-new-feature`
4. Make your changes and add tests
5. Run tests: `cd build && ctest`
6. Commit your changes: `git commit -am 'Add some feature'`
7. Push to the branch: `git push origin feature/my-new-feature`
8. Submit a pull request

### Code Style

- Follow the existing code style
- Use meaningful variable and function names
- Add comments for complex logic
- Update documentation for API changes

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

```
MIT License

Copyright (c) 2025 Choco-Technologies

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Acknowledgments

- [DMOD (Dynamic Modules)](https://github.com/choco-technologies/dmod) - The dynamic module loading framework
- Choco-Technologies team for creating and maintaining this project

## Related Projects

- [DMOD](https://github.com/choco-technologies/dmod) - Dynamic Module Loading Framework
- [DMOD Examples](https://github.com/choco-technologies/dmod/tree/develop/examples) - Example projects using DMOD

---

**For more information and support, please visit the [dmheap repository](https://github.com/choco-technologies/dmheap) or contact the Choco-Technologies team.** 
