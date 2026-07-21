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
typedef struct dmheap_context_t
{
    void*  heap_start;      //!< Pointer to the start of the heap memory.
    size_t heap_size;       //!< Size of the heap memory.
    block_t* free_list;     //!< Pointer to the list of free memory blocks.
    block_t* used_list;     //!< Pointer to the list of used memory blocks.
    size_t alignment;       //!< Alignment for allocations.
    module_t* module_list; //!< Pointer to the list of registered modules.
    char name[DMOD_MAX_MODULE_NAME_LENGTH]; //!< Optional name assigned via dmheap_set_context_name().
} dmheap_context_t;

/**
 * @brief Maximum number of heaps that can sit in the default heap list at once.
 */
#define DMHEAP_MAX_DEFAULT_CONTEXTS 8

static dmheap_context_t* g_default_contexts[DMHEAP_MAX_DEFAULT_CONTEXTS];
static int32_t g_default_context_count = 0;

/**
 * @brief Add a heap to the default heap list. Caller must hold the critical section.
 *
 * @param ctx Pointer to the heap context to add.
 *
 * @return true if the heap was added (or already present), false if ctx is NULL
 *         or the list is full.
 */
static bool add_default_context_locked( dmheap_context_t* ctx )
{
    if( ctx == NULL )
    {
        return false;
    }

    for( size_t i = 0; i < g_default_context_count; i++ )
    {
        if( g_default_contexts[i] == ctx )
        {
            return true;
        }
    }

    if( g_default_context_count >= DMHEAP_MAX_DEFAULT_CONTEXTS )
    {
        DMOD_LOG_ERROR("dmheap: default heap list is full (max %d), cannot add heap %p.\n", DMHEAP_MAX_DEFAULT_CONTEXTS, ctx);
        return false;
    }

    g_default_contexts[g_default_context_count++] = ctx;
    return true;
}

/**
 * @brief Remove a heap from the default heap list, if present. Caller must hold
 * the critical section.
 *
 * Used when a heap is being torn down (e.g. a driver/module that owned it is
 * being unloaded) so no later NULL-context call can dereference it.
 *
 * @param ctx Pointer to the heap context to remove.
 *
 * @return true if ctx was found (and removed), false otherwise.
 */
static bool remove_default_context_locked( dmheap_context_t* ctx )
{
    for( size_t i = 0; i < g_default_context_count; i++ )
    {
        if( g_default_contexts[i] == ctx )
        {
            for( size_t j = i; j + 1 < g_default_context_count; j++ )
            {
                g_default_contexts[j] = g_default_contexts[j + 1];
            }
            g_default_context_count--;
            return true;
        }
    }
    return false;
}

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
 * @param ctx   Pointer to the heap context.
 * @param block Pointer to the block to be split.
 * @param size  Size of the first block after splitting.
 * 
 * @return Pointer to the new block created after splitting, or NULL if not split.
 */
static block_t* split_block( dmheap_context_t* ctx, block_t* block, size_t size )
{
    if( block->size < size + sizeof(block_t) + 1 )
    {
        return NULL;
    }

    // Align the size to ensure the new block starts at an aligned address
    size_t aligned_size = align_size( size, ctx->alignment );
    
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
 * @brief Insert a block into the free list, keeping it sorted from smallest to
 * largest by size.
 *
 * This turns find_suitable_block()'s first-fit scan into a best-fit search - the
 * first block it finds big enough is also the smallest one big enough - so large
 * free blocks are kept intact for large requests instead of being handed out (and
 * fragmented) for small ones.
 *
 * @param list_head Pointer to the head of the free block list.
 * @param block_to_add Pointer to the block to be added.
 */
static void add_free_block( block_t** list_head, block_t* block_to_add )
{
    if( list_head == NULL || block_to_add == NULL )
    {
        return;
    }

    if( *list_head == NULL || block_to_add->size <= (*list_head)->size )
    {
        block_set_next(block_to_add, *list_head);
        *list_head = block_to_add;
        return;
    }

    block_t* current = *list_head;
    while( current->next != NULL && current->next->size < block_to_add->size )
    {
        current = current->next;
    }
    block_set_next(block_to_add, current->next);
    block_set_next(current, block_to_add);
}

/**
 * @brief Find a suitable free block for allocation.
 *
 * @param ctx       Pointer to the heap context.
 * @param size      Size of memory to allocate.
 * @param alignment Alignment requirement.
 *
 * @return Pointer to the suitable block, or NULL if none found.
 */
static block_t* find_suitable_block( dmheap_context_t* ctx, size_t size, size_t alignment )
{
    block_t* current = ctx->free_list;
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
 * @brief Merge every pair of adjacent free blocks in the free list.
 *
 * Caller must already hold the heap's critical section - this is the core of
 * _concatenate_free_blocks(), factored out so it can also be called from inside
 * an allocation that is already holding the lock (see dmheap_aligned_alloc's
 * retry-after-fragmentation path).
 *
 * @param ctx Pointer to the heap context.
 */
static void concatenate_free_blocks_locked( dmheap_context_t* ctx )
{
    block_t* current = ctx->free_list;
    while( current != NULL )
    {
        // `next` scans ahead for any block physically adjacent to `current`, regardless
        // of list order (the free list is not address-sorted). `prev` tracks next's real
        // predecessor *in the list* so that, on a match, we unlink `next` from where it
        // actually sits - not from current->next, which would silently drop every node
        // in between from both the free and used lists (they'd become permanently
        // unreachable, neither free nor allocated).
        block_t* prev = current;
        block_t* next = current->next;
        while( next != NULL )
        {
            if( (uintptr_t)current->address + current->size == (uintptr_t)next )
            {
                current->size += sizeof(block_t) + next->size;
                block_set_next( prev, next->next );
                next = prev->next;
            }
            else
            {
                prev = next;
                next = next->next;
            }
        }
        current = current->next;
    }

    // Merging grows blocks in place without moving them, so the free list (kept
    // sorted smallest-to-largest by add_free_block()) is likely out of order now -
    // rebuild it in sorted order.
    block_t* unsorted = ctx->free_list;
    ctx->free_list = NULL;
    while( unsorted != NULL )
    {
        block_t* next = unsorted->next;
        add_free_block( &ctx->free_list, unsorted );
        unsorted = next;
    }
}

/**
 * @brief Find a block by its address in the used list.
 * 
 * @param ctx     Pointer to the heap context.
 * @param address Pointer to the memory address.
 * 
 * @return Pointer to the block_t structure, or NULL if not found.
 */
static block_t* find_block_by_address( dmheap_context_t* ctx, void* address )
{
    block_t* current = ctx->used_list;
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
 * @param ctx  Pointer to the heap context.
 * @param name Name of the module.
 * 
 * @return Pointer to the module_t structure, or NULL if not found.
 */
static module_t* find_module_by_name( dmheap_context_t* ctx, const char* name )
{
    module_t* current = ctx->module_list;
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
 * @param ctx         Pointer to the heap context.
 * @param alignment   Alignment requirement.
 * @param size        Size of memory to allocate.
 * @param module_name Name of the module requesting allocation (for logging).
 * 
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 */
static module_t* create_module( dmheap_context_t* ctx, const char* name )
{
    block_t* block = find_suitable_block( ctx, sizeof(module_t), ctx->alignment );
    if( block == NULL )
    {
        DMOD_LOG_ERROR("dmheap: Unable to allocate memory for module %s.\n", name);
        return NULL;
    }
    remove_block( &ctx->free_list, block );
    block->owner = NULL;
    if(block->size > (sizeof(module_t) + sizeof(block_t) + ctx->alignment))
    {
        block_t* new_block = split_block( ctx, block, sizeof(module_t) );
        if( new_block != NULL )
        {
            add_free_block( &ctx->free_list, new_block );
        }
    }
    module_t* module = (module_t*)block->address;
    strncpy( module->name, name, DMOD_MAX_MODULE_NAME_LENGTH - 1 );
    module->name[DMOD_MAX_MODULE_NAME_LENGTH - 1] = '\0';
    add_block( &ctx->used_list, block );
    add_module_to_list( &ctx->module_list, module );
    return module;
}

/**
 * @brief Release all memory allocated by a specific module.
 * 
 * @param ctx    Pointer to the heap context.
 * @param module Pointer to the module whose memory is to be released.
 */
static void release_memory_of_module( dmheap_context_t* ctx, module_t* module )
{
    if( module == NULL )
    {
        return;
    }

    block_t* current = ctx->used_list;
    block_t* prev = NULL;

    while( current != NULL )
    {
        if( current->owner == module )
        {
            block_t* to_free = current;
            if( prev == NULL )
            {
                ctx->used_list = current->next;
                current = ctx->used_list;
            }
            else
            {
                block_set_next(prev, current->next);
                current = prev->next;
            }
            add_free_block( &ctx->free_list, to_free );
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
 * @param ctx    Pointer to the heap context.
 * @param module Pointer to the module to be deleted.
 */
static void delete_module( dmheap_context_t* ctx, module_t* module )
{
    if( module == NULL )
    {
        return;
    }

    release_memory_of_module( ctx, module );
    remove_module_from_list( &ctx->module_list, module );

    block_t* block = find_block_by_address( ctx, (void*)module );
    if( block != NULL )
    {
        remove_block( &ctx->used_list, block );
        add_free_block( &ctx->free_list, block );
    }
}

/**
 * @brief Get or create a module by its name.
 * 
 * @param ctx         Pointer to the heap context.
 * @param module_name Name of the module.
 * 
 * @return Pointer to the module_t structure.
 */
static module_t* get_or_create_module( dmheap_context_t* ctx, const char* module_name )
{
    module_t* module = find_module_by_name( ctx, module_name );
    if( module == NULL )
    {
        module = create_module( ctx, module_name );
    }
    return module;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, dmheap_context_t*,  _init, ( void* buffer, size_t size, size_t alignment ) )
{
    if(buffer == NULL || size == 0)
    {
        DMOD_LOG_ERROR("dmheap: _init called with invalid parameters.\n");
        return NULL;
    }
    
    // Check buffer alignment for context structure
    if(((uintptr_t)buffer % sizeof(void*)) != 0)
    {
        DMOD_LOG_ERROR("dmheap: buffer is not properly aligned (must be aligned to pointer size).\n");
        return NULL;
    }
    
    // The context structure is stored at the beginning of the buffer
    // Align context size to ensure heap starts at a proper boundary
    size_t context_size = align_size(sizeof(dmheap_context_t), alignment > sizeof(void*) ? alignment : sizeof(void*));
    if(size < context_size + sizeof(block_t) + alignment)
    {
        DMOD_LOG_ERROR("dmheap: buffer too small for context and minimum allocation.\n");
        return NULL;
    }
    
    Dmod_EnterCritical();
    dmheap_context_t* ctx = (dmheap_context_t*)buffer;
    
    // Calculate the start of the heap (after the aligned context structure)
    void* heap_buffer = (void*)((uintptr_t)buffer + context_size);
    size_t heap_size = size - context_size;
    
    ctx->heap_start = heap_buffer;
    ctx->heap_size  = heap_size;
    ctx->free_list  = create_block( heap_buffer, heap_size );
    ctx->used_list  = NULL;
    ctx->alignment  = alignment;
    ctx->module_list = NULL;  // Reset module list on initialization
    ctx->name[0] = '\0';      // No name assigned until dmheap_set_context_name() is called

    add_default_context_locked( ctx );

    Dmod_ExitCritical();
    DMOD_LOG_INFO("== dmheap ver. %s ==\n", DMHEAP_VERSION);
    DMOD_LOG_INFO("dmheap: Initialized with buffer %p of size %lu.\n", heap_buffer, (unsigned long)heap_size);
    return ctx;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, bool,  _set_context_name, ( dmheap_context_t* ctx, const char* name ) )
{
    if( ctx == NULL )
    {
        DMOD_LOG_ERROR("dmheap: set_context_name called with NULL context.\n");
        return false;
    }

    Dmod_EnterCritical();
    if( name != NULL )
    {
        strncpy( ctx->name, name, sizeof(ctx->name) - 1 );
        ctx->name[sizeof(ctx->name) - 1] = '\0';
    }
    else
    {
        ctx->name[0] = '\0';
    }
    Dmod_ExitCritical();
    return true;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, const char*,  _get_context_name, ( dmheap_context_t* ctx ) )
{
    if( ctx == NULL )
    {
        ctx = g_default_context_count > 0 ? g_default_contexts[0] : NULL;
    }
    return ctx != NULL ? ctx->name : "";
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void,  _set_default_context, ( dmheap_context_t* ctx ) )
{
    Dmod_EnterCritical();
    g_default_context_count = 0;
    if( ctx != NULL )
    {
        add_default_context_locked( ctx );
    }
    Dmod_ExitCritical();
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, bool,  _add_default_context, ( dmheap_context_t* ctx ) )
{
    Dmod_EnterCritical();
    bool added = add_default_context_locked( ctx );
    Dmod_ExitCritical();
    return added;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, bool,  _remove_default_context, ( dmheap_context_t* ctx ) )
{
    Dmod_EnterCritical();
    bool removed = remove_default_context_locked( ctx );
    Dmod_ExitCritical();
    return removed;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, dmheap_context_t*,  _get_default_context, ( void ) )
{
    return g_default_context_count > 0 ? g_default_contexts[0] : NULL;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, size_t,  _get_default_context_count, ( void ) )
{
    return g_default_context_count;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, dmheap_context_t*,  _get_default_context_at, ( size_t index ) )
{
    return index < g_default_context_count ? g_default_contexts[index] : NULL;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, dmheap_context_t*,  _get_context_by_name, ( const char* name ) )
{
    if( name == NULL )
    {
        return NULL;
    }

    dmheap_context_t* found = NULL;
    Dmod_EnterCritical();
    for( size_t i = 0; i < g_default_context_count; i++ )
    {
        if( g_default_contexts[i]->name[0] != '\0' &&
            strncmp( g_default_contexts[i]->name, name, DMOD_MAX_MODULE_NAME_LENGTH ) == 0 )
        {
            found = g_default_contexts[i];
            break;
        }
    }
    Dmod_ExitCritical();
    return found;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, bool,  _is_initialized, ( dmheap_context_t* ctx ) )
{
    if( ctx != NULL )
    {
        return ctx->heap_start != NULL;
    }
    return g_default_context_count > 0;
}

/**
 * @brief Register a module on a single, already-resolved heap context.
 *
 * @param ctx         Pointer to the heap context (must not be NULL).
 * @param module_name Name of the module to register.
 *
 * @return true if the module is registered (either just now or already), false on failure.
 */
static bool register_module_in_context( dmheap_context_t* ctx, const char* module_name )
{
    Dmod_EnterCritical();
    module_t* module = find_module_by_name( ctx, module_name );
    if( module != NULL )
    {
        Dmod_ExitCritical();
        DMOD_LOG_WARN("dmheap: Module %s is already registered.\n", module_name);
        return true;
    }
    module = create_module( ctx, module_name );
    Dmod_ExitCritical();
    if( module == NULL )
    {
        DMOD_LOG_ERROR("dmheap: Failed to register module %s.\n", module_name);
        return false;
    }
    DMOD_LOG_INFO("dmheap: Module %s registered successfully.\n", module_name);
    return true;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, bool,  _register_module, ( dmheap_context_t* ctx, const char* module_name ) )
{
    if( ctx != NULL )
    {
        return register_module_in_context( ctx, module_name );
    }

    if( g_default_context_count == 0 )
    {
        DMOD_LOG_ERROR("dmheap: No context available for register_module.\n");
        return false;
    }

    // A NULL context registers the module on every default heap, since a later
    // allocation with a NULL context (see dmheap_malloc) may land on any of them.
    bool all_succeeded = true;
    for( size_t i = 0; i < g_default_context_count; i++ )
    {
        if( !register_module_in_context( g_default_contexts[i], module_name ) )
        {
            all_succeeded = false;
        }
    }
    return all_succeeded;
}

/**
 * @brief Unregister a module (if present) on a single, already-resolved heap context.
 *
 * @param ctx         Pointer to the heap context (must not be NULL).
 * @param module_name Name of the module to unregister.
 */
static void unregister_module_in_context( dmheap_context_t* ctx, const char* module_name )
{
    Dmod_EnterCritical();
    module_t* module = find_module_by_name( ctx, module_name );
    if( module == NULL )
    {
        Dmod_ExitCritical();
        return;
    }
    delete_module( ctx, module );
    // Coalesce right away: a module's freed blocks otherwise sit as isolated,
    // module-shaped fragments that a differently-sized/aligned future allocation
    // can't reuse, permanently eating into the largest contiguous free region one
    // load/unload cycle at a time (see Dmod_Context_Delete for the same reasoning).
    concatenate_free_blocks_locked( ctx );
    Dmod_ExitCritical();
    DMOD_LOG_INFO("dmheap: Module %s unregistered successfully.\n", module_name);
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void,  _unregister_module, ( dmheap_context_t* ctx, const char* module_name ) )
{
    // it is very likely, that module_name is inside a buffer we will free, so we need to copy it first
    char module_name_copy[DMOD_MAX_MODULE_NAME_LENGTH];
    strncpy( module_name_copy, module_name, DMOD_MAX_MODULE_NAME_LENGTH - 1 );
    module_name_copy[DMOD_MAX_MODULE_NAME_LENGTH - 1] = '\0';

    if( ctx != NULL )
    {
        unregister_module_in_context( ctx, module_name_copy );
        return;
    }

    if( g_default_context_count == 0 )
    {
        DMOD_LOG_ERROR("dmheap: No context available for unregister_module.\n");
        return;
    }

    // A module registered with a NULL context may have ended up with blocks on any
    // default heap (see dmheap_malloc) - unregister it everywhere so nothing leaks.
    for( size_t i = 0; i < g_default_context_count; i++ )
    {
        unregister_module_in_context( g_default_contexts[i], module_name_copy );
    }
}

/**
 * @brief Allocate aligned memory from a single, already-resolved heap context.
 *
 * This is the core allocation algorithm, factored out so it can be reused both
 * for an explicit context and, per-heap, when searching the default heap list.
 *
 * @param ctx         Pointer to the heap context (must not be NULL).
 * @param alignment   Alignment requirement.
 * @param size        Size of memory to allocate.
 * @param module_name Name of the module requesting allocation, recorded as the
 *                    block's owner on success. Failures are not logged here -
 *                    only the caller knows whether this is a final failure or
 *                    one heap out of several being tried in a default-heap
 *                    fallback search, so it logs (or not) accordingly.
 *
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 */
static void* aligned_alloc_in_context( dmheap_context_t* ctx, size_t alignment, size_t size, const char* module_name )
{
    Dmod_EnterCritical();
    size_t aligned_size = align_size( size, alignment );
    block_t* block = find_suitable_block( ctx, aligned_size, alignment );
    if( block == NULL )
    {
        // Dmod_Free() never coalesces on its own (Concatenate=false) - a request can
        // fail here purely from fragmentation even when the aggregate free memory is
        // more than enough. Before giving up, try to merge adjacent free blocks and
        // search once more - this only pays the O(n) coalescing cost on the rare
        // allocation that actually needs it, instead of on every single free.
        concatenate_free_blocks_locked( ctx );
        block = find_suitable_block( ctx, aligned_size, alignment );
    }
    if( block == NULL )
    {
        Dmod_ExitCritical();
        return NULL;
    }

    void* aligned_address = align_pointer( block->address, alignment );
    size_t padding = (size_t)((uintptr_t)aligned_address - (uintptr_t)block->address);

    // First remove the block from free_list before splitting
    remove_block( &ctx->free_list, block );

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
            block_t* usable_block = split_block( ctx, block, split_at );
            if( usable_block != NULL )
            {
                // block now contains the padding area, add it to free list
                add_free_block( &ctx->free_list, block );
                // usable_block is what we'll actually use for allocation
                block = usable_block;
                // The usable_block's data (block->address) should now be at aligned_address
            }
            else
            {
                // If split failed, put the block back to free_list
                add_free_block( &ctx->free_list, block );
                // If we are here, something went wrong, 
                // because find_suitable_block should have ensured enough space
                // for splitting if padding was needed.
                DMOD_ASSERT_MSG(false, "Unexpected error - check find_suitable_block logic.");
                // Split failed, can't use this block efficiently
                Dmod_ExitCritical();
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
                block_t* usable_block = split_block( ctx, block, split_at );
                if( usable_block != NULL )
                {
                    add_free_block( &ctx->free_list, block );
                    block = usable_block;
                    aligned_address = block->address;  // Update aligned_address to the actual position
                }
                else
                {
                    // Can't split, return the block to free list and fail
                    add_free_block( &ctx->free_list, block );
                    Dmod_ExitCritical();
                    return NULL;
                }
            }
            else
            {
                // Not enough space, return the block and fail
                add_free_block( &ctx->free_list, block );
                Dmod_ExitCritical();
                return NULL;
            }
        }
    }

    if(block->size > aligned_size + sizeof(block_t) + 1)
    {
        block_t* new_block = split_block( ctx, block, aligned_size );
        if( new_block != NULL )
        {
            add_free_block( &ctx->free_list, new_block );
        }
    }

    module_t* module = module_name != NULL ? get_or_create_module( ctx, module_name ) : NULL;
    block->owner = module;

    add_block( &ctx->used_list, block );

    Dmod_ExitCritical();
    return aligned_address;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void*, _aligned_alloc, ( dmheap_context_t* ctx, size_t alignment, size_t size, const char* module_name) )
{
    if( ctx != NULL )
    {
        void* ptr = aligned_alloc_in_context( ctx, alignment, size, module_name );
        if( ptr == NULL )
        {
            DMOD_LOG_ERROR("dmheap: Unable to allocate %zu bytes with alignment %zu for module %s.\n", size, alignment, module_name);
        }
        return ptr;
    }

    if( g_default_context_count == 0 )
    {
        DMOD_LOG_ERROR("dmheap: No context available for aligned_alloc.\n");
        return NULL;
    }

    // Try every default heap in the order it was added, until one can satisfy the request.
    // Failing on any one heap along the way is expected, not an error - only log if every
    // heap in the search comes up empty (below).
    for( int32_t i = (g_default_context_count - 1); i >= 0; i-- )
    {
        void* ptr = aligned_alloc_in_context( g_default_contexts[i], alignment, size, module_name );
        if( ptr != NULL )
        {
            return ptr;
        }
    }

    DMOD_LOG_ERROR("dmheap: Unable to allocate %zu bytes with alignment %zu for module %s in any default heap.\n", size, alignment, module_name);
    return NULL;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void*, _malloc, ( dmheap_context_t* ctx, size_t size, const char* module_name ) )
{
    if( ctx != NULL )
    {
        void* ptr = aligned_alloc_in_context( ctx, ctx->alignment, size, module_name );
        if( ptr == NULL )
        {
            DMOD_LOG_ERROR("dmheap: Unable to allocate %zu bytes for module %s.\n", size, module_name);
        }
        return ptr;
    }

    if( g_default_context_count == 0 )
    {
        DMOD_LOG_ERROR("dmheap: No context available for malloc.\n");
        return NULL;
    }

    // Try every default heap in the order it was added, using each heap's own
    // alignment, until one can satisfy the request. Failing on any one heap along
    // the way is expected, not an error - only log if every heap comes up empty.
    for( int32_t i = (g_default_context_count - 1); i >= 0; i-- )
    {
        dmheap_context_t* heap = g_default_contexts[i];
        void* ptr = aligned_alloc_in_context( heap, heap->alignment, size, module_name );
        if( ptr != NULL )
        {
            return ptr;
        }
    }

    DMOD_LOG_ERROR("dmheap: Unable to allocate %zu bytes for module %s in any default heap.\n", size, module_name);
    return NULL;
}

/**
 * @brief Grow/shrink/no-op an already-located block in place, allocating a
 * replacement in the same context when it needs to grow. Caller must already
 * hold the heap's critical section.
 *
 * @param ctx         Pointer to the heap context that owns block/ptr.
 * @param block       The block currently backing ptr.
 * @param ptr         Pointer previously returned by an allocation function.
 * @param size        New size of memory to allocate.
 * @param module_name Name of the module requesting reallocation (for logging).
 *
 * @return Pointer to the reallocated memory, or NULL if growing failed.
 */
static void* realloc_block_locked( dmheap_context_t* ctx, block_t* block, void* ptr, size_t size, const char* module_name )
{
    void* new_ptr = NULL;
    if(size < block->size)
    {
        remove_block( &ctx->used_list, block );
        block_t* new_block = split_block( ctx, block, size );
        if( new_block != NULL )
        {
            add_free_block( &ctx->free_list, new_block );
        }
        add_block( &ctx->used_list, block );
        new_ptr = ptr;
    }
    else if(size > block->size)
    {
        new_ptr = aligned_alloc_in_context( ctx, ctx->alignment, size, module_name );
        if( new_ptr != NULL )
        {
            memcpy( new_ptr, ptr, block->size );
            remove_block( &ctx->used_list, block );
            add_free_block( &ctx->free_list, block );
        }
        else
        {
            DMOD_LOG_ERROR("dmheap: Unable to grow allocation to %zu bytes for module %s.\n", size, module_name);
        }
    }
    else
    {
        new_ptr = ptr;
    }
    return new_ptr;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void*, _realloc, ( dmheap_context_t* ctx, void* ptr, size_t size, const char* module_name) )
{
    if( ptr == NULL )
    {
        return dmheap_malloc( ctx, size, module_name );
    }

    if( ctx != NULL )
    {
        Dmod_EnterCritical();
        block_t* block = find_block_by_address( ctx, ptr );
        if( block == NULL )
        {
            Dmod_ExitCritical();
            DMOD_LOG_ERROR("dmheap: _realloc called with invalid pointer %p from module %s.\n", ptr, module_name);
            return NULL;
        }
        void* new_ptr = realloc_block_locked( ctx, block, ptr, size, module_name );
        Dmod_ExitCritical();
        return new_ptr;
    }

    if( g_default_context_count == 0 )
    {
        DMOD_LOG_ERROR("dmheap: No context available for realloc.\n");
        return NULL;
    }

    // ptr may have been handed out by any default heap (see dmheap_malloc) - find
    // whichever one actually owns it.
    for( int32_t i = (g_default_context_count - 1); i >= 0 ; i-- )
    {
        dmheap_context_t* heap = g_default_contexts[i];
        Dmod_EnterCritical();
        block_t* block = find_block_by_address( heap, ptr );
        if( block != NULL )
        {
            void* new_ptr = realloc_block_locked( heap, block, ptr, size, module_name );
            Dmod_ExitCritical();
            return new_ptr;
        }
        Dmod_ExitCritical();
    }

    DMOD_LOG_ERROR("dmheap: _realloc called with invalid pointer %p from module %s.\n", ptr, module_name);
    return NULL;
}

/**
 * @brief Free a block if it belongs to the given, already-resolved heap context.
 *
 * @param ctx         Pointer to the heap context to search (must not be NULL).
 * @param ptr         Pointer to the memory to free.
 * @param concatenate If true, attempt to merge adjacent free blocks after freeing.
 *
 * @return true if ptr was found in ctx (and freed), false otherwise.
 */
static bool free_block_in_context( dmheap_context_t* ctx, void* ptr, bool concatenate )
{
    Dmod_EnterCritical();
    block_t* block = find_block_by_address( ctx, ptr );
    if( block == NULL )
    {
        Dmod_ExitCritical();
        return false;
    }

    remove_block( &ctx->used_list, block );
    add_free_block( &ctx->free_list, block );

    if(concatenate)
    {
        concatenate_free_blocks_locked( ctx );
    }

    Dmod_ExitCritical();
    return true;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void , _free, ( dmheap_context_t* ctx, void* ptr, bool concatenate ) )
{
    if( ptr == NULL )
    {
        return;
    }

    if( ctx != NULL )
    {
        if( !free_block_in_context( ctx, ptr, concatenate ) )
        {
            DMOD_LOG_ERROR("dmheap: _free called with invalid pointer %p.\n", ptr);
        }
        return;
    }

    if( g_default_context_count == 0 )
    {
        DMOD_LOG_ERROR("dmheap: No context available for free.\n");
        return;
    }

    // ptr may have been handed out by any default heap (see dmheap_malloc) - find
    // whichever one actually owns it.
    for( int32_t i = (g_default_context_count - 1); i >= 0; i-- )
    {
        if( free_block_in_context( g_default_contexts[i], ptr, concatenate ) )
        {
            return;
        }
    }

    DMOD_LOG_ERROR("dmheap: _free called with invalid pointer %p.\n", ptr);
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void , _concatenate_free_blocks, ( dmheap_context_t* ctx ) )
{
    if( ctx != NULL )
    {
        Dmod_EnterCritical();
        concatenate_free_blocks_locked( ctx );
        Dmod_ExitCritical();
        return;
    }

    if( g_default_context_count == 0 )
    {
        DMOD_LOG_ERROR("dmheap: No context available for concatenate_free_blocks.\n");
        return;
    }

    for( int32_t i = (g_default_context_count - 1); i >= 0; i-- )
    {
        Dmod_EnterCritical();
        concatenate_free_blocks_locked( g_default_contexts[i] );
        Dmod_ExitCritical();
    }
}

/**
 * @brief Result of attempting to retag a pointer against a single heap context.
 */
typedef enum retag_result_t
{
    RETAG_NOT_FOUND,    //!< ptr does not belong to this context.
    RETAG_FAILED,       //!< ptr belongs to this context, but the target module could not be created.
    RETAG_OK,           //!< ptr was found and retagged successfully.
} retag_result_t;

/**
 * @brief Retag a block if it belongs to the given, already-resolved heap context.
 *
 * @param ctx             Pointer to the heap context to search (must not be NULL).
 * @param ptr             Pointer previously returned by an allocation function.
 * @param new_module_name Name of the module to attribute the block to from now on.
 */
static retag_result_t retag_block_in_context( dmheap_context_t* ctx, void* ptr, const char* new_module_name )
{
    Dmod_EnterCritical();
    block_t* block = find_block_by_address( ctx, ptr );
    if( block == NULL )
    {
        Dmod_ExitCritical();
        return RETAG_NOT_FOUND;
    }

    module_t* module = get_or_create_module( ctx, new_module_name );
    if( module == NULL )
    {
        Dmod_ExitCritical();
        return RETAG_FAILED;
    }

    block->owner = module;
    Dmod_ExitCritical();
    return RETAG_OK;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, bool, _retag, ( dmheap_context_t* ctx, void* ptr, const char* new_module_name ) )
{
    if( ptr == NULL || new_module_name == NULL )
    {
        DMOD_LOG_ERROR("dmheap: retag called with invalid arguments.\n");
        return false;
    }

    if( ctx != NULL )
    {
        retag_result_t result = retag_block_in_context( ctx, ptr, new_module_name );
        if( result == RETAG_NOT_FOUND )
        {
            DMOD_LOG_ERROR("dmheap: retag called with unknown pointer %p.\n", ptr);
        }
        else if( result == RETAG_FAILED )
        {
            DMOD_LOG_ERROR("dmheap: Unable to retag %p - failed to get/create module %s.\n", ptr, new_module_name);
        }
        return result == RETAG_OK;
    }

    if( g_default_context_count == 0 )
    {
        DMOD_LOG_ERROR("dmheap: No context available for retag.\n");
        return false;
    }

    // ptr may have been handed out by any default heap (see dmheap_malloc) - find
    // whichever one actually owns it.
    for( int32_t i = (g_default_context_count - 1); i >= 0; i-- )
    {
        retag_result_t result = retag_block_in_context( g_default_contexts[i], ptr, new_module_name );
        if( result == RETAG_OK )
        {
            return true;
        }
        if( result == RETAG_FAILED )
        {
            DMOD_LOG_ERROR("dmheap: Unable to retag %p - failed to get/create module %s.\n", ptr, new_module_name);
            return false;
        }
        // RETAG_NOT_FOUND: keep searching the remaining default heaps.
    }

    DMOD_LOG_ERROR("dmheap: retag called with unknown pointer %p.\n", ptr);
    return false;
}

/**
 * @brief Accumulate one heap context's statistics into a running total. Caller
 * must already hold the heap's critical section.
 *
 * @param ctx       Pointer to the heap context.
 * @param out_stats Statistics accumulator, updated in place.
 */
static void accumulate_stats_locked( dmheap_context_t* ctx, dmheap_stats_t* out_stats )
{
    out_stats->heap_size += ctx->heap_size;

    for( block_t* block = ctx->free_list; block != NULL; block = block->next )
    {
        out_stats->free_bytes += block->size;
        out_stats->free_block_count++;
        if( out_stats->largest_free_block == 0 || block->size > out_stats->largest_free_block )
        {
            out_stats->largest_free_block = block->size;
        }
        if( out_stats->smallest_free_block == 0 || block->size < out_stats->smallest_free_block )
        {
            out_stats->smallest_free_block = block->size;
        }
    }

    for( block_t* block = ctx->used_list; block != NULL; block = block->next )
    {
        out_stats->used_bytes += block->size;
        out_stats->used_block_count++;
    }
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, bool, _get_stats, ( dmheap_context_t* ctx, dmheap_stats_t* out_stats ) )
{
    if( out_stats == NULL )
    {
        DMOD_LOG_ERROR("dmheap: get_stats called with invalid arguments.\n");
        return false;
    }

    memset( out_stats, 0, sizeof(*out_stats) );

    if( ctx != NULL )
    {
        Dmod_EnterCritical();
        accumulate_stats_locked( ctx, out_stats );
        Dmod_ExitCritical();
        return true;
    }

    if( g_default_context_count == 0 )
    {
        DMOD_LOG_ERROR("dmheap: get_stats called with invalid arguments.\n");
        return false;
    }

    // Aggregate across every default heap.
    Dmod_EnterCritical();
    for( int32_t i = (g_default_context_count - 1); i >= 0; i-- )
    {
        accumulate_stats_locked( g_default_contexts[i], out_stats );
    }
    Dmod_ExitCritical();
    return true;
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void, _for_each_free_block, ( dmheap_context_t* ctx, dmheap_block_visitor_t visitor, void* user_data ) )
{
    if( visitor == NULL )
    {
        return;
    }

    if( ctx != NULL )
    {
        Dmod_EnterCritical();
        for( block_t* block = ctx->free_list; block != NULL; block = block->next )
        {
            visitor( block->address, block->size, NULL, user_data );
        }
        Dmod_ExitCritical();
        return;
    }

    Dmod_EnterCritical();
    for( int32_t i = (g_default_context_count - 1); i >= 0; i-- )
    {
        for( block_t* block = g_default_contexts[i]->free_list; block != NULL; block = block->next )
        {
            visitor( block->address, block->size, NULL, user_data );
        }
    }
    Dmod_ExitCritical();
}

DMOD_INPUT_API_DECLARATION( dmheap, 1.0, void, _for_each_used_block, ( dmheap_context_t* ctx, dmheap_block_visitor_t visitor, void* user_data ) )
{
    if( visitor == NULL )
    {
        return;
    }

    if( ctx != NULL )
    {
        Dmod_EnterCritical();
        for( block_t* block = ctx->used_list; block != NULL; block = block->next )
        {
            visitor( block->address, block->size, block->owner != NULL ? block->owner->name : NULL, user_data );
        }
        Dmod_ExitCritical();
        return;
    }

    Dmod_EnterCritical();
    for( int32_t i = (g_default_context_count - 1); i >= 0; i-- )
    {
        for( block_t* block = g_default_contexts[i]->used_list; block != NULL; block = block->next )
        {
            visitor( block->address, block->size, block->owner != NULL ? block->owner->name : NULL, user_data );
        }
    }
    Dmod_ExitCritical();
}

#ifndef DMHEAP_DONT_IMPLEMENT_DMOD_API
DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void*, _MallocEx, ( size_t Size, const char* ModuleName ))
{
    return dmheap_malloc( NULL, Size, ModuleName );
}

DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void*, _ReallocEx, ( void* Ptr, size_t Size, const char* ModuleName ))
{
    return dmheap_realloc( NULL, Ptr, Size, ModuleName );
}

DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void*, _AlignedMallocEx, ( size_t Size, size_t Alignment, const char* ModuleName ))
{
    return dmheap_aligned_alloc( NULL, Alignment, Size, ModuleName );
}

DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void , _FreeEx, ( void* Ptr, bool Concatenate ))
{
    dmheap_free( NULL, Ptr, Concatenate );
}

DMOD_INPUT_API_DECLARATION(Dmod, 1.0, void, _FreeModule, ( const char* ModuleName ))
{
    dmheap_unregister_module( NULL, ModuleName );
}

DMOD_INPUT_API_DECLARATION(Dmod, 1.0, bool, _RetagEx, ( void* Ptr, const char* ModuleName ))
{
    return dmheap_retag( NULL, Ptr, ModuleName );
}
#endif // DMHEAP_DONT_IMPLEMENT_DMOD_API