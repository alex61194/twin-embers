/* The options halfword of SaveBlock2 (0x14) keeps its binary layout: every named option sits in the
 * bits it always had, the two spare bits (13 and 14) are reserved and survive untouched,
 * and the struct keeps its size. Compiled against a bootstrapped tree's include/global.h. */
#include "global.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

static u16 Halfword(const struct SaveBlock2 *s)
{
    u16 h;
    memcpy(&h, (const u8 *)s + 0x14, sizeof(h));
    return h;
}

/* The bits a field occupies: set it to all ones in a zeroed block. */
#define MASK_OF(field, ones) ({ struct SaveBlock2 z; memset(&z, 0, sizeof(z)); z.field = (ones); Halfword(&z); })

int main(void)
{
    struct SaveBlock2 s;
    u16 reserved = (u16)(3u << 13);

    CHECK(sizeof(struct SaveBlock2) == 0xF24, "SaveBlock2 is %u bytes, expected 0xF24",
          (unsigned)sizeof(struct SaveBlock2));
    CHECK(offsetof(struct SaveBlock2, optionsButtonMode) == 0x13, "optionsButtonMode moved");
    CHECK(MASK_OF(optionsTextSpeed, 7) == 0x0007, "text speed bits");
    CHECK(MASK_OF(optionsWindowFrameType, 31) == 0x00F8, "frame bits");
    CHECK(MASK_OF(optionsSound, 1) == 0x0100, "sound bit");
    CHECK(MASK_OF(optionsBattleStyle, 1) == 0x0200, "battle style bit");
    CHECK(MASK_OF(optionsBattleSceneOff, 1) == 0x0400, "battle scene bit");
    CHECK(MASK_OF(regionMapZoom, 1) == 0x0800, "map zoom bit");
    CHECK(MASK_OF(optionsFpsOff, 1) == 0x1000, "FPS bit");
    CHECK(MASK_OF(optionsReserved3ds, 3) == reserved, "reserved bits");

    /* A save holding any value (0-3) in the reserved bits: every option reads as saved, and
     * writing options leaves the reserved bits alone (nothing resets or rewrites them). */
    for (unsigned old = 0; old < 4; old++) {
        u16 h = (u16)(0x0002 | (17 << 3) | 0x0100 | 0x0400 | 0x1000 | (old << 13)), after;

        memset(&s, 0, sizeof(s));
        memcpy((u8 *)&s + 0x14, &h, sizeof(h));
        CHECK(s.optionsTextSpeed == 2 && s.optionsWindowFrameType == 17 && s.optionsSound == 1
              && s.optionsBattleStyle == 0 && s.optionsBattleSceneOff == 1 && s.regionMapZoom == 0
              && s.optionsFpsOff == 1, "options of a save with reserved value %u", old);
        s.optionsTextSpeed = 1;
        s.optionsWindowFrameType = 3;
        s.optionsSound = 0;
        s.optionsBattleStyle = 1;
        s.optionsBattleSceneOff = 0;
        s.regionMapZoom = 1;
        s.optionsFpsOff = 0;
        after = Halfword(&s);
        CHECK((after & reserved) == (h & reserved), "reserved value %u changed by writing options", old);
        CHECK((after & ~reserved & 0xFFFF) == (0x0001 | (3 << 3) | 0x0200 | 0x0800), "written options %04x", after);
    }

    if (failures) { printf("%d failures\n", failures); return 1; }
    puts("PASS save option bits");
    return 0;
}
