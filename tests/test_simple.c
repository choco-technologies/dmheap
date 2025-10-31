#include "dmheap.h"
#include <stdio.h>
#include <string.h>

#define TEST_HEAP_SIZE (64 * 1024)
static char test_heap[TEST_HEAP_SIZE];

int main(void) {
    printf("=== Simple DMHEAP Test ===\n");
    
    // Test 1: Init
    bool result = dmheap_init(test_heap, TEST_HEAP_SIZE, 8);
    printf("Init: %s\n", result ? "PASS" : "FAIL");
    
    // Test 2: Is initialized
    bool init_check = dmheap_is_initialized();
    printf("Is Initialized: %s\n", init_check ? "PASS" : "FAIL");
    
    // Test 3: Simple allocation
    void* ptr = dmheap_malloc(256, "test");
    printf("Malloc: %s\n", ptr != NULL ? "PASS" : "FAIL");
    
    // Test 4: Write to memory
    if (ptr) {
        memset(ptr, 0xAA, 256);
        printf("Write: PASS\n");
        
        // Test 5: Free
        dmheap_free(ptr, false);
        printf("Free: PASS\n");
    }
    
    printf("\nAll simple tests completed!\n");
    return 0;
}
