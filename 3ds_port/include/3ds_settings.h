#ifndef FIRERED_3DS_SETTINGS_H
#define FIRERED_3DS_SETTINGS_H

/* The port's own settings that outlive a session, kept in a small text file on the SD card
 * (sdmc:/3ds/twinembers/settings.cfg, lines of "name=number") and not in the game save, so they
 * travel with the console and never touch the save format. Read once on first use; a missing or
 * damaged file means the defaults (turbo off, auto-run off, EMBERS not chosen, REPEL AGAIN on). Each setter writes the file only when
 * the value changes. */

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
/* The ball the battle's ball button last threw (an item number, 0 for none yet): the button starts on it. */
unsigned CtrSettings_LastBall(void);
void CtrSettings_SetLastBall(unsigned item);

#endif
