#ifndef CTR_ASSETS_H
#define CTR_ASSETS_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Fonts/UI retain their original array addresses, dimensions and alignment.
 * Only storage initialization crosses the data boundary. */
bool CtrAssets_Init(uintptr_t referenceRuntime);
void CtrAssets_Shutdown(void);
/* Preloads the lazy graphics groups (intro, overworld, battle, rest) on a
 * low-priority thread, into the same resident storage the lazy path fills.
 * Unavailable (thread creation fails): lazy loading is unchanged. */
void CtrAssets_StartWarmup(void);
bool CtrAssets_EnsureRange(const void *source, size_t bytes);
void CtrAssets_RequireRange(const void *source, size_t bytes);
void *CtrAssets_Memcpy(void *dest, const void *source, size_t bytes);
void *CtrAssets_Memmove(void *dest, const void *source, size_t bytes);
#ifdef CTR_EXTERNAL_ASSETS
#define CTR_ASSET_READ(source, bytes) do { \
    CtrAssets_RequireRange((source), (bytes)); \
    __asm__ volatile ("" ::: "memory"); \
} while (0)
#else
#define CTR_ASSET_READ(source, bytes) ((void)(source), (void)(bytes))
#endif
#endif
