#ifndef FIRERED_3DS_DAY_NIGHT_LIGHTS_H
#define FIRERED_3DS_DAY_NIGHT_LIGHTS_H

#include <stdint.h>

#include "3ds_day_night.h"

/*
 * The tiles of the outdoor tilesets that are windows (or glass doors), lit at
 * night by the compositor (3ds_video.c, DayNight): only their glass pixels
 * (CtrDayNight_Glass) are, so frames and sills stay as they are. An entry is
 * a tile number in the field's character block (a secondary tileset's start
 * at 640) with the palette it is drawn with, as a metatile writes it:
 * tile | palette << 12. A tile is listed with its palette because the same
 * colours are a roof elsewhere: the Poke Mart's blue roof shares the house
 * windows' palette 3 colours, and its tiles are not listed.
 *
 * Picked by looking at every tile with glass-coloured pixels on the towns'
 * and routes' maps (pokefirered's layouts and tilesets): the Kanto houses,
 * Pokemon Centers, Marts and Gyms of the General tileset, and the windows of
 * Pallet, Viridian, Pewter, Vermilion, Lavender, Celadon, Fuchsia, Saffron,
 * Indigo Plateau, Cinnabar and the Sevii Islands. Not lit: the Bike Shop's
 * skylight, the Pokemon Mansion.
 *
 * Glass that is not blue (Lavender's grey panes, the Pokemon Tower's and the
 * Pewter Museum's dark ones, Silph Co.'s bands that share their colours with
 * its walls) is listed with CTR_LIGHT_GLASS and the colour indices that are
 * its glass in that tile.
 *
 * CTR_LIGHT_PUBLIC (bit 10, where a metatile keeps its flip) marks the windows
 * of buildings open all night: Pokemon Centers, Marts, Gyms and the route
 * gatehouses. They stay lit; the others keep their own hours
 * (CtrDayNight_WindowOn).
 */

static const uint16_t kCtrLights_General[] = {
    384 | CTR_LIGHT_PUBLIC | 2 << 12, 196 | CTR_LIGHT_PUBLIC | 3 << 12, 248 | CTR_LIGHT_PUBLIC | 3 << 12,
    249 | CTR_LIGHT_PUBLIC | 3 << 12, 267 | CTR_LIGHT_PUBLIC | 3 << 12, 269 | CTR_LIGHT_PUBLIC | 3 << 12,
    285 | CTR_LIGHT_PUBLIC | 3 << 12, 292 | CTR_LIGHT_PUBLIC | 3 << 12, 293 | CTR_LIGHT_PUBLIC | 3 << 12,
    334 | CTR_LIGHT_PUBLIC | 3 << 12, 335 | CTR_LIGHT_PUBLIC | 3 << 12, 385 | CTR_LIGHT_PUBLIC | 3 << 12,
    557 | 3 << 12, 558 | 3 << 12, 611 | CTR_LIGHT_PUBLIC | 3 << 12, 612 | CTR_LIGHT_PUBLIC | 3 << 12,
    613 | CTR_LIGHT_PUBLIC | 3 << 12, 614 | CTR_LIGHT_PUBLIC | 3 << 12, 627 | CTR_LIGHT_PUBLIC | 3 << 12,
    628 | CTR_LIGHT_PUBLIC | 3 << 12, 629 | CTR_LIGHT_PUBLIC | 3 << 12, 630 | CTR_LIGHT_PUBLIC | 3 << 12,
    /* The flats of Celadon and Fuchsia. */
    583 | 3 << 12, 584 | 3 << 12, 599 | 3 << 12, 600 | 3 << 12,
};
static const uint16_t kCtrLights_PalletTown[] = {
    684 | 8 << 12, 685 | 8 << 12, 649 | 9 << 12, 654 | 9 << 12, 655 | 9 << 12, 665 | 9 << 12,
    652 | 10 << 12, 653 | 10 << 12,
};
static const uint16_t kCtrLights_ViridianCity[] = {
    684 | 12 << 12, 685 | 12 << 12, 695 | 12 << 12, 696 | 12 << 12,
};
static const uint16_t kCtrLights_PewterCity[] = {
    676 | 8 << 12, 677 | 8 << 12, 662 | 9 << 12, 663 | 9 << 12, 678 | 9 << 12, 679 | 9 << 12,
    671 | 11 << 12, 687 | 11 << 12,
    /* The Museum's arched upper windows. */
    CTR_LIGHT_GLASS(655 | 10 << 12, 1u << 7),
};
static const uint16_t kCtrLights_VermilionCity[] = {
    /* Not 601-603 (palette 8): the house walls' base trim and corners use them too, and lit a stripe along the wall. */
    644 | 9 << 12, 645 | 9 << 12, 646 | 9 << 12,
    660 | 9 << 12, 661 | 9 << 12, 662 | 9 << 12, 739 | 10 << 12,
};
static const uint16_t kCtrLights_LavenderTown[] = {
    862 | 10 << 12, 878 | 10 << 12, 894 | 10 << 12, 827 | 11 << 12, 828 | 11 << 12, 829 | 11 << 12,
    843 | 11 << 12, 844 | 11 << 12, 845 | 11 << 12, 859 | 11 << 12, 860 | 11 << 12, 861 | 11 << 12,
    875 | 11 << 12, 876 | 11 << 12, 877 | 11 << 12, 892 | 11 << 12, 893 | 11 << 12,
    /* The houses' grey panes, and the Pokemon Tower's. */
    CTR_LIGHT_GLASS(668 | 12 << 12, 1u << 7),
    CTR_LIGHT_GLASS(722 | 9 << 12, 1u << 6), CTR_LIGHT_GLASS(738 | 9 << 12, 1u << 6),
    CTR_LIGHT_GLASS(754 | 9 << 12, 1u << 6),
};
static const uint16_t kCtrLights_CeladonCity[] = {
    782 | 3 << 12, 783 | 3 << 12, 798 | 3 << 12, 799 | 3 << 12, 703 | 8 << 12, 647 | 10 << 12,
    648 | 10 << 12, 657 | 10 << 12, 658 | 10 << 12, 659 | 10 << 12, 660 | 10 << 12, 661 | 10 << 12,
    662 | 10 << 12, 673 | 10 << 12, 674 | 10 << 12, 675 | 10 << 12, 676 | 10 << 12, 677 | 10 << 12,
    678 | 10 << 12, 684 | 10 << 12, 687 | 10 << 12,
};
static const uint16_t kCtrLights_FuchsiaCity[] = {
    701 | 8 << 12, 702 | 8 << 12,
};
static const uint16_t kCtrLights_SaffronCity[] = {
    668 | 3 << 12, 669 | 3 << 12, 684 | 3 << 12, 685 | 3 << 12, 763 | 3 << 12, 764 | 3 << 12,
    765 | 3 << 12, 709 | 8 << 12, 710 | 8 << 12, 725 | 8 << 12, 726 | 8 << 12, 719 | 9 << 12,
    735 | 9 << 12, 690 | 12 << 12, 706 | 12 << 12, 722 | 12 << 12,
    /* Silph Co.'s bands of glass, one a floor. */
    CTR_LIGHT_GLASS(717 | 9 << 12, 1u << 13 | 1u << 14), CTR_LIGHT_GLASS(733 | 9 << 12, 1u << 14),
};
static const uint16_t kCtrLights_IndigoPlateau[] = {
    823 | 10 << 12, 824 | 10 << 12, 882 | 10 << 12, 883 | 10 << 12,
};
static const uint16_t kCtrLights_SeviiIslands123[] = {
    676 | 3 << 12, 677 | 3 << 12, 794 | 3 << 12, 795 | 3 << 12, 810 | 3 << 12, 811 | 3 << 12,
    747 | 11 << 12, 748 | 11 << 12, 749 | 11 << 12,
};
static const uint16_t kCtrLights_CinnabarIsland[] = {
    /* The Lab's panes, white and pale blue. */
    CTR_LIGHT_GLASS(644 | 8 << 12, 1u << 5 | 1u << 6 | 1u << 15),
    CTR_LIGHT_GLASS(645 | 8 << 12, 1u << 5 | 1u << 6 | 1u << 15),
    CTR_LIGHT_GLASS(660 | 8 << 12, 1u << 5), CTR_LIGHT_GLASS(661 | 8 << 12, 1u << 5),
};
static const uint16_t kCtrLights_SeviiIslands45[] = {
    676 | 3 << 12, 677 | 3 << 12,
    /* Four Island's Day Care. */
    CTR_LIGHT_GLASS(682 | 9 << 12, 1u << 14 | 1u << 15), CTR_LIGHT_GLASS(683 | 9 << 12, 1u << 14 | 1u << 15),
    CTR_LIGHT_GLASS(698 | 9 << 12, 1u << 14 | 1u << 15), CTR_LIGHT_GLASS(699 | 9 << 12, 1u << 14 | 1u << 15),
};
static const uint16_t kCtrLights_SeviiIslands67[] = {
    676 | 3 << 12, 677 | 3 << 12,
};

#endif
