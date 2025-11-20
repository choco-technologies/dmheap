/**
 * Simple verification test to check the fix for dmheap_aligned_alloc
 * This tests the specific scenario where alignment padding is required
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Simple structure to demonstrate the bug
typedef struct {
    void* next;
    void* address;
    size_t size;
} simple_block_t;

// Simulate the old (buggy) behavior
void* buggy_aligned_alloc(uintptr_t block_addr, size_t alignment) {
    // Block structure at block_addr
    // Data starts at block_addr + sizeof(simple_block_t)
    uintptr_t data_addr = block_addr + sizeof(simple_block_t);
    
    // Calculate aligned address from data
    uintptr_t aligned = (data_addr + alignment - 1) & ~(alignment - 1);
    uintptr_t padding = aligned - data_addr;
    
    printf("Original block at: 0x%lx\n", block_addr);
    printf("Original data at: 0x%lx\n", data_addr);
    printf("Aligned address should be: 0x%lx (padding=%lu)\n", aligned, padding);
    
    if (padding > 0) {
        // Split the block - new block starts at aligned position
        uintptr_t new_block_addr = aligned;  // This is where new block structure is
        uintptr_t new_data_addr = new_block_addr + sizeof(simple_block_t);
        
        printf("After split:\n");
        printf("  New block structure at: 0x%lx\n", new_block_addr);
        printf("  New block data at: 0x%lx\n", new_data_addr);
        printf("  BUG: Returning 0x%lx (block structure, not data!)\n", aligned);
        
        return (void*)aligned;  // BUG: Returns address of block structure
    }
    
    return (void*)aligned;
}

// Simulate the fixed behavior
void* fixed_aligned_alloc(uintptr_t block_addr, size_t alignment) {
    // Block structure at block_addr
    // Data starts at block_addr + sizeof(simple_block_t)
    uintptr_t data_addr = block_addr + sizeof(simple_block_t);
    
    // Calculate aligned address from data
    uintptr_t aligned = (data_addr + alignment - 1) & ~(alignment - 1);
    uintptr_t padding = aligned - data_addr;
    
    printf("Original block at: 0x%lx\n", block_addr);
    printf("Original data at: 0x%lx\n", data_addr);
    printf("Aligned address should be: 0x%lx (padding=%lu)\n", aligned, padding);
    
    if (padding > 0) {
        // CORRECT FIX: Split at padding - sizeof(block_t) so new block data is at aligned address
        uintptr_t split_at = padding - sizeof(simple_block_t);
        uintptr_t new_block_addr = data_addr + split_at;  // Where new block structure goes
        uintptr_t new_data_addr = new_block_addr + sizeof(simple_block_t);
        
        printf("After split at %lu bytes:\n", split_at);
        printf("  New block structure at: 0x%lx\n", new_block_addr);
        printf("  New block data at: 0x%lx\n", new_data_addr);
        printf("  FIX: Returning 0x%lx (correct aligned data address!)\n", new_data_addr);
        printf("  Alignment check: aligned = %s\n", (new_data_addr % alignment == 0) ? "YES" : "NO");
        
        return (void*)new_data_addr;  // FIX: Returns correct data address
    }
    
    return (void*)aligned;
}

int main() {
    printf("=== Demonstration of the Bug and Fix ===\n\n");
    printf("sizeof(simple_block_t) = %zu\n\n", sizeof(simple_block_t));
    
    // Simulate a block starting at an address that will need padding for 64-byte alignment
    uintptr_t block_start = 0x1000;  // Arbitrary address
    size_t alignment = 64;
    
    printf("BUGGY BEHAVIOR:\n");
    printf("================\n");
    void* buggy_result = buggy_aligned_alloc(block_start, alignment);
    printf("\n");
    
    printf("FIXED BEHAVIOR:\n");
    printf("===============\n");
    void* fixed_result = fixed_aligned_alloc(block_start, alignment);
    printf("\n");
    
    printf("COMPARISON:\n");
    printf("===========\n");
    printf("Buggy returned: %p\n", buggy_result);
    printf("Fixed returned: %p\n", fixed_result);
    printf("Difference: %ld bytes\n", (uintptr_t)fixed_result - (uintptr_t)buggy_result);
    printf("\n");
    
    // Check alignment
    printf("Buggy alignment check: %s (should be aligned but points to wrong place)\n",
           ((uintptr_t)buggy_result % alignment == 0) ? "ALIGNED" : "NOT ALIGNED");
    printf("Fixed alignment check: %s\n",
           ((uintptr_t)fixed_result % alignment == 0) ? "ALIGNED" : "NOT ALIGNED");
    printf("\n");
    
    printf("EXPLANATION:\n");
    printf("============\n");
    printf("The bug returns the address of the block structure (0x%lx)\n", (uintptr_t)buggy_result);
    printf("which is aligned, but writing to it corrupts the block metadata.\n");
    printf("The fix returns the correct data address (0x%lx)\n", (uintptr_t)fixed_result);
    printf("which is properly aligned and safe to write to.\n");
    
    return 0;
}
