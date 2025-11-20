#ifndef DMHEAP_H
#define DMHEAP_H

#include "dmod.h"

/**
 * @brief Opaque type for dmheap context.
 */
typedef struct dmheap_context_t dmheap_context_t;

/**
 * @brief Initialize the heap with a given buffer and size.
 * 
 * @param buffer Pointer to the memory buffer to be used as heap.
 * @param size   Size of the memory buffer.
 * @param alignment Alignment for allocations.
 * 
 * @return Pointer to the heap context, or NULL if initialization fails.
 */
DMOD_BUILTIN_API( dmheap, 1.0, dmheap_context_t*, _init, ( void* buffer, size_t size, size_t alignment ) );
/**
 * @brief Set the default heap context to use when NULL is passed to API functions.
 * 
 * @param ctx Pointer to the heap context to set as default.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void             , _set_default_context, ( dmheap_context_t* ctx ) );
/**
 * @brief Get the default heap context.
 * 
 * @return Pointer to the default heap context, or NULL if not set.
 */
DMOD_BUILTIN_API( dmheap, 1.0, dmheap_context_t*, _get_default_context, ( void ) );
/**
 * @brief Check if the heap is initialized.
 * 
 * @param ctx Pointer to the heap context (NULL to use default context).
 * 
 * @return true if initialized, false otherwise.
 */
DMOD_BUILTIN_API( dmheap, 1.0, bool             , _is_initialized, ( dmheap_context_t* ctx ) );
/**
 * @brief Register a module with the heap.
 * 
 * @param ctx         Pointer to the heap context (NULL to use default context).
 * @param module_name Name of the module to register.
 * 
 * @return true if registration is successful, false otherwise.
 */
DMOD_BUILTIN_API( dmheap, 1.0, bool             , _register_module, ( dmheap_context_t* ctx, const char* module_name ) );
/**
 * @brief Unregister a module from the heap.
 * 
 * @param ctx         Pointer to the heap context (NULL to use default context).
 * @param module_name Name of the module to unregister.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void             , _unregister_module, ( dmheap_context_t* ctx, const char* module_name ) );
/**
 * @brief Allocate memory from the heap.
 * 
 * @param ctx         Pointer to the heap context (NULL to use default context).
 * @param size        Size of memory to allocate.
 * @param module_name Name of the module requesting allocation (for logging).
 * 
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void*            , _malloc, ( dmheap_context_t* ctx, size_t size, const char* module_name ) );
/**
 * @brief Reallocate memory from the heap.
 * 
 * @param ctx         Pointer to the heap context (NULL to use default context).
 * @param ptr         Pointer to the previously allocated memory.
 * @param size        New size of memory to allocate.
 * @param module_name Name of the module requesting reallocation (for logging).
 * 
 * @return Pointer to the reallocated memory, or NULL if reallocation fails.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void*            , _realloc, ( dmheap_context_t* ctx, void* ptr, size_t size, const char* module_name) );
/**
 * @brief Free memory back to the heap.
 * 
 * @param ctx         Pointer to the heap context (NULL to use default context).
 * @param ptr         Pointer to the memory to free.
 * @param concatenate If true, attempt to merge adjacent free blocks to avoid fragmentation.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void             , _free, ( dmheap_context_t* ctx, void* ptr, bool concatenate ) );
/**
 * @brief Allocate aligned memory from the heap.
 * 
 * @param ctx         Pointer to the heap context (NULL to use default context).
 * @param alignment   Alignment requirement.
 * @param size        Size of memory to allocate.
 * @param module_name Name of the module requesting allocation (for logging).
 * 
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void*            , _aligned_alloc, ( dmheap_context_t* ctx, size_t alignment, size_t size, const char* module_name) );
/**
 * @brief Concatenate all adjacent free blocks in the heap.
 * 
 * @param ctx Pointer to the heap context (NULL to use default context).
 */
DMOD_BUILTIN_API( dmheap, 1.0, void             , _concatenate_free_blocks, ( dmheap_context_t* ctx ) );

#endif // DMHEAP_H