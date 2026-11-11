/* Real renderer with synthetic pack/game bridges; no ROM-derived fixtures. */
#include <assert.h>
#ifndef PT_SOURCE
#define PT_SOURCE "3ds_poketouch.c"
#endif
#include PT_SOURCE
#include "3ds_pak.h"

static uint8_t fixtureTiles[512], fixtureColors[32];
static int fixtureSize = 288, fixtureFail;
static uint8_t font[256 * 64];
static FILE *realPack;
static CtrPakHeader realHeader;
static CtrPakEntry *realIndex;
const uint8_t *CtrTwinFontData(void) { return font; }
static uint8_t fontWidths[256];   /* the game font's widths with PT_PREVIEW_WIDTHS (a viewing aid); else a fixed 5 */
unsigned CtrTwinFontWidth(unsigned code) { return code < 256 && fontWidths[code] ? fontWidths[code] : 5; }
const uint8_t *CtrTwinJapaneseGlyph(unsigned code, uint8_t out[64]) { (void)code; memset(out, 0, 64); return out; }
unsigned CtrTwinJapaneseFontWidth(unsigned code) { (void)code; return 8; }
static uint8_t smallFont[256 * 32];
const uint8_t *CtrTwinSmallFontData(void) { return smallFont; }
unsigned CtrTwinSmallFontWidth(unsigned code) { (void)code; return 5; }

/* Literal-only GBA LZ stream, freshly synthesized from the fixture bytes. */
void *CtrData_Load(const char *path, uint32_t *len)
{
    if (realPack) {
        const CtrPakEntry *entry=CtrPak_Find(realIndex,realHeader.entryCount,path);
        assert(entry && !entry->flags && entry->storedSize==entry->rawSize);
        uint8_t *out=malloc(entry->rawSize); assert(out);
        assert(!fseek(realPack,(long)entry->offset,SEEK_SET));
        assert(fread(out,1,entry->rawSize,realPack)==entry->rawSize);
        assert(CtrPak_Crc32(0,out,entry->rawSize)==entry->crc32);
        *len=entry->rawSize; return out;
    }
    if (fixtureFail) return NULL;
    int palette = strstr(path, "icon_palettes/") != NULL;
    const uint8_t *src = palette ? fixtureColors : fixtureTiles;
    unsigned size = palette ? 32u : (unsigned)fixtureSize;
    *len = 4 + size + (size + 7) / 8;
    uint8_t *out = malloc(*len); assert(out);
    out[0] = 0x10; out[1] = size; out[2] = size >> 8; out[3] = 0;
    unsigned pos = 4;
    for (unsigned i = 0; i < size; ++i) {
        if (!(i % 8)) out[pos++] = 0;
        out[pos++] = src[i];
    }
    assert(pos == *len);
    return out;
}
int CtrPokeTouch_DexMonPic(unsigned species, unsigned char *tiles, unsigned short *palette)
{ (void)species; (void)tiles; (void)palette; return 0; }
int CtrPokeTouch_DexMonPic80(unsigned species, unsigned char *pixels, unsigned short *palette)
{ (void)species; (void)pixels; (void)palette; return 0; }
int CtrPokeTouch_DexMenuIcon(int id, unsigned char *tiles, unsigned short *palette)
{ (void)id; (void)tiles; (void)palette; return 0; }
int CtrPokeTouch_DexFootprint(unsigned species, unsigned char *bits)
{ (void)species; (void)bits; return 0; }
int CtrPokeTouch_DexCaughtMarker(unsigned char *tiles, unsigned short *palette)
{ (void)tiles; (void)palette; return 0; }
int CtrPokeTouch_CardTrainerPic(unsigned char *tiles, unsigned short *palette)
{ (void)tiles; (void)palette; return 0; }
int CtrPokeTouch_CardBadges(unsigned char *tiles, unsigned short *palette)
{ (void)tiles; (void)palette; return 0; }
int CtrPokeTouch_MonIcon(unsigned species, unsigned personality, int egg, unsigned char *tiles, unsigned short *palette)
{
    (void)species; (void)personality; (void)egg; (void)tiles; (void)palette;
    return 0;
}
int CtrPokeTouch_ItemIcon(unsigned item, unsigned char *tiles, unsigned short *palette)
{
    memset(tiles, 0x11, 288); memset(palette, 0, 32);
    palette[1] = item == 1 ? 31 : 31 << 10;
    return item != 0;
}
int CtrPokeTouch_MapArt(int group, uint16_t *map, uint8_t *tiles, uint16_t *pal,
                      uint16_t *bgMap, uint8_t *bgTiles)
{
    (void)group;
    memset(map, 0, 30 * 20 * 2); memset(bgMap, 0, 30 * 20 * 2);
    memset(tiles, 0x11, MAP_TILES * 32); memset(bgTiles, 0, MAP_TILES * 32);
    memset(pal, 0, MAP_COLORS * 2); pal[1] = 0x5ab3;
    return 1;
}
int CtrPokeTouch_MapPlayerIcon(uint8_t *tiles, uint16_t *pal)
{ (void)tiles; (void)pal; return 0; }
int CtrPokeTouch_MapCursorIcon(uint8_t *tiles, uint16_t *pal)
{ (void)tiles; (void)pal; return 0; }
int CtrBottomParty_Icon(int slot, unsigned char *tiles, unsigned short *pal)
{ (void)slot; (void)tiles; (void)pal; return 0; }

unsigned CtrBottomSummary_IconSpecies(void) { return 0; }
int CtrBottomSummary_Icon(unsigned char *tiles, unsigned short *pal)
{ (void)tiles; (void)pal; return 0; }
int CtrBottomSummary_Pic(unsigned char *tiles, unsigned short *pal, int *flip)
{ (void)tiles; (void)pal; (void)flip; return 0; }
int CtrBottomSummary_Pic80(unsigned char *pixels, unsigned short *pal, int *flip)
{ (void)pixels; (void)pal; (void)flip; return 0; }
int CtrPokeTouch_Wallpaper(unsigned id, unsigned short *rgb555)
{ (void)id; (void)rgb555; return 0; }
int CtrPokeTouch_MarkShape(int mark, unsigned char *indexes)
{ (void)mark; (void)indexes; return 0; }

static uint16_t canvas[W * H], reference[W * H];
static uint16_t at(int x, int y) { return canvas[x * H + H - 1 - y]; }
static void update(CtrPokeTouchView *old, const CtrPokeTouchView *v)
{
    CtrDirtyList dirty; CtrDirty_Clear(&dirty);
    CtrPokeTouch_Update(canvas, old, v, &dirty);
    memcpy(reference, canvas, sizeof(canvas));
    CtrPokeTouch_Draw(canvas, v);
#ifndef PT_BASELINE
    if (memcmp(reference, canvas, sizeof(canvas))) {
        for(int x=0;x<W;++x) for(int y=0;y<H;++y) {
            unsigned n=x*H+H-1-y;
            if(reference[n]!=canvas[n]) {
                fprintf(stderr,"redraw mismatch (opt sel %u pressed %d zone %u page %u cnt %u options %u) at %d,%d selected=%u pressed=%u save=%u/%u back=%u\n",
                        v->optSelected,v->optPressed,v->optZone,v->optPage,v->optCount,v->options,x,y,v->selected,v->pressed,v->save,v->saveState,v->backVisible);
                assert(reference[n]==canvas[n]);
            }
        }
    }
#endif
    *old = *v;
}
/* The canvas as a PPM named dir/name.ppm. */
static void ppm(const char *dir, const char *name)
{
    char path[512];
    snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);
    FILE *f=fopen(path,"wb"); assert(f);
    fprintf(f,"P6\n320 240\n255\n");
    for(int y=0;y<H;++y) for(int x=0;x<W;++x) {
        uint16_t c=at(x,y); uint8_t p[3]={(uint8_t)((c>>11)*255/31),(uint8_t)(((c>>5)&63)*255/63),(uint8_t)((c&31)*255/31)};
        fwrite(p,1,3,f);
    }
    fclose(f);
}
static void decoder(void)
{
#ifndef PT_BASELINE
    uint8_t out[512], pixels[144], saved[144]; uint16_t pal[16];
    const uint8_t literals[] = {0x10,3,0,0,0,1,2,3};
    assert(PokeTouch_Lz77(literals,sizeof(literals),out,3)==3);
    for (unsigned n=0;n<sizeof(literals);++n) assert(!PokeTouch_Lz77(literals,n,out,3));
    assert(!PokeTouch_Lz77(literals,sizeof(literals),out,2));
    const uint8_t repeated[] = {0x10,6,0,0,0x10,1,2,3,0,2};
    assert(PokeTouch_Lz77(repeated,sizeof(repeated),out,6)==6);
    assert(!memcmp(out,"\1\2\3\1\2\3",6));
    const uint8_t invalid[] = {0x10,3,0,0,0x80,0,0};
    assert(!PokeTouch_Lz77(invalid,sizeof(invalid),out,512));
    const uint8_t overrun[] = {0x10,2,0,0,0x40,1,0,0};
    assert(!PokeTouch_Lz77(overrun,sizeof(overrun),out,512));
    assert(!strcmp(sMenuItemArt[3],"card_key"));
    assert(!strcmp(sMenuItemArt[5],"teachy_tv"));
    for (int pitch=3;pitch<=4;++pitch) {
        fixtureSize=pitch==3?288:512;
        memset(fixtureTiles,0,sizeof(fixtureTiles));
        memset(fixtureColors,0,sizeof(fixtureColors));
        fixtureColors[2]=0xff; fixtureColors[3]=0x7f; /* index 1 white */
        /* Odd x/y, across both tile boundaries: upper-left sampling loses it. */
        fixtureTiles[(9/8*pitch+9/8)*32+9%8*4+9%8/2]=0x10;
        assert(CtrPokeTouch_PackMenuIcon(0,pixels,pal));
        assert(pixels[4*12+4]==1 && pixels[0]==0 && pal[0]==0 && pal[1]==0xffff);
        /* Opaque black must remain an opaque index. */
        fixtureColors[2]=fixtureColors[3]=0;
        assert(CtrPokeTouch_PackMenuIcon(0,pixels,pal) && pal[1]==0);
        memcpy(saved,pixels,sizeof(saved)); fixtureFail=1;
        assert(!CtrPokeTouch_PackMenuIcon(0,pixels,pal));
        assert(!memcmp(saved,pixels,sizeof(saved))); fixtureFail=0;
    }
    {
        /* The side menu's soft icons: the pack's 24x24 reduced by area to 16x16, colour over the drawn pixels only. */
        static PtSoft soft;

        fixtureSize=288; memset(fixtureColors,0,sizeof(fixtureColors)); fixtureColors[2]=0xff; fixtureColors[3]=0x7f;
        memset(fixtureTiles,0x11,288);
        assert(PokeTouch_SoftIcon("fame_checker",&soft) && soft.ok);
        for (int i=0;i<PT_SOFT*PT_SOFT;++i) assert(soft.alpha[i]==255 && soft.rgb[i]==0xffff);
        /* Opaque only left of x=11 in every row: a whole output column (x 0-10.5), then a third of one, then none. */
        memset(fixtureTiles,0,288);
        for (int tr=0;tr<3;++tr) for (int row=0;row<8;++row) {
            memset(&fixtureTiles[(tr*3+0)*32+row*4],0x11,4);
            fixtureTiles[(tr*3+1)*32+row*4+0]=0x11; fixtureTiles[(tr*3+1)*32+row*4+1]=0x01;
        }
        assert(PokeTouch_SoftIcon("fame_checker",&soft));
        assert(soft.alpha[5]==255 && soft.alpha[7]>70 && soft.alpha[7]<100 && soft.alpha[8]==0 && soft.alpha[15]==0);
        assert(soft.rgb[7]==0xffff);                              /* a half-covered pixel keeps the colour, not a darker mix */
        memset(fixtureTiles,0,sizeof(fixtureTiles)); assert(!PokeTouch_SoftIcon("fame_checker",&soft));   /* nothing drawn */
        fixtureFail=1; assert(!PokeTouch_SoftIcon("fame_checker",&soft)); fixtureFail=0;
    }
    {
        /* Ascuas 2's text: a line that does not fit ends in "..." (never a glyph cut in half), and fits; one that fits is untouched; a UTF-8
         * letter is not split. (Every glyph is 5 wide in this test.) */
        char out[64];
        assert(!Em2Fit("HELLO",25,out,sizeof(out)) && !strcmp(out,"HELLO"));
        assert(Em2Fit("HELLO WORLD ABC",40,out,sizeof(out)) && !strcmp(out,"HELLO..."));
        assert(UiTextWidth(out,1)<=40);
        assert(Em2Fit("ABCD EFGH",30,out,sizeof(out)) && !strcmp(out,"ABC..."));          /* no space before the dots */
        assert(Em2Fit("AB\303\201" "CDEFG",30,out,sizeof(out)) && !strcmp(out,"AB\303\201..."));
        assert(Em2Fit("ABCDEFGHIJ",12,out,sizeof(out)) && !strcmp(out,"..."));            /* too narrow for any letter: only the dots */
        assert(Em2Fit("ABCDEFGHIJ",50,out,6) && strlen(out)<6);                           /* a small buffer is respected */
        Em2Join(out,sizeof(out),"ONE","TWO"); assert(!strcmp(out,"ONE TWO"));
        Em2Join(out,sizeof(out),"","TWO"); assert(!strcmp(out,"TWO"));
        Em2Join(out,5,"ONE","TWO"); assert(!strcmp(out,"ONE "));
    }
    fixtureSize=287; assert(!CtrPokeTouch_PackMenuIcon(0,pixels,pal));
    fixtureSize=288; memset(fixtureTiles,0,sizeof(fixtureTiles));
    assert(!CtrPokeTouch_PackMenuIcon(0,pixels,pal));
    assert(!CtrPokeTouch_PackMenuIcon(6,pixels,pal));
#endif
}
int main(int argc, char **argv)
{
    decoder(); fixtureFail=1; /* Exercise original fallbacks for the UI. */
#ifndef PT_BASELINE
    const char *packPath=getenv("FIRERED_TEST_PAK");
    if(packPath) {
        realPack=fopen(packPath,"rb"); assert(realPack);
        uint8_t header[64]; assert(fread(header,1,64,realPack)==64);
        static const uint8_t romHash[20]={0x41,0xcb,0x23,0xd8,0xdc,0xcc,0x8e,0xbd,0x7c,0x64,
            0x9c,0xd8,0xfb,0xb5,0x8e,0xea,0xce,0x6e,0x2f,0xdc};
        assert(CtrPak_ParseHeader(header,0x8f71cf3a,romHash,&realHeader)==CTR_PAK_OK);
        size_t size=realHeader.entryCount*40;
        uint8_t *index=malloc(size); realIndex=calloc(realHeader.entryCount,sizeof(*realIndex));
        assert(index && realIndex && fread(index,1,size,realPack)==size);
        assert(CtrPak_ParseIndex(index,&realHeader,realIndex)==CTR_PAK_OK); free(index);
        for(unsigned i=0;i<6;++i) {
            uint8_t pixels[144]; uint16_t palette[16];
            assert(CtrPokeTouch_PackMenuIcon(i,pixels,palette));
        }
        puts("PASS six production icon decoders with real user-owned pack, header/index/payload CRCs");
    }
#endif
    for (int y=-1;y<=H;++y) for (int x=-1;x<=W;++x) {
        static const CtrPokeTouchRect expected[PT_COUNT] = {
            {250,4,68,21},{250,29,68,21},{250,54,68,21},{250,79,68,21},
            {250,104,68,21},{250,129,68,21},{250,158,32,40},{286,158,32,40},{250,204,68,30}};
        int hit=PT_NONE;
        for(int i=0;i<PT_COUNT;++i) if(UiIn(&expected[i],x,y)) hit=i;
        assert(CtrPokeTouch_HitTest(x,y)==hit);
        int gx=-1,gy=-1;
        int inside=x>=4&&x<244&&y>=4&&y<164;
        assert(CtrPokeTouch_ViewportToGba(x,y,&gx,&gy)==inside);
        if(inside) assert(gx==x-4&&gy==y-4);
    }
    CtrPokeTouchView v={0}; v.selected=v.pressed=PT_NONE;
    v.home=1; v.playerX=v.playerY=v.cursorX=v.cursorY=-1;
    v.registeredItem=1; v.bikeItem=2;
    strcpy(v.playerName,"Trades"); strcpy(v.bar[0],"TEST MAP");
    for(int i=0;i<PT_COUNT;++i) v.enabled[i]=1;
    CtrPokeTouch_Draw(canvas,&v);
    for(int x=4;x<244;++x) for(int y=7;y<164;++y) assert(at(x,y)==Gba(0x5ab3));     /* the first three rows are shaded: the map sits in its frame */
    for(int x=4;x<244;++x) for(int y=4;y<7;++y) assert(at(x,y)!=Gba(0x5ab3));
    if(argc>1) { /* Synthetic render for visual QA, never a ROM screenshot. */
        FILE *f=fopen(argv[1],"wb"); assert(f);
        fprintf(f,"P6\n320 240\n255\n");
        for(int y=0;y<H;++y) for(int x=0;x<W;++x) {
            uint16_t c=at(x,y); uint8_t p[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};
            fwrite(p,1,3,f);
        }
        fclose(f);
    }
    FILE *viewports=argc>2?fopen(argv[2],"wb"):NULL;
    CtrPokeTouchView old=v;
    for(int i=0;i<PT_COUNT;++i) {
        v.pressed=i; update(&old,&v); v.pressed=PT_NONE; v.selected=i; update(&old,&v);
        v.enabled[i]=0; v.selected=PT_NONE; update(&old,&v); v.enabled[i]=1; update(&old,&v);
    }
    v.running=1; update(&old,&v); v.registeredItem=2; v.bikeItem=1; update(&old,&v);
    strcpy(v.playerName,"Alex"); update(&old,&v);
    for(int screen=0;screen<4;++screen) {
        v.home=screen==0; v.viewport=screen==1; v.options=screen==2; v.save=screen==3;
        v.optCount=2; strcpy(v.optLabel[0],"SOUND"); strcpy(v.optValue[0],"MONO");
        v.savePressed=PT_SAVE_NONE;
        update(&old,&v);
        if(viewports) for(int x=4;x<244;++x) for(int y=4;y<164;++y) {
            uint16_t c=at(x,y); fwrite(&c,2,1,viewports);
        }
    }
    for(int state=SAVE_CONFIRM;state<=SAVE_ERROR;++state) {
        v.saveState=state; update(&old,&v);
        v.saveSel=1; update(&old,&v); v.savePressed=PT_SAVE_FIRST; update(&old,&v);
        assert(CtrPokeTouch_SaveHit(180,state==SAVE_CONFIRM?180:200,state)==
               (state==SAVE_WRITING?PT_SAVE_NONE:PT_SAVE_FIRST));
        v.savePressed=PT_SAVE_NONE; update(&old,&v);
    }
#ifndef PT_BASELINE
    /* The preview under OPTIONS' look rows: a note card in the rows the page does not use, with the look's walking picture
     * (here a pattern and a palette), its name and its artists; it follows the look chosen, and goes with the row. */
    {
        v.home=0; v.viewport=0; v.options=1; v.save=0; v.optCount=4; v.optSelected=1;
        v.optLook.on=1; v.optLook.has=1;
        v.optPageCount=4; v.optPage=3; strcpy(v.optPageName,"LOOKS"); v.optSwitch[0]=2; v.optSwitch[1]=1;   /* the PAGE button's dots, the switches */
        strcpy(v.optLook.name,"ASH HGSS"); strcpy(v.optLook.credit,"Made by PKMNTrainerRick");
        for(int i=0;i<512;++i) v.optLook.tiles[i]=(uint8_t)(i*7);
        for(int i=1;i<16;++i) v.optLook.palette[i]=(uint16_t)(i*0x0421);
        update(&old,&v);
        {   /* For a look at the screen: PT_PREVIEW_PPM names the file to write, PT_PREVIEW_WALK a walk.bin to show, with its
             * name and artists in PT_PREVIEW_NAME and PT_PREVIEW_CREDIT. */
            const char *out=getenv("PT_PREVIEW_PPM"), *walk=getenv("PT_PREVIEW_WALK");
            if(out && walk) {
                const char *fp=getenv("PT_PREVIEW_FONT");   /* a 256x64-byte glyph file, a local viewing aid */
                if(fp) { FILE *ff=fopen(fp,"rb"); assert(ff); assert(fread(font,1,sizeof(font),ff)==sizeof(font)); fclose(ff); }
                FILE *w=fopen(walk,"rb"); assert(w);
                uint8_t entry[40+512]; assert(fread(entry,1,sizeof(entry),w)==sizeof(entry)); fclose(w);
                memcpy(v.optLook.tiles,entry+40,512);
                for(int i=0;i<16;++i) v.optLook.palette[i]=(uint16_t)(entry[8+i*2]|entry[9+i*2]<<8);
                snprintf(v.optLook.name,sizeof(v.optLook.name),"%s",getenv("PT_PREVIEW_NAME")?getenv("PT_PREVIEW_NAME"):"LOOK");
                snprintf(v.optLook.credit,sizeof(v.optLook.credit),"%s",getenv("PT_PREVIEW_CREDIT")?getenv("PT_PREVIEW_CREDIT"):"");
                int first=getenv("PT_PREVIEW_ROWS") && atoi(getenv("PT_PREVIEW_ROWS"))==5;   /* the page with REUSABLE TMS: five rows */
                v.optCount=first?5:4; v.optSelected=first?2:1;
                strcpy(v.optLabel[0],"EXP SHARE"); strcpy(v.optValue[0],"OFF");
                if(first) { strcpy(v.optLabel[1],"REUSABLE TMS"); strcpy(v.optValue[1],"OFF"); }
                strcpy(v.optLabel[first+1],"RED LOOK"); strcpy(v.optValue[first+1],v.optLook.name);
                strcpy(v.optLabel[first+2],"LEAF LOOK"); strcpy(v.optValue[first+2],"DEFAULT");
                strcpy(v.optLabel[first+3],"RIVAL LOOK"); strcpy(v.optValue[first+3],"DEFAULT");
                update(&old,&v);
                FILE *f=fopen(out,"wb"); assert(f);
                fprintf(f,"P6\n320 240\n255\n");
                for(int y=0;y<H;++y) for(int x=0;x<W;++x) {
                    uint16_t c=at(x,y); uint8_t p[3]={(uint8_t)((c>>11)*255/31),(uint8_t)(((c>>5)&63)*255/63),(uint8_t)((c&31)*255/31)};
                    fwrite(p,1,3,f);
                }
                fclose(f);
            }
        }
        strcpy(v.optLook.name,"GARY"); strcpy(v.optLook.credit,"Made by Alan_Flygon, a credit long enough to need a second and a third line of the note card");
        update(&old,&v);
        v.optLook.has=0; strcpy(v.optLook.name,"DEFAULT"); update(&old,&v);       /* the default: no picture */
        v.optCount=5; v.optLook.has=1; strcpy(v.optLook.name,"ASH HGSS"); update(&old,&v);   /* five rows: the card is 42 high, the picture at its own size */
        v.optCount=6; update(&old,&v);                                            /* six: no room for the card at all */
        v.optCount=7; update(&old,&v);                                            /* a page of seven rows has no room: no panel */
        v.optCount=4; v.optLook.on=0; update(&old,&v);                           /* another row selected: the panel goes */
        memset(&v.optLook,0,sizeof(v.optLook)); v.optCount=2; v.optSelected=0; v.options=0; v.home=1; update(&old,&v);
    }
#endif
    /* Ascuas 2: the screens the owner's mockup draws, each through the incremental path first (every press, selection and value
     * change must equal a full redraw) and then, with PT_PREVIEW_DIR, written as a PPM (PT_PREVIEW_FONT and PT_PREVIEW_WIDTHS: the
     * game's glyphs and widths, local viewing aids). */
    {
        const char *dir=getenv("PT_PREVIEW_DIR"), *fp=getenv("PT_PREVIEW_FONT"), *wp=getenv("PT_PREVIEW_WIDTHS");
        if(dir && fp) { FILE *ff=fopen(fp,"rb"); assert(ff); assert(fread(font,1,sizeof(font),ff)==sizeof(font)); fclose(ff); memset(sPtFontReady,0,sizeof(sPtFontReady)); }
        if(dir && wp) { FILE *ff=fopen(wp,"rb"); assert(ff); assert(fread(fontWidths,1,sizeof(fontWidths),ff)==sizeof(fontWidths)); fclose(ff); memset(sPtFontReady,0,sizeof(sPtFontReady)); }
        CtrPokeTouchView s={0}; CtrPokeTouchView before; s.selected=s.pressed=PT_NONE; s.optPressed=-1;
        for(int i=0;i<PT_COUNT;++i) s.enabled[i]=1;
        s.registeredItem=1; s.bikeItem=2; strcpy(s.playerName,"Rick"); s.playerX=s.playerY=-1;
        #define SHOT(name) do { update(&old,&s); if(dir) ppm(dir,name); } while(0)
        /* HOME with a destination chosen on the map: the name, the region and the hint. */
        memset(&old,0,sizeof(old)); old.selected=old.pressed=PT_NONE;
        s.home=1; s.mapGroup=0; s.cursorX=10; s.cursorY=8; s.flyOk=1; strcpy(s.bar[0],"VERMILION CITY"); strcpy(s.bar[1],"Fly available"); strcpy(s.bar[2],"Tap again to Fly");
        SHOT("home");
        s.selected=PT_OPTIONS; s.pressed=PT_BAG; SHOT("home_pressed"); s.pressed=PT_NONE; s.selected=PT_NONE; s.running=1; SHOT("home_running"); s.running=0;
        /* OPTIONS, page 2: switches, values, levels; then each press and selection moves. */
        s.home=0; s.options=1; s.optCount=7; s.optPage=1; s.optPageCount=4; strcpy(s.optPageName,"EXTRAS");
        static const char *const labels[7]={"TYPE CHART","MOVE DATA","DAY/NIGHT","TIME","EMBERS","REPELLENT","COMPANION"};
        for(int i=0;i<7;++i) strcpy(s.optLabel[i],labels[i]);
        s.optSwitch[0]=2; s.optSwitch[1]=2; strcpy(s.optValue[2],"CLOCK"); strcpy(s.optValue[3],"12:00");
        s.optSegCount[4]=3; s.optSegValue[4]=2; strcpy(s.optSegName[4][0],"OFF"); strcpy(s.optSegName[4][1],"LOW"); strcpy(s.optSegName[4][2],"HIGH");
        s.optSwitch[5]=1; s.optSegCount[6]=3; s.optSegValue[6]=2; strcpy(s.optSegName[6][0],"OFF"); strcpy(s.optSegName[6][1],"FOLLOW"); strcpy(s.optSegName[6][2],"BE");
        s.optSelected=6; strcpy(s.bar[0],"COMPANION"); strcpy(s.bar[1],"A Pokemon follows you,"); strcpy(s.bar[2],"or you are that Pokemon.");
        s.backVisible=1; s.backEnabled=1;
        SHOT("options2");
        before=s;
        for(int row=0;row<7;++row) for(int zone=PT_OPT_ZONE_LABEL;zone<=PT_OPT_ZONE_NEXT;++zone) { s.optSelected=(uint8_t)row; s.optPressed=(int8_t)row; s.optZone=(uint8_t)zone; update(&old,&s); s.optPressed=-1; update(&old,&s); }
        for(int k=0;k<3;++k) { s.optPressed=4; s.optZone=(uint8_t)(PT_OPT_ZONE_SEG+k); update(&old,&s); s.optPressed=-1; s.optSegValue[4]=(uint8_t)k; update(&old,&s); }
        s.optPagePressed=1; update(&old,&s); s.optPagePressed=0; s.optPage=2; update(&old,&s); s.optPage=1;
        strcpy(s.optValue[3],"13:45"); update(&old,&s);
        s=before; SHOT("options2_again");
        s.optPage=0; strcpy(s.optPageName,"GAME"); s.optSelected=0; strcpy(s.bar[0],"TYPE CHART"); strcpy(s.bar[1],"Shows how effective a move"); strcpy(s.bar[2],"will be on the foe.");
        SHOT("options1");
        /* The look page: three look rows and the preview card. */
        s.optPage=3; strcpy(s.optPageName,"LOOKS"); s.optCount=4; memset(s.optSwitch,0,sizeof(s.optSwitch)); memset(s.optSegCount,0,sizeof(s.optSegCount));
        strcpy(s.optLabel[0],"EXP SHARE"); s.optSegCount[0]=3; s.optSegValue[0]=1; strcpy(s.optSegName[0][0],"OFF"); strcpy(s.optSegName[0][1],"HALF"); strcpy(s.optSegName[0][2],"ALL");
        strcpy(s.optLabel[1],"RED LOOK"); strcpy(s.optValue[1],"GOLD HGSS"); strcpy(s.optLabel[2],"LEAF LOOK"); strcpy(s.optValue[2],"MAY HGSS"); strcpy(s.optLabel[3],"RIVAL LOOK"); strcpy(s.optValue[3],"SILVER");
        s.optSelected=2; s.optLook.on=1; s.optLook.has=1; strcpy(s.optLook.name,"MAY HGSS"); strcpy(s.optLook.credit,"Made by DiegoWT");
        for(int i=0;i<512;++i) s.optLook.tiles[i]=(uint8_t)(i*7);
        for(int i=1;i<16;++i) s.optLook.palette[i]=(uint16_t)(i*0x0421);
        strcpy(s.bar[0],"LEAF LOOK"); strcpy(s.bar[1],"How Leaf looks."); strcpy(s.bar[2],"Looks from the Builder.");
        SHOT("options_look");
        (void)before;
    }
    v.save=0; v.home=1; v.backVisible=1; update(&old,&v);
    v.backEnabled=1; update(&old,&v); v.backPressed=1; update(&old,&v);
    if(viewports) fclose(viewports);
    if(realPack) fclose(realPack);
    free(realIndex);
    puts("PASS real renderer: decoder, 4bpp, geometry, viewport, incremental states, X/Y/RUN, save/back hit-tests");
    return 0;
}
