"""Exercise the production save code with synthetic bytes and injected I/O faults."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from host_compiler import compile_host

ROOT = Path(__file__).resolve().parents[1]


class SaveMigration(unittest.TestCase):
    def test_production_migration(self):
        dest = ROOT / 'build/tests'
        dest.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=dest) as directory:
            work = Path(directory)
            (work / 'gba').mkdir()
            (work / 'global.h').write_text('''#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
''')
            (work / 'gba/flash_internal.h').write_text('''#define FLASH_ROM_SIZE_1M 131072
struct FlashType {
    u32 romSize;
    struct { u32 size; u8 shift; u16 count, top; } sector;
    u16 wait[2];
    union { struct { u8 makerId, deviceId; } separate; u16 joined; } ids;
};
''')
            harness = work / 'migration.c'
            harness.write_text(r'''
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
static int fault, close_count;
static size_t read_fault(void *p, size_t s, size_t n, FILE *f) {
    if (fault == 1) return 0;
    return fread(p, s, n, f);
}
static int error_fault(FILE *f) { return fault == 2 || ferror(f); }
static size_t write_fault(const void *p, size_t s, size_t n, FILE *f) {
    if (fault == 3) return fwrite(p, s, n / 2, f);
    return fwrite(p, s, n, f);
}
static int flush_fault(FILE *f) {
    int result = fflush(f);
    return fault == 4 ? EOF : result;
}
static int close_fault(FILE *f) {
    int result = fclose(f);
    ++close_count;
    return (fault == 5 && close_count == 2) ||
           (fault == 6 && close_count == 1) ? EOF : result;
}
static FILE *open_fault(const char *p, const char *m) {
    if (fault == 7 || (fault == 9 && strcmp(p, "preferred.sav") == 0)) {
        errno = EACCES; return NULL;
    }
    return fopen(p, m);
}
static int exclusive_fault(const char *p, int flags, int mode) {
    if (fault == 8) {
        FILE *racer = fopen(p, "wb");
        assert(racer && fputs("racer", racer) >= 0 && fclose(racer) == 0);
    }
    return open(p, flags, mode);
}
#define fread read_fault
#define ferror error_fault
#define fwrite write_fault
#define fflush flush_fault
#define fclose close_fault
#define fopen open_fault
#define open exclusive_fault
#include "3ds_flash.c"
#undef fread
#undef ferror
#undef fwrite
#undef fflush
#undef fclose
#undef fopen
#undef open
static void seed(const char *path, int value) {
    FILE *f = fopen(path, "wb");
    assert(f);
    for (int i = 0; i < 131072; ++i) assert(fputc(value, f) != EOF);
    assert(fclose(f) == 0);
}
static void check(const char *path, int value) {
    FILE *f = fopen(path, "rb");
    assert(f);
    for (int i = 0; i < 131072; ++i) assert(fgetc(f) == value);
    assert(fgetc(f) == EOF && !ferror(f) && fclose(f) == 0);
}
int main(void) {
    seed("preferred.sav", 0x42);
    seed("fallback.sav", 0x17);
    assert(CtrFlash_InitMigrating("new.sav", "preferred.sav", "fallback.sav"));
    CtrFlash_Close();
    check("new.sav", 0x42);
    seed("preferred.sav", 0x33);
    assert(CtrFlash_InitMigrating("new.sav", "preferred.sav", "fallback.sav"));
    CtrFlash_Close();
    check("new.sav", 0x42);
    remove("new.sav");
    assert(CtrFlash_InitMigrating("new.sav", "absent.sav", "fallback.sav"));
    CtrFlash_Close();
    check("new.sav", 0x17);
    remove("new.sav");
    for (fault = 1; fault <= 7; ++fault) {
        close_count = 0;
        assert(!CtrFlash_InitMigrating("new.sav", "preferred.sav", "fallback.sav"));
        assert(fopen("new.sav", "rb") == NULL);
        check("preferred.sav", 0x33);
        check("fallback.sav", 0x17);
    }
    close_count = 0;
    assert(!CtrFlash_InitMigrating("new.sav", "preferred.sav", "fallback.sav"));
    fault = 0;
    FILE *racer = fopen("new.sav", "rb");
    assert(racer && fgetc(racer) == 'r' && fseek(racer, 0, SEEK_END) == 0 && ftell(racer) == 5);
    fclose(racer);
    assert(!CtrFlash_InitMigrating("new.sav", "preferred.sav", "fallback.sav"));
    remove("new.sav");
    fault = 9;
    assert(!CtrFlash_InitMigrating("new.sav", "preferred.sav", "fallback.sav"));
    fault = 0;
    assert(fopen("new.sav", "rb") == NULL);
    FILE *oversized = fopen("preferred.sav", "ab");
    assert(oversized && fputc(0, oversized) != EOF && fclose(oversized) == 0);
    assert(!CtrFlash_InitMigrating("new.sav", "preferred.sav", "fallback.sav"));
    assert(fopen("new.sav", "rb") == NULL);
    assert(CtrFlash_InitMigrating("new.sav", "absent.sav", "also-absent.sav"));
    CtrFlash_Close();
    check("new.sav", 0xff);
    return 0;
}
''', encoding='utf-8')
            exe = work / 'migration.exe'
            compile_host([os.environ.get('HOST_CC', 'gcc'), '-std=c11', '-O2',
                          '-Wall', '-Wextra', '-Werror', '-I' + str(work),
                          '-I' + str(ROOT / '3ds_port/include'),
                          '-I' + str(ROOT / '3ds_port/src'), str(harness), '-o', str(exe)])
            subprocess.run([str(exe)], cwd=work, check=True)

    def test_flush_keeps_sectors_until_confirmed(self):
        """A failed seek, write, flush or close never counts as written: the sectors stay
        pending, a retry writes them again and only a confirmed flush reports success."""
        dest = ROOT / 'build/tests'
        dest.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=dest) as directory:
            work = Path(directory)
            (work / 'gba').mkdir()
            (work / 'global.h').write_text('#include <stdint.h>\ntypedef uint8_t u8;\n'
                                           'typedef uint16_t u16;\ntypedef uint32_t u32;\n')
            (work / 'gba/flash_internal.h').write_text('''#define FLASH_ROM_SIZE_1M 131072
struct FlashType {
    u32 romSize;
    struct { u32 size; u8 shift; u16 count, top; } sector;
    u16 wait[2];
    union { struct { u8 makerId, deviceId; } separate; u16 joined; } ids;
};
''')
            harness = work / 'flush.c'
            harness.write_text(r'''
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <assert.h>
#include <string.h>
static int fault;          /* 1 seek, 2 short write, 3 flush, 4 close, 5 second write only */
static int writes, closes;
static size_t written;
static int seek_fault(FILE *f, long o, int w) { return fault == 1 ? -1 : fseek(f, o, w); }
static size_t write_fault(const void *p, size_t s, size_t n, FILE *f) {
    ++writes;
    if (fault == 2 || (fault == 5 && writes == 2)) return 0;
    size_t r = fwrite(p, s, n, f);
    written += r * s;
    return r;
}
/* The data never reaches the file: a lost flush, as a removed or full card gives. */
static int flush_fault(FILE *f) { return fault == 3 ? EOF : fflush(f); }
static int close_fault(FILE *f) { ++closes; int r = fclose(f); return fault == 4 ? EOF : r; }
#define fseek seek_fault
#define fwrite write_fault
#define fflush flush_fault
#define fclose close_fault
#include "3ds_flash.c"
#undef fseek
#undef fwrite
#undef fflush
#undef fclose
static u8 sector[4096];
static void expect(const char *path, unsigned at, int value) {
    FILE *f = fopen(path, "rb");
    assert(f && fseek(f, (long)at * 4096, SEEK_SET) == 0);
    for (int i = 0; i < 4096; ++i) assert(fgetc(f) == value);
    assert(fclose(f) == 0);
}
int main(void) {
    const char *path = "flush.sav";
    remove(path);
    assert(CtrFlash_Init(path));
    /* Each fault: the flush fails, the sector stays pending, the retry rewrites it. */
    for (fault = 1; fault <= 3; ++fault) {
        memset(sector, 0x40 + fault, sizeof(sector));
        assert(ProgramFlashSectorAndVerify(7, sector) == 0);
        assert(!CtrFlash_Flush());
        assert(sDirtySectors == 1u << 7);
        assert(!CtrFlash_Flush());          /* still failing: still not reported written */
        int f = fault;
        fault = 0;
        written = 0;
        assert(CtrFlash_Flush());
        assert(written == 4096 && sDirtySectors == 0);
        expect(path, 7, 0x40 + f);
        assert(CtrFlash_Flush() && written == 4096);   /* nothing pending: nothing written */
        fault = f;
    }
    fault = 0;
    /* Two sectors, the second write fails: both stay pending, neither is reported written. */
    memset(sector, 0x51, sizeof(sector));
    assert(ProgramFlashSectorAndVerify(3, sector) == 0);
    memset(sector, 0x52, sizeof(sector));
    assert(ProgramFlashSectorAndVerify(9, sector) == 0);
    fault = 5; writes = 0;
    assert(!CtrFlash_Flush());
    assert(sDirtySectors == ((1u << 3) | (1u << 9)));
    fault = 0; written = 0;
    assert(CtrFlash_Flush() && written == 8192 && sDirtySectors == 0);
    expect(path, 3, 0x51);
    expect(path, 9, 0x52);
    /* A sector dirtied while a flush is failing is kept with the failed ones. */
    memset(sector, 0x61, sizeof(sector));
    assert(ProgramFlashSectorAndVerify(1, sector) == 0);
    fault = 3;
    assert(!CtrFlash_Flush());
    assert(EraseFlashSector(2) == 0);
    fault = 0;
    assert(CtrFlash_Flush() && sDirtySectors == 0);
    expect(path, 1, 0x61);
    expect(path, 2, 0xff);
    /* Close: a failed flush or fclose is reported, and the file is released either way. */
    memset(sector, 0x71, sizeof(sector));
    assert(ProgramFlashSectorAndVerify(4, sector) == 0);
    fault = 3;
    assert(!CtrFlash_Close());
    assert(sFile == NULL);
    fault = 0;
    expect(path, 3, 0x51);                  /* earlier sectors untouched */
    assert(CtrFlash_Init(path));
    assert(ProgramFlashSectorAndVerify(4, sector) == 0);
    fault = 4; closes = 0;
    assert(!CtrFlash_Close() && closes == 1 && sFile == NULL);
    fault = 0;
    expect(path, 4, 0x71);
    assert(CtrFlash_Init(path));
    assert(CtrFlash_Flush() && CtrFlash_Close());
    assert(!CtrFlash_Flush());              /* closed: never reports a save */
    expect(path, 9, 0x52);
    puts("PASS flush");
    return 0;
}
''', encoding='utf-8')
            exe = work / 'flush.exe'
            compile_host([os.environ.get('HOST_CC', 'gcc'), '-std=c11', '-O2',
                          '-Wall', '-Wextra', '-Werror', '-I' + str(work),
                          '-I' + str(ROOT / '3ds_port/include'),
                          '-I' + str(ROOT / '3ds_port/src'), str(harness), '-o', str(exe)])
            run = subprocess.run([str(exe)], cwd=work, capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('PASS flush', run.stdout)


if __name__ == '__main__':
    unittest.main()
