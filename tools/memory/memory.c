#include <dmod.h>
#include <dmheap.h>
#include <string.h>
#include <errno.h>

#define MAX_HEAPS               8
#define DEFAULT_LARGEST_COUNT   10
#define MAX_LARGEST_COUNT       32
#define HEAP_COLUMN_WIDTH       10

// Heap selected with --heap, or NULL for every default heap.
static dmheap_context_t* g_selected_heap = NULL;

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
    Dmod_Printf("  -a, --allocations     Print per-heap allocation counters (explicit, first\n");
    Dmod_Printf("                        choice, fallback) and why heaps refused requests\n");
    Dmod_Printf("  -e, --fallbacks       Print the most recent allocations that had to fall\n");
    Dmod_Printf("                        back to a lower-priority heap, with the reason\n");
    Dmod_Printf("  -l, --largest [N]     Print the N largest used blocks (default %d)\n", DEFAULT_LARGEST_COUNT);
    Dmod_Printf("  -H, --heap NAME       Limit --modules, --fragmentation and --largest to one\n");
    Dmod_Printf("                        heap (its name or its #index from --stats)\n");
    Dmod_Printf("  -h, --help            Show this help message\n\n");
    Dmod_Printf("Multiple options can be combined in a single call, e.g.\n");
    Dmod_Printf("  memory --stats --modules\n\n");
    Dmod_Printf("If more than one heap was registered as a default heap (see\n");
    Dmod_Printf("dmheap_add_default_context()), --stats reports each heap individually plus\n");
    Dmod_Printf("a combined total, --modules adds a column per heap, and --fragmentation/--largest\n");
    Dmod_Printf("report combined across all of them (unless --heap selects one).\n");
    Dmod_Printf("Heaps named via dmheap_set_context_name() are shown by name; --stats also\n");
    Dmod_Printf("prints a used/total percentage per heap and a VT100 usage bar for each of\n");
    Dmod_Printf("them plus the combined total.\n");
}

// ============================================================================
//                              Heap helpers
// ============================================================================

// Name of a heap for column headers and listings: its name, or "#<index>"
// (the index from --stats) when it was never named.
static const char* heap_display_name( dmheap_context_t* ctx, char* buffer, size_t buffer_size )
{
    const char* name = ctx != NULL ? dmheap_get_context_name( ctx ) : NULL;
    if( name != NULL && name[0] != '\0' )
    {
        return name;
    }
    size_t count = dmheap_get_default_context_count();
    for( size_t i = 0; i < count; i++ )
    {
        if( dmheap_get_default_context_at(i) == ctx )
        {
            Dmod_SnPrintf( buffer, buffer_size, "#%u", (unsigned)i );
            return buffer;
        }
    }
    Dmod_SnPrintf( buffer, buffer_size, "?" );
    return buffer;
}

// Resolves the argument of --heap: a heap name, or "#<index>"/"<index>" as shown by --stats.
static dmheap_context_t* find_heap( const char* arg )
{
    dmheap_context_t* ctx = dmheap_get_context_by_name( arg );
    if( ctx != NULL )
    {
        return ctx;
    }
    const char* digits = arg[0] == '#' ? arg + 1 : arg;
    if( digits[0] == '\0' )
    {
        return NULL;
    }
    size_t index = 0;
    for( const char* c = digits; *c != '\0'; c++ )
    {
        if( *c < '0' || *c > '9' )
        {
            return NULL;
        }
        index = index * 10 + (size_t)( *c - '0' );
    }
    return dmheap_get_default_context_at( index );
}

// The heaps a report covers: the one selected with --heap, or every default heap.
static size_t selected_heaps( dmheap_context_t** heaps )
{
    if( g_selected_heap != NULL )
    {
        heaps[0] = g_selected_heap;
        return 1;
    }
    size_t count = dmheap_get_default_context_count();
    if( count > MAX_HEAPS )
    {
        count = MAX_HEAPS;
    }
    for( size_t i = 0; i < count; i++ )
    {
        heaps[i] = dmheap_get_default_context_at(i);
    }
    return count;
}

// ============================================================================
//                              --stats
// ============================================================================

// Share of the heap's total size currently handed out to allocations.
static double usage_percent( const dmheap_stats_t* stats )
{
    if( stats->heap_size == 0 )
    {
        return 0.0;
    }
    return ( (double)stats->used_bytes / (double)stats->heap_size ) * 100.0;
}

static void print_one_heap_stats( const dmheap_stats_t* stats )
{
    size_t total_block_count = stats->free_block_count + stats->used_block_count;

    // Share of free memory that sits outside the single largest free block, i.e. how
    // much of it is unusable for an allocation bigger than that block.
    double fragmentation_percent = 0.0;
    if( stats->free_bytes > 0 )
    {
        fragmentation_percent = ( (double)(stats->free_bytes - stats->largest_free_block) / (double)stats->free_bytes ) * 100.0;
    }

    Dmod_Printf("  Total size:     %zu bytes\n", stats->heap_size);
    Dmod_Printf("  Free:           %zu bytes\n", stats->free_bytes);
    Dmod_Printf("  Used:           %zu bytes\n", stats->used_bytes);
    Dmod_Printf("  Usage:          %.1f%%\n", usage_percent(stats));
    Dmod_Printf("  Blocks:         %zu (%zu free, %zu used)\n",
        total_block_count, stats->free_block_count, stats->used_block_count);
    Dmod_Printf("  Largest free:   %zu bytes\n", stats->largest_free_block);
    Dmod_Printf("  Smallest free:  %zu bytes\n", stats->smallest_free_block);
    Dmod_Printf("  Fragmentation:  %.1f%%\n", fragmentation_percent);
}

// ============================================================================
//                              VT100 usage bar
// ============================================================================

#define USAGE_BAR_WIDTH 40

// Same block glyphs as DMOD_LOG_STEP_PROGRESS (see Dmod_GetStepBar): each cell
// is a 3-byte UTF-8 sequence, so the bar can't just be memset() like a plain
// char string - it has to be assembled cell by cell.
#define USAGE_BAR_CELL_FULL  "\xe2\x96\x88" // █
#define USAGE_BAR_CELL_EMPTY "\xe2\x96\x91" // ░
#define USAGE_BAR_CELL_BYTES 3

// Green below 70%, yellow up to 90%, red beyond that - a quick visual cue for
// heaps that are getting dangerously full.
static const char* usage_bar_color( double percent )
{
    if( percent >= 90.0 )
    {
        return "\033[31;1m"; // red
    }
    else if( percent >= 70.0 )
    {
        return "\033[33;1m"; // yellow
    }
    return "\033[32;1m"; // green
}

static void print_usage_bar( const char* label, double percent )
{
    if( percent < 0.0 )
    {
        percent = 0.0;
    }
    else if( percent > 100.0 )
    {
        percent = 100.0;
    }

    size_t filled = (size_t)( ( percent / 100.0 ) * USAGE_BAR_WIDTH + 0.5 );
    if( filled > USAGE_BAR_WIDTH )
    {
        filled = USAGE_BAR_WIDTH;
    }

    char bar[USAGE_BAR_WIDTH * USAGE_BAR_CELL_BYTES + 1];
    size_t i = 0;
    for( ; i < filled; i++ )
    {
        memcpy( bar + i * USAGE_BAR_CELL_BYTES, USAGE_BAR_CELL_FULL, USAGE_BAR_CELL_BYTES );
    }
    for( ; i < USAGE_BAR_WIDTH; i++ )
    {
        memcpy( bar + i * USAGE_BAR_CELL_BYTES, USAGE_BAR_CELL_EMPTY, USAGE_BAR_CELL_BYTES );
    }
    bar[USAGE_BAR_WIDTH * USAGE_BAR_CELL_BYTES] = '\0';

    Dmod_Printf("  %-24s [%s%s\033[0m] %5.1f%%\n", label, usage_bar_color(percent), bar, percent);
}

// Prints "Heap #<index>" or, if the context was named via dmheap_set_context_name(),
// "Heap #<index> (<name>)".
static void print_heap_label( size_t index, dmheap_context_t* ctx )
{
    const char* name = dmheap_get_context_name( ctx );
    if( name != NULL && name[0] != '\0' )
    {
        Dmod_Printf("Heap #%zu (%s):\n", index, name);
    }
    else
    {
        Dmod_Printf("Heap #%zu:\n", index);
    }
}

static void print_stats( void )
{
    size_t heap_count = dmheap_get_default_context_count();
    if( heap_count == 0 )
    {
        DMOD_LOG_ERROR("Failed to read heap statistics\n");
        return;
    }

    // A single default heap: keep the original, unlabeled output.
    if( heap_count == 1 )
    {
        dmheap_context_t* ctx = dmheap_get_default_context_at(0);
        dmheap_stats_t stats;
        if( !dmheap_get_stats( ctx, &stats ) )
        {
            DMOD_LOG_ERROR("Failed to read heap statistics\n");
            return;
        }
        const char* name = dmheap_get_context_name( ctx );
        if( name != NULL && name[0] != '\0' )
        {
            Dmod_Printf("Heap statistics (%s):\n", name);
        }
        else
        {
            Dmod_Printf("Heap statistics:\n");
        }
        print_one_heap_stats( &stats );
        Dmod_Printf("\n");
        print_usage_bar( name != NULL && name[0] != '\0' ? name : "Heap", usage_percent(&stats) );
        return;
    }

    // Multiple default heaps: break the numbers down per heap, then show the combined total.
    Dmod_Printf("Heap statistics (%zu heaps):\n", heap_count);
    for( size_t i = 0; i < heap_count; i++ )
    {
        dmheap_context_t* ctx = dmheap_get_default_context_at(i);
        dmheap_stats_t stats;
        if( !dmheap_get_stats( ctx, &stats ) )
        {
            DMOD_LOG_ERROR("Failed to read statistics for heap #%zu\n", i);
            continue;
        }
        print_heap_label( i, ctx );
        print_one_heap_stats( &stats );
    }

    dmheap_stats_t total;
    bool have_total = dmheap_get_stats( NULL, &total );
    if( have_total )
    {
        Dmod_Printf("Total (all heaps):\n");
        print_one_heap_stats( &total );
    }

    // Visual overview: one usage bar per heap, followed by the combined total.
    Dmod_Printf("\n");
    for( size_t i = 0; i < heap_count; i++ )
    {
        dmheap_context_t* ctx = dmheap_get_default_context_at(i);
        dmheap_stats_t stats;
        if( !dmheap_get_stats( ctx, &stats ) )
        {
            continue;
        }
        const char* name = dmheap_get_context_name( ctx );
        print_usage_bar( name != NULL && name[0] != '\0' ? name : "Heap", usage_percent(&stats) );
    }
    if( have_total )
    {
        print_usage_bar( "Total", usage_percent(&total) );
    }
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
    size_t heap_bytes[MAX_HEAPS];       // Bytes in each heap of the report
} module_summary_entry_t;

typedef struct module_summary_t
{
    module_summary_entry_t entries[MAX_MODULE_SUMMARY_ENTRIES];
    size_t count;
    bool overflowed;
    size_t current_heap;                // Heap being walked (index into the report's heaps)
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
            entry->heap_bytes[summary->current_heap] += size;
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
    memset( entry->heap_bytes, 0, sizeof(entry->heap_bytes) );
    entry->heap_bytes[summary->current_heap] = size;
}

// Simple insertion sort (list is at most MAX_MODULE_SUMMARY_ENTRIES long, and this
// avoids depending on a libc qsort that may not be available/linked for the target
// architecture - see the same reasoning on the fragmentation histogram below).
static void sort_module_summary( module_summary_t* summary )
{
    for( size_t i = 1; i < summary->count; i++ )
    {
        module_summary_entry_t key = summary->entries[i];
        const char* key_name = key.is_null ? "(null)" : key.name;

        size_t j = i;
        while( j > 0 )
        {
            module_summary_entry_t* prev = &summary->entries[j - 1];
            const char* prev_name = prev->is_null ? "(null)" : prev->name;
            if( strcmp( prev_name, key_name ) <= 0 )
            {
                break;
            }
            summary->entries[j] = *prev;
            j--;
        }
        summary->entries[j] = key;
    }
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

    dmheap_context_t* heaps[MAX_HEAPS];
    size_t heap_count = selected_heaps( heaps );
    for( size_t h = 0; h < heap_count; h++ )
    {
        summary->current_heap = h;
        dmheap_for_each_used_block( heaps[h], module_summary_visitor, summary );
    }
    sort_module_summary( summary );

    // With more than one heap, show where each module's memory actually sits.
    bool per_heap = heap_count > 1;
    char label[16];
    if( g_selected_heap != NULL )
    {
        Dmod_Printf("Module allocation summary for heap %s (%zu module%s):\n",
            heap_display_name( g_selected_heap, label, sizeof(label) ), summary->count, summary->count == 1 ? "" : "s");
    }
    else
    {
        Dmod_Printf("Module allocation summary (%zu module%s):\n",
            summary->count, summary->count == 1 ? "" : "s");
    }
    Dmod_Printf("  %-32s %10s %14s", "MODULE", "BLOCKS", "BYTES");
    if( per_heap )
    {
        for( size_t h = 0; h < heap_count; h++ )
        {
            Dmod_Printf(" %*.*s", HEAP_COLUMN_WIDTH, HEAP_COLUMN_WIDTH, heap_display_name( heaps[h], label, sizeof(label) ));
        }
    }
    Dmod_Printf("\n");
    for( size_t i = 0; i < summary->count; i++ )
    {
        module_summary_entry_t* entry = &summary->entries[i];
        Dmod_Printf("  %-32s %10zu %14zu",
            entry->is_null ? "(null)" : entry->name, entry->block_count, entry->total_bytes);
        if( per_heap )
        {
            for( size_t h = 0; h < heap_count; h++ )
            {
                Dmod_Printf(" %*zu", HEAP_COLUMN_WIDTH, entry->heap_bytes[h]);
            }
        }
        Dmod_Printf("\n");
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

    dmheap_for_each_free_block( g_selected_heap, fragmentation_visitor, frag );

    char label[16];
    if( g_selected_heap != NULL )
    {
        Dmod_Printf("Free block fragmentation of heap %s (%zu distinct size%s):\n",
            heap_display_name( g_selected_heap, label, sizeof(label) ), frag->count, frag->count == 1 ? "" : "s");
    }
    else
    {
        Dmod_Printf("Free block fragmentation (%zu distinct size%s):\n",
            frag->count, frag->count == 1 ? "" : "s");
    }
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
//                              --allocations
// ============================================================================

static void print_allocation_counters_row( const char* label, const dmheap_alloc_counters_t* c )
{
    Dmod_Printf("  %-10s %8zu %10zu %8zu %10zu %8zu %10zu %9zu %10zu\n", label,
        c->explicit_count, c->explicit_bytes,
        c->first_choice_count, c->first_choice_bytes,
        c->fallback_count, c->fallback_bytes,
        c->refused_exhausted_count, c->refused_fragmented_count);
}

static void print_allocations( void )
{
    dmheap_context_t* heaps[MAX_HEAPS];
    size_t heap_count = selected_heaps( heaps );
    if( heap_count == 0 )
    {
        DMOD_LOG_ERROR("No heap to report on\n");
        return;
    }

    Dmod_Printf("Allocation counters since boot (requested bytes):\n");
    Dmod_Printf("  %-10s %19s %19s %19s %20s\n", "", "EXPLICIT", "FIRST CHOICE", "FALLBACK", "REFUSED");
    Dmod_Printf("  %-10s %8s %10s %8s %10s %8s %10s %9s %10s\n", "HEAP",
        "COUNT", "BYTES", "COUNT", "BYTES", "COUNT", "BYTES", "EXHAUSTED", "FRAGMENTED");
    char label[16];
    for( size_t h = 0; h < heap_count; h++ )
    {
        dmheap_alloc_counters_t counters;
        if( dmheap_get_alloc_counters( heaps[h], &counters ) )
        {
            print_allocation_counters_row( heap_display_name( heaps[h], label, sizeof(label) ), &counters );
        }
    }
    dmheap_alloc_counters_t total;
    if( g_selected_heap == NULL && heap_count > 1 && dmheap_get_alloc_counters( NULL, &total ) )
    {
        print_allocation_counters_row( "Total", &total );
    }
    Dmod_Printf("\n");
    Dmod_Printf("EXPLICIT: the allocation asked for this heap. FIRST CHOICE: served while this was\n");
    Dmod_Printf("the first heap searched (the newest default heap - for an early-added heap that\n");
    Dmod_Printf("means before the later heaps existed). FALLBACK: served only because every heap\n");
    Dmod_Printf("searched before it refused (see --fallbacks). REFUSED: requests this heap turned\n");
    Dmod_Printf("down - EXHAUSTED: too little free memory, FRAGMENTED: enough free memory in total\n");
    Dmod_Printf("but no single free block big enough.\n");
}

// ============================================================================
//                              --fallbacks
// ============================================================================

static void print_fallbacks( void )
{
    dmheap_fallback_event_t* events = Dmod_Malloc( sizeof(dmheap_fallback_event_t) * DMHEAP_MAX_FALLBACK_EVENTS );
    if( events == NULL )
    {
        DMOD_LOG_ERROR("Failed to allocate memory for the fallback report\n");
        return;
    }
    size_t count = dmheap_get_fallback_events( events, DMHEAP_MAX_FALLBACK_EVENTS );
    if( count == 0 )
    {
        Dmod_Printf("No allocation had to fall back to a lower-priority heap.\n");
        Dmod_Free( events );
        return;
    }

    uint32_t total = events[count - 1].sequence;
    Dmod_Printf("Most recent fallback allocations (%zu of %u since boot, oldest first):\n", count, (unsigned)total);
    Dmod_Printf("  %6s %-20s %8s %-10s %-10s %-10s %10s %10s\n",
        "#", "MODULE", "SIZE", "SERVED BY", "REFUSED BY", "REASON", "FREE", "LARGEST");
    char served[16];
    char refused[16];
    for( size_t i = 0; i < count; i++ )
    {
        dmheap_fallback_event_t* e = &events[i];
        Dmod_Printf("  %6u %-20.20s %8zu %-10.10s %-10.10s %-10s %10zu %10zu\n",
            (unsigned)e->sequence,
            e->module_name[0] != '\0' ? e->module_name : "(null)",
            e->size,
            heap_display_name( e->served_by, served, sizeof(served) ),
            heap_display_name( e->refused_by, refused, sizeof(refused) ),
            e->reason == DMHEAP_REFUSE_REASON_FRAGMENTED ? "fragmented" : "exhausted",
            e->refused_free_bytes, e->refused_largest_free);
    }
    Dmod_Printf("\nFREE/LARGEST: free memory and largest free block of the refusing heap at that moment.\n");
    Dmod_Free( events );
}

// ============================================================================
//                              --largest
// ============================================================================

typedef struct largest_entry_t
{
    char owner[DMOD_MAX_MODULE_NAME_LENGTH];
    void* address;
    size_t size;
    size_t heap;                        // Index into the report's heaps
} largest_entry_t;

typedef struct largest_t
{
    largest_entry_t entries[MAX_LARGEST_COUNT];
    size_t count;
    size_t limit;
    size_t current_heap;
} largest_t;

// Keeps entries[] sorted by descending size, holding at most `limit` blocks.
static void largest_visitor( void* address, size_t size, const char* owner_name, void* user_data )
{
    largest_t* largest = (largest_t*)user_data;
    if( largest->count == largest->limit && size <= largest->entries[largest->count - 1].size )
    {
        return;
    }

    size_t insert_at = largest->count < largest->limit ? largest->count : largest->limit - 1;
    while( insert_at > 0 && largest->entries[insert_at - 1].size < size )
    {
        if( insert_at < largest->limit )
        {
            largest->entries[insert_at] = largest->entries[insert_at - 1];
        }
        insert_at--;
    }

    largest_entry_t* entry = &largest->entries[insert_at];
    entry->address = address;
    entry->size = size;
    entry->heap = largest->current_heap;
    entry->owner[0] = '\0';
    if( owner_name != NULL )
    {
        strncpy( entry->owner, owner_name, sizeof(entry->owner) - 1 );
        entry->owner[sizeof(entry->owner) - 1] = '\0';
    }
    if( largest->count < largest->limit )
    {
        largest->count++;
    }
}

static void print_largest( size_t limit )
{
    largest_t* largest = Dmod_Malloc( sizeof(largest_t) );
    if( largest == NULL )
    {
        DMOD_LOG_ERROR("Failed to allocate memory for the largest block report\n");
        return;
    }
    memset( largest, 0, sizeof(*largest) );
    largest->limit = limit;

    dmheap_context_t* heaps[MAX_HEAPS];
    size_t heap_count = selected_heaps( heaps );
    for( size_t h = 0; h < heap_count; h++ )
    {
        largest->current_heap = h;
        dmheap_for_each_used_block( heaps[h], largest_visitor, largest );
    }

    char label[16];
    Dmod_Printf("Largest used blocks (%zu):\n", largest->count);
    Dmod_Printf("  %10s  %-10s %-10s %s\n", "SIZE", "ADDRESS", "HEAP", "MODULE");
    for( size_t i = 0; i < largest->count; i++ )
    {
        largest_entry_t* entry = &largest->entries[i];
        Dmod_Printf("  %10zu  %p %-10.10s %s\n", entry->size, entry->address,
            heap_display_name( heaps[entry->heap], label, sizeof(label) ),
            entry->owner[0] != '\0' ? entry->owner : "(null)");
    }
    Dmod_Free( largest );
}

// Parses the optional count after --largest; returns 0 when `arg` is not a number.
static size_t parse_count( const char* arg )
{
    if( arg == NULL || arg[0] == '\0' )
    {
        return 0;
    }
    size_t value = 0;
    for( const char* c = arg; *c != '\0'; c++ )
    {
        if( *c < '0' || *c > '9' )
        {
            return 0;
        }
        value = value * 10 + (size_t)( *c - '0' );
        if( value > MAX_LARGEST_COUNT )
        {
            return MAX_LARGEST_COUNT;
        }
    }
    return value;
}

// ============================================================================
//                              Entry point
// ============================================================================

/**
 * @brief Entry point for the 'memory' tool module.
 *
 * Inspects the dmheap allocator's current state: overall occupancy, a
 * per-module (and per-heap) allocation breakdown, free-block fragmentation,
 * the largest allocations, the per-heap allocation counters and the most
 * recent fallbacks to lower-priority heaps.
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

    // --heap applies to every report of the call, wherever it appears.
    g_selected_heap = NULL;
    for( int i = 1; i < argc; i++ )
    {
        if( strcmp( argv[i], "-H" ) == 0 || strcmp( argv[i], "--heap" ) == 0 )
        {
            if( i + 1 >= argc )
            {
                DMOD_LOG_ERROR("Option %s needs a heap name\n", argv[i]);
                return -EINVAL;
            }
            g_selected_heap = find_heap( argv[i + 1] );
            if( g_selected_heap == NULL )
            {
                DMOD_LOG_ERROR("Unknown heap: %s\n", argv[i + 1]);
                return -EINVAL;
            }
        }
    }

    for( int i = 1; i < argc; i++ )
    {
        const char* arg = argv[i];

        if( strcmp( arg, "-H" ) == 0 || strcmp( arg, "--heap" ) == 0 )
        {
            i++; // already handled above
        }
        else if( strcmp( arg, "-h" ) == 0 || strcmp( arg, "--help" ) == 0 )
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
        else if( strcmp( arg, "-a" ) == 0 || strcmp( arg, "--allocations" ) == 0 )
        {
            print_allocations();
        }
        else if( strcmp( arg, "-e" ) == 0 || strcmp( arg, "--fallbacks" ) == 0 )
        {
            print_fallbacks();
        }
        else if( strcmp( arg, "-l" ) == 0 || strcmp( arg, "--largest" ) == 0 )
        {
            size_t limit = i + 1 < argc ? parse_count( argv[i + 1] ) : 0;
            if( limit > 0 )
            {
                i++;
            }
            print_largest( limit > 0 ? limit : DEFAULT_LARGEST_COUNT );
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
