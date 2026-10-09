#ifndef CTR_FLASH_H
#define CTR_FLASH_H

#include <stdbool.h>

bool CtrFlash_Init(const char *path);
/* Keep an existing destination; otherwise copy the first present legacy save.
 * Sources are untouched. Any read/write/close error aborts initialization.
 * Only if both sources are absent may a new blank save be created. */
bool CtrFlash_InitMigrating(const char *path, const char *legacyPath,
                            const char *fallbackPath);
/* Writes the pending sectors; true only once the write is confirmed. On failure they stay pending. */
bool CtrFlash_Flush(void);
/* Flushes (one retry) and closes; false if anything pending could not be written. */
bool CtrFlash_Close(void);

#endif
