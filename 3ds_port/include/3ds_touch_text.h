#ifndef CTR_TOUCH_TEXT_H
#define CTR_TOUCH_TEXT_H

/* The port's own text for the touch MENU, in FireRed's character encoding, and the Party message the
 * lower screen shows (3ds_touch_text.c). They live in the port so that no port-authored table sits
 * in a game object, where the clean-release audit would count it as FireRed content. */

/* What the TRAINER TIPS sign, the girl who copies it and Oak's lab sign show (patch 0129). */
extern const unsigned char CtrTouchText_TrainerTips[];
extern const unsigned char CtrTouchText_TrainerTipsCopy[];
extern const unsigned char CtrTouchText_OpenMenu[];
/* The help system's "How to open the MENU" topic, in the port's own words. */
extern const unsigned char CtrTouchText_HelpOpenMenu[];

/* The Party's message as FireRed prints it, for the lower screen: mixed case, the accent of POKeMON in
 * UTF-8, its punctuation; control codes left out, line breaks and prompts as single spaces. */
void CtrTouchText_PartyMessage(const unsigned char *str, char *out, unsigned size);

#endif
