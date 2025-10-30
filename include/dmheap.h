#ifndef DMHEAP_H
#define DMHEAP_H

#include "dmod.h"

DMOD_BUILTIN_API( dmheap, 1.0, bool             , _init, ( void* buffer, size_t size, size_t alignment ) );
DMOD_BUILTIN_API( dmheap, 1.0, bool             , _is_initialized, ( void ) );
DMOD_BUILTIN_API( dmheap, 1.0, bool             , _register_module, ( const char* module_name ) );
DMOD_BUILTIN_API( dmheap, 1.0, void             , _unregister_module, ( const char* module_name ) );
DMOD_BUILTIN_API( dmheap, 1.0, void*            , _malloc, ( size_t size, const char* module_name ) );
DMOD_BUILTIN_API( dmheap, 1.0, void*            , _realloc, ( void* ptr, size_t size, const char* module_name) );
DMOD_BUILTIN_API( dmheap, 1.0, void             , _free, ( void* ptr, bool concatenate ) );
DMOD_BUILTIN_API( dmheap, 1.0, void*            , _aligned_alloc, ( size_t alignment, size_t size, const char* module_name) );
DMOD_BUILTIN_API( dmheap, 1.0, void             , _concatenate_free_blocks, ( void ) );

#endif // DMHEAP_H