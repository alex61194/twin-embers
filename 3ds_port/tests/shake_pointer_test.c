/* Issue #85: the shake animation (gShakeMonOrTerrainSpriteTemplate) keeps a 32-bit address in two s16 sprite words
 * (StoreSpriteCallbackInData6 style: low half in data[6], high half in data[7]). Rebuilding it with
 *   data[6] | (data[7] << 16)
 * sign-extends data[6] when its bit 15 is set, which turns the address into 0xFFFFxxxx (a data abort on the 3DS, where
 * the addresses of the coordinate offsets have bit 15 set in their low half). Recipe 0256 rebuilds it from two u16
 * halves. This test runs both forms on the same words, for a spread of addresses, and checks that the recipe's form
 * gives every 32-bit value back and the old one fails exactly when bit 15 is set. */
#include <stdint.h>
#include <stdio.h>

typedef int16_t s16;
typedef uint16_t u16;
typedef uint32_t u32;

static int failures;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

/* The store, as battle_anim_mons.c writes it. */
static void Store(s16 data[8], u32 address)
{
    data[6] = (s16)(address & 0xFFFF);
    data[7] = (s16)(address >> 16);
}

/* The old read (what the three places did), computed in 32 bits as the target's int. */
static u32 OldRead(const s16 data[8])
{
    return (u32)((int32_t)data[6] | ((int32_t)data[7] * 65536));
}

/* The recipe's read: the same text as battle_anim_normal.c after recipe 0256. */
static u32 NewRead(const s16 data[8])
{
    return (u32)(u16)data[6] | ((u32)(u16)data[7] << 16);
}

int main(void)
{
    static const u32 samples[] = {0x00000000u, 0x00007FFFu, 0x00008000u, 0x0000FFFFu, 0x02008000u, 0x0203C9A8u, 0x03005D90u,
                                  0x08000000u, 0x0807F000u, 0x7FFF8000u, 0x80008000u, 0xFFFF0000u, 0xFFFF8000u, 0xFFFFFFFFu};
    s16 data[8] = {0};

    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        u32 a = samples[i];

        Store(data, a);
        CHECK(NewRead(data) == a, "the recipe's read of 0x%08X gives 0x%08X", a, NewRead(data));
        if (a & 0x8000u)
            CHECK(OldRead(data) != a || (a >> 16) == 0xFFFFu, "the old read of 0x%08X should lose the high half (got 0x%08X)", a, OldRead(data));
        else
            CHECK(OldRead(data) == a, "the old read of 0x%08X is fine without bit 15 (got 0x%08X)", a, OldRead(data));
    }
    /* The reported case: an address whose low half has bit 15 set reads back as 0xFFFFxxxx before the fix. */
    Store(data, 0x0203C9A8u);
    CHECK(OldRead(data) == 0xFFFFC9A8u && NewRead(data) == 0x0203C9A8u, "the Metal Claw case: old 0x%08X, new 0x%08X", OldRead(data), NewRead(data));
    /* A whole sweep of low halves under one high half. */
    for (u32 low = 0; low <= 0xFFFF; ++low) {
        Store(data, 0x02030000u | low);
        if (NewRead(data) != (0x02030000u | low)) {
            CHECK(0, "sweep: low half 0x%04X came back 0x%08X", low, NewRead(data));
            break;
        }
    }
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("shake pointer test: ok\n");
    return 0;
}
