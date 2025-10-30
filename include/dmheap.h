#ifndef DMHEAP_H
#define DMHEAP_H

#include "dmod.h"

/**
 * @brief Initialize the heap with a given buffer and size.
 * 
 * @param buffer Pointer to the memory buffer to be used as heap.
 * @param size   Size of the memory buffer.
 * @param alignment Alignment for allocations.
 * 
 * @return true if initialization is successful, false otherwise.
 */
DMOD_BUILTIN_API( dmheap, 1.0, bool             , _init, ( void* buffer, size_t size, size_t alignment ) );
/**
 * @brief Check if the heap is initialized.
 * 
 * @return true if initialized, false otherwise.
 */
DMOD_BUILTIN_API( dmheap, 1.0, bool             , _is_initialized, ( void ) );
/**
 * @brief Register a module with the heap.
 * 
 * @param module_name Name of the module to register.
 * 
 * @return true if registration is successful, false otherwise.
 */
DMOD_BUILTIN_API( dmheap, 1.0, bool             , _register_module, ( const char* module_name ) );
/**
 * @brief Unregister a module from the heap.
 * 
 * @param module_name Name of the module to unregister.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void             , _unregister_module, ( const char* module_name ) );
/**
 * @brief Allocate memory from the heap.
 * 
 * @param size        Size of memory to allocate.
 * @param module_name Name of the module requesting allocation (for logging).
 * 
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void*            , _malloc, ( size_t size, const char* module_name ) );
/**
 * @brief Reallocate memory from the heap.
 * 
 * @param ptr         Pointer to the previously allocated memory.
 * @param size        New size of memory to allocate.
 * @param module_name Name of the module requesting reallocation (for logging).
 * 
 * @return Pointer to the reallocated memory, or NULL if reallocation fails.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void*            , _realloc, ( void* ptr, size_t size, const char* module_name) );
/**
 * @brief Free memory back to the heap.
 * 
 * @param ptr         Pointer to the memory to free.
 * @param concatenate If true, attempt to merge adjacent free blocks to avoid fragmentation.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void             , _free, ( void* ptr, bool concatenate ) );
/**
 * @brief Allocate aligned memory from the heap.
 * 
 * @param alignment Alignment requirement.
 * @param size      Size of memory to allocate.
 * @param module_name Name of the module requesting allocation (for logging).
 * 
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void*            , _aligned_alloc, ( size_t alignment, size_t size, const char* module_name) );
/**
 * @brief Concatenate all adjacent free blocks in the heap.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void             , _concatenate_free_blocks, ( void ) );

#endif // DMHEAP_H