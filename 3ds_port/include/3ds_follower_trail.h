/* The companion's memory of the player's way, kept in map coordinates (3ds_port, with a host test).
 *
 * Every frame the companion compares the player's coordinates with those it saw last; when the tile the player left (previousCoords)
 * is the one seen last, the player took a step and the tile is added to the trail; otherwise the player was moved (a warp, a jump)
 * and the companion hides until the next step.
 *
 * A seamless map connection (route to route) changes the coordinate frame under all of this: the game subtracts the camera's shift from
 * every object event, the player's and the companion's among them. The seen coordinates and the trail must be moved by the same
 * amount, or the next step looks like a jump and the companion vanishes until the player's following step. */
#ifndef CTR_FOLLOWER_TRAIL_H
#define CTR_FOLLOWER_TRAIL_H

/* 1 when the player's last step began on the tile seen last (so it is a step to follow), 0 when the player was moved. */
static inline int CtrFollowerTrail_Continues(int prevX, int prevY, int seenX, int seenY)
{
    return prevX == seenX && prevY == seenY;
}

/* The game's own move of object events for a camera update: coordinates -= (dx, dy). Applied to the tiles of the trail that are
 * in use and to the coordinates seen last. */
static inline void CtrFollowerTrail_Shift(short *trailX, short *trailY, unsigned count, short *seenX, short *seenY, int dx, int dy)
{
    unsigned i;

    for (i = 0; i < count; i++)
    {
        trailX[i] = (short)(trailX[i] - dx);
        trailY[i] = (short)(trailY[i] - dy);
    }
    *seenX = (short)(*seenX - dx);
    *seenY = (short)(*seenY - dy);
}

#endif
