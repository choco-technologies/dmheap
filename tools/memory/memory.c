#include <dmod.h>
#include <dmheap.h>
#include <string.h>
#include <errno.h>

// ============================================================================
//                              Usage / help
// ============================================================================

static void print_usage( void )
{
    Dmod_Printf("Usage: memory [options]\n");
    Dmod_Printf("Inspect the state of the dmheap allocator.\n\n");
    Dmod_Printf("Options:\n");
    Dmod_Printf("  -s, --stats           Print overall heap occupancy statistics\n");
    Dmod_Printf("  -m, --modules         Print a per-module allocation summary\n");
    Dmod_Printf("  -f, --fragmentation   Print a histogram of free block sizes\n");
    Dmod_Printf("  -h, --help            Show this help message\n\n");
    Dmod_Printf("Multiple options can be combined in a single call, e.g.\n");
    Dmod_Printf("  memory --stats --modules\n");
}

// ============================================================================
//                              --stats
// ============================================================================

static void print_stats( void )
{
    dmheap_stats_t stats;
    if( !dmheap_get_stats( NULL, &stats ) )
    {
        DMOD_LOG_ERROR("Failed to read heap statistics\n");
        return;
    }

    size_t total_block_count = stats.free_block_count + stats.used_block_count;

    Dmod_Printf("Heap statistics:\n");
    Dmod_Printf("  Total size:     %zu bytes\n", stats.heap_size);
    Dmod_Printf("  Free:           %zu bytes\n", stats.free_bytes);
    Dmod_Printf("  Used:           %zu bytes\n", stats.used_bytes);
    Dmod_Printf("  Blocks:         %zu (%zu free, %zu used)\n",
        total_block_count, stats.free_block_count, stats.used_block_count);
    Dmod_Printf("  Largest free:   %zu bytes\n", stats.largest_free_block);
    Dmod_Printf("  Smallest free:  %zu bytes\n", stats.smallest_free_block);
}

// ============================================================================
//                              --modules
// ============================================================================

#define MAX_MODULE_SUMMARY_ENTRIES 64

typedef struct module_summary_entry_t
{
    char name[DMOD_MAX_MODULE_NAME_LENGTH];
    bool is_null;
    size_t block_count;
    size_t total_bytes;
} module_summary_entry_t;

typedef struct module_summary_t
{
    module_summary_entry_t entries[MAX_MODULE_SUMMARY_ENTRIES];
    size_t count;
    bool overflowed;
} module_summary_t;

static void module_summary_visitor( void* address, size_t size, const char* owner_name, void* user_data )
{
    (void)address;
    module_summary_t* summary = (module_summary_t*)user_data;

    for( size_t i = 0; i < summary->count; i++ )
    {
        module_summary_entry_t* entry = &summary->entries[i];
        bool is_same = ( owner_name == NULL )
            ? entry->is_null
            : ( !entry->is_null && strncmp( entry->name, owner_name, DMOD_MAX_MODULE_NAME_LENGTH ) == 0 );
        if( is_same )
        {
            entry->block_count++;
            entry->total_bytes += size;
            return;
        }
    }

    if( summary->count >= MAX_MODULE_SUMMARY_ENTRIES )
    {
        summary->overflowed = true;
        return;
    }

    module_summary_entry_t* entry = &summary->entries[summary->count++];
    entry->is_null = ( owner_name == NULL );
    entry->name[0] = '\0';
    if( owner_name != NULL )
    {
        strncpy( entry->name, owner_name, sizeof(entry->name) - 1 );
        entry->name[sizeof(entry->name) - 1] = '\0';
    }
    entry->block_count = 1;
    entry->total_bytes = size;
}

static void print_modules( void )
{
    module_summary_t* summary = Dmod_Malloc( sizeof(module_summary_t) );
    if( summary == NULL )
    {
        DMOD_LOG_ERROR("Failed to allocate memory for the module summary\n");
        return;
    }
    memset( summary, 0, sizeof(*summary) );

    dmheap_for_each_used_block( NULL, module_summary_visitor, summary );

    Dmod_Printf("Module allocation summary (%zu module%s):\n",
        summary->count, summary->count == 1 ? "" : "s");
    Dmod_Printf("  %-32s %10s %14s\n", "MODULE", "BLOCKS", "BYTES");
    for( size_t i = 0; i < summary->count; i++ )
    {
        module_summary_entry_t* entry = &summary->entries[i];
        Dmod_Printf("  %-32s %10zu %14zu\n",
            entry->is_null ? "(null)" : entry->name, entry->block_count, entry->total_bytes);
    }
    if( summary->overflowed )
    {
        Dmod_Printf("  (more than %d distinct modules found - list truncated)\n", MAX_MODULE_SUMMARY_ENTRIES);
    }

    Dmod_Free( summary );
}

// ============================================================================
//                              --fragmentation
// ============================================================================

#define MAX_SIZE_BUCKETS 128

typedef struct size_bucket_t
{
    size_t size;
    size_t block_count;
} size_bucket_t;

typedef struct fragmentation_t
{
    size_bucket_t buckets[MAX_SIZE_BUCKETS];
    size_t count;
    bool overflowed;
} fragmentation_t;

// Keeps buckets[] sorted ascending by size as entries are inserted, so no
// separate sort step (and no dependency on a libc qsort) is needed afterward.
static void fragmentation_visitor( void* address, size_t size, const char* owner_name, void* user_data )
{
    (void)address;
    (void)owner_name;
    fragmentation_t* frag = (fragmentation_t*)user_data;

    size_t insert_at = frag->count;
    for( size_t i = 0; i < frag->count; i++ )
    {
        if( frag->buckets[i].size == size )
        {
            frag->buckets[i].block_count++;
            return;
        }
        if( frag->buckets[i].size > size )
        {
            insert_at = i;
            break;
        }
    }

    if( frag->count >= MAX_SIZE_BUCKETS )
    {
        frag->overflowed = true;
        return;
    }

    for( size_t j = frag->count; j > insert_at; j-- )
    {
        frag->buckets[j] = frag->buckets[j - 1];
    }
    frag->buckets[insert_at].size = size;
    frag->buckets[insert_at].block_count = 1;
    frag->count++;
}

static void print_fragmentation( void )
{
    fragmentation_t* frag = Dmod_Malloc( sizeof(fragmentation_t) );
    if( frag == NULL )
    {
        DMOD_LOG_ERROR("Failed to allocate memory for the fragmentation report\n");
        return;
    }
    memset( frag, 0, sizeof(*frag) );

    dmheap_for_each_free_block( NULL, fragmentation_visitor, frag );

    Dmod_Printf("Free block fragmentation (%zu distinct size%s):\n",
        frag->count, frag->count == 1 ? "" : "s");
    Dmod_Printf("  %12s %10s %14s\n", "BLOCK SIZE", "COUNT", "TOTAL BYTES");
    for( size_t i = 0; i < frag->count; i++ )
    {
        size_bucket_t* bucket = &frag->buckets[i];
        Dmod_Printf("  %10zu B %10zu %14zu\n",
            bucket->size, bucket->block_count, bucket->size * bucket->block_count);
    }
    if( frag->overflowed )
    {
        Dmod_Printf("  (more than %d distinct free block sizes found - list truncated)\n", MAX_SIZE_BUCKETS);
    }

    Dmod_Free( frag );
}

// ============================================================================
//                              Entry point
// ============================================================================

/**
 * @brief Entry point for the 'memory' tool module.
 *
 * Inspects the dmheap allocator's current state: overall occupancy, a
 * per-module allocation breakdown, and free-block fragmentation.
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return int Exit code (0 on success, negative on error)
 */
int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        print_usage();
        return 0;
    }

    for( int i = 1; i < argc; i++ )
    {
        const char* arg = argv[i];

        if( strcmp( arg, "-h" ) == 0 || strcmp( arg, "--help" ) == 0 )
        {
            print_usage();
            return 0;
        }
        else if( strcmp( arg, "-s" ) == 0 || strcmp( arg, "--stats" ) == 0 )
        {
            print_stats();
        }
        else if( strcmp( arg, "-m" ) == 0 || strcmp( arg, "--modules" ) == 0 )
        {
            print_modules();
        }
        else if( strcmp( arg, "-f" ) == 0 || strcmp( arg, "--fragmentation" ) == 0 )
        {
            print_fragmentation();
        }
        else
        {
            DMOD_LOG_ERROR("Unknown option: %s\n", arg);
            print_usage();
            return -EINVAL;
        }
    }

    return 0;
}
