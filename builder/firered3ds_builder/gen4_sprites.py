"""Optional HeartGold battle sprites for the pack: downloaded, checked, converted.

The sprites are not in the player's FireRed ROM and never ship with Twin
Embers. When the player asks for them, the Builder downloads the PNG files that
gen4_sources.py lists from pret/pokeheartgold at the pinned commit, keeps each
one only if its size and SHA-256 match the list, and converts it for the pack.
The pack stays valid without them: the engine ABI does not cover these
entries, a species without its file keeps FireRed's pictures, and a build with
no network writes the same pack as before.

Pack entries, one per species with a complete set, and the icons:

    graphics/gen4/battle/NNN.bin    NNN = national dex number, 001-386
    graphics/gen4/battle/201_FF.bin Unown's letters, FF = 01-27 (B to Z, !, ?), as FireRed numbers them
    graphics/gen4/battle/351_FF.bin Castform's weather forms, FF = 01-03 (sun, rain, ice), FireRed's order
    graphics/gen4/spinda.bin        where HeartGold draws Spinda's spots (from pret's pokepic.c)
    graphics/gen4/icons.bin         every menu icon, only when all of them could be had
                                    (their palettes replace FireRed's for every icon)
    graphics/gen4/trainer/NNN.bin   NNN = FireRed's trainer picture (TRAINER_PIC_*), the
                                    HeartGold class TRAINER_CLASSES pairs with it; a picture
                                    left out (Lorelei, Agatha, Oak, Leaf...) keeps FireRed's
    graphics/gen4/back/NNN.bin      NNN = 000 Red's, 001 Leaf's back (TRAINER_BACK_PIC_*): fan
                                    art in HeartGold's style (BACK_FILES), not pret's

Layout (little endian):

    0    b"G4B1"
    4    u8   flags: bit 0 = a female pair follows the male one
    5    u8   0
    6    u16  0
    8    u16 x 16  normal palette, BGR555 as the GBA's (colour 0 is transparent)
    40   u16 x 16  shiny palette
    72   pictures, 80 x 80 pixels each, 4 bits a pixel, rows top to bottom,
         the left pixel in the low nibble: front, back, then the female
         front and back when bit 0 is set (a female-only species has its
         one pair first, bit 0 clear)

icons.bin (little endian):

    0    b"G4I1"
    4    u16  icons, 414: key 0 the egg, 1-386 the national dex, 387-413 Unown's letters B to ?
    6    u16  palettes used (at most 6, FireRed's icon palette slots)
    8    u16 x 16 x 6  the palettes, BGR555, unused ones zero
    200  u8 x icons  each icon's palette, then zeros to a multiple of 4
    then 1024 bytes an icon: two 32x32 frames, each 16 8x8 4bpp tiles row by row,
         as FireRed's icons

trainer/NNN.bin (little endian): b"G4T1", u32 0, u16 x 16 the palette (BGR555, colour 0
transparent), then the picture's first frame, 80 x 80 pixels, 4 bits a pixel as above. HeartGold
keeps the trainer pictures in one NARC (files/a/0/5/8), five files a class: the fifth is the picture (an NCGR
of two 80x80 frames side by side, its pixels scrambled as Diamond and Pearl's are), its palette
(the second, an NCLR); the Builder does not need the other three.

back/NNN.bin: b"G4K1", u8 frames (5, FireRed's throw), three zero bytes, u16 x 16 the palette, then
each frame, 80 x 80 pixels, 4 bits a pixel. HeartGold has no Red or Leaf back: these come from the
fan-made HGSS-style backs that the gen1recomp HGSS Visual Overhaul mod (LucianoNeo/gen1recomp-mods)
keeps as 400x80 RGBA strips, Leaf's by setogabes, Red's from a public Red pack; the Builder turns
each into at most 15 colours (BGR555) and transparency.

bg/NNN.bin (NNN = FireRed's BATTLE_TERRAIN_*): b"G4G1", u16 tiles, u16 0, u16 x 16 the palette (BGR555, colour 0
unused), then the tiles (4 bits a pixel, FireRed's own layout, 32 bytes each) and the 64x32 tile map (two 32x32
screen blocks, entries as FireRed's BG3 reads them, palette 2), ready for the VRAM the battle scene is drawn from.
A scene is 240x112 pixels; around it the map mirrors it with flipped tiles.

npc/NNN.bin (NNN = FireRed's OBJ_EVENT_GFX_*): b"G4N1", u8 frames (9 for a person; the player's as many as FireRed's pic
table: 20 walking and running, 12 surfing or fishing, 6 for the VS Seeker on the bike), u8 32 (their size), u16 0,
u16 x 16 the palette (BGR555, colour 0 unused), then the 32x32 frames in FireRed's order (down, up, left, then the
walking ones), each as 16 tiles of 4 bits a pixel, row by row of tiles: what the field draws a person from.
The player's back and front picture (Ash's and Green's, from Emerald Lens) are back/000-001.bin and
trainer/135-136.bin: their 64x64 frames enlarged as pixel art, 1.5 times for the back and 1.25 for the front.
player/front_boy.4bpp and .gbapal, intro_boy.8bpp and .gbapal, icon_boy.4bpp and .gbapal (and _girl): the front
picture, the Oak intro's picture and the Region Map icon in FireRed's own format and size (4bpp tiles and 16 colours;
96 8bpp tiles on BG colours 64-95 and those 32 colours), which the port puts in the place of Red's and Leaf's.

spinda.bin: b"G4P1", then the four spots, each a u8 count and that many
(x, y) u8 pairs: the pixels of the spot on the front picture before the
personality moves it (HeartGold's RawChardata_PlaceSpindaSpots).
"""
from __future__ import annotations

import hashlib
import os
import queue
import re
import struct
import threading
import urllib.request
import zlib
from pathlib import Path
from typing import Callable

from . import gen4_followers, gen4_hd, gen4_sources, nsbtx

SIZE = 80
PICTURE = SIZE * SIZE // 2
MAGIC = b"G4B1"
PREFIX = "graphics/gen4/battle/"
ICONS = "graphics/gen4/icons.bin"
SPINDA = "graphics/gen4/spinda.bin"
TRAINER_PREFIX = "graphics/gen4/trainer/"
TRAINER_MAGIC = b"G4T1"
BACK_PREFIX = "graphics/gen4/back/"
BACK_MAGIC = b"G4K2"
BACK_FRAMES = 5
# The back is enlarged 1.25 as pixel art (Scale3x, then every 2.4th pixel) to 100x100, on a 104x104 picture of
# 13x13 tiles (CTR_GEN4_BACK_SIZE): shown 1:1, it stands 1.25 times as large as the 80x80 art.
BACK_SHOWN, BACK_SIZE = 100, 104
BACK_PICTURE = BACK_SIZE * BACK_SIZE // 2
# The fan-made backs, pinned like pret's files: (FireRed's TRAINER_BACK_PIC_*, "back", path, size, SHA-256).
BACK_REPO = "LucianoNeo/gen1recomp-mods"
BACK_COMMIT = "768f60d9d0f097b9cc65e78099574c296a3f63d2"
BACK_FILES = (
    (0, "back", "hgss_sprites/assets/battle/back-animated/redplayer.png", 10199,
     "26448af5283a349c17016e920aa7c8a6b75e93791b1cad4147870db221ca3cc5"),
    (1, "back", "hgss_sprites/assets/battle/back-animated/leafplayer.png", 5524,
     "8c222cfdae83898370c60806bf1c9297207023da59a46ec1f0fce2e9acd8945d"),
)
# The battle backgrounds: Facets' SCENERY set (LibertyTwins' Battle Backgrounds Patch FR, converted to 240x112
# PNGs of 15 colours), pinned like the rest: (scene, "bg", path, size, SHA-256). BG_TERRAIN gives each of FireRed's
# BATTLE_TERRAIN_* (grass, long grass, sand, underwater, water, pond, mountain, cave, building, plain, link, gym, leader,
# indoor 2, indoor 1, Lorelei, Bruno, Agatha, Lance, champion) its scene, as Facets maps them.
BG_REPO = "ElvieBlooms/facets"
BG_COMMIT = "9b4940d54f8bad152746481b56d841698a548e64"
BG_PREFIX = "graphics/gen4/bg/"
BG_MAGIC = b"G4G1"
BG_W, BG_H = 240, 112
BG_MAP_COLUMNS, BG_MAP_ROWS = 64, 32
BG_TILES_MAX = 512
BG_FILES = (
    (0, "bg", "assets/battlebg/scenery/grass.png", 7203,
     "26a618485a29fa74b389439d3f730b3fc0e236f2399c99e907e07e6c5df7bf74"),
    (1, "bg", "assets/battlebg/scenery/forest.png", 8960,
     "62cd98d05159d1c15e9fb45d11457d6fa776307759eb2d1c5301e0b73e5e1e62"),
    (2, "bg", "assets/battlebg/scenery/beach.png", 6480,
     "5c800d2ae752b2e04cd4105df6ccdccf1943d083451bde17eb594b712d4ad25f"),
    (3, "bg", "assets/battlebg/scenery/underwater.png", 5485,
     "c7f3e9e468feaea6b63cde9c0664eca40ef4bbb73e68aa32f5bb4ab24cb44c3c"),
    (4, "bg", "assets/battlebg/scenery/sea.png", 3666,
     "8c10cea856f281511e3f7bded7f26624c7702f9eb4bcb988d1f64f380cae3611"),
    (5, "bg", "assets/battlebg/scenery/lake.png", 5866,
     "11592e3ed8ed148231078ba0756496e09b212b89d25e0a770e4f654f7ed0d770"),
    (6, "bg", "assets/battlebg/scenery/mountain.png", 9330,
     "c66d9d846c18593d0d4053ae6be0b8362e4b781cfc2858204276860ba0cf1591"),
    (7, "bg", "assets/battlebg/scenery/cave.png", 11799,
     "ab0dd0dfaaa2f9ecdf15cd2c3c71131b3e3d46b913692aca35faa8b2e84df44a"),
    (8, "bg", "assets/battlebg/scenery/lab.png", 3966,
     "95d12a070159cd58313309c189eab2d659eb942c218a6e03c545a3ea6010f415"),
    (9, "bg", "assets/battlebg/scenery/route.png", 9873,
     "9b16990868cdc3aa0bd60e71510999e252dfee08755d76875d571525e5829809"),
    (10, "bg", "assets/battlebg/scenery/indoors.png", 1586,
     "d21831b0da6d3f7c992de0474245c2b9d8dda184b6e9e34926785efeff393b24"),
    (11, "bg", "assets/battlebg/scenery/neutral.png", 2211,
     "3e99166d28c748adaabded65d486d3cf0ca4a86c8a1950a50030aef3c162a865"),
    (12, "bg", "assets/battlebg/scenery/gym.png", 4170,
     "edc34b6191b8da6f46c8488a40473ef6fc35a967e01807d54aab41e5253030b4"),
    (13, "bg", "assets/battlebg/scenery/space.png", 2205,
     "13ee861c57029aa50cbe6ff2f7d532c78e149188973db1ee04d54697681db72d"),
)
# The field's people: Facets' NPC sets for FireRed and LeafGreen at the same pinned commit, one file for each of the 74
# people it draws (FireRed's OBJ_EVENT_GFX_* in the first place): the community set where it has one, DiegoWT's,
# Taiga's, then PurpleZaffre's. Each is a 32x288 RGBA sheet of nine 32x32 frames, ordered as FireRed's own.
NPC_PREFIX = "graphics/gen4/npc/"
NPC_MAGIC = b"G4N1"
NPC_FRAMES = 9
NPC_FILES = (
    (16, "npc", "assets/npcs/community/gen3/frlg/LITTLE_BOY.png", 1727,
     "774ba68a4057f5fa7041b9bb8f0b7df99d41511cf390580b1d8900c2ec10e2b2"),
    (17, "npc", "assets/npcs/community/gen3/frlg/LITTLE_GIRL.png", 2041,
     "f069dfb2aa18a69abe95fe6816a7d3170095286fc2fa2ef5f30a512da0eaa2f2"),
    (18, "npc", "assets/npcs/diego/gen3/frlg/YOUNGSTER.png", 2147,
     "6b7116a5a551953556ff6e7acbde04cccc905830f25b6dcf736a344c86505d4e"),
    (19, "npc", "assets/npcs/diego/gen3/frlg/BOY.png", 1643,
     "716ed945ecbe8ae886cbfb47d642a37fe3bec4bf5f0c27313c01e4ae4defa2bb"),
    (20, "npc", "assets/npcs/taiga/gen3/frlg/BUG_CATCHER.png", 2283,
     "94c1d2ba16317c06d5f993f20ef284d2fd5adc8604794622b1434b3e5f6a7e70"),
    (21, "npc", "assets/npcs/community/gen3/frlg/SITTING_BOY.png", 1054,
     "42a45f6fc4fcb425f5d25e0036ba99603f9158e1a3b82fb21da8ce431b9ab954"),
    (22, "npc", "assets/npcs/diego/gen3/frlg/LASS.png", 2636,
     "09d0189611a651e9b6d34373fd8b5458008ba39bfa7b5870084e7167bc46acdc"),
    (23, "npc", "assets/npcs/community/gen3/frlg/WOMAN_1.png", 1899,
     "87e0e261a5a6d43fb7d70907964f08c5ecd147651c2b817fe7ded1910576c1d0"),
    (24, "npc", "assets/npcs/diego/gen3/frlg/CRUSH_GIRL.png", 2207,
     "7636a664691600e5c80058fb891ac8feafadbf4a41a908136f37a3a3711961e2"),
    (25, "npc", "assets/npcs/community/gen3/frlg/MAN.png", 2759,
     "75d723215f787932406a7ffdadfd41cc857ccfc5b0f818eaa3a677a7a9386c23"),
    (26, "npc", "assets/npcs/diego/gen3/frlg/ROCKER.png", 3107,
     "70978119d206bb38eb0f5508f293b079ba6875d2ff02e108915ddcba92fe4f0c"),
    (27, "npc", "assets/npcs/community/gen3/frlg/FAT_MAN.png", 2611,
     "2c662d91ce38fff2bfef1e180bb813774e9a6f8fde6c4521db81fdd5c3bc422b"),
    (28, "npc", "assets/npcs/community/gen3/frlg/WOMAN_2.png", 2637,
     "516364c4d90f7f94836ba171008ba350dfbe3d18718b1f1500d2172700f4be77"),
    (29, "npc", "assets/npcs/diego/gen3/frlg/BEAUTY.png", 2418,
     "ecd85827d671874879df235920e102f258674f26c7a8bdec7d637083a0f9c6a8"),
    (30, "npc", "assets/npcs/community/gen3/frlg/BALDING_MAN.png", 2277,
     "1119ba41db634865c67db1740a54be9870be38a80320b37d69c51f6e30aeca19"),
    (31, "npc", "assets/npcs/community/gen3/frlg/WOMAN_3.png", 2444,
     "cbf14682dadb3be47955d8d4f488984b2cb5a78772bbd98b888a86d5986cd423"),
    (32, "npc", "assets/npcs/community/gen3/frlg/OLD_MAN_1.png", 1821,
     "1d4184d5f4b85c442b866423747f59599558a843e35cc8fbdd73d4aafaa0cac3"),
    (33, "npc", "assets/npcs/diego/gen3/frlg/OLD_MAN_2.png", 2045,
     "0edca25760ee955358c53981b8bbde9344f1c33cf99d162a45a6760c2df359cd"),
    (34, "npc", "assets/npcs/community/gen3/frlg/OLD_MAN_LYING_DOWN.png", 708,
     "aeafeefece683881216932bdfeb4c10e7972c294601bc210ab4d42df69abc89f"),
    (35, "npc", "assets/npcs/community/gen3/frlg/OLD_WOMAN.png", 1793,
     "3335ebbe0b22f9da3893c68d2f8a50021ca0ada721536da85b0513859b5a47be"),
    (36, "npc", "assets/npcs/community/gen3/frlg/TUBER_M_WATER.png", 1047,
     "31532db27232ee5ef3cdf3ef7c7f3249241b42cb207d58b8f04bf7ad72b682cf"),
    (37, "npc", "assets/npcs/community/gen3/frlg/TUBER_F.png", 1288,
     "05de9d86fbd84bb304e3bfcad0f992323fdc224fc5f9b672f84cbc25e4772f2f"),
    (38, "npc", "assets/npcs/community/gen3/frlg/TUBER_M_LAND.png", 1139,
     "287a3469661c4a34c7c6cf1bc1a46bbbadc96fdefe0811847fcaa6890979b8d7"),
    (39, "npc", "assets/npcs/taiga/gen3/frlg/CAMPER.png", 2293,
     "e6f38fff59cd33d6fabc2de0ea176d05d7e8395aa790bd755116a628652885e2"),
    (40, "npc", "assets/npcs/community/gen3/frlg/PICNICKER.png", 2600,
     "288d05f1dcd306679c42fd5e013fc0af8ba2c58a46007e0ec5689906ddd5e366"),
    (41, "npc", "assets/npcs/diego/gen3/frlg/COOLTRAINER_M.png", 2034,
     "9e23887e621740d9ebe15bc84af51e422521393ad97643239ea5dca65bf5ea97"),
    (42, "npc", "assets/npcs/diego/gen3/frlg/COOLTRAINER_F.png", 2924,
     "f4a558111df0bf78c8a98b01fddbd5013ebec0bacb5aeec163f53d697cb819b7"),
    (43, "npc", "assets/npcs/community/gen3/frlg/SWIMMER_M_WATER.png", 1822,
     "99725cd8e58fb7a48d7102c6fafe5c1b5b7cd5c2e9627c82080334bf7e15054f"),
    (44, "npc", "assets/npcs/community/gen3/frlg/SWIMMER_F_WATER.png", 1951,
     "24ec173bb6a81c5fffd85fd9cfb87988175669e263a1e845684c4d7912e7d072"),
    (45, "npc", "assets/npcs/community/gen3/frlg/SWIMMER_M_LAND.png", 2331,
     "a9e844afaca8321d27f8ef6aa460cfb1863108fa25826329e5c23dd3ff6afa15"),
    (46, "npc", "assets/npcs/community/gen3/frlg/SWIMMER_F_LAND.png", 2790,
     "66363fb204f6f6da435e88639f6d09833e68d4238d7b222f4beef9163bdd57fe"),
    (47, "npc", "assets/npcs/diego/gen3/frlg/WORKER_M.png", 2060,
     "fbf269493b349223774380a1e68f68e7b9b8d9ac04fc633c49baacf22ca7d5ae"),
    (48, "npc", "assets/npcs/community/gen3/frlg/WORKER_F.png", 2626,
     "659143c68149edb236096ca64e5bd51ced5c4a0bd953ac3b56c53a65cb5df78b"),
    (49, "npc", "assets/npcs/community/gen3/frlg/ROCKET_M.png", 2085,
     "344f7ee3e4c55fe726cf16375ed4501c7a8489b0cc1b9403c499abb83b870263"),
    (50, "npc", "assets/npcs/community/gen3/frlg/ROCKET_F.png", 2127,
     "4a6cc596a799a0bc371dd3ef5b765f8c9f25404ecb1c3c3a2f5f7303b84c4feb"),
    (51, "npc", "assets/npcs/community/gen3/frlg/GBA_KID.png", 2389,
     "72f7333bfeded9ea8c41c6a64938d752a9cd6fc2c53919172b0dcdff09bc069b"),
    (52, "npc", "assets/npcs/taiga/gen3/frlg/POKE_MANIAC.png", 2438,
     "c471f1f38a19ccdba03ba69c0c9a478d6c0309203719e3a9978600a0253a073a"),
    (53, "npc", "assets/npcs/diego/gen3/frlg/BIKER.png", 2060,
     "e20d880fdb9f9f97a6d9e758c20810cac597d543fbfbcc4ec6e9c8de47260a6c"),
    (54, "npc", "assets/npcs/diego/gen3/frlg/BLACK_BELT.png", 2614,
     "8c2364f1a87b3c655f13a45ff6626fb8e7292b455046805c9fd34d4f9c279410"),
    (55, "npc", "assets/npcs/diego/gen3/frlg/SCIENTIST.png", 2283,
     "123eb88028821953a7b3306ebd53bacf62b03032a908b65354d558d423ac0710"),
    (56, "npc", "assets/npcs/diego/gen3/frlg/HIKER.png", 2352,
     "c44108dc43a4c95115e6cec2fa8ad3f608361acb89c6e643c8e8493afe87eba7"),
    (57, "npc", "assets/npcs/diego/gen3/frlg/FISHER.png", 2496,
     "ede4622165f704ec0484316fb7d06f51f590581e2483cb9a4d95dfcd0f983572"),
    (58, "npc", "assets/npcs/community/gen3/frlg/CHANNELER.png", 3078,
     "868dccc9b88dcd3c48c6032263182e7c82256922d97e12a54015fdf65d758a41"),
    (59, "npc", "assets/npcs/diego/gen3/frlg/CHEF.png", 2334,
     "1074cf14d8a9fa013893948cceec162737686d8d00cdf7dba30fbee40e655f6b"),
    (60, "npc", "assets/npcs/diego/gen3/frlg/POLICEMAN.png", 2289,
     "5ae9a0029b6a59ced791c364e4ab3109629b29dda1dbb5d6f93ebe7d4460441f"),
    (61, "npc", "assets/npcs/diego/gen3/frlg/GENTLEMAN.png", 2303,
     "178d0882c2c1661334fda5ee74a99c3762758e847ae1dfe9a9b3026eeb1c7dcb"),
    (62, "npc", "assets/npcs/taiga/gen3/frlg/SAILOR.png", 3196,
     "4ec7008e65284632a9ee3553c574ceaa3216aa14150d4d6b7f57da313aaf8edc"),
    (63, "npc", "assets/npcs/diego/gen3/frlg/CAPTAIN.png", 2328,
     "f977fa58034996d5671eccfc9cd199c6f4d9b39791db6ef39068419421a941c7"),
    (64, "npc", "assets/npcs/diego/gen3/frlg/NURSE.png", 2472,
     "3c5133e123abb20355953ec479e494256dfa500cc7bbc126d9e332d5deb3d1cf"),
    (65, "npc", "assets/npcs/community/gen3/frlg/CABLE_CLUB_RECEPTIONIST.png", 1869,
     "5e8e98d12b8f1baa65a92d6aa127a0c9872ab35f2435bde9d5810b820f3be672"),
    (66, "npc", "assets/npcs/community/gen3/frlg/UNION_ROOM_RECEPTIONIST.png", 1952,
     "9bcf25073656b64d54ef4d0d3241e9e3a76f9106376805594d26cfd5127d0d76"),
    (68, "npc", "assets/npcs/diego/gen3/frlg/CLERK.png", 1909,
     "c5ab73f3f25be514ab2ed14d8df0d43c8e8d30962eb4afa29d69d6aa32e14fbf"),
    (69, "npc", "assets/npcs/taiga/gen3/frlg/MG_DELIVERYMAN.png", 2779,
     "018bbd0edec470590be71f05335f108c0be8f2b119343b5cead175564986629d"),
    (70, "npc", "assets/npcs/community/gen3/frlg/TRAINER_TOWER_DUDE.png", 2367,
     "d9cc5103d7386a2a16225a467ea0e6d76118bdcc8ac1e355919e9d44f0c59aff"),
    (71, "npc", "assets/npcs/community/gen3/frlg/PROF_OAK.png", 2693,
     "86a41c7c152d7a483ceae8c000a455ad37ec02e50db9e948003bf9c0cbb55d6e"),
    (73, "npc", "assets/npcs/community/gen3/frlg/BILL.png", 2684,
     "58b0c3f53b8f560f5a87c2bdfa03510250f39a762cd419982b3c1b33bbeb472a"),
    (74, "npc", "assets/npcs/community/gen3/frlg/LANCE.png", 2848,
     "8d5336796ce32ea67b1bde39af13cb2194d392b6b088d9576472c8b88772dc60"),
    (75, "npc", "assets/npcs/zaffre/gen3/frlg/AGATHA.png", 2257,
     "37913540a3dca1f1df9342af552332823d2372883fbb62017a3b31b2c45a1d57"),
    (76, "npc", "assets/npcs/community/gen3/frlg/DAISY.png", 2901,
     "7eab83d35c3f7ca7a87efd55c8e4791be16cf2bb5a8d271cd8915ed0c601d72f"),
    (77, "npc", "assets/npcs/zaffre/gen3/frlg/LORELEI.png", 2488,
     "8155386a388c66fa88cdf19bbf57c19835d24a1745b325a8b6ea8d7e812ae41f"),
    (78, "npc", "assets/npcs/zaffre/gen3/frlg/MR_FUJI.png", 1736,
     "b9c5eee86e5d33a5032e1db4405aa7292e4b2fb8871fb47acd3451ca3f7d29cc"),
    (79, "npc", "assets/npcs/community/gen3/frlg/BRUNO.png", 3788,
     "9b0fbe0a195bfa829e91ba23d98bd0bd4818b45ec132b6000492693d23e79a99"),
    (80, "npc", "assets/npcs/community/gen3/frlg/BROCK.png", 2780,
     "53fe765b0f0cb4bab252a2760bb1875b1f2c90240cbae4927634177732cbeb81"),
    (81, "npc", "assets/npcs/community/gen3/frlg/MISTY.png", 2746,
     "1f93c32e474a362dbe195c86706d7fcce27b0bb00500fcc771d7114eab9cca80"),
    (82, "npc", "assets/npcs/community/gen3/frlg/LT_SURGE.png", 2720,
     "9e8e785ec2f25810fcb8549f4b37b46e5da4c7252d60836ab9ffb5b8b3186812"),
    (83, "npc", "assets/npcs/community/gen3/frlg/ERIKA.png", 2473,
     "12f1ef5c9d4b63a6cdbf023e0f0873bd7045e3d3735b815a906eebefaaa84497"),
    (84, "npc", "assets/npcs/community/gen3/frlg/KOGA.png", 2817,
     "f660fe8608e0521f21903d355d433e8217848ce16fbddf02b8df1b702c546bb4"),
    (85, "npc", "assets/npcs/community/gen3/frlg/SABRINA.png", 2272,
     "d6e9b2a121cc7bad6b8aece2d5108d89368ba63c190d38bb169cdab444817a98"),
    (86, "npc", "assets/npcs/community/gen3/frlg/BLAINE.png", 2514,
     "4f51ba6732ef9c51a7fea28cc5131daf66ac3c531780b60d50b8cad86826caf9"),
    (87, "npc", "assets/npcs/zaffre/gen3/frlg/GIOVANNI.png", 2106,
     "21d8217e56392877b928b6ea40b7db12678b94cfa37da51993dddb6b6866b745"),
    (88, "npc", "assets/npcs/community/gen3/frlg/MOM.png", 2300,
     "ab7b16fac5954439a29182fa6be876e16997b41fe696aa13bff21e428187f46b"),
    (89, "npc", "assets/npcs/community/gen3/frlg/CELIO.png", 2578,
     "7100a2d7922cc4e789549020d7e9e90c079041f721c72e808c966bda796d9b31"),
    (90, "npc", "assets/npcs/community/gen3/frlg/TEACHY_TV_HOST.png", 2008,
     "9b47f544d74dc1f20c06bd37c783be47c587750d2cc2224e6702d3ca5eb29106"),
    (91, "npc", "assets/npcs/community/gen3/frlg/GYM_GUY.png", 2570,
     "be50e7bb159d3b15975de2fc5885bb93e2847fbf1587aaf1c949e000c7651a92"),
)
# The player, as Ash (for Red) and Green (for Leaf): Emerald Lens' HGSS-style looks, by PKMNTrainerRick (Ash) and
# compiled by ChromeValiant (Green), with Green's Gen 3 look ("Big Puff") for her back, front, intro picture and map
# icon and Ash's Gen 3 one (kalarie's) for his map icon, which their HGSS looks do not draw. Pinned like the rest: ("boy" or "girl", "player", path, size, SHA-256). Each sheet's frames are in
# FireRed's pic table order; PLAYER_SHEETS gives each the OBJ_EVENT_GFX_* it draws (Leaf's are Red's plus 7).
PLAYER_REPO = "ElvieBlooms/emerald_lens"
PLAYER_COMMIT = "36a945a5ed4d1efef24449aab71cc04f067ba4fe"
PLAYER_FILES = (
    ("boy", "player", "assets/sprites/ash_hgss/gen3/frlg/walk.png", 5415,
     "2dc1442d2d082fbdd6450f147bed0222b2a770f5080a9ef9785cd917e804bc6f"),
    ("boy", "player", "assets/sprites/ash_hgss/gen3/frlg/bike.png", 2650,
     "b0682b395a7cf91f2a9a4ecbebef3b39e2de7988894f5976036101ce8fbb0dea"),
    ("boy", "player", "assets/sprites/ash_hgss/gen3/frlg/surf.png", 1970,
     "4e1a4cb313db08400831181ebc69178c01e411a1731fde1d500495f967039dc2"),
    ("boy", "player", "assets/sprites/ash_hgss/gen3/frlg/fishing.png", 4426,
     "cea7677de4382786f0dbfeea029a423c208fae6f16077b13072339a69a2d90a1"),
    ("boy", "player", "assets/sprites/ash_hgss/gen3/frlg/fieldMove.png", 1273,
     "7b545868d459ae62f60c9abc38c310f2dbe7981d69ccb278d6036c1a357b1aac"),
    ("boy", "player", "assets/sprites/ash_hgss/gen3/frlg/vsSeekerBike.png", 862,
     "52f2568567cc072b8405df9235b0cf68105199a9c5fabe14c04f8de3b328baf8"),
    ("boy", "player", "assets/sprites/ash_hgss/gen3/frlg/back.png", 6181,
     "1ce1e780a245fd9aa3cb3d6a797890a2143bab5d781ad3046c952cff7874a7d8"),
    ("boy", "player", "assets/sprites/ash_hgss/gen3/frlg/front.png", 1975,
     "975beeb274d2d07df74af63fc6b6348909d6c5d4a4ef18fbfa65309b9a9086dc"),
    ("boy", "player", "assets/sprites/ash_hgss/gen3/frlg/intro.png", 2159,
     "b7684428b5ca5abcfee0edb3d515356ba33b1088e1b1cc15df3da92df6c3c208"),
    ("boy", "player", "assets/sprites/ash/gen3/frlg/mapIcon.png", 343,
     "e4d9d24bff2f9b0ad9ede0ef5e013fbfeb6dc0500fb76c60ca77c8ef2e0cdf87"),
    ("boy", "player", "assets/sprites/ash_hgss/gen3/frlg/flyBird.png", 1656,
     "4cc11b57eb3d6ed294ac7224270e886b281f8662c12923c4093ef9f79651588f"),
    ("girl", "player", "assets/sprites/green_hgss/gen3/frlg/walk.png", 3784,
     "5151530a52e432d7d465671272d1a3ff3468ebf615b4f437cf2a566da7f49a20"),
    ("girl", "player", "assets/sprites/green_hgss/gen3/frlg/bike.png", 1986,
     "39539a15291cf8e557fad2e2217bad20bcc026d07db743aeebbc0c78d5b213b9"),
    ("girl", "player", "assets/sprites/green_hgss/gen3/frlg/surf.png", 1439,
     "d0f4656dcdf48ad48a6656df43d9c6bf88bff3ba286e1b7ff579ae109e048a54"),
    ("girl", "player", "assets/sprites/green_hgss/gen3/frlg/fishing.png", 3329,
     "328ea9352d000a22b72f11798aa11108ce71510a97a66cf46185a1113b361a19"),
    ("girl", "player", "assets/sprites/green_hgss/gen3/frlg/fieldMove.png", 867,
     "dbc6c87a2b817d4097e9de52ea4186408656865eaf0e1fcfbc3882485ce2b6dc"),
    ("girl", "player", "assets/sprites/green_hgss/gen3/frlg/vsSeekerBike.png", 722,
     "d6129f754332c8c688f97cacd4a594328d62add941f94d0f309b6e9ab59343d8"),
    ("girl", "player", "assets/sprites/green/gen3/frlg/back.png", 5182,
     "7515b4f3ba55b979f21fa80c638490c38dd72b92b7921317cadbaa2e956a6f37"),
    ("girl", "player", "assets/sprites/green/gen3/frlg/front.png", 1840,
     "07e5418d6212cc10d4442c7baaa540ef065c208c2431c25d600c64508661b803"),
    ("girl", "player", "assets/sprites/green/gen3/frlg/intro.png", 2583,
     "b75d8666d96bcf121943c9342a9a336e83b287351cc465a1d710e29503de7e5e"),
    ("girl", "player", "assets/sprites/green/gen3/frlg/mapIcon.png", 293,
     "0ee911c1b14ef22b14f656645253b69da8e65338747de577079471c0f8f2b02a"),
    ("girl", "player", "assets/sprites/green_hgss/gen3/frlg/flyBird.png", 5206,
     "decccc300af17b6027ff77f03cf1d8413d0661ad596848f5eced7f58d3ad6d28"),
)
# Sheet name: (the OBJ_EVENT_GFX_* of Red it draws, its frames). The field move sheet is also the VS Seeker's (both
# take FireRed's "item" pictures).
PLAYER_SHEETS = {"walk": ((0,), 20), "bike": ((1,), 9), "surf": ((2,), 12), "fieldMove": ((3, 5), 9),
                 "fishing": ((4,), 12), "vsSeekerBike": ((6,), 6)}
PLAYER_GFX = {"boy": 0, "girl": 7}
# The Fly bird carrying the player (FireRed's bird images 1-2 Red's, 3-4 Leaf's): two 64x64 frames, as the
# graphics ids the port asks for it (CTR_NPC_BIRD + gender).
PLAYER_BIRD = {"boy": 250, "girl": 251}
PLAYER_BACK = {"boy": 0, "girl": 1}           # TRAINER_BACK_PIC_RED, _LEAF
PLAYER_FRONT = {"boy": 135, "girl": 136}      # TRAINER_PIC_RED, _LEAF
# The pictures FireRed copies out whole, in its own format, for the port to put in place of Red's and Leaf's
# (CtrPlayer_Swap): the front picture (trainer card, Hall of Fame), the Oak intro's and the Region Map icon.
PLAYER_PREFIX = "graphics/gen4/player/"
GROUP_POKEMON, GROUP_NPC, GROUP_BG, GROUP_PLAYER, GROUP_FOLLOWER = "pokemon", "npc", "bg", "player", "follower"
GROUP_HD = "hd"     # the HD battle scenes (Bentley734's): graphics/gen4/hdn/ and hd/, each as it is in the pack
GROUPS = (GROUP_POKEMON, GROUP_NPC, GROUP_BG, GROUP_PLAYER, GROUP_FOLLOWER, GROUP_HD)
# The Pokémon that walks behind the player (HeartGold's): one pack entry a national dex number (graphics/gen4/follower/NNN.bin),
# a second one named NNN_f.bin where HeartGold has a female picture of its own. Each is a person's entry (convert_npc's
# nine 32x32 frames), the game flipping the west frames for the east. Rows: (dex, or 1000 + dex for the female one,
# "follower", the file's path in the pinned commit, size, SHA-256).
FOLLOWER_PREFIX = "graphics/gen4/follower/"
FOLLOWER_FEMALE = 1000
FOLLOWER_FILES = tuple(
    (dex + (FOLLOWER_FEMALE if body == "female" else 0), "follower",
     gen4_followers.MMODEL_PATH.format(gen4_followers.BASE + number), size, digest)
    for dex, body, number, size, digest in gen4_followers.FILES)
BG_TERRAIN = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 12, 10, 10, 12, 12, 12, 12, 13)
ICON_MAGIC = b"G4I1"
ICON_PALETTES = 6
URL = "https://raw.githubusercontent.com/{repo}/{commit}/{path}"
TIMEOUT = 30


class Gen4Error(ValueError):
    pass


# --- PNG (the 4- and 8-bit indexed images pret keeps) -------------------------------------------

def read_png(data: bytes) -> tuple[int, int, list[tuple[int, int, int]], list[list]]:
    """(width, height, palette as RGB, rows of palette indices) of an indexed PNG; of an RGBA or RGB one, no palette
    and rows of (r, g, b, a)."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise Gen4Error("not a PNG file")
    pos, idat, palette, header = 8, bytearray(), [], None
    while pos + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body) - 2, 3)]
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    if header is None:
        raise Gen4Error("PNG without a header")
    width, height, depth, colour, _, _, interlace = header
    rgba = colour == 6 and depth == 8
    rgb = colour == 2 and depth == 8
    if not (colour == 3 and depth in (4, 8) or rgba or rgb) or interlace:
        raise Gen4Error("only non-interlaced 4- or 8-bit indexed or 8-bit RGB or RGBA PNGs are read")
    step = 4 if rgba else 3 if rgb else 1
    stride = width * step if (rgba or rgb) else (width * depth + 7) // 8
    raw = zlib.decompress(bytes(idat))
    if len(raw) != (stride + 1) * height:
        raise Gen4Error("PNG data has the wrong size")
    rows, previous = [], bytearray(stride)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            left = line[x - step] if x >= step else 0
            up = previous[x]
            corner = previous[x - step] if x >= step else 0
            if kind == 1:
                line[x] = (line[x] + left) & 0xFF
            elif kind == 2:
                line[x] = (line[x] + up) & 0xFF
            elif kind == 3:
                line[x] = (line[x] + (left + up) // 2) & 0xFF
            elif kind == 4:
                guess = left + up - corner
                near = min((abs(guess - left), 0, left), (abs(guess - up), 1, up), (abs(guess - corner), 2, corner))
                line[x] = (line[x] + near[2]) & 0xFF
            elif kind:
                raise Gen4Error("unknown PNG filter")
        previous = line
        if rgba:
            rows.append([tuple(line[x * 4:x * 4 + 4]) for x in range(width)])
        elif rgb:
            rows.append([tuple(line[x * 3:x * 3 + 3]) + (255,) for x in range(width)])
        elif depth == 8:
            rows.append(list(line[:width]))
        else:
            rows.append([(line[x // 2] >> (0 if x & 1 else 4)) & 0xF for x in range(width)])
    return width, height, palette, rows


def bgr555(palette: list[tuple[int, int, int]]) -> bytes:
    colours = (palette + [(0, 0, 0)] * 16)[:16]
    return b"".join(struct.pack("<H", (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10) for r, g, b in colours)


def first_frame(rows: list[list[int]]) -> bytes:
    """The sprite's first 80x80 frame (pret keeps two side by side) as 4-bit pixels."""
    if len(rows) != SIZE or len(rows[0]) < SIZE:
        raise Gen4Error("a battle sprite is 80 pixels high and at least 80 wide")
    out = bytearray()
    for row in rows:
        if max(row[:SIZE]) > 15:
            raise Gen4Error("a battle sprite uses 16 colours")
        out += bytes(row[x] | row[x + 1] << 4 for x in range(0, SIZE, 2))
    return bytes(out)


def icon_tiles(rows: list[list[int]]) -> bytes:
    """A 32x64 icon (two frames one above the other) as FireRed's 1024 bytes of tiles."""
    if len(rows) != 64 or len(rows[0]) != 32 or max(max(row) for row in rows) > 15:
        raise Gen4Error("an icon is 32x64 in 16 colours")
    out = bytearray()
    for frame in range(2):
        for ty in range(4):
            for tx in range(4):
                for r in range(8):
                    row = rows[frame * 32 + ty * 8 + r]
                    out += bytes(row[tx * 8 + x] | row[tx * 8 + x + 1] << 4 for x in range(0, 8, 2))
    return bytes(out)


def convert_icons(icons: list[bytes]) -> bytes:
    """icons.bin from the icons' PNG files in key order. Each PNG carries its own palette, one of a few shared ones."""
    palettes: list[bytes] = []
    indices, tiles = bytearray(), bytearray()
    for data in icons:
        _, _, palette, rows = read_png(data)
        colours = bgr555(palette)
        if colours not in palettes:
            palettes.append(colours)
        indices.append(palettes.index(colours))
        tiles += icon_tiles(rows)
    if len(palettes) > ICON_PALETTES:
        raise Gen4Error("more icon palettes than FireRed has slots")
    indices += bytes(-len(indices) % 4)
    return (ICON_MAGIC + struct.pack("<HH", len(icons), len(palettes))
            + b"".join(palettes) + bytes(32 * (ICON_PALETTES - len(palettes))) + bytes(indices) + bytes(tiles))


def convert_spinda(source: bytes) -> bytes:
    """spinda.bin from pret's pokepic.c: its four sSpindaSpotNCoords tables, up to their 0xFF end."""
    text = source.decode("utf-8")
    out = bytearray(b"G4P1")
    for spot in range(1, 5):
        match = re.search(r"sSpindaSpot%dCoords\[\]\[2\] = \{(.*?)\};" % spot, text, re.S)
        if not match:
            raise Gen4Error("no Spinda spot table %d" % spot)
        pairs = [(int(x, 0), int(y, 0)) for x, y in re.findall(r"\{\s*(\w+)\s*,\s*(\w+)\s*\}", match.group(1))]
        if pairs[-1:] != [(0xFF, 0xFF)] or len(pairs) > 256 or any(x > 0xFF or y > 0xFF for x, y in pairs):
            raise Gen4Error("Spinda spot table %d is not as expected" % spot)
        out.append(len(pairs) - 1)
        for x, y in pairs[:-1]:
            out += bytes((x, y))
    return bytes(out)


# FireRed's trainer pictures (include/constants/trainers.h, TRAINER_PIC_*) and the HeartGold trainer class
# (pret/pokeheartgold include/constants/trainer_class.h, TRAINERCLASS_*) whose front picture stands for each one.
# FireRed's player pictures (backs, the Oak intro) are not trainer front pictures and are never replaced.
TRAINER_CLASSES = {
    7: 44,     # RS_TUBER_M -> TUBER_M
    13: 32,    # RICH_BOY -> RICH_BOY
    18: 64,    # KINDLER -> FIREBREATHER
    29: 60,    # SCHOOL_KID_M -> SCHOOL_KID_M
    30: 61,    # SCHOOL_KID_F -> SCHOOL_KID_F
    32: 20,    # POKEFAN_M -> POKEFAN_M
    33: 21,    # POKEFAN_F -> POKEFAN
    39: 12,    # CYCLING_TRIATHLETE_M -> CYCLIST_M
    40: 13,    # CYCLING_TRIATHLETE_F -> CYCLIST_F
    41: 19,    # RUNNING_TRIATHLETE_M -> JOGGER
    45: 29,    # DRAGON_TAMER -> DRAGON_TAMER
    47: 28,    # NINJA_BOY -> NINJA_BOY
    48: 10,    # BATTLE_GIRL -> BATTLE_GIRL
    49: 80,    # PARASOL_LADY -> PARASOL_LADY
    54: 37,    # COLLECTOR -> COLLECTOR
    82: 2,     # YOUNGSTER -> YOUNGSTER
    83: 6,     # BUG_CATCHER -> BUG_CATCHER
    84: 3,     # LASS -> LASS
    85: 46,    # SAILOR -> SAILOR
    86: 4,     # CAMPER -> CAMPER
    87: 5,     # PICNICKER -> PICNICKER
    88: 68,    # POKEMANIAC -> POKE_MANIAC
    89: 78,    # SUPER_NERD -> SUPER_NERD
    90: 9,     # HIKER -> HIKER
    91: 65,    # BIKER -> BIKER
    92: 63,    # BURGLAR -> BURGLAR
    93: 59,    # ENGINEER -> WORKER
    94: 11,    # FISHERMAN -> FISHERMAN
    95: 42,    # SWIMMER_M -> SWIMMER_M
    96: 57,    # CUE_BALL -> ROUGHNECK
    98: 36,    # BEAUTY -> BEAUTY
    99: 43,    # SWIMMER_F -> SWIMMER_F
    100: 49,   # PSYCHIC_M -> PSYCHIC_M
    101: 52,   # ROCKER -> GUITARIST
    102: 31,   # JUGGLER -> JUGGLER
    104: 69,   # BIRD_KEEPER -> BIRD_KEEPER_GS
    105: 14,   # BLACK_BELT -> BLACK_BELT
    106: 110,  # RIVAL_EARLY -> LEADER_BLUE
    107: 113,  # SCIENTIST -> SCIENTIST_GS
    108: 124,  # LEADER_GIOVANNI -> ROCKET_BOSS
    109: 55,   # ROCKET_GRUNT_M -> TEAM_ROCKET
    110: 53,   # COOLTRAINER_M -> ACE_TRAINER_M_GS
    111: 54,   # COOLTRAINER_F -> ACE_TRAINER_F_GS
    113: 112,  # ELITE_FOUR_BRUNO -> ELITE_FOUR_BRUNO
    115: 86,   # ELITE_FOUR_LANCE -> CHAMPION (Lance)
    116: 98,   # LEADER_BROCK -> LEADER_BROCK
    117: 103,  # LEADER_MISTY -> LEADER_MISTY
    118: 104,  # LEADER_LT_SURGE -> LEADER_LT_SURGE
    119: 105,  # LEADER_ERIKA -> LEADER_ERIKA
    120: 89,   # LEADER_KOGA -> ELITE_FOUR_KOGA
    121: 108,  # LEADER_BLAINE -> LEADER_BLAINE
    122: 107,  # LEADER_SABRINA -> LEADER_SABRINA
    123: 34,   # GENTLEMAN -> GENTLEMAN
    124: 110,  # RIVAL_LATE -> LEADER_BLUE
    125: 110,  # CHAMPION_RIVAL -> LEADER_BLUE
    126: 82,   # CHANNELER -> MEDIUM
    127: 8,    # TWINS -> TWINS
    128: 121,  # COOL_COUPLE -> DOUBLE_TEAM
    129: 122,  # YOUNG_COUPLE -> YOUNG_COUPLE
    135: 109,  # RED -> PKMN_TRAINER_RED
    137: 62,   # ROCKET_GRUNT_F -> TEAM_ROCKET_F
    138: 50,   # PSYCHIC_F -> PSYCHIC_F
    139: 10,   # CRUSH_GIRL -> BATTLE_GIRL
    140: 45,   # TUBER_F -> TUBER_F
    141: 17,   # POKEMON_BREEDER -> PKMN_BREEDER_F
    142: 39,   # POKEMON_RANGER_M -> PKMN_RANGER_M
    143: 40,   # POKEMON_RANGER_F -> PKMN_RANGER_F
    144: 7,    # AROMA_LADY -> AROMA_LADY
    145: 48,   # RUIN_MANIAC -> RUIN_MANIAC
    146: 33,   # LADY -> LADY
    147: 15,   # PAINTER -> ARTIST
}
TRAINER_FILES = 5  # a class's files in the NARC: a cut picture, the palette, cells, animation, the whole picture
TRAINER_PICTURE, TRAINER_PALETTE = 4, 1


def narc_files(data: bytes) -> list[bytes]:
    """The members of a Nitro archive (NARC: BTAF offsets, BTNF names, GMIF data)."""
    if data[:4] != b"NARC" or len(data) < 28:
        raise Gen4Error("not a NARC archive")
    pos = struct.unpack_from("<H", data, 12)[0]
    if data[pos:pos + 4] != b"BTAF":
        raise Gen4Error("NARC without its file table")
    size, count = struct.unpack_from("<IH", data, pos + 4)
    if pos + 12 + 8 * count > len(data):
        raise Gen4Error("NARC file table is cut short")
    spans = [struct.unpack_from("<II", data, pos + 12 + 8 * i) for i in range(count)]
    pos += size
    if data[pos:pos + 4] != b"BTNF":
        raise Gen4Error("NARC without its name table")
    pos += struct.unpack_from("<I", data, pos + 4)[0]
    if data[pos:pos + 4] != b"GMIF":
        raise Gen4Error("NARC without its data")
    base = pos + 8
    if any(start > end or base + end > len(data) for start, end in spans):
        raise Gen4Error("NARC member outside the archive")
    return [data[base + start:base + end] for start, end in spans]


def nitro_block(data: bytes, magic: bytes, kind: bytes) -> bytes:
    """The first block of a Nitro graphics file (NCGR, NCLR...) after its block header."""
    if data[:4] != magic:
        raise Gen4Error("not a %s file" % magic[::-1].decode())
    pos = struct.unpack_from("<H", data, 12)[0]
    if data[pos:pos + 4] != kind:
        raise Gen4Error("%s without its %s block" % (magic[::-1].decode(), kind[::-1].decode()))
    return data[pos:pos + struct.unpack_from("<I", data, pos + 4)[0]]


def unscramble_dp(pixels: bytes) -> bytes:
    """Diamond and Pearl's scrambled pictures (HeartGold's UnscanPokepic_DP, used for its trainer pictures): each
    16-bit word from the last back is XORed with an LCG seeded by the last word."""
    words = list(struct.unpack("<%dH" % (len(pixels) // 2), pixels))
    seed = words[-1]
    for i in range(len(words) - 1, -1, -1):
        words[i] ^= seed & 0xFFFF
        seed = (seed * 1103515245 + 24691) & 0xFFFFFFFF
    return struct.pack("<%dH" % len(words), *words)


def trainer_entry(picture: bytes, palette: bytes) -> bytes:
    """One trainer picture's pack entry from HeartGold's NCGR (two 80x80 frames side by side) and NCLR."""
    block = nitro_block(picture, b"RGCN", b"RAHC")
    height, width, depth = struct.unpack_from("<HHI", block, 8)
    pixels = block[0x20:]
    if (height, width, depth) != (SIZE // 8, 2 * SIZE // 8, 3) or len(pixels) != 2 * PICTURE:
        raise Gen4Error("a trainer picture is two 80x80 frames in 16 colours")
    pixels = unscramble_dp(pixels)
    frame = b"".join(pixels[y * SIZE:y * SIZE + SIZE // 2] for y in range(SIZE))
    colours = nitro_block(palette, b"RLCN", b"TTLP")[0x18:0x18 + 32]
    if len(colours) != 32:
        raise Gen4Error("a trainer palette has 16 colours")
    return TRAINER_MAGIC + bytes(4) + colours + frame


def convert_trainers(archive: bytes) -> list[tuple[str, bytes]]:
    """The pack entries for FireRed's trainer pictures from HeartGold's archive (TRAINER_CLASSES)."""
    files = narc_files(archive)
    out = []
    for pic, cls in sorted(TRAINER_CLASSES.items()):
        if (cls + 1) * TRAINER_FILES > len(files):
            raise Gen4Error("the trainer archive has no class %d" % cls)
        first = cls * TRAINER_FILES
        out.append(("%s%03d.bin" % (TRAINER_PREFIX, pic),
                    trainer_entry(files[first + TRAINER_PICTURE], files[first + TRAINER_PALETTE])))
    return out


def quantize(pixels: list[tuple[int, int, int, int]], colours: int = 15) -> tuple[list[int], list[int]]:
    """RGBA pixels to at most `colours` BGR555 colours and transparency (alpha under 128): (palette with colour 0
    the transparent one, each pixel's index). The rarest colour is folded into its nearest one until they fit,
    so a few edge pixels of a strip with more colours change shade and nothing else does."""
    def key(pixel):
        r, g, b, _ = pixel
        return (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10
    counts: dict[int, int] = {}
    for pixel in pixels:
        if pixel[3] >= 128:
            counts[key(pixel)] = counts.get(key(pixel), 0) + 1
    merged = {c: c for c in counts}

    def channels(c):
        return c & 31, (c >> 5) & 31, (c >> 10) & 31

    while len(counts) > colours:
        rare = min(counts, key=lambda c: (counts[c], c))
        rest = [c for c in counts if c != rare]
        near = min(rest, key=lambda c: (sum((a - b) ** 2 for a, b in zip(channels(c), channels(rare))), c))
        counts[near] += counts.pop(rare)
        for c, into in merged.items():
            if into == rare:
                merged[c] = near
    order = sorted(counts, key=lambda c: (-counts[c], c))
    index = {c: order.index(merged[c]) + 1 for c in merged}
    return [0] + order, [index[key(pixel)] if pixel[3] >= 128 else 0 for pixel in pixels]


def scale3x(rows: list[list[int]]) -> list[list[int]]:
    """Scale3x (AdvMAME3x) on a grid of palette indices: three times as large, its stairs rounded off, no colour
    it did not have."""
    h, w = len(rows), len(rows[0])

    def at(x, y):
        return rows[min(max(y, 0), h - 1)][min(max(x, 0), w - 1)]
    out = [[0] * (w * 3) for _ in range(h * 3)]
    for y in range(h):
        for x in range(w):
            a, b, c = at(x - 1, y - 1), at(x, y - 1), at(x + 1, y - 1)
            d, e, f = at(x - 1, y), at(x, y), at(x + 1, y)
            g, hh, i = at(x - 1, y + 1), at(x, y + 1), at(x + 1, y + 1)
            cells = [e] * 9
            if b != hh and d != f:
                cells = [d if d == b else e,
                         b if (d == b and e != c) or (b == f and e != a) else e,
                         f if b == f else e,
                         d if (d == b and e != g) or (d == hh and e != a) else e,
                         e,
                         f if (b == f and e != i) or (hh == f and e != c) else e,
                         d if d == hh else e,
                         hh if (d == hh and e != i) or (hh == f and e != g) else e,
                         f if hh == f else e]
            for k, v in enumerate(cells):
                out[y * 3 + k // 3][x * 3 + k % 3] = v
    return out


def enlarge(frame: list[list[int]], shown: int, size: int) -> list[list[int]]:
    """A square frame of indices enlarged as pixel art (Scale3x, then every so many pixels) to `shown` pixels, centred
    on a `size` x `size` picture."""
    big = scale3x(frame)
    step = len(big) / shown
    picture = [[0] * size for _ in range(size)]
    edge = (size - shown) // 2
    for y in range(shown):
        source = big[int((y + 0.5) * step)]
        for x in range(shown):
            picture[edge + y][edge + x] = source[int((x + 0.5) * step)]
    return picture


def enlarge_back(frame: list[list[int]]) -> list[list[int]]:
    """An 80x80 frame of indices as the 104x104 picture the port shows (BACK_SHOWN pixels, centred); a 64x64 one, drawn
    for the GBA, 1.5 times as large (as the battle shows FireRed's)."""
    return enlarge(frame, BACK_SHOWN if len(frame) == SIZE else len(frame) * 3 // 2, BACK_SIZE)


def convert_back(strip: bytes) -> bytes:
    """A back's pack entry from its five frames, each enlarged to 104x104: a fan-made HGSS-style back is a 400x80 RGBA
    strip of five 80x80 frames, an Emerald Lens one a 64x320 column of five 64x64 frames (FireRed's own size)."""
    width, height, _, rows = read_png(strip)
    if (width, height) == (BACK_FRAMES * SIZE, SIZE):
        side, across = SIZE, True
    elif (width, height) == (64, BACK_FRAMES * 64):
        side, across = 64, False
    else:
        raise Gen4Error("a back is an RGBA strip of five 80x80 frames or a column of five 64x64 ones")
    if not isinstance(rows[0][0], tuple):
        raise Gen4Error("a back is an RGBA PNG")
    palette, indices = quantize([pixel for row in rows for pixel in row])
    frames = b""
    for frame in range(BACK_FRAMES):
        left, top = (frame * side, 0) if across else (0, frame * side)
        grid = [indices[(top + y) * width + left:(top + y) * width + left + side] for y in range(side)]
        for line in enlarge_back(grid):
            frames += bytes(line[x] | line[x + 1] << 4 for x in range(0, BACK_SIZE, 2))
    colours = (palette + [0] * 16)[:16]
    return BACK_MAGIC + bytes([BACK_FRAMES, BACK_SIZE, 0, 0]) + struct.pack("<16H", *colours) + frames


def gba_tiles(indices: list[list[int]]) -> bytes:
    """Rows of 4-bit indices as GBA tiles: 8x8, row by row of tiles, the left pixel in the low nibble."""
    out = bytearray()
    for ty in range(0, len(indices), 8):
        for tx in range(0, len(indices[0]), 8):
            for y in range(8):
                row = indices[ty + y]
                out += bytes(row[tx + x] | row[tx + x + 1] << 4 for x in range(0, 8, 2))
    return bytes(out)


def convert_gba_sprite(picture: bytes, side: int) -> tuple[bytes, bytes]:
    """A square RGBA picture drawn for the GBA (the 64x64 front, the 16x16 map icon) as FireRed keeps its own:
    (4bpp tiles, 16 colours)."""
    width, height, _, rows = read_png(picture)
    if (width, height) != (side, side) or not isinstance(rows[0][0], tuple):
        raise Gen4Error("a %dx%d RGBA picture was expected" % (side, side))
    palette, indices = quantize([pixel for row in rows for pixel in row])
    grid = [indices[y * side:(y + 1) * side] for y in range(side)]
    return gba_tiles(grid), struct.pack("<16H", *(palette + [0] * 16)[:16])


def convert_intro(picture: bytes) -> tuple[bytes, bytes]:
    """The Oak intro's 64x96 picture as FireRed keeps the player's: 96 8bpp tiles, eight to a row, whose pixels take
    BG colours 64 to 95 (the 32 it loads from BG palette 4), 0 transparent; and those 32 colours."""
    width, height, _, rows = read_png(picture)
    if (width, height) != (64, 96) or not isinstance(rows[0][0], tuple):
        raise Gen4Error("an intro picture is a 64x96 RGBA PNG")
    palette, indices = quantize([pixel for row in rows for pixel in row], 31)
    tiles = bytearray()
    for ty in range(12):
        for tx in range(8):
            for y in range(8):
                base = (ty * 8 + y) * 64 + tx * 8
                tiles += bytes(63 + indices[base + x] if indices[base + x] else 0 for x in range(8))
    return bytes(tiles), struct.pack("<32H", *(palette[1:] + [0] * 32)[:32])


def convert_front(picture: bytes) -> bytes:
    """A trainer picture's pack entry (as HeartGold's, 80x80) from a 64x64 RGBA one drawn for the GBA, enlarged 1.25
    as pixel art to fill it."""
    width, height, _, rows = read_png(picture)
    if (width, height) != (64, 64) or not isinstance(rows[0][0], tuple):
        raise Gen4Error("a front picture is a 64x64 RGBA PNG")
    palette, indices = quantize([pixel for row in rows for pixel in row])
    grid = [indices[y * 64:y * 64 + 64] for y in range(64)]
    frame = b"".join(bytes(line[x] | line[x + 1] << 4 for x in range(0, SIZE, 2)) for line in enlarge(grid, SIZE, SIZE))
    return TRAINER_MAGIC + bytes(4) + struct.pack("<16H", *(palette + [0] * 16)[:16]) + frame


def convert_npc(sheet: bytes, count: int = NPC_FRAMES, size: int = 32) -> bytes:
    """A person's pack entry from its 32x288 RGBA sheet of nine 32x32 frames (the player's sheets have `count` of
    them, the Fly bird carrying the player two of 64x64): one palette of 15 colours (the rarest folded into their
    nearest, as for the backs) and each frame as tiles, row by row of tiles."""
    width, height, _, rows = read_png(sheet)
    if (width, height) != (size, size * count) or not isinstance(rows[0][0], tuple):
        raise Gen4Error("a person is a %d-wide RGBA sheet of %d %dx%d frames" % (size, count, size, size))
    palette, indices = quantize([pixel for row in rows for pixel in row])
    frames = b""
    for frame in range(count):
        for ty in range(size // 8):
            for tx in range(size // 8):
                for y in range(8):
                    base = (frame * size + ty * 8 + y) * size + tx * 8
                    frames += bytes(indices[base + x] | indices[base + x + 1] << 4 for x in range(0, 8, 2))
    colours = (palette + [0] * 16)[:16]
    return NPC_MAGIC + bytes([count, size, 0, 0]) + struct.pack("<16H", *colours) + frames


def convert_follower(archive: bytes) -> bytes:
    """A follower Pokémon's pack entry from its HeartGold texture archive: the person entry of nine 32x32 frames (64x64
    for the big ones, which HeartGold draws a tile taller than a person: Steelix, Lugia, Ho-Oh, Wailord, Kyogre, Groudon,
    Rayquaza) in
    FireRed's order (south, north and west standing, then two steps of each, which the game cycles with the standing
    one), from the archive's eight textures: back (north), front (south), left (west) and right, two each. The east
    frames are the west ones flipped by the game, so the archive's right ones are not kept. Its normal palette; colour
    0 is transparent."""
    try:
        source = nsbtx.Archive(archive)
        textures = [source.decode(n) for n in range(len(source.textures))]
    except (nsbtx.NsbtxError, IndexError, struct.error) as error:
        raise Gen4Error("a follower is not a readable texture archive: %s" % error) from error
    if len(textures) != 8:
        raise Gen4Error("a follower has eight textures")
    size = textures[0][0]
    if size not in (32, 64) or any((width, height) != (size, size) for width, height, _ in textures):
        raise Gen4Error("a follower's textures are 32x32 or 64x64")
    # The textures are ordered back, front, left and right, two of each: [back 1, back 2, front 1, front 2, ...].
    north, south, west = (textures[0][2], textures[1][2]), (textures[2][2], textures[3][2]), (textures[4][2], textures[5][2])
    frames = [south[0], north[0], west[0], south[1], south[1], north[1], north[1], west[1], west[1]]
    palette, indices = quantize([pixel for frame in frames for pixel in frame])
    tiles = b""
    for frame in range(NPC_FRAMES):
        for ty in range(size // 8):
            for tx in range(size // 8):
                for y in range(8):
                    base = (frame * size + ty * 8 + y) * size + tx * 8
                    tiles += bytes(indices[base + x] | indices[base + x + 1] << 4 for x in range(0, 8, 2))
    colours = (palette + [0] * 16)[:16]
    return NPC_MAGIC + bytes([NPC_FRAMES, size, 0, 0]) + struct.pack("<16H", *colours) + tiles


def convert_bg(scene: bytes) -> bytes:
    """A battle background's pack entry from its 240x112 PNG: the 15-colour palette, the tiles (identical ones once)
    and the 64x32 tile map of BG3's two screen blocks."""
    width, height, _, rows = read_png(scene)
    if (width, height) != (BG_W, BG_H) or not isinstance(rows[0][0], tuple):
        raise Gen4Error("a battle background is a 240x112 RGB or RGBA PNG")
    palette, indices = quantize([pixel for row in rows for pixel in row])
    columns, tile_rows = BG_W // 8, BG_H // 8
    tiles: list[bytes] = []
    seen: dict[bytes, int] = {}
    grid = [[0] * columns for _ in range(tile_rows)]
    for ty in range(tile_rows):
        for tx in range(columns):
            tile = bytearray()
            for y in range(8):
                base = (ty * 8 + y) * width + tx * 8
                tile += bytes(indices[base + x] | indices[base + x + 1] << 4 for x in range(0, 8, 2))
            tile = bytes(tile)
            if tile not in seen:
                if len(tiles) == BG_TILES_MAX:
                    raise Gen4Error("a battle background has more than %d different tiles" % BG_TILES_MAX)
                seen[tile] = len(tiles)
                tiles.append(tile)
            grid[ty][tx] = seen[tile]
    # BG3 reads a 64x32 map (screen blocks 26 and 27, 32 columns each) with palette 2. The 3DS shows the scene past
    # its 240x112, so around the picture the map mirrors it with flipped tiles: the rows above (wrapping from 31) are
    # its top rows upside down, the rows below its bottom ones, the columns on the right (30 up) its last columns
    # reversed and those on the left (wrapping from 63) its first ones. A mirror has no seam, unlike a repeated edge.
    def mirror(position: int, size: int, wrap: int, below: int) -> tuple[int, int]:
        if position < size:
            return position, 0
        if position < below:
            return 2 * size - 1 - position, 1
        return wrap - 1 - position, 1
    mapped = [[0] * BG_MAP_COLUMNS for _ in range(BG_MAP_ROWS)]
    for row in range(BG_MAP_ROWS):
        source_row, vflip = mirror(row, tile_rows, BG_MAP_ROWS, tile_rows + 4)
        for column in range(BG_MAP_COLUMNS):
            source_column, hflip = mirror(column, columns, BG_MAP_COLUMNS, 47)
            mapped[row][column] = grid[source_row][source_column] | hflip << 10 | vflip << 11 | 2 << 12
    tilemap = b"".join(struct.pack("<H", mapped[row][block * 32 + column]) for block in (0, 1)
                       for row in range(BG_MAP_ROWS) for column in range(32))
    colours = (palette + [0] * 16)[:16]
    return BG_MAGIC + struct.pack("<HH", len(tiles), 0) + struct.pack("<16H", *colours) + b"".join(tiles) + tilemap


BG_WIDE_W = 272         # the battle view is 400 / 1.5 = 267 GBA pixels wide, centred on the 240: 16 pixels on each side
BG_WIDE_LEFT = 2        # tile columns of the picture left of the scene's column 0 (map columns 62 and 63)


def convert_bg_wide(scene: bytes) -> bytes:
    """A battle scene that fills the 3DS's width: its 272x112 PNG (the 240 of the GBA's screen and 16 pixels more on each
    side) as the same pack entry as convert_bg's, but the two outer tile columns go where the GBA's map wraps (columns 62
    and 63) and the rest are the map's columns 0-31, so nothing is mirrored in the sight of the player. At most 34 x 14
    tiles, under the 512 a screen block takes."""
    width, height, _, rows = read_png(scene)
    if (width, height) != (BG_WIDE_W, BG_H) or not isinstance(rows[0][0], tuple):
        raise Gen4Error("a wide battle background is a 272x112 RGB or RGBA PNG")
    palette, indices = quantize([pixel for row in rows for pixel in row])
    columns, tile_rows = BG_WIDE_W // 8, BG_H // 8
    tiles: list[bytes] = []
    seen: dict[bytes, int] = {}
    grid = [[0] * columns for _ in range(tile_rows)]
    for ty in range(tile_rows):
        for tx in range(columns):
            tile = bytearray()
            for y in range(8):
                base = (ty * 8 + y) * width + tx * 8
                tile += bytes(indices[base + x] | indices[base + x + 1] << 4 for x in range(0, 8, 2))
            tile = bytes(tile)
            if tile not in seen:
                if len(tiles) == BG_TILES_MAX:
                    raise Gen4Error("a battle background has more than %d different tiles" % BG_TILES_MAX)
                seen[tile] = len(tiles)
                tiles.append(tile)
            grid[ty][tx] = seen[tile]

    def column_of(map_column: int) -> int:
        """The picture's tile column in a map column: 0-31 are its columns 2-33, 62 and 63 its 0 and 1; the columns the
        view never reaches (32-61) repeat the nearest edge."""
        if map_column < 32:
            return map_column + BG_WIDE_LEFT
        if map_column >= BG_MAP_COLUMNS - BG_WIDE_LEFT:
            return map_column - (BG_MAP_COLUMNS - BG_WIDE_LEFT)
        return columns - 1 if map_column < 48 else 0

    def row_of(row: int) -> tuple[int, int]:
        """The rows below the 14 of the scene (the message box covers them) mirror it, as convert_bg's do."""
        if row < tile_rows:
            return row, 0
        if row < tile_rows + 4:
            return 2 * tile_rows - 1 - row, 1
        return BG_MAP_ROWS - 1 - row, 1
    mapped = [[0] * BG_MAP_COLUMNS for _ in range(BG_MAP_ROWS)]
    for row in range(BG_MAP_ROWS):
        source_row, vflip = row_of(row)
        for column in range(BG_MAP_COLUMNS):
            mapped[row][column] = grid[source_row][column_of(column)] | vflip << 11 | 2 << 12
    tilemap = b"".join(struct.pack("<H", mapped[row][block * 32 + column]) for block in (0, 1)
                       for row in range(BG_MAP_ROWS) for column in range(32))
    colours = (palette + [0] * 16)[:16]
    return BG_MAGIC + struct.pack("<HH", len(tiles), 0) + struct.pack("<16H", *colours) + b"".join(tiles) + tilemap


HD_NATIVE_MAGIC, HD_MAP_MAGIC = b"G4H1", b"G4M1"
HD_NATIVE_W, HD_NATIVE_H = 400, 240


def check_hd(path: str, data: bytes) -> bytes:
    """An HD scene file as it goes into the pack, once it is known to be what the game reads: graphics/gen4/hdn/NNN.bin is
    the scene at the 3DS's own resolution (G4H1, u16 400, u16 240, RGB565 rows), graphics/gen4/hd/NNN.bin a tile scene
    like convert_bg's and graphics/gen4/hd/map.bin the table of maps (G4M1, u16 count, four bytes each)."""
    if path.startswith("graphics/gen4/hdn/"):
        if (data[:4] != HD_NATIVE_MAGIC or struct.unpack_from("<HH", data, 4) != (HD_NATIVE_W, HD_NATIVE_H)
                or len(data) != 8 + HD_NATIVE_W * HD_NATIVE_H * 2):
            raise Gen4Error("%s is not a 400x240 HD scene" % path)
    elif path == "graphics/gen4/hd/map.bin":
        if data[:4] != HD_MAP_MAGIC or len(data) != 6 + 4 * struct.unpack_from("<H", data, 4)[0]:
            raise Gen4Error("%s is not the HD map table" % path)
    elif path.startswith("graphics/gen4/hd/"):
        if data[:4] != BG_MAGIC:
            raise Gen4Error("%s is not an HD tile scene" % path)
    else:
        raise Gen4Error("%s is not an HD scene file" % path)
    return data


def convert(front: bytes, back: bytes, female: tuple[bytes, bytes] | None = None) -> bytes:
    """One species' pack entry from its PNG files (male front and back, optional female pair)."""
    _, _, normal, front_rows = read_png(front)
    _, _, shiny, back_rows = read_png(back)
    pictures = first_frame(front_rows) + first_frame(back_rows)
    if female is not None:
        pictures += first_frame(read_png(female[0])[3]) + first_frame(read_png(female[1])[3])
    return MAGIC + struct.pack("<BBH", 1 if female else 0, 0, 0) + bgr555(normal) + bgr555(shiny) + pictures


# --- Download ---------------------------------------------------------------------------------

def cache_dir() -> Path:
    base = os.environ.get("LOCALAPPDATA") or str(Path.home() / ".cache")
    return Path(base) / "TwinEmbers" / "gen4" / gen4_sources.COMMIT[:12]


def http_get(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=TIMEOUT) as response:
        return response.read()


def source_path(row: tuple) -> str:
    if row[1] == "icon":
        return gen4_sources.ICON_PATH.format(row[2])
    if row[1] == "unown":
        return gen4_sources.UNOWN_PATH.format(gen4_sources.UNOWN_NAMES[row[0]], row[2])
    if row[1] == "castform":
        return gen4_sources.CASTFORM_PATH.format(gen4_sources.CASTFORM_NAMES[row[0]], row[2])
    if row[1] == "spinda":
        return gen4_sources.SPINDA_PATH
    if row[1] == "trainers":
        return gen4_sources.TRAINERS_PATH
    if row[1] in ("back", "bg", "npc", "player", "follower", "hd"):
        return row[2]
    return gen4_sources.PATH.format(*row[:3])


def file_name(row: tuple) -> str:
    """The name a listed file has in the cache or in a folder given for offline use."""
    if row[1] == "icon":
        return "icon_{:08d}.png".format(row[2])
    if row[1] == "unown":
        return "unown_{:02d}_{}.png".format(row[0], row[2])
    if row[1] == "castform":
        return "castform_{:02d}_{}.png".format(row[0], row[2])
    if row[1] == "spinda":
        return "pokepic.c"
    if row[1] == "trainers":
        return "trainers_a058.narc"
    if row[1] == "back":
        return "back_{:03d}.png".format(row[0])
    if row[1] == "bg":
        return "bg_{:02d}.png".format(row[0])
    if row[1] == "npc":
        return "npc_{:03d}.png".format(row[0])
    if row[1] == "player":
        return "player_{}_{}".format(row[0], row[2].rsplit("/", 1)[1])
    if row[1] == "follower":
        return "follower_{:04d}.nsbtx".format(row[0])
    if row[1] == "hd":
        return "hd_" + row[2].rsplit("gen4/", 1)[1].replace("/", "_")
    return "{:03d}_{}_{}.png".format(*row[:3])


def file_url(row: tuple) -> str:
    if row[1] == "back":
        return URL.format(repo=BACK_REPO, commit=BACK_COMMIT, path=source_path(row))
    if row[1] in ("bg", "npc"):
        return URL.format(repo=BG_REPO, commit=BG_COMMIT, path=source_path(row))
    if row[1] == "player":
        return URL.format(repo=PLAYER_REPO, commit=PLAYER_COMMIT, path=source_path(row))
    if row[1] == "hd":
        return URL.format(repo=gen4_hd.REPO, commit=gen4_hd.COMMIT, path=row[2])
    if row[1] == "follower":
        return URL.format(repo=gen4_followers.REPO, commit=gen4_followers.COMMIT, path=source_path(row))
    return URL.format(repo=gen4_sources.REPO, commit=gen4_sources.COMMIT, path=source_path(row))


def all_files(groups: tuple = GROUPS) -> tuple:
    """Every file the download may fetch for the groups asked: HeartGold's (pret's gen4_sources.FILES and the fan-made
    backs), the field's people, the battle scenes, the player (whose backs then take the fan-made ones' place)."""
    rows: tuple = ()
    if GROUP_POKEMON in groups:
        rows += tuple(gen4_sources.FILES) + (BACK_FILES if GROUP_PLAYER not in groups else ())
    if GROUP_NPC in groups:
        rows += NPC_FILES
    if GROUP_BG in groups:
        rows += BG_FILES
    if GROUP_PLAYER in groups:
        rows += PLAYER_FILES
    if GROUP_FOLLOWER in groups:
        rows += FOLLOWER_FILES
    if GROUP_HD in groups:
        rows += tuple(gen4_hd.FILES)
    return rows


def matches(row: tuple, data: bytes | None) -> bool:
    return data is not None and len(data) == row[3] and hashlib.sha256(data).hexdigest() == row[4]


class Downloader:
    """Gets each listed PNG once: a folder given for offline use, then the cache, then GitHub.

    Anything that does not match its listed size and SHA-256 is thrown away. `threads` 0 fetches
    on the calling thread (the web Builder: Pyodide cannot start threads)."""

    def __init__(self, cache: Path | None = None, folder: Path | None = None,
                 get: Callable[[str], bytes] | None = http_get, threads: int = 8):
        self.cache, self.folder, self.get, self.threads = cache, folder, get, threads

    def fetch(self, row: tuple) -> tuple[bytes | None, bool]:
        """(the file or None, whether it came from the network)."""
        for place in (self.folder, self.cache):
            if place is not None:
                path = place / file_name(row)
                data = path.read_bytes() if path.is_file() else None
                if matches(row, data):
                    return data, False
        if self.get is None:
            return None, False
        try:
            data = self.get(file_url(row))
        except Exception:  # noqa: BLE001 - any failure leaves the species on FireRed's pictures
            return None, False
        if not matches(row, data):
            return None, False
        if self.cache is not None:
            try:
                self.cache.mkdir(parents=True, exist_ok=True)
                part = self.cache / (file_name(row) + ".part")
                part.write_bytes(data)
                part.replace(self.cache / file_name(row))
            except OSError:
                pass
        return data, True


def pack_files(downloader: Downloader, progress: Callable[[int, int], None] | None = None,
               groups: tuple = GROUPS) -> tuple[list[tuple[str, bytes]], dict]:
    """The pack entries of the groups asked (HeartGold's pictures, the field's people, the battle backgrounds): every
    species whose files could be had, the icons if all could, and a summary."""
    rows = list(all_files(groups))
    got: dict[tuple, bytes] = {}
    downloaded = done = 0
    # Plain threads taking rows from a queue: concurrent.futures would bring multiprocessing (and ctypes, libffi,
    # pyexpat) into the frozen Builder for nothing.
    todo: queue.Queue = queue.Queue()
    results: queue.Queue = queue.Queue()
    for row in rows:
        todo.put(row)

    def work() -> None:
        while True:
            try:
                row = todo.get_nowait()
            except queue.Empty:
                return
            results.put((row, downloader.fetch(row)))

    workers = [threading.Thread(target=work, daemon=True) for _ in range(downloader.threads)]
    for worker in workers:
        worker.start()
    if not workers:
        work()
    for _ in rows:
        row, (data, fresh) = results.get()
        done += 1
        downloaded += fresh
        if data is not None:
            got[row[:3]] = data
        if progress is not None:
            progress(done, len(rows))
    for worker in workers:
        worker.join()
    files, missing = [], []
    icon_rows = sorted((row for row in rows if row[1] == "icon"), key=lambda row: row[0])
    form_rows = {kind: [row for row in rows if row[1] == kind] for kind in ("unown", "castform")}
    spinda = [row for row in rows if row[1] == "spinda"]
    trainers = [row for row in rows if row[1] == "trainers"]
    backs = [row for row in rows if row[1] == "back"]
    scenes = [row for row in rows if row[1] == "bg"]
    people = [row for row in rows if row[1] == "npc"]
    player = [row for row in rows if row[1] == "player"]
    followers = [row for row in rows if row[1] == "follower"]
    hd_rows = [row for row in rows if row[1] == "hd"]
    rows = [row for row in rows if row[1] in ("male", "female")]
    for dex in sorted({row[0] for row in rows}):
        # The first listed pair is the species' own; a second one (female) differs from it.
        bodies = [body for body in ("male", "female") if any(row[:2] == (dex, body) for row in rows)]
        pairs = [(got.get((dex, body, "front")), got.get((dex, body, "back"))) for body in bodies]
        if any(None in pair for pair in pairs):
            missing.append(dex)
            continue
        files.append(("%s%03d.bin" % (PREFIX, dex), convert(*pairs[0], pairs[1] if len(pairs) > 1 else None)))
    species = len(files)
    for kind, dex in (("unown", 201), ("castform", 351)):
        for form in sorted({row[0] for row in form_rows[kind]}):
            pair = (got.get((form, kind, "front")), got.get((form, kind, "back")))
            if None not in pair:
                files.append(("%s%03d_%02d.bin" % (PREFIX, dex, form), convert(*pair)))
    if spinda and spinda[0][:3] in got:
        files.append((SPINDA, convert_spinda(got[spinda[0][:3]])))
    icons = icon_rows and all(row[:3] in got for row in icon_rows)
    if icons:
        files.append((ICONS, convert_icons([got[row[:3]] for row in icon_rows])))
    trainer_files = convert_trainers(got[trainers[0][:3]]) if trainers and trainers[0][:3] in got else []
    # The player's own front pictures take the place of HeartGold's Red when they come too.
    fronts = {"%s%03d.bin" % (TRAINER_PREFIX, PLAYER_FRONT[row[0]]) for row in player
              if row[:3] in got and row[2].endswith("/front.png")}
    trainer_files = [entry for entry in trainer_files if entry[0] not in fronts]
    files += trainer_files
    back_files = [("%s%03d.bin" % (BACK_PREFIX, row[0]), convert_back(got[row[:3]])) for row in backs
                  if row[:3] in got]
    files += back_files
    # Every terrain gets its scene, or none of them does (half a set would mix two looks).
    converted = {row[0]: convert_bg(got[row[:3]]) for row in scenes if row[:3] in got}
    bg_files = []
    if scenes and len(converted) == len(scenes):
        bg_files = [("%s%03d.bin" % (BG_PREFIX, terrain), converted[scene]) for terrain, scene in enumerate(BG_TERRAIN)]
    files += bg_files
    # A person whose sheet could not be had keeps FireRed's picture; the others are drawn.
    npc_files = [("%s%03d.bin" % (NPC_PREFIX, row[0]), convert_npc(got[row[:3]])) for row in people if row[:3] in got]
    files += npc_files
    # A Pokémon whose archive could not be had has no follower; the others walk behind the player.
    follower_files = []
    for row in (row for row in followers if row[:3] in got):
        dex = row[0] % FOLLOWER_FEMALE
        name = "%s%03d%s.bin" % (FOLLOWER_PREFIX, dex, "_f" if row[0] >= FOLLOWER_FEMALE else "")
        follower_files.append((name, convert_follower(got[row[:3]])))
    files += follower_files
    # The HD scenes: all of them or none (the map table names every scene), each checked and added as it is.
    hd_files = []
    if hd_rows and all(row[:3] in got for row in hd_rows):
        hd_files = [(row[2], check_hd(row[2], got[row[:3]])) for row in hd_rows]
    files += hd_files
    # The player: each sheet that could be had replaces FireRed's for its graphics ids, the back and the front picture.
    player_files = []
    for row in (row for row in player if row[:3] in got):
        sheet = row[2].rsplit("/", 1)[1][:-4]
        data = got[row[:3]]
        if sheet in PLAYER_SHEETS:
            gfx, count = PLAYER_SHEETS[sheet]
            entry = convert_npc(data, count)
            player_files += [("%s%03d.bin" % (NPC_PREFIX, PLAYER_GFX[row[0]] + red), entry) for red in gfx]
        elif sheet == "back":
            player_files.append(("%s%03d.bin" % (BACK_PREFIX, PLAYER_BACK[row[0]]), convert_back(data)))
        elif sheet == "front":
            player_files.append(("%s%03d.bin" % (TRAINER_PREFIX, PLAYER_FRONT[row[0]]), convert_front(data)))
            tiles, colours = convert_gba_sprite(data, 64)
            player_files += [("%sfront_%s.4bpp" % (PLAYER_PREFIX, row[0]), tiles),
                             ("%sfront_%s.gbapal" % (PLAYER_PREFIX, row[0]), colours)]
        elif sheet == "intro":
            tiles, colours = convert_intro(data)
            player_files += [("%sintro_%s.8bpp" % (PLAYER_PREFIX, row[0]), tiles),
                             ("%sintro_%s.gbapal" % (PLAYER_PREFIX, row[0]), colours)]
        elif sheet == "flyBird":
            player_files.append(("%s%03d.bin" % (NPC_PREFIX, PLAYER_BIRD[row[0]]), convert_npc(data, 2, 64)))
        elif sheet == "mapIcon":
            tiles, colours = convert_gba_sprite(data, 16)
            player_files += [("%sicon_%s.4bpp" % (PLAYER_PREFIX, row[0]), tiles),
                             ("%sicon_%s.gbapal" % (PLAYER_PREFIX, row[0]), colours)]
    files += player_files
    return files, {"species": species, "missing": missing, "icons": bool(icons), "trainers": len(trainer_files),
                   "backs": len(back_files), "backgrounds": len(bg_files), "npcs": len(npc_files),
                   "followers": len(follower_files), "hd": len(hd_files),
                   "player": len(player_files), "downloaded": downloaded}
