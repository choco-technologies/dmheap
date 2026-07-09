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
| `-s`, `--stats` | Print overall heap statistics: total size, free space, block count (free/used), largest and smallest free block, and a fragmentation percentage (`(Free - LargestFree) / Free * 100`, the share of free memory outside the single largest free block). |
| `-m`, `--modules` | Print a summary of every module that has allocated memory, including untracked (`(null)`) allocations, with block count and total bytes per module. |
| `-f`, `--fragmentation` | Print a histogram of free block sizes: for each distinct block size, how many blocks of that size exist and how many bytes they add up to. |
| `-h`, `--help` | Show usage information. |

Options can be combined in a single call, e.g. `memory --stats --modules`.
Running `memory` with no arguments prints the help message.

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
