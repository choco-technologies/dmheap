# memory - dmheap Inspection Command

## Overview

`memory` is a DMOD application module that inspects the live state of the
dmheap allocator: overall occupancy, a per-module allocation breakdown, and a
fragmentation histogram of free blocks. It is a thin CLI wrapper around
dmheap's own introspection API (`dmheap_get_stats`,
`dmheap_for_each_used_block`, `dmheap_for_each_free_block`) declared in
[dmheap.h](../../../docs/dmheap.md) - it does not maintain any state of its
own.

## Usage

```
memory [options]
```

| Option | Description |
|---|---|
| `-s`, `--stats` | Print overall heap statistics: total size, free space, used space, usage percentage (`Used / TotalSize * 100`), block count (free/used), largest and smallest free block, and a fragmentation percentage (`(Free - LargestFree) / Free * 100`, the share of free memory outside the single largest free block). Finishes with a VT100 usage bar per heap plus the combined total. |
| `-m`, `--modules` | Print a summary of every module that has allocated memory, including untracked (`(null)`) allocations, with block count and total bytes per module. |
| `-f`, `--fragmentation` | Print a histogram of free block sizes: for each distinct block size, how many blocks of that size exist and how many bytes they add up to. |
| `-h`, `--help` | Show usage information. |

Options can be combined in a single call, e.g. `memory --stats --modules`.
Running `memory` with no arguments prints the help message.

## Multiple heaps

All reports are driven by dmheap's [default heap list](../../../docs/dmheap.md#the-default-heap-list)
(`dmheap_get_stats`/`dmheap_for_each_*_block` called with a `NULL` context),
not a single fixed heap:

- `--stats` prints each default heap's statistics individually (`Heap #0`,
  `Heap #1`, ...) followed by a combined `Total (all heaps)` section, when
  more than one heap is registered as a default heap. With only one default
  heap, it prints the original unlabeled `Heap statistics:` output. A heap
  named via `dmheap_set_context_name()` (see [dmheap.md](../../../docs/dmheap.md))
  is shown as `Heap #N (name)` instead of a bare index, and that name is
  reused as the label on its VT100 usage bar.
- `--modules` and `--fragmentation` walk every default heap's blocks and
  report the combined totals, since a module's allocations may be spread
  across more than one of them.

## Implementation Notes

- The module-summary and fragmentation reports are built by walking dmheap's
  used/free block lists via a visitor callback. Since the callback runs while
  dmheap's internal lock is held, it only accumulates into fixed-size arrays
  (up to 64 distinct modules, up to 128 distinct free-block sizes) - no
  printing happens until the walk is done.
- Those aggregation arrays are allocated on the heap (`Dmod_Malloc`), not the
  stack, to keep the module's stack usage small and predictable regardless of
  how many modules or distinct free-block sizes exist.
- The free-block size histogram is kept sorted by inserting each new size in
  place, rather than collecting first and calling a library sort - this
  avoids depending on a libc `qsort` that may not be available/linked for the
  target architecture.

## Exit Codes

- `0` - Success
- `-EINVAL` - Unknown option

## Sample `--stats` output

With two named default heaps (`sensors`, `network`):

```
Heap statistics (2 heaps):
Heap #0 (sensors):
  Total size:     16384 bytes
  Free:           6144 bytes
  Used:           10240 bytes
  Usage:          62.5%
  Blocks:         5 (2 free, 3 used)
  Largest free:   4096 bytes
  Smallest free:  2048 bytes
  Fragmentation:  33.3%
Heap #1 (network):
  Total size:     8192 bytes
  Free:           7372 bytes
  Used:           820 bytes
  Usage:          10.0%
  ...
Total (all heaps):
  Total size:     24576 bytes
  ...
  Usage:          45.0%
  ...

  sensors                  [##########################--------------] 62.5%
  network                  [####------------------------------------] 10.0%
  Total                    [##################----------------------] 45.0%
```

The bars are colored green below 70% usage, yellow from 70-90%, and red above
90%.

## Examples

```bash
# Overall heap occupancy
memory --stats

# Which modules are holding memory
memory --modules

# How fragmented the free list is
memory --fragmentation

# Everything at once
memory -s -m -f
```
