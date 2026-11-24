#ifndef FIRERED_3DS_SETTINGS_H
#define FIRERED_3DS_SETTINGS_H

/* The port's own settings that outlive a session, kept in a small text file on the SD card
 * (sdmc:/3ds/twinembers/settings.cfg, lines of "name=number") and not in the game save, so they
 * travel with the console and never touch the save format. Read once on first use; a missing or
 * damaged file means the defaults (turbo off, auto-run off, EMBERS not chosen, REPEL AGAIN on). Each setter writes the file only when
 * the value changes. */

#include <stddef.h>
#include <stdint.h>

#ifndef CTR_SETTINGS_PATH
#define CTR_SETTINGS_PATH "sdmc:/3ds/twinembers/settings.cfg"
#endif

/* The overworld turbo multiplier that is chosen: 0 (off) or 2..4. */
unsigned CtrSettings_Turbo(void);
void CtrSettings_SetTurbo(unsigned turbo);

/* Whether the RUN toggle (auto-run) is on. */
int CtrSettings_AutoRun(void);
void CtrSettings_SetAutoRun(int on);

/* The EMBERS level the player chose (CTR_EMBERS_*, 0..2), or -1 when none was: the console's default then. */
int CtrSettings_Embers(void);
void CtrSettings_SetEmbers(int level);

/* The lower screen's look (STYLE on the OPTIONS screen): 1 Pokeball (the default), 0 Ascuas. A file with no style= line is the default; the
 * choice of Ascuas is kept as style=0. */
int CtrSettings_Style(void);
void CtrSettings_SetStyle(int style);

/* Whether a Repel that wears off offers to use another (REPEL AGAIN on the OPTIONS screen): on by default. */
int CtrSettings_RepelAgain(void);
void CtrSettings_SetRepelAgain(int on);
/* The COMPANION choice (the OPTIONS screen): 0 none, 1 the first Pokemon walks behind the player, 2 the player is that
 * Pokemon on the map. Off until chosen. */
int CtrSettings_Companion(void);
void CtrSettings_SetCompanion(int mode);
/* The EXP SHARE choice (the OPTIONS screen): 0 the game's own, 1 the whole team shares as if each held an EXP. SHARE,
 * 2 every Pokemon that can still fight gets the full share of one that fought. Off until chosen. */
int CtrSettings_ExpShare(void);
void CtrSettings_SetExpShare(int mode);
int CtrSettings_TmReuse(void);               /* a TM is not used up when it teaches a move (REUSABLE TMS, off by default) */
void CtrSettings_SetTmReuse(int on);
int CtrSettings_Nuzlocke(void);              /* the Nuzlocke rules are on (NUZLOCKE, off by default) */
void CtrSettings_SetNuzlocke(int on);
int CtrSettings_NuzDuplicates(void);          /* Nuzlocke clause: a species already caught does not use the area's catch (off by default) */
void CtrSettings_SetNuzDuplicates(int on);
int CtrSettings_NuzShiny(void);               /* Nuzlocke clause: a shiny does not use the area's catch (off by default) */
void CtrSettings_SetNuzShiny(int on);
int CtrSettings_NuzLevelCap(void);            /* Nuzlocke level caps: no experience at or above the badge's cap (off by default) */
void CtrSettings_SetNuzLevelCap(int on);
int CtrSettings_NuzSet(void);                 /* Nuzlocke: the battle style is Set while the rules are on (off by default) */
void CtrSettings_SetNuzSet(int on);
int CtrSettings_RandomTarget(void);           /* Randomlocke: 0 wild and trainers, 1 wild only, 2 trainers only */
void CtrSettings_SetRandomTarget(int target);
int CtrSettings_RandomNoLegendary(void);      /* Randomlocke: the random species are never legendary (off by default) */
void CtrSettings_SetRandomNoLegendary(int on);
int CtrSettings_WildDensity(void);             /* WILD POKEMON: 0 OFF, 1 FEW, 2 NORMAL, 3 MANY wild Pokemon seen in the overworld (visiblewilds=; OFF by default) */
void CtrSettings_SetWildDensity(int density);
int CtrSettings_WildSilhouette(void);          /* WILD DISPLAY: 1 draws them as silhouettes (wildsil=) */
void CtrSettings_SetWildSilhouette(int on);
int CtrSettings_WildAggro(void);               /* AGGRESSIVE: 0 OFF, 1 RARE 10%, 2 LOW 20%, 3 MODERATE 30%, 4 HIGH 40% of the wilds (wildaggro=) */
void CtrSettings_SetWildAggro(int aggro);
int CtrSettings_Randomlocke(void);           /* wild and trainer Pokemon are random species (RANDOMLOCKE, off by default) */
void CtrSettings_SetRandomlocke(int on);
/* ONLINE ROOM: the private room of a six-digit code instead of the relay's public room. PrivateRoom is on only with a code; RoomCode is
 * the six digits or "" (anything but six digits is no code). */
int CtrSettings_PrivateRoom(void);
void CtrSettings_SetPrivateRoom(int on);
const char *CtrSettings_RoomCode(void);
void CtrSettings_SetRoomCode(const char *code);
/* A private room's code is CTR_ROOM_CODE_DIGITS digits (about 53 bits when chosen at random: the only secret of the room, see docs/WL-RELAY-PROTOCOL.md).
 * RoomCodeOk: exactly that many digits and at least six different ones (the plainest codes, 0000000000000000 or 1212121212121212, are refused, whoever
 * typed them). MakeRoomCode: a code from `n` random bytes (a byte of 250 or more is skipped so that no digit is likelier than another); 0, and `out`
 * untouched, when the bytes run out or what they make is refused: ask for more. An older six-digit code in settings.cfg is dropped when it is read. */
#define CTR_ROOM_CODE_DIGITS 16
int CtrSettings_RoomCodeOk(const char *code);
int CtrSettings_MakeRoomCode(char out[CTR_ROOM_CODE_DIGITS + 1], const uint8_t *random, size_t n);
/* When settings.cfg held a private room's code that is no longer one (an older build's six digits, or a damaged line), Load switches the private
 * room off in the file and RoomCodeDropped is 1 (and kept in the file as roomnote=1) until the player chooses a room again (a new code, or
 * AckRoomCodeDropped): the OPTIONS screen says that online play is in the public room and why. */
int CtrSettings_RoomCodeDropped(void);
void CtrSettings_AckRoomCodeDropped(void);
int CtrSettings_LocalPlay(void);             /* MULTIPLAYER: 1 plays with consoles nearby (the 3DS radio), 0 online through the relay (the default) */
void CtrSettings_SetLocalPlay(int on);
int CtrSettings_FreeHm(void);                /* an HM in the Bag works in the field with no Pokemon that knows it (FREE HMS, off by default) */
void CtrSettings_SetFreeHm(int on);
/* The look chosen for Red (slot 0), Leaf (1) and the rival Blue (2): the name of a folder of the pack's graphics/gen4/look/ (the
 * Builder's --alt-looks), "" for none: the picture that was there before. Only [a-z0-9_] and 15 characters are kept. */
enum { CTR_LOOK_RED, CTR_LOOK_LEAF, CTR_LOOK_BLUE, CTR_LOOK_SURF, CTR_LOOK_SLOTS };   /* SURF: the Pokemon to surf on, a folder of graphics/gen4/surfmon */
const char *CtrSettings_Look(int slot);
void CtrSettings_SetLook(int slot, const char *id);
/* The ball the battle's ball button last threw (an item number, 0 for none yet): the button starts on it. */
unsigned CtrSettings_LastBall(void);
void CtrSettings_SetLastBall(unsigned item);

#endif
