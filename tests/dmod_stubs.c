/**
 * @file dmod_stubs.c
 * @brief Stub implementations for DMOD linker symbols required by tests
 * 
 * These symbols are normally provided by the dmod-common.ld linker script.
 * For test executables, we provide stub implementations to satisfy the linker.
 */

// Stub for DMOD input API symbols
void* __dmod_inputs_start __attribute__((weak)) = 0;
void* __dmod_inputs_size __attribute__((weak)) = 0;

// Stub for DMOD output API symbols
void* __dmod_outputs_start __attribute__((weak)) = 0;
void* __dmod_outputs_size __attribute__((weak)) = 0;
