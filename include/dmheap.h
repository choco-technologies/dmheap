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

/**
 * @brief Change the module a previously-allocated block is attributed to.
 *
 * For allocations whose true owner is only known after the fact (e.g. a decompression
 * buffer allocated by generic kernel code before the caller has assigned it to a specific
 * module instance). Looks the block up by its existing address - the pointer itself never
 * changes, only which module's bucket it is tracked under. Creates the target module if it
 * is not already registered.
 *
 * @param ctx           Pointer to the heap context (NULL to use default context).
 * @param ptr           Pointer previously returned by _malloc/_aligned_alloc/_realloc.
 * @param new_module_name Name of the module to attribute the block to from now on.
 *
 * @return true if the block was found and retagged, false otherwise.
 */
DMOD_BUILTIN_API( dmheap, 1.0, bool             , _retag, ( dmheap_context_t* ctx, void* ptr, const char* new_module_name ) );

/**
 * @brief Aggregate statistics about the heap's current state.
 */
typedef struct dmheap_stats_t
{
    size_t heap_size;              //!< Total heap size (bytes), as passed to dmheap_init.
    size_t free_bytes;             //!< Sum of usable (data) bytes across all free blocks.
    size_t used_bytes;             //!< Sum of usable (data) bytes across all used blocks.
    size_t free_block_count;       //!< Number of free blocks.
    size_t used_block_count;       //!< Number of used blocks.
    size_t largest_free_block;     //!< Size (bytes) of the largest free block, 0 if none.
    size_t smallest_free_block;    //!< Size (bytes) of the smallest free block, 0 if none.
} dmheap_stats_t;

/**
 * @brief Get aggregate statistics about the heap.
 *
 * @param ctx        Pointer to the heap context (NULL to use default context).
 * @param out_stats  Filled in with the current heap statistics.
 *
 * @return true on success, false if no context is available or out_stats is NULL.
 */
DMOD_BUILTIN_API( dmheap, 1.0, bool             , _get_stats, ( dmheap_context_t* ctx, dmheap_stats_t* out_stats ) );

/**
 * @brief Callback invoked once per block by dmheap_for_each_free_block()/dmheap_for_each_used_block().
 *
 * Called while the heap's internal lock is held, so it must be fast and must not call back
 * into dmheap (allocate/free/retag) or do blocking I/O (e.g. printing) - accumulate into a
 * caller-owned buffer instead and do any slow work after the *_for_each_*_block() call
 * returns.
 *
 * @param address     The block's data address (as returned by an allocation function).
 * @param size        The block's usable data size in bytes.
 * @param owner_name  Name of the module the block is attributed to, or NULL if untracked
 *                     (always NULL for free blocks; NULL for used blocks with no known owner).
 * @param user_data   Opaque pointer passed through from the *_for_each_*_block() call.
 */
typedef void (*dmheap_block_visitor_t)( void* address, size_t size, const char* owner_name, void* user_data );

/**
 * @brief Walk every free block in the heap, in no particular guaranteed order.
 *
 * @param ctx        Pointer to the heap context (NULL to use default context).
 * @param visitor    Called once per free block (see dmheap_block_visitor_t).
 * @param user_data  Passed through to each visitor call.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void             , _for_each_free_block, ( dmheap_context_t* ctx, dmheap_block_visitor_t visitor, void* user_data ) );

/**
 * @brief Walk every used (allocated) block in the heap, in no particular guaranteed order.
 *
 * @param ctx        Pointer to the heap context (NULL to use default context).
 * @param visitor    Called once per used block (see dmheap_block_visitor_t).
 * @param user_data  Passed through to each visitor call.
 */
DMOD_BUILTIN_API( dmheap, 1.0, void             , _for_each_used_block, ( dmheap_context_t* ctx, dmheap_block_visitor_t visitor, void* user_data ) );

#endif // DMHEAP_H