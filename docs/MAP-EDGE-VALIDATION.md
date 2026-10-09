# Map edges on the 400x240 field

## Root cause

Outside a map the game draws the layout's 2x2 border block, repeated
(`GetBorderBlockAt()` in `fieldmap.c`): the backup map keeps a 7-metatile margin
filled with `MAPGRID_UNDEFINED`, `MapGridGetMetatileIdAt()` turns an undefined
cell into the border block, and `DrawMetatileAt()` in `field_camera.c` draws it.
The native viewport (`GetViewportConnectionMetatile()`) only reaches past that
margin to read a *connected* map; where no connection covers a position it also
falls back to the border block. On the original 240x160 screen the border was a
sliver; on the 400x240 view a mountain, plateau or island can end in a straight
line against whatever the border is (usually sea).

An earlier attempt repeated the last row or column of every outdoor map. That
turns any edge that is not a plain body into stripes: on Two Island, Cape Brink
the west edge column is a shore tile (different from the metatile beside it) for
15 rows, so the blind stretch drew that shore tile over and over to the left, the
field of vertical lines in the reported screenshot (map `TwoIsland_CapeBrink`,
identified from the layout: 24x40, a 3x3 waterfall at (11..13, 23..25), the
5-wide pond, the purple-roofed house and the west shore column `0x274`).

## Behaviour

Only the picture changes. `3ds_port/include/3ds_map_edge.h` decides, from the
map data alone, whether a position outside the map shows the nearest edge
metatile (patch 0111). Collision, behaviour, scripts, objects, the save grid,
camera coordinates, Surf and Fly never see it.

| Boundary | Behaviour |
| --- | --- |
| Connected side | Unchanged: the connected map's tiles (backup margin, then the viewport's connection lookup); where the connection does not reach, the border block, as before. |
| Closed side, edge is a wide plain body | The nearest edge metatile continues outward. Plain = the same metatile at the edge and one position inside, in a run of at least 8 equal metatiles along the edge. |
| Closed side, anything else | Border block, as before: shores, cliff faces and corners, stairs, 2-wide channels, tree lines, fences, roofs, roads. |
| Closed side, plain body that is land next to a land border | Border block (a designed surround: a forest, a rock wall). |
| Closed side, sea next to a sea border | Border block (only a seam of slightly different blues). |
| Closed side, blocked land over a sea border, or sea over a land border | Extended. These are the cuts the reporter saw. |
| Corner | Extended only if both of its sides qualify and neither is connected. |
| Indoor, cave and other non-outdoor maps | Border block. |

Nine maps change: Four Island, One Island, Kindle Road, Treasure Beach,
Route 25, Water Path, Bond Bridge, Two Island and Cape Brink. Everywhere else the
fallback draws exactly what vanilla drew.

Known limits: where an extended body ends, the edge it leaves is straight (a
plateau that continues now ends at a horizontal cliff instead of a vertical one
on Four Island); a body that touches the edge for only part of a side extends only
that part; a transition leaves the cells the game does not redraw as they were
(stale in vanilla too), so the first screens after crossing a connection can show
the previous map's extension on its far side until it scrolls out.

## Tests

- `python tools/test_map_edge.py`: the rule on synthetic maps (uniform body,
  connected and closed sides, corners, shore column, narrow channel and the
  exact run threshold, alternating lines, tiny maps, land/sea decision).
- `python tools/test_map_edge.py --tree build/<workspace>` (a tree made by
  `tools/bootstrap.py`, patches applied): the production `fieldmap.c` and
  `field_camera.c` are compiled on the host with the hardware stubbed and run
  over every outdoor map of the pinned upstream data. For each map the whole
  view is drawn at every camera position (every third along a side longer than
  40), with the fallback and as vanilla
  (map type forced to indoor); the two pictures may differ only in the
  positions the rule extends, and there must show the extended metatile's own
  tiles. Scrolling in all four directions through `CameraUpdate()` is compared
  with a fresh `DrawWholeMapView()` at each step (144 runs); walks in four
  directions cross 87 real connections, and real tiles must stay identical. Cape Brink's 15 shore rows
  must keep the border at every camera position that shows them, and the set
  of nine changed maps is pinned. Mutating the rule to a blind stretch fails
  these checks. Nothing derived from the game is stored in the repository.

Not tested: a real 3DS. Azahar smoke only.
