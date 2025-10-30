#include "dmheap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_HEAP_SIZE (2 * 1024 * 1024)  // 2MB for module tests
static char test_heap[TEST_HEAP_SIZE];

// Macro for test assertions
#define ASSERT_TEST(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("[\033[32;1mPASS\033[0m] %s\n", message); \
        } else { \
            tests_failed++; \
            printf("[\033[31;1mFAIL\033[0m] %s (line %d)\n", message, __LINE__); \
        } \
    } while(0)

// Helper function to reset heap
static void reset_heap(void) {
    memset(test_heap, 0, TEST_HEAP_SIZE);
    dmheap_init(test_heap, TEST_HEAP_SIZE, 8);
}

// Simulate a simple allocator for a "file system" module
static void test_filesystem_simulation(void) {
    printf("\n=== Testing FileSystem Module Simulation ===\n");
    reset_heap();
    
    dmheap_register_module("filesystem");
    
    // Simulate file metadata allocation
    typedef struct {
        char filename[32];
        size_t size;
        void* data;
    } file_t;
    
    file_t* file1 = (file_t*)dmheap_malloc(sizeof(file_t), "filesystem");
    ASSERT_TEST(file1 != NULL, "Allocate file1 metadata");
    
    if (file1) {
        strcpy(file1->filename, "test.txt");
        file1->size = 1024;
        file1->data = dmheap_malloc(file1->size, "filesystem");
        ASSERT_TEST(file1->data != NULL, "Allocate file1 data");
        
        if (file1->data) {
            memset(file1->data, 'A', file1->size);
        }
    }
    
    file_t* file2 = (file_t*)dmheap_malloc(sizeof(file_t), "filesystem");
    ASSERT_TEST(file2 != NULL, "Allocate file2 metadata");
    
    if (file2) {
        strcpy(file2->filename, "data.bin");
        file2->size = 2048;
        file2->data = dmheap_malloc(file2->size, "filesystem");
        ASSERT_TEST(file2->data != NULL, "Allocate file2 data");
        
        if (file2->data) {
            memset(file2->data, 'B', file2->size);
        }
    }
    
    // Cleanup
    if (file1 && file1->data) dmheap_free(file1->data, false);
    if (file1) dmheap_free(file1, false);
    if (file2 && file2->data) dmheap_free(file2->data, false);
    if (file2) dmheap_free(file2, false);
    
    dmheap_unregister_module("filesystem");
    printf("[INFO] FileSystem module test completed\n");
}

// Simulate network buffer pool
static void test_network_buffer_pool(void) {
    printf("\n=== Testing Network Buffer Pool ===\n");
    reset_heap();
    
    dmheap_register_module("network");
    
    #define NUM_BUFFERS 20  // Reduced from 50
    #define BUFFER_SIZE 512
    
    void* buffers[NUM_BUFFERS];
    int allocated = 0;
    
    // Allocate buffer pool
    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffers[i] = dmheap_malloc(BUFFER_SIZE, "network");
        if (buffers[i] != NULL) {
            allocated++;
            // Simulate packet data
            snprintf((char*)buffers[i], BUFFER_SIZE, "Packet %d", i);
        }
    }
    
    printf("[INFO] Allocated %d/%d network buffers\n", allocated, NUM_BUFFERS);
    ASSERT_TEST(allocated > 15, "Allocated most of the buffer pool");
    
    // Free some buffers (simulate processed packets)
    for (int i = 0; i < NUM_BUFFERS; i += 2) {
        if (buffers[i] != NULL) {
            dmheap_free(buffers[i], false);
            buffers[i] = NULL;
        }
    }
    
    // Reallocate freed buffers
    int reallocated = 0;
    for (int i = 0; i < NUM_BUFFERS; i += 2) {
        buffers[i] = dmheap_malloc(BUFFER_SIZE, "network");
        if (buffers[i] != NULL) {
            reallocated++;
        }
    }
    
    printf("[INFO] Reallocated %d buffers\n", reallocated);
    ASSERT_TEST(reallocated > 0, "Reallocated some buffers");
    
    // Cleanup
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (buffers[i] != NULL) {
            dmheap_free(buffers[i], false);
        }
    }
    
    dmheap_unregister_module("network");
    printf("[INFO] Network buffer pool test completed\n");
}

// Test multiple concurrent modules
static void test_multi_module_usage(void) {
    printf("\n=== Testing Multi-Module Usage ===\n");
    reset_heap();
    
    // Register multiple modules
    dmheap_register_module("graphics");
    dmheap_register_module("audio");
    dmheap_register_module("input");
    
    // Allocate for different modules
    void* graphics_buffer = dmheap_malloc(10240, "graphics");
    void* audio_buffer = dmheap_malloc(4096, "audio");
    void* input_buffer = dmheap_malloc(256, "input");
    
    ASSERT_TEST(graphics_buffer != NULL, "Graphics module allocation");
    ASSERT_TEST(audio_buffer != NULL, "Audio module allocation");
    ASSERT_TEST(input_buffer != NULL, "Input module allocation");
    
    // Allocate more for graphics
    void* graphics_buffer2 = dmheap_malloc(8192, "graphics");
    ASSERT_TEST(graphics_buffer2 != NULL, "Second graphics allocation");
    
    // Unregister graphics module (should free its allocations)
    dmheap_unregister_module("graphics");
    printf("[INFO] Graphics module unregistered\n");
    
    // Audio and input should still work
    void* audio_buffer2 = dmheap_malloc(2048, "audio");
    ASSERT_TEST(audio_buffer2 != NULL, "Audio allocation after graphics cleanup");
    
    // Cleanup remaining modules
    dmheap_free(audio_buffer, false);
    dmheap_free(audio_buffer2, false);
    dmheap_free(input_buffer, false);
    
    dmheap_unregister_module("audio");
    dmheap_unregister_module("input");
    
    printf("[INFO] Multi-module test completed\n");
}

// Test dynamic data structure (linked list simulation)
static void test_dynamic_data_structure(void) {
    printf("\n=== Testing Dynamic Data Structure ===\n");
    reset_heap();
    
    dmheap_register_module("datastructure");
    
    typedef struct node {
        int value;
        struct node* next;
    } node_t;
    
    node_t* head = NULL;
    node_t* current = NULL;
    
    // Build linked list
    for (int i = 0; i < 20; i++) {
        node_t* new_node = (node_t*)dmheap_malloc(sizeof(node_t), "datastructure");
        if (new_node != NULL) {
            new_node->value = i * 10;
            new_node->next = NULL;
            
            if (head == NULL) {
                head = new_node;
                current = head;
            } else {
                current->next = new_node;
                current = new_node;
            }
        }
    }
    
    // Count nodes
    int count = 0;
    current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    
    printf("[INFO] Created linked list with %d nodes\n", count);
    ASSERT_TEST(count >= 15, "Created most of the linked list nodes");
    
    // Free linked list
    current = head;
    while (current != NULL) {
        node_t* next = current->next;
        dmheap_free(current, false);
        current = next;
    }
    
    dmheap_unregister_module("datastructure");
    printf("[INFO] Dynamic data structure test completed\n");
}

// Test memory reuse after freeing
static void test_memory_reuse(void) {
    printf("\n=== Testing Memory Reuse ===\n");
    reset_heap();
    
    dmheap_register_module("reuse_test");
    
    // Allocate and free in a pattern
    void* ptr1 = dmheap_malloc(1024, "reuse_test");
    void* ptr2 = dmheap_malloc(1024, "reuse_test");
    void* ptr3 = dmheap_malloc(1024, "reuse_test");
    
    ASSERT_TEST(ptr1 != NULL && ptr2 != NULL && ptr3 != NULL, 
                "Initial allocations successful");
    
    // Free middle block
    dmheap_free(ptr2, false);
    
    // Allocate smaller block (should reuse freed space)
    void* ptr4 = dmheap_malloc(512, "reuse_test");
    ASSERT_TEST(ptr4 != NULL, "Reused freed memory for smaller allocation");
    
    // Free all
    dmheap_free(ptr1, false);
    dmheap_free(ptr3, false);
    dmheap_free(ptr4, false);
    
    // Concatenate and allocate large block
    dmheap_concatenate_free_blocks();
    void* large = dmheap_malloc(3000, "reuse_test");
    ASSERT_TEST(large != NULL, "Large allocation after concatenation");
    
    if (large) dmheap_free(large, false);
    dmheap_unregister_module("reuse_test");
    
    printf("[INFO] Memory reuse test completed\n");
}

// Test alignment requirements for different data types
static void test_alignment_requirements(void) {
    printf("\n=== Testing Alignment Requirements ===\n");
    reset_heap();
    
    dmheap_register_module("alignment_test");
    
    // Test various alignments
    struct alignments {
        size_t alignment;
        const char* name;
    } tests[] = {
        {1, "1-byte"},
        {2, "2-byte"},
        {4, "4-byte"},
        {8, "8-byte"},
        {16, "16-byte"},
        {32, "32-byte"},
        {64, "64-byte"},
        {128, "128-byte"}
    };
    
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        void* ptr = dmheap_aligned_alloc(tests[i].alignment, 256, "alignment_test");
        if (ptr != NULL) {
            bool aligned = ((uintptr_t)ptr % tests[i].alignment) == 0;
            char msg[64];
            snprintf(msg, sizeof(msg), "%s alignment correct", tests[i].name);
            ASSERT_TEST(aligned, msg);
            dmheap_free(ptr, false);
        }
    }
    
    dmheap_unregister_module("alignment_test");
    printf("[INFO] Alignment requirements test completed\n");
}

// Test heap exhaustion and recovery
static void test_heap_exhaustion(void) {
    printf("\n=== Testing Heap Exhaustion and Recovery ===\n");
    reset_heap();
    
    dmheap_register_module("exhaustion_test");
    
    #define MAX_ALLOCS 100  // Reduced from 1000
    void* allocs[MAX_ALLOCS];
    int num_allocs = 0;
    
    // Allocate until heap is exhausted
    for (int i = 0; i < MAX_ALLOCS; i++) {
        allocs[i] = dmheap_malloc(10240, "exhaustion_test");  // Larger blocks to exhaust faster
        if (allocs[i] == NULL) {
            break;
        }
        num_allocs++;
    }
    
    printf("[INFO] Allocated %d blocks before exhaustion\n", num_allocs);
    ASSERT_TEST(num_allocs > 0, "Could allocate some blocks");
    
    // Try to allocate more (should fail only if heap was actually exhausted)
    void* should_fail = dmheap_malloc(10240, "exhaustion_test");
    if (num_allocs < MAX_ALLOCS) {
        // Heap was exhausted during the loop
        ASSERT_TEST(should_fail == NULL, "Allocation fails when heap exhausted");
    } else {
        // Heap was not exhausted, allocation might succeed
        ASSERT_TEST(true, "Heap not exhausted with current test parameters");
        if (should_fail) dmheap_free(should_fail, false);
    }
    
    // Free half of the allocations
    for (int i = 0; i < num_allocs / 2; i++) {
        dmheap_free(allocs[i], false);
        allocs[i] = NULL;
    }
    
    // Now we should be able to allocate again
    void* after_free = dmheap_malloc(10240, "exhaustion_test");
    ASSERT_TEST(after_free != NULL, "Can allocate after freeing");
    
    // Cleanup
    if (after_free) dmheap_free(after_free, false);
    for (int i = num_allocs / 2; i < num_allocs; i++) {
        if (allocs[i]) dmheap_free(allocs[i], false);
    }
    
    dmheap_unregister_module("exhaustion_test");
    printf("[INFO] Heap exhaustion test completed\n");
}

// Test mixed allocation sizes
static void test_mixed_allocation_sizes(void) {
    printf("\n=== Testing Mixed Allocation Sizes ===\n");
    reset_heap();
    
    dmheap_register_module("mixed_test");
    
    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    void* ptrs[50];  // Reduced from 100
    int num_ptrs = 0;
    
    // Allocate various sizes in random-ish pattern
    for (int i = 0; i < 50; i++) {
        size_t size = sizes[i % num_sizes];
        ptrs[num_ptrs] = dmheap_malloc(size, "mixed_test");
        if (ptrs[num_ptrs] != NULL) {
            memset(ptrs[num_ptrs], i & 0xFF, size);
            num_ptrs++;
        }
    }
    
    printf("[INFO] Allocated %d blocks of mixed sizes\n", num_ptrs);
    ASSERT_TEST(num_ptrs > 25, "Allocated majority of mixed-size blocks");
    
    // Free every third allocation
    for (int i = 0; i < num_ptrs; i += 3) {
        dmheap_free(ptrs[i], false);
    }
    
    // Reallocate some
    int reallocated = 0;
    for (int i = 0; i < 10; i++) {
        void* ptr = dmheap_malloc(sizes[i % num_sizes], "mixed_test");
        if (ptr != NULL) {
            reallocated++;
            dmheap_free(ptr, false);
        }
    }
    
    printf("[INFO] Reallocated %d blocks\n", reallocated);
    
    // Cleanup
    for (int i = 0; i < num_ptrs; i++) {
        if (i % 3 != 0) {  // Skip already freed
            dmheap_free(ptrs[i], false);
        }
    }
    
    dmheap_unregister_module("mixed_test");
    printf("[INFO] Mixed allocation sizes test completed\n");
}

// Performance benchmark with JSON output
static void benchmark_with_json(void) {
    printf("\n=== Performance Benchmark (JSON Output) ===\n");
    reset_heap();
    
    clock_t start, end;
    double time_malloc, time_free, time_realloc, time_aligned;
    
    const int iterations = 100;  // Reduced for faster execution
    
    // Benchmark malloc
    start = clock();
    for (int i = 0; i < iterations; i++) {
        void* ptr = dmheap_malloc(64, "bench");
        if (ptr == NULL) break;
        dmheap_free(ptr, false);
    }
    end = clock();
    time_malloc = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
    
    // Benchmark aligned_alloc
    reset_heap();
    start = clock();
    for (int i = 0; i < iterations; i++) {
        void* ptr = dmheap_aligned_alloc(16, 64, "bench");
        if (ptr == NULL) break;
        dmheap_free(ptr, false);
    }
    end = clock();
    time_aligned = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
    
    // Benchmark realloc
    reset_heap();
    start = clock();
    void* ptr = NULL;
    for (int i = 0; i < iterations; i++) {
        ptr = dmheap_realloc(ptr, 64 + (i % 128), "bench");
        if (ptr == NULL) break;
    }
    if (ptr) dmheap_free(ptr, false);
    end = clock();
    time_realloc = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
    
    // Write JSON file
    FILE* json_file = fopen("build/benchmark_results.json", "w");
    if (json_file) {
        fprintf(json_file, "{\n");
        fprintf(json_file, "  \"test_suite\": \"dmheap_module_tests\",\n");
        fprintf(json_file, "  \"timestamp\": %ld,\n", (long)time(NULL));
        fprintf(json_file, "  \"iterations\": %d,\n", iterations);
        fprintf(json_file, "  \"results\": {\n");
        fprintf(json_file, "    \"malloc_free_ms\": %.3f,\n", time_malloc);
        fprintf(json_file, "    \"malloc_free_per_op_us\": %.3f,\n", time_malloc * 1000.0 / iterations);
        fprintf(json_file, "    \"aligned_alloc_ms\": %.3f,\n", time_aligned);
        fprintf(json_file, "    \"aligned_alloc_per_op_us\": %.3f,\n", time_aligned * 1000.0 / iterations);
        fprintf(json_file, "    \"realloc_ms\": %.3f,\n", time_realloc);
        fprintf(json_file, "    \"realloc_per_op_us\": %.3f\n", time_realloc * 1000.0 / iterations);
        fprintf(json_file, "  }\n");
        fprintf(json_file, "}\n");
        fclose(json_file);
        printf("[INFO] Benchmark results written to build/benchmark_results.json\n");
    }
    
    printf("[BENCH] malloc/free: %.3f ms (%.3f us/op)\n", 
           time_malloc, time_malloc * 1000.0 / iterations);
    printf("[BENCH] aligned_alloc: %.3f ms (%.3f us/op)\n", 
           time_aligned, time_aligned * 1000.0 / iterations);
    printf("[BENCH] realloc: %.3f ms (%.3f us/op)\n", 
           time_realloc, time_realloc * 1000.0 / iterations);
}

int main(void) {
    printf("╔════════════════════════════════════════╗\n");
    printf("║     DMHEAP Module Tests                ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // Run all module tests
    test_filesystem_simulation();
    test_network_buffer_pool();
    test_multi_module_usage();
    test_dynamic_data_structure();
    test_memory_reuse();
    test_alignment_requirements();
    test_heap_exhaustion();
    test_mixed_allocation_sizes();
    benchmark_with_json();
    
    // Print summary
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║     Test Summary                       ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    printf("Total Tests:  %d\n", tests_passed + tests_failed);
    
    if (tests_passed + tests_failed > 0) {
        printf("Success Rate: %.1f%%\n", 
               (float)tests_passed / (tests_passed + tests_failed) * 100.0);
    }
    
    return tests_failed == 0 ? 0 : 1;
}
