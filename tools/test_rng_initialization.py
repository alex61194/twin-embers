"""Controlled-entropy tests of production adapters and pinned game functions.

--tree tests actual external FireRed functions and save layout; no game source,
ROM or save fixture is copied into the repository. All save bytes are synthetic.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from host_compiler import compile_host

ROOT = Path(__file__).resolve().parents[1]


def function(text, name):
    match = re.search(r'^(?:static )?\w+ ' + name + r'\([^;]*?\)\n\{', text, re.M)
    if not match:
        raise ValueError('Missing game function: ' + name)
    start = match.start()
    end = match.end()
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'


def fold(random, ticks, milliseconds, sample):
    def mix(x):
        x ^= x >> 16
        x = x * 0x7feb352d & 0xffffffff
        x ^= x >> 15
        x = x * 0x846ca68b & 0xffffffff
        return x ^ (x >> 16)
    x = mix(random & 0xffffffff)
    for value in (random >> 32, ticks & 0xffffffff, ticks >> 32,
                  milliseconds & 0xffffffff, milliseconds >> 32, sample):
        x = mix(x ^ value)
    return x & 0xffff


class RngInitialization(unittest.TestCase):
    def compile_run(self, temp, sources, includes=(), flags=()):
        cc = os.environ.get('CC') or shutil.which('gcc') or 'gcc'
        exe = temp / ('rng-test.exe' if os.name == 'nt' else 'rng-test')
        compile_host([cc, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
                      *flags, '-I' + str(temp), '-I' + str(ROOT/'3ds_port/include'),
                      *['-iquote' + str(p) for p in includes],
                      *[str(p) for p in sources], '-o', str(exe)])
        result = subprocess.run([str(exe)], cwd=temp, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        print(result.stdout.strip())

    def stub(self, temp):
        (temp/'3ds.h').write_text('''#include <stdint.h>
#include <stddef.h>
typedef int32_t Result;
#define R_SUCCEEDED(r) ((r) >= 0)
#define R_FAILED(r) ((r) < 0)
Result psInit(void);
void psExit(void);
Result PS_GenerateRandomBytes(void *, size_t);
uint64_t svcGetSystemTick(void);
uint64_t osGetTime(void);
''')

    def test_platform_success_failure_and_vectors(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            self.stub(temp)
            vectors = [(0, 0, 0, 0), (0xffffffffffffffff, 0x123456789abcdef0,
                        0x876543219abcdef0, 0xffffffff)]
            vectors += [(1 << bit, 1 << ((bit+19) % 64), 1 << ((bit+41) % 64), bit+1)
                        for bit in range(64)]
            checks = '\n'.join('assert(CtrRng_Fold(UINT64_C(%d), UINT64_C(%d), '
                               'UINT64_C(%d), UINT32_C(%d)) == %d);' % (*v, fold(*v))
                               for v in vectors)
            harness = temp/'test.c'
            harness.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "3ds.h"
#include "3ds_rng.h"
static int init_result, generate_result, opens, closes, generates;
static uint64_t random_word, ticks, milliseconds;
Result psInit(void) { ++opens; return init_result; }
void psExit(void) { ++closes; }
Result PS_GenerateRandomBytes(void *out, size_t len) {
    ++generates; assert(len == sizeof(random_word));
    memcpy(out, &random_word, len); return generate_result;
}
uint64_t svcGetSystemTick(void) { return ticks; }
uint64_t osGetTime(void) { return milliseconds; }
int main(void) {
''' + checks + r'''
    random_word = UINT64_C(0x0123456789abcdef);
    ticks = UINT64_C(0xfedcba9876543210); milliseconds = 123456789;
    assert(CtrRng_GetSeed() == CtrRng_Fold(random_word,ticks,milliseconds,1));
    assert(opens == 1 && closes == 1 && generates == 1);
    generate_result = -1;
    assert(CtrRng_GetSeed() == CtrRng_Fold(0,ticks,milliseconds,2));
    assert(opens == 2 && closes == 2 && generates == 2);
    init_result = -1;
    assert(CtrRng_GetSeed() == CtrRng_Fold(0,ticks,milliseconds,3));
    assert(opens == 3 && closes == 2 && generates == 2);
    unsigned seen[65536] = {0}, unique = 0;
    for (unsigned i = 0; i < 4096; ++i) {
        ticks = UINT64_C(0x0123456700000000) + i * 98765;
        milliseconds = UINT64_C(0xabc00000000) + i * 17;
        uint16_t seed = CtrRng_GetSeed();
        assert(seed == CtrRng_Fold(0,ticks,milliseconds,i+4));
        if (!seen[seed]++) ++unique;
    }
    assert(unique > 3900); /* fixed fixture, not a probabilistic acceptance gate */
    printf("PASS 66 vectors, service failures, 4096 fallback samples (%u distinct)\n", unique);
}
''')
            self.compile_run(temp, [harness, ROOT/'3ds_port/src/3ds_rng.c',
                                    ROOT/'3ds_port/src/3ds_rng_platform.c'])

    def test_external_game_boundaries_and_save_roundtrip(self):
        tree = os.environ.get('RNG_TEST_TREE')
        if not tree:
            self.skipTest('pass --tree for pinned FireRed integration')
        tree = Path(tree).resolve()
        main = (tree/'src/main.c').read_text(encoding='utf-8')
        new = (tree/'src/new_game.c').read_text(encoding='utf-8')
        title = (tree/'src/title_screen.c').read_text(encoding='utf-8')
        naming = (tree/'src/naming_screen.c').read_text(encoding='utf-8')
        self.assertIn('CtrRng_GetSeed()', function(main, 'SeedRngAndSetTrainerId'))
        self.assertIn('SeedRngAndSetTrainerId();', function(title, 'SetTitleScreenScene_Cry'))
        self.assertIn('NAMING_SCREEN_PLAYER', function(naming, 'MainState_Exit'))
        self.assertIn('SeedRngAndSetTrainerId();', function(naming, 'MainState_Exit'))
        # Audit all external calls: only title and player naming may sample.
        callers = []
        for path in (tree/'src').rglob('*.c'):
            text = path.read_text(encoding='utf-8')
            for _ in re.finditer(r'\bSeedRngAndSetTrainerId\(\);', text):
                callers.append(path.name)
        self.assertEqual(sorted(callers), ['naming_screen.c', 'title_screen.c'])
        self.assertEqual((tree/'src/random.c').read_bytes(), subprocess.check_output(
            ['git', '-C', str(tree), 'show', 'HEAD:src/random.c']))
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            # Actual game functions supplied by the ignored, pinned local tree.
            game = temp/'game.c'
            game.write_text('#include "global.h"\n#include "random.h"\n#include "3ds_rng.h"\n'
                            'u16 gTrainerId;\nstatic u16 timer;\n'
                            '#undef REG_TM1CNT_L\n#undef REG_TM1CNT_H\n'
                            '#define REG_TM1CNT_L timer\n#define REG_TM1CNT_H timer\n'
                            'struct SaveBlock2 *gSaveBlock2Ptr;\n'
                            + ''.join(function(main, n) for n in ('StartTimer1',
                                'SeedRngAndSetTrainerId', 'GetGeneratedTrainerIdLower'))
                            + ''.join(function(new, n) for n in ('SetTrainerId', 'InitPlayerTrainerId'))
                            + 'void TestInitTrainer(void) { InitPlayerTrainerId(); }\n')
            harness = temp/'test.c'
            harness.write_text(r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "random.h"
#include "3ds_rng.h"
#include "3ds_flash.h"
#include "gba/flash_internal.h"
extern u16 gTrainerId;
extern struct SaveBlock2 *gSaveBlock2Ptr;
void SeedRngAndSetTrainerId(void);
void TestInitTrainer(void);
static unsigned samples;
static u16 controlled;
uint16_t CtrRng_GetSeed(void) { ++samples; return controlled; }
int main(void) {
    _Static_assert(sizeof(struct SaveBlock2) == 0xf24, "FireRed save ABI");
    _Static_assert(offsetof(struct SaveBlock2, playerTrainerId) == 0xa, "ID offset");
    struct SaveBlock2 saved;
    gSaveBlock2Ptr = &saved;
    unsigned seen[65536] = {0};
    /* Independent resets, same frame schedule, default and typed-name paths. */
    for (unsigned i = 0; i < 256; ++i) {
        samples = 0; gRngValue = 0; gTrainerId = 0;
        memset(&saved, 0xa5, sizeof(saved));
        controlled = CtrRng_Fold(i*UINT64_C(0x100010001), i*98765, i*17, 1);
        SeedRngAndSetTrainerId(); /* title exit */
        assert(samples == 1 && gRngValue == controlled && gTrainerId == controlled);
        if (i & 1) { /* player keyboard exit replaces the title seed */
            controlled = CtrRng_Fold(i*UINT64_C(0x987654321), i*23456, i*31, 2);
            SeedRngAndSetTrainerId();
            assert(samples == 2 && gRngValue == controlled && gTrainerId == controlled);
        }
        u16 seed = controlled;
        /* Original LCG and upper (secret) ID construction, no new RNG. */
        uint32_t expected = ((uint32_t)seed * 1103515245u + 24691u);
        TestInitTrainer();
        assert(gRngValue == expected);
        assert(saved.playerTrainerId[0] == (u8)seed);
        assert(saved.playerTrainerId[1] == (u8)(seed >> 8));
        assert(saved.playerTrainerId[2] == (u8)(expected >> 16));
        assert(saved.playerTrainerId[3] == (u8)(expected >> 24));
        ++seen[seed];
        unsigned before = samples;
        for (unsigned frame = 0; frame < 600; ++frame) {
            expected = expected * 1103515245u + 24691u;
            assert(Random() == (u16)(expected >> 16));
        }
        assert(samples == before); /* gameplay never asks for entropy */
    }
    unsigned distinct = 0;
    for (unsigned i = 0; i < 65536; ++i) distinct += seen[i] != 0;
    assert(distinct > 250);
    /* Existing IDs, including buggy 00000, survive title entropy and disk IO. */
    const u32 ids[] = {0, 0x12340000, 0x89abcdef, 0xffffffff};
    for (unsigned i = 0; i < sizeof(ids)/sizeof(ids[0]); ++i) {
        u8 sector[4096], readback[4096];
        memset(sector, 0xa5, sizeof(sector));
        memcpy(sector, &saved, sizeof(saved));
        memcpy(sector + offsetof(struct SaveBlock2, playerTrainerId), &ids[i], 4);
        assert(CtrFlash_Init("synthetic.sav"));
        assert(ProgramFlashSectorAndVerify(0, sector) == 0);
        assert(CtrFlash_Flush() && CtrFlash_Close());
        assert(CtrFlash_Init("synthetic.sav"));
        ReadFlash(0, 0, readback, sizeof(readback));
        assert(memcmp(sector, readback, sizeof(sector)) == 0);
        memcpy(&saved, readback, sizeof(saved));
        controlled = 42; SeedRngAndSetTrainerId(); /* Continue title sampling */
        assert(memcmp(saved.playerTrainerId, &ids[i], 4) == 0);
        assert(CtrFlash_Close());
        FILE *f = fopen("synthetic.sav", "rb");
        assert(f && fseek(f, 0, SEEK_END) == 0 && ftell(f) == 131072);
        fclose(f); remove("synthetic.sav");
    }
    /* Zero remains a valid original seed, rather than biased remapping. */
    controlled = 0; SeedRngAndSetTrainerId(); TestInitTrainer();
    assert(saved.playerTrainerId[0] == 0 && saved.playerTrainerId[1] == 0);
    puts("PASS 256 independent new games, default/typed names, LCG, save ABI and four ID roundtrips");
}
''')
            self.compile_run(temp, [harness, game, tree/'src/random.c',
                                    ROOT/'3ds_port/src/3ds_rng.c', ROOT/'3ds_port/src/3ds_flash.c'],
                             [tree/'include'], ['-DPLATFORM_3DS', '-DMODERN=1',
                             '-DFIRERED', '-DENGLISH', '-DREVISION=0', '-Wno-pointer-to-int-cast'])

    def test_original_zero_timer_overwrites_prior_seed(self):
        tree = os.environ.get('RNG_TEST_TREE')
        if not tree:
            self.skipTest('pass --tree for pinned FireRed reproduction')
        tree = Path(tree).resolve()
        original = subprocess.check_output(['git', '-C', str(tree), 'show',
                                             'HEAD:src/main.c']).decode('utf-8')
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            # Original functions are read at runtime, never redistributed.
            harness = temp/'reproduce.c'
            harness.write_text('#include <assert.h>\n#include <stdio.h>\n'
                '#include "global.h"\n#include "random.h"\n'
                '#undef REG_TM1CNT_L\n#undef REG_TM1CNT_H\n'
                'static u16 control;\nu16 gTrainerId;\n'
                '#define REG_TM1CNT_L 0\n#define REG_TM1CNT_H control\n'
                + function(original, 'SeedRngAndSetTrainerId') + r'''
int main(void) {
    for (unsigned i = 1; i <= 64; ++i) {
        SeedRng((u16)i); /* even a varying startup seed cannot fix the bug */
        SeedRngAndSetTrainerId(); /* title exit */
        assert(gTrainerId == 0 && gRngValue == 0);
        for (unsigned frame = 0; frame < 120; ++frame) Random();
        assert(gRngValue != 0);
        SeedRngAndSetTrainerId(); /* player naming exit */
        assert(gTrainerId == 0 && gRngValue == 0 && control == 0);
    }
    puts("PASS reproduced both original zero-timer overwrites in 64 runs");
}
''')
            self.compile_run(temp, [harness, tree/'src/random.c'], [tree/'include'],
                             ['-DPLATFORM_3DS', '-DMODERN=1', '-DFIRERED', '-DENGLISH'])

    def test_actual_firered_save_slots_and_continue(self):
        tree = os.environ.get('RNG_TEST_TREE')
        if not tree:
            self.skipTest('pass --tree for actual sector/checksum/load integration')
        tree = Path(tree).resolve()
        save = (tree/'src/save.c').read_text(encoding='utf-8')
        names = ('SetDamagedSectorBits', 'WriteSaveSectorOrSlot', 'HandleWriteSector',
                 'TryWriteSector', 'TryLoadSaveSlot', 'CopySaveSlotData',
                 'GetSaveValidStatus', 'ReadFlashSector', 'CalculateChecksum')
        bodies = [function(save, name) for name in names]
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            harness = temp/'slots.c'
            harness.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "random.h"
#include "3ds_rng.h"
#include "3ds_flash.h"
#include "3ds_log.h"
#include "save.h"
#include "gba/flash_internal.h"
''' + r'''
/* Native host pointers enlarge SaveBlock1's ObjectEventTemplate. Use opaque
 * synthetic serialized blocks with exact ARM/GBA sizes; SaveBlock2 and the
 * sector/checksum/slot functions themselves use actual external definitions. */
static u8 block1[0x3d68], expected1[0x3d68];
static struct SaveBlock2 block2, expected2;
static u8 storage[0x83d0], expectedStorage[0x83d0];
static struct SaveSectorLocation locations[NUM_SECTORS_PER_SLOT];
u16 gLastWrittenSector, gLastKnownGoodSector;
u32 gLastSaveCounter, gSaveCounter, gDamagedSaveSectors;
struct SaveSector gSaveDataBuffer, *gSaveDataBufferPtr;
void CtrLog_Write(CtrLogCategory category, const char *format, ...) {
    (void)category; (void)format;
}
u16 gTrainerId;
static u16 timer, controlled;
#undef REG_TM1CNT_L
#undef REG_TM1CNT_H
#define REG_TM1CNT_L timer
#define REG_TM1CNT_H timer
uint16_t CtrRng_GetSeed(void) { return controlled; }
''' + '\n'.join(body[:body.index('{')].strip() + ';' for body in bodies)
                + '\n' + ''.join(bodies)
                + function((tree/'src/main.c').read_text(encoding='utf-8'), 'SeedRngAndSetTrainerId') + r'''
static void BindLocations(void) {
    locations[0] = (struct SaveSectorLocation){(u8 *)&block2, sizeof(block2)};
    for (unsigned i = 0; i < 4; ++i) {
        unsigned offset = i * SECTOR_DATA_SIZE;
        unsigned size = sizeof(block1) - offset;
        if (size > SECTOR_DATA_SIZE) size = SECTOR_DATA_SIZE;
        locations[1+i] = (struct SaveSectorLocation){block1 + offset, size};
    }
    for (unsigned i = 0; i < 9; ++i) {
        unsigned offset = i * SECTOR_DATA_SIZE;
        unsigned size = sizeof(storage) - offset;
        if (size > SECTOR_DATA_SIZE) size = SECTOR_DATA_SIZE;
        locations[5+i] = (struct SaveSectorLocation){storage + offset, size};
    }
}
int main(void) {
    const u32 ids[] = {0, 0x12340000, 0x89abcdef, 0xffffffff};
    for (unsigned id = 0; id < sizeof(ids)/sizeof(ids[0]); ++id) {
        remove("slots.sav");
        /* MinGW open() defaults to text descriptors, unlike the production
         * 3DS/POSIX target. Open an existing binary fixture so 0x0a sector IDs
         * do not receive host-only CRLF translation during creation. */
        FILE *blank = fopen("slots.sav", "wb");
        assert(blank);
        for (unsigned byte = 0; byte < 131072; ++byte) assert(fputc(0xff, blank) != EOF);
        assert(fclose(blank) == 0);
        gSaveCounter = 0; gLastWrittenSector = 0; gDamagedSaveSectors = 0;
        assert(CtrFlash_Init("slots.sav"));
        for (unsigned generation = 0; generation < 3; ++generation) {
            memset(&block1, 0x40 + generation, sizeof(block1));
            memset(&block2, 0x50 + generation, sizeof(block2));
            memset(&storage, 0x60 + generation, sizeof(storage));
            memcpy(block2.playerTrainerId, &ids[id], 4);
            memcpy(expected1, block1, sizeof(block1)); expected2 = block2;
            memcpy(expectedStorage, storage, sizeof(storage)); BindLocations();
            assert(WriteSaveSectorOrSlot(FULL_SAVE_SLOT, locations) == SAVE_STATUS_OK);
            assert(CtrFlash_Flush() && CtrFlash_Close());
            memset(&block1, 0, sizeof(block1)); memset(&block2, 0, sizeof(block2));
            memset(&storage, 0, sizeof(storage));
            assert(CtrFlash_Init("slots.sav"));
            controlled = (u16)(100 + generation); SeedRngAndSetTrainerId();
            /* Actual slot loader used by Continue selects the newest slot,
             * validates all checksums, then restores all three blocks. */
            assert(TryLoadSaveSlot(FULL_SAVE_SLOT, locations) == SAVE_STATUS_OK);
            assert(gSaveCounter == generation + 1);
            assert(memcmp(&block1, &expected1, sizeof(block1)) == 0);
            assert(memcmp(&block2, &expected2, sizeof(block2)) == 0);
            assert(memcmp(&storage, &expectedStorage, sizeof(storage)) == 0);
            assert(memcmp(block2.playerTrainerId, &ids[id], 4) == 0);
        }
        assert(CtrFlash_Close()); remove("slots.sav");
    }
    puts("PASS actual FireRed save/load: 12 rotating/alternating slot roundtrips, checksums and unchanged IDs/blocks");
}
''')
            self.compile_run(temp, [harness, tree/'src/random.c', ROOT/'3ds_port/src/3ds_flash.c'],
                             [tree/'include'], ['-DPLATFORM_3DS', '-DMODERN=1', '-DFIRERED',
                             '-DENGLISH', '-DREVISION=0', '-ffunction-sections', '-fdata-sections',
                             '-Wl,--gc-sections', '-Wno-unused-parameter', '-Wno-sign-compare',
                             '-Wno-pointer-to-int-cast', '-fno-strict-aliasing'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tree', type=Path)
    args, rest = parser.parse_known_args()
    if args.tree:
        os.environ['RNG_TEST_TREE'] = str(args.tree)
    unittest.main(argv=[sys.argv[0]] + rest)
