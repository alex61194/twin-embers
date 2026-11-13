#ifndef CTR_PARTY_GRID_H
#define CTR_PARTY_GRID_H

/* The layout of the Party's action menu (SUMMARY, SWITCH, ITEM, HEAL, REMEMBER, field moves, CANCEL) and the D-pad rule that
 * walks it, in ONE place: the lower screen draws it from CtrPartyGrid_Col / _Row (3ds_battle_ui_impl.h BPMenuRow) and the
 * game's own input (recipe 0257, Task_HandleSelectionMenuInput in party_menu.c) moves its cursor with CtrPartyGrid_Move, so
 * the two cannot disagree (3ds_port/tests/party_grid_test.c and battle_ui_render_test.c fail if they do).
 *
 * Up to four actions are one column (FireRed's own list: UP / DOWN, the game's input untouched). Beyond four there are two
 * columns filled column by column: rows = (count + 1) / 2, action i is in column i / rows, row i % rows (so the right column
 * is as long as the left one or one shorter); at most eight actions.
 *
 * Header-only and free of tables and switches: the game's objects include it (clean-release audit). */

#define CTR_PGRID_MAX 8
enum { CTR_PGRID_UP = 0, CTR_PGRID_DOWN = 1, CTR_PGRID_LEFT = 2, CTR_PGRID_RIGHT = 3 };

static inline int CtrPartyGrid_Count(int count)
{
    return count < 0 ? 0 : count > CTR_PGRID_MAX ? CTR_PGRID_MAX : count;
}

/* 1 column up to four actions, 2 beyond. */
static inline int CtrPartyGrid_Cols(int count)
{
    return CtrPartyGrid_Count(count) > 4 ? 2 : 1;
}

static inline int CtrPartyGrid_Rows(int count)
{
    int n = CtrPartyGrid_Count(count), cols = CtrPartyGrid_Cols(n);

    return (n + cols - 1) / cols;
}

static inline int CtrPartyGrid_Col(int count, int i)
{
    return CtrPartyGrid_Cols(count) == 2 ? i / CtrPartyGrid_Rows(count) : 0;
}

static inline int CtrPartyGrid_Row(int count, int i)
{
    return CtrPartyGrid_Cols(count) == 2 ? i % CtrPartyGrid_Rows(count) : i;
}

/* How many actions column `col` holds. */
static inline int CtrPartyGrid_ColLen(int count, int col)
{
    int n = CtrPartyGrid_Count(count), rows = CtrPartyGrid_Rows(n);

    if (CtrPartyGrid_Cols(n) == 1)
        return col == 0 ? n : 0;
    return col == 0 ? rows : col == 1 ? n - rows : 0;
}

/* The action the cursor goes to from `cur` on a D-pad direction, or -1 where the grid does not apply (four actions or fewer: the
 * game's own list input stays). UP and DOWN move inside the cursor's column and wrap inside it; LEFT and RIGHT go to the same
 * row of the other column, or to the last row of the right column when it is shorter. */
static inline int CtrPartyGrid_Move(int count, int cur, int dir)
{
    int n = CtrPartyGrid_Count(count), rows, col, row, len;

    if (CtrPartyGrid_Cols(n) != 2 || cur < 0 || cur >= n)
        return -1;
    rows = CtrPartyGrid_Rows(n);
    col = cur / rows;
    row = cur % rows;
    len = CtrPartyGrid_ColLen(n, col);
    if (dir == CTR_PGRID_UP)
        row = row == 0 ? len - 1 : row - 1;
    else if (dir == CTR_PGRID_DOWN)
        row = row == len - 1 ? 0 : row + 1;
    else if (dir == CTR_PGRID_LEFT || dir == CTR_PGRID_RIGHT) {
        col = 1 - col;
        len = CtrPartyGrid_ColLen(n, col);
        if (row >= len)
            row = len - 1;
    } else
        return -1;
    return col * rows + row;
}

#endif
