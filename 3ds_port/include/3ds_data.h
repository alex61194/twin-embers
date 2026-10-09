/* Adapted from ZallaxDev 6419a400; MIT: licenses/ZallaxDev-MIT.txt. */
#ifndef CTR_DATA_H
#define CTR_DATA_H

/*
 * Game data access.
 *
 * Everything derived from the game itself (graphics, maps, scripts, songs,
 * linked game tables, voxel data) is read through this interface, never with
 * fopen. Three backends serve the same relative paths ("maps/layouts.bin"):
 *
 *   romfs  the executable's own RomFS, in a development build that embeds
 *          its data;
 *   loose  sdmc:/3ds/twinembers/devdata/<path>, for iterating on one
 *          generated file without rebuilding anything else;
 *   pak    sdmc:/3ds/twinembers/twinembers.pak, the single data pack the
 *          Pokemon FireRed 3Ds Dual Screen Builder generates from the player's own ROM.
 *
 * Files that only describe the executable (asset index, relocation tables,
 * shaders, the engine ABI) are not game data and are always read from the
 * RomFS directly.
 *
 * SDK-free: game translation units include this header.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef CTR_DATA_DIR
#define CTR_DATA_DIR "sdmc:/3ds/twinembers/"
#endif
#ifndef CTR_DATA_ROMFS_DIR
#define CTR_DATA_ROMFS_DIR "romfs:/"
#endif
#define CTR_DATA_PAK_PATH CTR_DATA_DIR "twinembers.pak"
#define CTR_DATA_LOOSE_DIR CTR_DATA_DIR "devdata/"
#define CTR_DATA_LOOSE_MARKER CTR_DATA_LOOSE_DIR ".firered3ds-dev"
/* Present in the RomFS of a build that embeds its game data. */
#define CTR_DATA_EMBEDDED_MARKER CTR_DATA_ROMFS_DIR "data.embedded"

/* Pack format, see docs/ASSET_PIPELINE.md and builder/firered3ds_builder/pak.py. */
#define CTR_PAK_MAGIC "FR3DPAK"
#define CTR_PAK_SCHEMA 2

typedef enum
{
    CTR_DATA_NONE,
    CTR_DATA_ROMFS,
    CTR_DATA_LOOSE,
    CTR_DATA_PAK,
} CtrDataBackend;

/* Chooses and validates the backend. On failure CtrData_ErrorTitle and
 * CtrData_ErrorDetail describe what the player has to do. */
bool CtrData_Init(void);
void CtrData_Shutdown(void);
CtrDataBackend CtrData_GetBackend(void);
const char *CtrData_BackendName(void);
const char *CtrData_ErrorTitle(void);
const char *CtrData_ErrorDetail(void);
uint32_t CtrData_EngineAbi(void);

/* A read-only stdio stream over one data file, or NULL if it does not exist.
 * fseek/ftell/fread/fclose work as on any file. Safe to call from several
 * threads; each stream must stay on the thread that opened it. */
FILE *CtrData_Open(const char *path);
bool CtrData_Exists(const char *path);
bool CtrData_Size(const char *path, uint32_t *outSize);
/* Exact-size load into caller-owned storage, with payload CRC validation.
 * Prefer this to Open: it reads and checks each payload only once. */
bool CtrData_ReadInto(const char *path, void *buffer, uint32_t size);
/* Same read, optionally returning the computed CRC for an engine manifest.
 * The pack CRC and manifest CRC can thus share one computation. */
bool CtrData_ReadIntoCrc(const char *path, void *buffer, uint32_t size, uint32_t *crc);
/* The whole file in a malloc'd buffer (one extra zero byte past the end), or
 * NULL. The caller frees it. */
void *CtrData_Load(const char *path, uint32_t *outSize);

#endif
