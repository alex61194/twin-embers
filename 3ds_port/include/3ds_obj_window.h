/* The window of sprite coordinates the native field can show without aliasing.
 *
 * GBA OAM keeps X in 9 bits and Y in 8 bits, so a sprite whose true position is
 * 256 (Y) or 512 (X) away from another one has the same OAM word. The GBA's own
 * 160-line screen never meets that: its culling margin ends at 176. The native
 * field is 400x240, and the compositor (3ds_video.c, DrawObjects) reads a raw
 * coordinate at or beyond the view edge as the negative one:
 *
 *     raw Y 0..239   -> Y 0..239       raw Y 240..255 -> Y -16..-1
 *     raw X 0..399   -> X 0..399       raw X 400..511 -> X -112..-1
 *
 * A sprite whose top-left lies outside [-112,400) x [-16,240) therefore draws
 * somewhere else on screen (a Cut tree 256 lines below the view reappears at the
 * top edge). The object-event visibility code must hide those sprites, and the
 * compositor and the culling must agree on this one window, so both use this file.
 */
#ifndef CTR_OBJ_WINDOW_H
#define CTR_OBJ_WINDOW_H

#define CTR_OBJ_SCREEN_W 400
#define CTR_OBJ_SCREEN_H 240
#define CTR_OBJ_X_RANGE 512
#define CTR_OBJ_Y_RANGE 256

/* Compositor: the true coordinate a raw OAM value stands for, given the view's
 * right/bottom edge (400/240 in the native field). */
static inline int CtrObj_UnwrapX(int raw, int viewRight) { return raw >= viewRight ? raw - CTR_OBJ_X_RANGE : raw; }
static inline int CtrObj_UnwrapY(int raw, int viewBottom) { return raw >= viewBottom ? raw - CTR_OBJ_Y_RANGE : raw; }

/* True when OAM can carry this top-left position without aliasing. */
static inline int CtrObj_TopLeftRepresentable(int x, int y)
{
    return x < CTR_OBJ_SCREEN_W && x >= CTR_OBJ_SCREEN_W - CTR_OBJ_X_RANGE
        && y < CTR_OBJ_SCREEN_H && y >= CTR_OBJ_SCREEN_H - CTR_OBJ_Y_RANGE;
}

/* The object-event visibility decision (nonzero: hide). x,y is the sprite's top-left
 * and x2,y2 its bottom-right in screen coordinates, as event_object_movement.c computes
 * them; leftCut/topCut are the game's own margins, kept as they were. */
static inline int CtrObj_OffScreen(int x, int y, int x2, int y2, int leftCut, int topCut)
{
    return !CtrObj_TopLeftRepresentable(x, y) || x2 < leftCut || y2 < topCut;
}

#endif
