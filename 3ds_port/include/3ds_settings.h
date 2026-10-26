#ifndef FIRERED_3DS_SETTINGS_H
#define FIRERED_3DS_SETTINGS_H

/* The port's own settings that outlive a session, kept in a small text file on the SD card
 * (sdmc:/3ds/twinembers/settings.cfg, lines of "name=number") and not in the game save, so they
 * travel with the console and never touch the save format. Read once on first use; a missing or
 * damaged file means the defaults (turbo off, auto-run off). Each setter writes the file only when
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

#endif
