#include "dmheap.h"
#include <string.h>
#include <stdint.h>

/**
 * @brief Structure to represent a registered module.
 */
typedef struct module_t
{
    char name[DMOD_MAX_MODULE_NAME_LENGTH];     //!< Name of the module.
    struct module_t* next;               //!< Pointer to the next module in the list.
} module_t;

/**
 * @brief Structure to represent a memory block in the heap.
 */
typedef struct block_t
{
    struct block_t* next;       //!< Pointer to the next memory block.
    void* address;              //!< Pointer to the memory block address.
    size_t size;                //!< Size of the memory block.
    module_t* owner;            //!< Pointer to the owning module.
} block_t;


/**
 * @brief Structure to hold the context of the heap.
 */
typedef struct 
{
    void*  heap_start;      //!< Pointer to the start of the heap memory.
    size_t heap_size;       //!< Size of the heap memory.
    block_t* free_list;     //!< Pointer to the list of free memory blocks.
    block_t* used_list;     //!< Pointer to the list of used memory blocks.
    size_t alignment;       //!< Alignment for allocations.
    module_t* module_list; //!< Pointer to the list of registered modules.
} context_t;

context_t g_dmheap_context = { NULL, 0 };

/**
 * @brief Align a pointer to the specified alignment.
 * 
 * @param ptr       Pointer to be aligned.
 * @param alignment Alignment boundary.
 * 
 * @return Aligned pointer.
 */
static void* align_pointer( void* ptr, size_t alignment )
{
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t aligned_p = (p + (alignment - 1)) & ~(alignment - 1);
    return (void*)aligned_p;
}

/**
 * @brief Align a size to the specified alignment.
 * 
 * @param size      Size to be aligned.
 * @param alignment Alignment boundary.
 * 
 * @return Aligned size.
 */
static size_t align_size( size_t size, size_t alignment )
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

/**
 * @brief Set the next pointer of a block.
 * 
 * @param block Pointer to the block.
 * @param next  Pointer to the next block.
 */
static void block_set_next( block_t* block, block_t* next )
{
    DMOD_ASSERT(block != next);
    if( block != NULL )
    {
        block->next = next;
    }
}

/**
 * @brief Create a new memory block.
 * 
 * @param address Pointer to the start of the block.
 * @param size    Size of the block.
 * 
 * @return Pointer to the created block_t structure.
 */
static block_t* create_block( void* address, size_t size )
{
    block_t* block = (block_t*)address;
    block_set_next(block, NULL);
    block->address = (void*)((uintptr_t)address + sizeof(block_t));
    block->size    = size - sizeof(block_t);
    return block;
}

/**
 * @brief Split a memory block into two if it's larger than the requested size.
 * 
 * @param block Pointer to the block to be split.
 * @param size  Size of the first block after splitting.
 * 
 * @return Pointer to the new block created after splitting, or NULL if not split.
 */
static block_t* split_block( block_t* block, size_t size )
{
    if( block->size < size + sizeof(block_t) + 1 )
    {
        return NULL;
    }

    // Align the size to ensure the new block starts at an aligned address
    size_t aligned_size = align_size( size, g_dmheap_context.alignment );
    
    // Make sure we still have enough space after alignment
    if( block->size < aligned_size + sizeof(block_t) + 1 )
    {
        return NULL;
    }

    void* new_block_address = (void*)((uintptr_t)block->address + aligned_size);
    block_t* new_block = create_block( new_block_address, block->size - aligned_size );
    block_set_next(new_block, block->next);
    new_block->owner = block->owner;
    block_set_next(block, new_block);
    block->size = aligned_size;

    return new_block;
}

/**
 * @brief Merge two adjacent memory blocks into one.
 * 
 * @param first  Pointer to the first block.
 * @param second Pointer to the second block.
 * 
 * @return Pointer to the merged block, or NULL if not merged.
 */
static block_t* merge_blocks( block_t* first, block_t* second )
{
    if( first == NULL || second == NULL )
    {
        return NULL;
    }
    if( (uintptr_t)first->address + first->size + sizeof(block_t) != (uintptr_t)second )
    {
        return NULL;
    }

    first->size += sizeof(block_t) + second->size;
    block_set_next(first, second->next);

    return first;
}

/**
 * @brief Remove a block from a linked list of blocks.
 * 
 * @param list_head Pointer to the head of the block list.
 * @param block_to_remove Pointer to the block to be removed.
 */
static void remove_block( block_t** list_head, block_t* block_to_remove )
{
    if( *list_head == NULL || block_to_remove == NULL )
    {
        return;
    }

    if( *list_head == block_to_remove )
    {
        *list_head = block_to_remove->next;
        return;
    }

    block_t* current = *list_head;
    while( current->next != NULL )
    {
        if( current->next == block_to_remove )
        {
            block_set_next(current, block_to_remove->next);
            block_set_next(block_to_remove, NULL);
            return;
        }
        current = current->next;
    }
}

/**
 * @brief Add a block to the front of a linked list of blocks.
 * 
 * @param list_head Pointer to the head of the block list.
 * @param block_to_add Pointer to the block to be added.
 */
static void add_block( block_t** list_head, block_t* block_to_add )
{
    if( list_head == NULL || block_to_add == NULL )
    {
        return;
    }

    block_set_next(block_to_add, *list_head);
    *list_head = block_to_add;
}

/**
 * @brief Find a suitable free block for allocation.
 * 
 * @param size      Size of memory to allocate.
 * @param alignment Alignment requirement.
 * 
 * @return Pointer to the suitable block, or NULL if none found.
 */
static block_t* find_suitable_block( size_t size, size_t alignment )
{
    block_t* current = g_dmheap_context.free_list;
    while( current != NULL )
    {
        void* aligned_address = align_pointer( current->address, alignment );
        size_t padding = (size_t)((uintptr_t)aligned_address - (uintptr_t)current->address);
        size_t min_size = size;
        if(padding > 0)
        {
            min_size += padding + sizeof(block_t);
        }
        if( current->size > min_size )
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * @brief Find a block by its address in the used list.
 * 
 * @param address Pointer to the memory address.
 * 
 * @return Pointer to the block_t structure, or NULL if not found.
 */
static block_t* find_block_by_address( void* address )
{
    block_t* current = g_dmheap_context.used_list;
    while( current != NULL )
    {
        if( current->address == address )
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * @brief Add a module to the module list.
 * 
 * @param list_head Pointer to the head of the module list.
 * @param module    Pointer to the module to be added.
 */
static void add_module_to_list( module_t** list_head, module_t* module )
{
    if( list_head == NULL || module == NULL )
    {
        return;
    }

    module->next = *list_head;
    *list_head = module;
}

/**
 * @brief Find a module by its name.
 * 
 * @param name Name of the module.
 * 
 * @return Pointer to the module_t structure, or NULL if not found.
 */
static module_t* find_module_by_name( const char* name )
{
    module_t* current = g_dmheap_context.module_list;
    while( current != NULL )
    {
        if( strncmp( current->name, name, DMOD_MAX_MODULE_NAME_LENGTH ) == 0 )
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * @brief Remove a module from the module list.
 * 
 * @param list_head Pointer to the head of the module list.
 * @param module_to_remove Pointer to the module to be removed.
 */
static void remove_module_from_list( module_t** list_head, module_t* module_to_remove )
{
    if( *list_head == NULL || module_to_remove == NULL )
    {
        return;
    }

    if( *list_head == module_to_remove )
    {
        *list_head = module_to_remove->next;
        return;
    }

    module_t* current = *list_head;
    while( current->next != NULL )
    {
        if( current->next == module_to_remove )
        {
            current->next = module_to_remove->next;
            return;
        }
        current = current->next;
    }
}

/**
 * @brief Allocate aligned memory from the heap.
 * 
 * @param alignment Alignment requirement.
 * @param size      Size of memory to allocate.
 * @param module_name Name of the module requesting allocation (for logging).
 * 
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 */
static module_t* create_module( const char* name )
{
    block_t* block = find_suitable_block( sizeof(module_t), g_dmheap_context.alignment );
    if( block == NULL )
    {
        DMOD_LOG_ERROR("dmheap: Unable to allocate memory for module %s.\n", name);
        return NULL;
    }
    remove_block( &g_dmheap_context.free_list, block );
    if(block->size > (sizeof(module_t) + sizeof(block_t) + g_dmheap_context.alignment))
    {
        block_t* new_block = split_block( block, sizeof(module_t) );
        if( new_block != NULL )
        {
            add_block( &g_dmheap_context.free_list, new_block );
        }
    }
    module_t* module = (module_t*)block->address;
    strncpy( module->name, name, DMOD_MAX_MODULE_NAME_LENGTH - 1 );
    module->name[DMOD_MAX_MODULE_NAME_LENGTH - 1] = '\0';
    add_block( &g_dmheap_context.used_list, block );
    add_module_to_list( &g_dmheap_context.module_list, module );
    return module;
}

/**
 * @brief Release all memory allocated by a specific module.
 * 
 * @param module Pointer to the module whose memory is to be released.
 */
static void release_memory_of_module( module_t* module )
{
    if( module == NULL )
    {
        return;
    }

    block_t* current = g_dmheap_context.used_list;
    block_t* prev = NULL;

    while( current != NULL )
    {
        if( current->owner == module )
        {
            block_t* to_free = current;
            if( prev == NULL )
            {
                g_dmheap_context.used_list = current->next;
                current = g_dmheap_context.used_list;
            }
            else
            {
                block_set_next(prev, current->next);
                current = prev->next;
            }
            add_block( &g_dmheap_context.free_list, to_free );
        }
        else
        {
            prev = current;
            current = current->next;
        }
    }
}

/**
 * @brief Delete a registered module and free its memory.
 * 
 * @param module Pointer to the module to be deleted.
 */
static void delete_module( module_t* module )
{
    if( module == NULL )
    {
        return;
    }

    release_memory_of_module( module );
    remove_module_from_list( &g_dmheap_context.module_list, module );

    block_t* block = find_block_by_address( (void*)module );
    if( block != NULL )
    {
        remove_block( &g_dmheap_context.used_list, block );
        add_block( &g_dmheap_context.free_list, block );
    }
}

/**
 * @brief Get or create a module by its name.
 * 
 * @param module_name Name of the module.
 * 
 * @return Pointer to the module_t structure.
 */
static module_t* get_or_create_module( const char* module_name )
{
    module_t* module = find_module_by_name( module_name );
    if( module == NULL )
    {
        module = create_module( module_name );
    }
    return module;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, bool,  _init, ( void* buffer, size_t size, size_t alignment ) )
{
    if(buffer == NULL || size == 0)
    {
        DMOD_LOG_ERROR("dmheap: _init called with invalid parameters.\n");
        return false;
    }
    Dmod_EnterCritical();
    g_dmheap_context.heap_start = buffer;
    g_dmheap_context.heap_size  = size;
    g_dmheap_context.free_list  = create_block( buffer, size );
    g_dmheap_context.used_list  = NULL;
    g_dmheap_context.alignment  = alignment;
    g_dmheap_context.module_list = NULL;  // Reset module list on initialization
    Dmod_ExitCritical();
    DMOD_LOG_INFO("== dmheap ver. %s ==\n", DMHEAP_VERSION);
    DMOD_LOG_INFO("dmheap: Initialized with buffer %p of size %lu.\n", buffer, (unsigned long)size);
    return true;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, bool,  _is_initialized, ( void ) )
{
    return g_dmheap_context.heap_start != NULL;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, bool,  _register_module, ( const char* module_name ) )
{
    Dmod_EnterCritical();
    module_t* module = find_module_by_name( module_name );
    if( module != NULL )
    {
        Dmod_ExitCritical();
        DMOD_LOG_WARN("dmheap: Module %s is already registered.\n", module_name);
        return true;
    }
    module = create_module( module_name );
    Dmod_ExitCritical();
    if( module == NULL )
    {
        DMOD_LOG_ERROR("dmheap: Failed to register module %s.\n", module_name);
        return false;
    }
    DMOD_LOG_INFO("dmheap: Module %s registered successfully.\n", module_name);
    return true;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void,  _unregister_module, ( const char* module_name ) )
{
    // it is very likely, that module_name is inside a buffer we will free, so we need to copy it first
    char module_name_copy[DMOD_MAX_MODULE_NAME_LENGTH];
    strncpy( module_name_copy, module_name, DMOD_MAX_MODULE_NAME_LENGTH - 1 );
    module_name_copy[DMOD_MAX_MODULE_NAME_LENGTH - 1] = '\0';

    Dmod_EnterCritical();
    module_t* module = find_module_by_name( module_name_copy );
    if( module == NULL )
    {
        Dmod_ExitCritical();
        DMOD_LOG_WARN("dmheap: Module %s is not registered.\n", module_name_copy);
        return;
    }
    delete_module( module );
    Dmod_ExitCritical();
    DMOD_LOG_INFO("dmheap: Module %s unregistered successfully.\n", module_name_copy);
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void*, _malloc, ( size_t size, const char* module_name ) )
{
    return dmheap_aligned_alloc( g_dmheap_context.alignment, size, module_name );
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void*, _realloc, ( void* ptr, size_t size, const char* module_name) )
{
    if( ptr == NULL )
    {
        return dmheap_aligned_alloc( g_dmheap_context.alignment, size, module_name );
    }

    Dmod_EnterCritical();
    block_t* block = find_block_by_address( ptr );
    if( block == NULL )
    {
        Dmod_ExitCritical();
        DMOD_LOG_ERROR("dmheap: _realloc called with invalid pointer %p from module %s.\n", ptr, module_name);
        return NULL;
    }

    void* new_ptr = NULL;
    if(size < block->size)
    {
        remove_block( &g_dmheap_context.used_list, block );
        block_t* new_block = split_block( block, size );
        if( new_block != NULL )
        {
            add_block( &g_dmheap_context.free_list, new_block );
        }
        add_block( &g_dmheap_context.used_list, block );
        new_ptr = ptr;
    }
    else if(size > block->size)
    {
        new_ptr = dmheap_aligned_alloc( g_dmheap_context.alignment, size, module_name );
        if( new_ptr != NULL )
        {
            memcpy( new_ptr, ptr, block->size );
            remove_block( &g_dmheap_context.used_list, block );
            add_block( &g_dmheap_context.free_list, block );
        }
    }
    else
    {
        new_ptr = ptr;
    }
    
    Dmod_ExitCritical();
    return new_ptr;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void , _free, ( void* ptr, bool concatenate ) )
{
    if( ptr == NULL )
    {
        return;
    }

    Dmod_EnterCritical();
    block_t* block = find_block_by_address( ptr );
    if( block == NULL )
    {
        Dmod_ExitCritical();
        DMOD_LOG_ERROR("dmheap: _free called with invalid pointer %p.\n", ptr);
        return;
    }

    remove_block( &g_dmheap_context.used_list, block );
    add_block( &g_dmheap_context.free_list, block );

    if(concatenate)
    {
        // Attempt to merge with adjacent free blocks
        block_t* current = g_dmheap_context.free_list;
        while( current != NULL )
        {
            if( current != block )
            {
                merge_blocks( block, current );
                merge_blocks( current, block );
            }
            current = current->next;
        }
    }

    Dmod_ExitCritical();
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void*, _aligned_alloc, ( size_t alignment, size_t size, const char* module_name) )
{
    Dmod_EnterCritical();
    size_t aligned_size = align_size( size, alignment );
    block_t* block = find_suitable_block( aligned_size, alignment );
    if( block == NULL )
    {
        Dmod_ExitCritical();
        DMOD_LOG_ERROR("dmheap: Unable to allocate %zu bytes with alignment %zu for module %s.\n", size, alignment, module_name);
        return NULL;
    }

    void* aligned_address = align_pointer( block->address, alignment );
    size_t padding = (size_t)((uintptr_t)aligned_address - (uintptr_t)block->address);

    // First remove the block from free_list before splitting
    remove_block( &g_dmheap_context.free_list, block );

    // If there's any padding, we need to handle it
    if( padding > 0 )
    {
        // Check if we have enough padding to place a block structure before the aligned address
        if( padding >= sizeof(block_t) )
        {
            // Calculate the split point to position the new block's data at aligned_address
            // The new block structure will be at aligned_address - sizeof(block_t)
            // So we need to split at: (aligned_address - sizeof(block_t)) - block->address
            size_t split_at = padding - sizeof(block_t);
            
            // Create a new block for the usable part
            block_t* usable_block = split_block( block, split_at );
            if( usable_block != NULL )
            {
                // block now contains the padding area, add it to free list
                add_block( &g_dmheap_context.free_list, block );
                // usable_block is what we'll actually use for allocation
                block = usable_block;
                // The usable_block's data (block->address) should now be at aligned_address
            }
            else
            {
                // If split failed, put the block back to free_list
                add_block( &g_dmheap_context.free_list, block );
                // If we are here, something went wrong, 
                // because find_suitable_block should have ensured enough space
                // for splitting if padding was needed.
                DMOD_ASSERT_MSG(false, "Unexpected error - check find_suitable_block logic.");
                // Split failed, can't use this block efficiently
                Dmod_ExitCritical();
                DMOD_LOG_ERROR("dmheap: Unable to allocate %zu bytes with alignment %zu for module %s - split failed.\n", size, alignment, module_name);
                return NULL;
            }
        }
        else
        {
            // Padding is too small to fit a block structure.
            // In this case, we skip ahead to the next aligned position that can accommodate
            // both the block structure and be properly aligned.
            // Calculate the next suitable aligned position
            size_t needed_space = sizeof(block_t) + aligned_size;
            void* search_start = (void*)((uintptr_t)block->address + sizeof(block_t));
            void* next_aligned = align_pointer( search_start, alignment );
            size_t new_padding = (size_t)((uintptr_t)next_aligned - (uintptr_t)block->address);
            
            if( new_padding >= sizeof(block_t) && block->size >= new_padding - sizeof(block_t) + aligned_size )
            {
                // Split at the position before the next aligned address
                size_t split_at = new_padding - sizeof(block_t);
                block_t* usable_block = split_block( block, split_at );
                if( usable_block != NULL )
                {
                    add_block( &g_dmheap_context.free_list, block );
                    block = usable_block;
                    aligned_address = block->address;  // Update aligned_address to the actual position
                }
                else
                {
                    // Can't split, return the block to free list and fail
                    add_block( &g_dmheap_context.free_list, block );
                    Dmod_ExitCritical();
                    DMOD_LOG_ERROR("dmheap: Unable to allocate %zu bytes with alignment %zu for module %s - insufficient padding.\n", size, alignment, module_name);
                    return NULL;
                }
            }
            else
            {
                // Not enough space, return the block and fail
                add_block( &g_dmheap_context.free_list, block );
                Dmod_ExitCritical();
                DMOD_LOG_ERROR("dmheap: Unable to allocate %zu bytes with alignment %zu for module %s - insufficient space.\n", size, alignment, module_name);
                return NULL;
            }
        }
    }

    if(block->size > aligned_size + sizeof(block_t) + 1)
    {
        block_t* new_block = split_block( block, aligned_size );
        if( new_block != NULL )
        {
            add_block( &g_dmheap_context.free_list, new_block );
        }
    }

    module_t* module = module_name != NULL ? get_or_create_module( module_name ) : NULL;
    block->owner = module;

    add_block( &g_dmheap_context.used_list, block );

    Dmod_ExitCritical();
    return aligned_address;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void , _concatenate_free_blocks, ( void ) )
{
    Dmod_EnterCritical();
    block_t* current = g_dmheap_context.free_list;
    while( current != NULL )
    {
        block_t* next = current->next;
        while( next != NULL )
        {
            if( merge_blocks( current, next ) != NULL )
            {
                next = current->next;
            }
            else
            {
                next = next->next;
            }
        }
        current = current->next;
    }
    Dmod_ExitCritical();
}

#ifndef DMHEAP_DONT_IMPLEMENT_DMOD_API
DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void*, _MallocEx, ( size_t Size, const char* ModuleName ))
{
    return dmheap_aligned_alloc( g_dmheap_context.alignment, Size, ModuleName );
}

DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void*, _ReallocEx, ( void* Ptr, size_t Size, const char* ModuleName ))
{
    return dmheap_realloc( Ptr, Size, ModuleName );
}

DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void*, _AlignedMallocEx, ( size_t Size, size_t Alignment, const char* ModuleName ))
{
    return dmheap_aligned_alloc( Alignment, Size, ModuleName );
}

DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void , _FreeEx, ( void* Ptr, bool Concatenate ))
{
    dmheap_free( Ptr, Concatenate );
}

DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void, _FreeModule, ( const char* ModuleName ))
{
    dmheap_unregister_module( ModuleName );
}
#endif // DMHEAP_DONT_IMPLEMENT_DMOD_API