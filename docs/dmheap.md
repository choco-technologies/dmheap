# dmheap - DMOD Heap Memory Manager

## Overview

dmheap is a module-aware heap allocator for the DMOD framework. Every block it
hands out is attributed to a module (by name), so memory usage can be broken
down per module, bulk-freed when a module unloads, and inspected without
guessing which subsystem is responsible for a given allocation.

## Architecture

A `dmheap_context_t` owns a `free_list` and a `used_list` of `block_t` entries
(next pointer, address, size, owning module) plus a `module_list` of
registered module names. Allocation walks `free_list` for a big-enough block,
splitting off any leftover space back into `free_list`; freeing moves a block
from `used_list` back to `free_list`. `free_list` is kept sorted smallest to
largest, which turns the first-fit search into an effective best-fit search
and keeps large blocks intact for large requests.

Multiple independent heaps can coexist via separate `dmheap_context_t`
instances (`dmheap_init`); most call sites pass `NULL` to fall back to the
*default heap list* instead of naming a context explicitly.

### The default heap list

The default heap list holds up to `DMHEAP_MAX_DEFAULT_CONTEXTS` (8) heaps.
The very first heap ever initialized via `dmheap_init()` joins it
automatically; `dmheap_add_default_context(ctx)` adds further heaps,
`dmheap_remove_default_context(ctx)` takes one back out, and
`dmheap_set_default_context(ctx)` replaces the whole list with a single entry
(or clears it if `ctx` is `NULL`).

Removing a heap from the list doesn't tear it down - it just stops NULL-context
calls from reaching it. Do this *before* actually freeing/unmapping a heap's
backing buffer (e.g. when unloading a driver/module that owns its own heap),
otherwise a concurrent NULL-context call (malloc, free, get_stats, ...) could
walk into memory that no longer belongs to the heap.

When a `NULL` context is passed to an API function, the behavior depends on
what the call needs:

- **Allocation** (`dmheap_malloc`, `dmheap_aligned_alloc`, `dmheap_realloc`
  growing a block, and the SAL `Dmod_Malloc`/`Dmod_MallocEx`/... wrappers)
  tries every heap in the default list, in the order it was added, and
  returns the first successful allocation - so a heap that's simply full
  doesn't fail the request as long as another default heap has room.
- **Pointer-based operations** (`dmheap_free`, `dmheap_realloc`,
  `dmheap_retag`) search the default list for the heap that actually owns the
  pointer, since a `NULL`-context allocation may have landed on any of them.
- **Module registration** (`dmheap_register_module`,
  `dmheap_unregister_module`, and `Dmod_FreeModule`) applies to every heap in
  the list, so a module's memory is fully reclaimed regardless of which
  default heap(s) it ended up allocating from.
- **Reporting** (`dmheap_get_stats`, `dmheap_for_each_free_block`,
  `dmheap_for_each_used_block`, `dmheap_concatenate_free_blocks`) aggregates
  across every heap in the list.

A heap not in the default list is never touched by a `NULL`-context call -
pass its `dmheap_context_t*` explicitly to work with it.

## Module Tracking

Every allocation is tagged with a module name string. Untracked/kernel
allocations (no ambient module identity available) are tagged `NULL` rather
than attributed to the wrong owner. A block's owner can be changed after the
fact with `dmheap_retag()` / `Dmod_RetagEx()` - useful when the true owner
is only known once something else (a decompression buffer, a thread stack)
has already been allocated under a different or absent identity.

## API Reference

### Initialization

- `dmheap_init(buffer, size, alignment)` - initialize a heap over a raw
  buffer, returning a context. The first heap ever initialized joins the
  default heap list automatically (see above).
- `dmheap_add_default_context(ctx)` - add another heap to the default heap
  list.
- `dmheap_remove_default_context(ctx)` - remove a heap from the default heap
  list (e.g. before tearing it down), without affecting the heap itself.
- `dmheap_set_default_context(ctx)` - replace the default heap list with a
  single entry (or clear it, if `ctx` is `NULL`).
- `dmheap_get_default_context()` - the first (primary) heap in the default
  list, or `NULL` if it's empty.
- `dmheap_get_default_context_count()` / `dmheap_get_default_context_at(index)`
  - enumerate every heap currently in the default list.
- `dmheap_is_initialized(ctx)` - check whether a context has been
  initialized (`NULL` checks whether the default heap list is non-empty).

### Module registration

- `dmheap_register_module(ctx, module_name)` - register a module explicitly
  (allocation functions also register on first use).
- `dmheap_unregister_module(ctx, module_name)` - unregister a module and free
  every block it still owns.

### Allocation

- `dmheap_malloc(ctx, size, module_name)`
- `dmheap_aligned_alloc(ctx, alignment, size, module_name)`
- `dmheap_realloc(ctx, ptr, size, module_name)`
- `dmheap_free(ctx, ptr, concatenate)` - `concatenate = true` additionally
  merges adjacent free blocks around the freed one.
- `dmheap_concatenate_free_blocks(ctx)` - merge every pair of adjacent free
  blocks in the heap; called automatically as a retry step when an allocation
  fails purely due to fragmentation.
- `dmheap_retag(ctx, ptr, new_module_name)` - reattribute an already
  allocated block to a different module.

### DMOD SAL integration

When built without `DMHEAP_DONT_IMPLEMENT_DMOD_API`, dmheap also implements
the generic `Dmod_MallocEx` / `Dmod_ReallocEx` / `Dmod_AlignedMallocEx` /
`Dmod_FreeEx` / `Dmod_FreeModule` / `Dmod_RetagEx` DMOD SAL functions,
backing the whole system's memory management.

### Inspection

- `dmheap_get_stats(ctx, dmheap_stats_t* out_stats)` - aggregate statistics:
  total heap size, free/used bytes, free/used block counts, largest and
  smallest free block.
- `dmheap_for_each_free_block(ctx, visitor, user_data)` /
  `dmheap_for_each_used_block(ctx, visitor, user_data)` - walk every
  free/used block, calling `visitor(address, size, owner_name, user_data)`
  for each one. The visitor runs while the heap's internal lock is held, so
  it must be fast and must not call back into dmheap or do blocking I/O.

See [tools/memory](../tools/memory/docs/memory.md) for a ready-made CLI tool
built on top of `dmheap_get_stats`/`dmheap_for_each_*_block` that prints heap
occupancy, per-module allocation summaries, and free-block fragmentation
reports.
