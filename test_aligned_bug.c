/**
 * Minimal standalone test to reproduce the dmheap_aligned_alloc bug
 * Compile with: gcc -I./include -I./build/_deps/dmod-src/inc -I./build/_deps/dmod-build -DDMHEAP_VERSION=\"1.0\" test_aligned_bug.c src/dmheap.c -o test_aligned_bug
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// Minimal DMOD stub implementations
#define DMOD_MAX_MODULE_NAME_LENGTH 64
#define DMOD_ASSERT(x) do { if (!(x)) { printf("ASSERT FAILED: %s:%d\n", __FILE__, __LINE__); } } while(0)
#define DMOD_ASSERT_MSG(x, msg) do { if (!(x)) { printf("ASSERT FAILED: %s:%d - %s\n", __FILE__, __LINE__, msg); } } while(0)
#define DMOD_LOG_ERROR(fmt, ...) printf("[ERROR] " fmt, ##__VA_ARGS__)
#define DMOD_LOG_WARN(fmt, ...) printf("[WARN] " fmt, ##__VA_ARGS__)
#define DMOD_LOG_INFO(fmt, ...) printf("[INFO] " fmt, ##__VA_ARGS__)
#define DMOD_BUILTIN_API(module, ver, ret, name, params) ret module##name params
#define DMOD_INPUT_API_DECLARATION(module, ver, ret, name, params) ret module##name params

void Dmod_EnterCritical(void) {}
void Dmod_ExitCritical(void) {}
void Dmod_Assert(bool condition, const char* message, const char* file, int line, const char* func) {
    if (!condition) {
        printf("DMOD_ASSERT FAILED: %s at %s:%d in %s\n", message, file, line, func);
    }
}
int Dmod_CheckLogLevel(int level) { return 1; }
void Dmod_Printf(const char* fmt, ...) {
    // Simple stub - don't print
}

#include "dmheap.h"

#define TEST_HEAP_SIZE (64 * 1024)
static char test_heap[TEST_HEAP_SIZE];

int main(void) {
    printf("=== Testing dmheap_aligned_alloc Bug ===\n\n");
    
    // Initialize heap
    bool result = dmheap_init(test_heap, TEST_HEAP_SIZE, 8);
    if (!result) {
        printf("FAILED: Unable to initialize heap\n");
        return 1;
    }
    printf("✓ Heap initialized\n");
    
    // Test 1: Aligned allocation that requires padding
    // This should trigger the bug when alignment is larger than the default
    printf("\nTest 1: Aligned allocation with alignment=64, size=128\n");
    void* ptr1 = dmheap_aligned_alloc(64, 128, "test");
    if (ptr1 == NULL) {
        printf("FAILED: Unable to allocate memory\n");
        return 1;
    }
    printf("  Allocated at address: %p\n", ptr1);
    
    // Check if the address is properly aligned
    if ((uintptr_t)ptr1 % 64 != 0) {
        printf("  ✗ FAILED: Address not aligned to 64 bytes (alignment=%lu)\n", (uintptr_t)ptr1 % 64);
        return 1;
    }
    printf("  ✓ Address is properly aligned to 64 bytes\n");
    
    // Try to write to the allocated memory
    // If the bug exists, this might corrupt the used_list
    printf("  Writing pattern to allocated memory...\n");
    memset(ptr1, 0xAA, 128);
    printf("  ✓ Write completed without crash\n");
    
    // Allocate another block to see if used_list is still intact
    printf("\nTest 2: Second allocation to verify list integrity\n");
    void* ptr2 = dmheap_malloc(64, "test");
    if (ptr2 == NULL) {
        printf("  ✗ FAILED: Unable to allocate second block (list might be corrupted)\n");
        return 1;
    }
    printf("  ✓ Second allocation succeeded at %p\n", ptr2);
    
    // Try to free the first allocation
    // If the bug exists and corrupted used_list, this will crash
    printf("\nTest 3: Free first allocation (this tests if used_list is intact)\n");
    dmheap_free(ptr1, false);
    printf("  ✓ Free succeeded without crash\n");
    
    // Free second allocation
    dmheap_free(ptr2, false);
    printf("  ✓ Second free succeeded\n");
    
    // Test 3: Try with even larger alignment to force padding
    printf("\nTest 4: Larger alignment (256 bytes) with size=512\n");
    void* ptr3 = dmheap_aligned_alloc(256, 512, "test");
    if (ptr3 == NULL) {
        printf("  ✗ FAILED: Unable to allocate with 256-byte alignment\n");
        return 1;
    }
    printf("  Allocated at address: %p\n", ptr3);
    
    if ((uintptr_t)ptr3 % 256 != 0) {
        printf("  ✗ FAILED: Address not aligned to 256 bytes\n");
        return 1;
    }
    printf("  ✓ Address is properly aligned to 256 bytes\n");
    
    // Write to it
    memset(ptr3, 0xBB, 512);
    printf("  ✓ Write completed\n");
    
    // Allocate another block
    void* ptr4 = dmheap_malloc(32, "test");
    if (ptr4 == NULL) {
        printf("  ✗ FAILED: List might be corrupted\n");
        return 1;
    }
    printf("  ✓ Another allocation succeeded\n");
    
    // Free them
    dmheap_free(ptr3, false);
    dmheap_free(ptr4, false);
    printf("  ✓ Both frees succeeded\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Note: If the bug exists, the test would have crashed during write or free operations.\n");
    
    return 0;
}
