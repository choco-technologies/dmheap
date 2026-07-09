# memory - dmheap Inspection Command

## Description

The `memory` command inspects the state of the dmheap allocator: overall heap
occupancy, a per-module allocation breakdown, and a fragmentation histogram of
free blocks. It is built on top of dmheap's introspection API
(`dmheap_get_stats`, `dmheap_for_each_used_block`, `dmheap_for_each_free_block`)
declared in `dmheap.h`.

## Usage

```
memory [options]
```

## Options

- `-s`, `--stats` - Print overall heap statistics: total size, free space,
  block count (free/used), the largest/smallest free block, and a
  fragmentation percentage (share of free memory outside the largest free
  block).
- `-m`, `--modules` - Print a summary of every module that has allocated
  memory, including untracked (`(null)`) allocations, with block count and
  total bytes per module.
- `-f`, `--fragmentation` - Print a histogram of free block sizes: for each
  distinct block size, how many blocks of that size exist and how many bytes
  they add up to.
- `-h`, `--help` - Show usage information.

Options can be combined in a single call, e.g. `memory --stats --modules`.
Running `memory` with no arguments prints the help message.

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
