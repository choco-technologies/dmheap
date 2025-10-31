#include "dmheap.h"
#include <stdio.h>
#include <string.h>

#define TEST_HEAP_SIZE (1024 * 1024)  
static char test_heap[TEST_HEAP_SIZE];

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TEST(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("[PASS] %s\n", message); \
        } else { \
            tests_failed++; \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

static void reset_heap(void) {
    memset(test_heap, 0, TEST_HEAP_SIZE);
    dmheap_init(test_heap, TEST_HEAP_SIZE, 8);
}

static void test_init(void) {
    printf("\n=== Testing Initialization ===\n");
    
    char heap[1024];
    bool result = dmheap_init(heap, 1024, 8);
    ASSERT_TEST(result == true, "Init with valid parameters");
    ASSERT_TEST(dmheap_is_initialized() == true, "Heap is initialized after init");
    
    result = dmheap_init(NULL, 1024, 8);
    ASSERT_TEST(result == false, "Init with NULL buffer should fail");
    
    result = dmheap_init(heap, 0, 8);
    ASSERT_TEST(result == false, "Init with zero size should fail");
}

static void test_basic_allocation(void) {
    printf("\n=== Testing Basic Allocation ===\n");
    reset_heap();
    
    void* ptr1 = dmheap_malloc(64, "test_module");
    ASSERT_TEST(ptr1 != NULL, "Allocate 64 bytes");
    
    void* ptr2 = dmheap_malloc(128, "test_module");
    ASSERT_TEST(ptr2 != NULL, "Allocate 128 bytes");
    ASSERT_TEST(ptr1 != ptr2, "Different pointers for different allocations");
    
    if (ptr1 && ptr2) {
        memset(ptr1, 0xAA, 64);
        memset(ptr2, 0xBB, 128);
        ASSERT_TEST(((unsigned char*)ptr1)[0] == 0xAA, "Write to first allocation");
        ASSERT_TEST(((unsigned char*)ptr2)[0] == 0xBB, "Write to second allocation");
        
        dmheap_free(ptr1, false);
        dmheap_free(ptr2, false);
    }
}

int main(void) {
    printf("╔════════════════════════════════════════╗\n");
    printf("║     DMHEAP Limited Unit Tests          ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    test_init();
    test_basic_allocation();
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║     Test Summary                       ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    
    return tests_failed == 0 ? 0 : 1;
}
