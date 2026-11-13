/* The Party's action menu grid (3ds_party_grid.h): the layout of up to eight actions in one column or two, and the D-pad rule the game's input
 * (recipe 0257) walks it by. Every count, every cell, every direction. The drawing's own rectangles are checked against the same rule in
 * battle_ui_render_test.c (BPMenuRow), so the layout and the D-pad cannot drift apart.
 *
 * usage: party_grid_test */
#include <assert.h>
#include <stdio.h>
#include "3ds_party_grid.h"

int main(void)
{
    int n, i, d;

    /* Four actions or fewer: one column, and the grid takes no part (the game's own list input stays). */
    for (n = 1; n <= 4; ++n) {
        assert(CtrPartyGrid_Cols(n) == 1 && CtrPartyGrid_Rows(n) == n);
        assert(CtrPartyGrid_ColLen(n, 0) == n && CtrPartyGrid_ColLen(n, 1) == 0);
        for (i = 0; i < n; ++i) {
            assert(CtrPartyGrid_Col(n, i) == 0 && CtrPartyGrid_Row(n, i) == i);
            for (d = 0; d < 4; ++d)
                assert(CtrPartyGrid_Move(n, i, d) == -1);
        }
    }

    /* Five to eight: two columns filled column by column, the left one the longer or as long. */
    for (n = 5; n <= 8; ++n) {
        int rows = (n + 1) / 2;

        assert(CtrPartyGrid_Cols(n) == 2 && CtrPartyGrid_Rows(n) == rows);
        assert(CtrPartyGrid_ColLen(n, 0) == rows && CtrPartyGrid_ColLen(n, 1) == n - rows && CtrPartyGrid_ColLen(n, 0) + CtrPartyGrid_ColLen(n, 1) == n);
        assert(CtrPartyGrid_ColLen(n, 1) == rows || CtrPartyGrid_ColLen(n, 1) == rows - 1);
        for (i = 0; i < n; ++i) {
            int col = CtrPartyGrid_Col(n, i), row = CtrPartyGrid_Row(n, i), len = CtrPartyGrid_ColLen(n, col);

            assert(col == i / rows && row == i % rows && col * rows + row == i && row < len);
            /* UP / DOWN stay in the column and wrap in it. */
            {
                int up = CtrPartyGrid_Move(n, i, CTR_PGRID_UP), down = CtrPartyGrid_Move(n, i, CTR_PGRID_DOWN);

                assert(up >= 0 && up < n && down >= 0 && down < n);
                assert(CtrPartyGrid_Col(n, up) == col && CtrPartyGrid_Col(n, down) == col);
                assert(CtrPartyGrid_Row(n, up) == (row + len - 1) % len && CtrPartyGrid_Row(n, down) == (row + 1) % len);
                assert(CtrPartyGrid_Move(n, down, CTR_PGRID_UP) == i && CtrPartyGrid_Move(n, up, CTR_PGRID_DOWN) == i);
            }
            /* LEFT / RIGHT go to the other column, the same row, or the last row of the right column when it is shorter. */
            for (d = CTR_PGRID_LEFT; d <= CTR_PGRID_RIGHT; ++d) {
                int to = CtrPartyGrid_Move(n, i, d), tcol = CtrPartyGrid_Col(n, to), trow = CtrPartyGrid_Row(n, to);

                assert(to >= 0 && to < n && tcol == 1 - col);
                assert(trow == (row < CtrPartyGrid_ColLen(n, tcol) ? row : CtrPartyGrid_ColLen(n, tcol) - 1));
            }
            assert(CtrPartyGrid_Move(n, i, 4) == -1 && CtrPartyGrid_Move(n, i, -1) == -1);
        }
        assert(CtrPartyGrid_Move(n, -1, CTR_PGRID_UP) == -1 && CtrPartyGrid_Move(n, n, CTR_PGRID_UP) == -1);
    }

    /* The short right column (an odd count): the last row of the left column goes right to the right column's last row. */
    assert(CtrPartyGrid_Move(5, 2, CTR_PGRID_RIGHT) == 4);       /* 0 1 2 | 3 4: SUMMARY SWITCH ITEM | HEAL CANCEL */
    assert(CtrPartyGrid_Move(5, 4, CTR_PGRID_LEFT) == 1);        /* CANCEL (row 1) back to row 1 */
    assert(CtrPartyGrid_Move(5, 3, CTR_PGRID_DOWN) == 4 && CtrPartyGrid_Move(5, 4, CTR_PGRID_DOWN) == 3);
    assert(CtrPartyGrid_Move(7, 3, CTR_PGRID_RIGHT) == 6);       /* 0 1 2 3 | 4 5 6 */
    assert(CtrPartyGrid_Move(7, 3, CTR_PGRID_DOWN) == 0 && CtrPartyGrid_Move(7, 6, CTR_PGRID_DOWN) == 4);
    assert(CtrPartyGrid_Move(6, 2, CTR_PGRID_RIGHT) == 5 && CtrPartyGrid_Move(8, 3, CTR_PGRID_RIGHT) == 7);

    /* More than eight is clamped as the drawing clamps it. */
    assert(CtrPartyGrid_Count(9) == 8 && CtrPartyGrid_Rows(12) == 4 && CtrPartyGrid_Move(12, 3, CTR_PGRID_DOWN) == 0);

    printf("PASS party grid\n");
    return 0;
}
