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
/* The map's two icons: none, unless a test asks for synthetic art (a red square for the player, a blue one for the cursor, whose second frame is half of it). */
static int iconArt;
int CtrPokeTouch_MapPlayerIcon(uint8_t *tiles, uint16_t *pal)
{
    if(!iconArt) return 0;
    memset(tiles,0x11,4*32); memset(pal,0,32); pal[1]=0x001f; return 1;
}
int CtrPokeTouch_MapCursorIcon(uint8_t *tiles, uint16_t *pal)
{
    if(!iconArt) return 0;
    memset(tiles,0x22,4*32); memset(tiles+4*32,0x20,4*32); memset(pal,0,32); pal[2]=0x7c00; return 1;
}
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
#ifndef PT_BASELINE
/* The Llama pieces' test: how many pixels of (x, y, w, h) are not the marker colour; a pixel outside it grown by `grow` that is not is a failure. */
static unsigned llChanged(int x,int y,int w,int h,int grow,const char *name)
{
    unsigned inside=0;
    for(int cy=0;cy<H;++cy) for(int cx=0;cx<W;++cx) {
        int in=cx>=x&&cx<x+w&&cy>=y&&cy<y+h, near=cx>=x-grow&&cx<x+w+grow&&cy>=y-grow&&cy<y+h+grow;
        if(canvas[cx*H+H-1-cy]==0x1357) continue;
        if(!near) { fprintf(stderr,"%s painted outside its rectangle, at %d,%d\n",name,cx,cy); assert(0); }
        if(in) ++inside;
    }
    return inside;
}
#endif
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
    {   /* HOME: the map fills the viewport, but for the region's red ribbon at its corner (the Llama mockup's KANTO); the first three rows are shaded: the map sits in its frame */
        int rib=UiTextWidth("KANTO",1)+18;
        for(int x=4;x<244;++x) for(int y=4;y<164;++y) {
            int onRibbon=x<4+rib&&y<4+13;
            if(onRibbon) continue;
            if(y<7) assert(at(x,y)!=Gba(0x5ab3)); else assert(at(x,y)==Gba(0x5ab3));
        }
        assert(at(4,4)==LL_RIB_TOP && at(4+3,4+6)!=Gba(0x5ab3) && at(4,4+13)==Gba(0x5ab3));      /* the ribbon: red from its top, 13 rows */
    }
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
        /* The PC box and the naming keyboard (Llama, mockup panels 8 and 10): each state through the incremental path. */
        {
            CtrPokeTouchView b2=s; memset(&old,0,sizeof(old)); old.selected=old.pressed=PT_NONE;
            b2.home=0; b2.box.on=1; b2.box.state=PT_BOX_STATE_BOX; b2.box.area=ST_AREA_BOX; b2.box.pos=7; b2.box.pressed=-1; b2.box.message[0]=0xff;
            for(int i=0;i<PT_BOX_SLOTS;i+=3) b2.box.slot[i].species=1+i;
            memset(b2.box.boxName,0xff,sizeof(b2.box.boxName)); memcpy(b2.box.boxName,"»¼½",3);
            update(&old,&b2); if(dir) ppm(dir,"box_cursor");
            b2.box.pos=8; update(&old,&b2); b2.box.area=ST_AREA_TITLE; update(&old,&b2);
            b2.box.area=ST_AREA_BUTTONS; b2.box.pos=1; update(&old,&b2); b2.box.pos=0; b2.box.pressed=ST_HIT_BUTTON_PARTY; update(&old,&b2); b2.box.pressed=-1;
            b2.box.area=ST_AREA_PARTY; b2.box.pos=2; for(int i=0;i<6;++i) b2.box.party[i].species=1+i; update(&old,&b2); if(dir) ppm(dir,"box_party");
        {
            CtrPokeTouchView b3=s; CtrPokeTouchView o3; memset(&o3,0,sizeof(o3)); o3.selected=o3.pressed=PT_NONE;
            b3.home=0; b3.bag.on=1; b3.bag.pocket=0; b3.bag.state=PT_BAG_STATE_LIST; b3.bag.count=9; b3.bag.cursor=1; b3.bag.pressed=-1;
            for(int i=0;i<PT_BAG_ROWS;++i) { b3.bag.row[i].item=13+i; b3.bag.row[i].qty=(uint16_t)(3+i*20); strcpy(b3.bag.row[i].name,"POTION"); }
            strcpy(b3.bag.selName,"POTION"); b3.bag.selItem=13; strcpy(b3.bag.selDesc,"Restores 20 HP.");
            update(&o3,&b3); if(dir) ppm(dir,"bag_list");
        }
            b2.box.pos=6; update(&old,&b2); b2.box.area=ST_AREA_BOX; b2.box.pos=3;
            b2.box.state=PT_BOX_STATE_MENU; b2.box.menuCount=4; b2.box.menuCursor=1; update(&old,&b2); if(dir) ppm(dir,"box_menu");
            b2.box.menuCursor=3; update(&old,&b2);
            b2.box.state=PT_BOX_STATE_YESNO; b2.box.menuCursor=1; update(&old,&b2); if(dir) ppm(dir,"box_yesno");
            b2.box.state=PT_BOX_STATE_CHOOSE; b2.box.chooseCount=12; update(&old,&b2); if(dir) ppm(dir,"box_choose");
            b2.box.state=PT_BOX_STATE_MARKS; b2.box.marks=5; b2.box.menuCursor=1; update(&old,&b2); b2.box.menuCursor=5; update(&old,&b2); if(dir) ppm(dir,"box_marks");
            b2.box.state=PT_BOX_STATE_BOX; b2.box.on=0; b2.name.on=1; b2.name.page=1; b2.name.cols=8; b2.name.maxChars=7; b2.name.len=3; b2.name.curCol=4; b2.name.curRow=1; b2.name.pressed=-1;
            memset(b2.name.key,0xff,sizeof(b2.name.key)); memset(b2.name.text,0xff,sizeof(b2.name.text));
            for(int r=0;r<3;++r) for(int c=0;c<8;++c) b2.name.key[r][c]=(uint8_t)(0xbb+r*8+c);
            b2.name.text[0]=0xbb; b2.name.text[1]=0xc5; b2.name.text[2]=0xbf; memset(b2.name.title,0xff,sizeof(b2.name.title));
            update(&old,&b2); if(dir) ppm(dir,"naming");
            b2.name.curCol=8; b2.name.curRow=0; update(&old,&b2); b2.name.curRow=2; update(&old,&b2); b2.name.pressed=PT_NAME_HIT_KEY; update(&old,&b2);
            b2.name.on=0; update(&old,&b2);
        }
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
        s.optSwitch[5]=1; s.optSegCount[6]=3; s.optSegValue[6]=2; strcpy(s.optSegName[6][0],"OFF"); strcpy(s.optSegName[6][1],"TRAIL"); strcpy(s.optSegName[6][2],"BE");
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
        /* The move to forget for a new one (Llama look): its rows, the cursor and a press move nothing but themselves. */
        {
            static const char *const mv[5]={"FLAMETHROWER","WING ATTACK","DRAGON CLAW","SLASH","HYPER BEAM"};
            static const char *const ty[5]={"FIRE","FLYING","DRAGON","NORMAL","NORMAL"};
            static const int8_t tn[5]={10,2,16,0,0};
            s.options=0; s.backVisible=0; memset(&s.learn,0,sizeof(s.learn)); s.learn.on=1; s.learn.pressed=-1; s.learn.species=6;
            strcpy(s.learn.nickname,"CHARIZARD"); strcpy(s.learn.level,"Lv50"); strcpy(s.learn.question,"Make it forget which move?");
            for(int i=0;i<PT_LEARN_ROWS;++i) { s.learn.row[i].move=(uint16_t)(i+1); s.learn.row[i].type=tn[i]; strcpy(s.learn.row[i].name,mv[i]); strcpy(s.learn.row[i].typeName,ty[i]); strcpy(s.learn.row[i].pp,"15/15"); strcpy(s.learn.row[i].power,"90"); strcpy(s.learn.row[i].accuracy,"100"); }
            strcpy(s.learn.desc,"A powerful fire attack that may burn the target.");
            s.learn.cursor=1; SHOT("learn");
            for(int i=0;i<PT_LEARN_ROWS;++i) { s.learn.cursor=(int8_t)i; update(&old,&s); s.learn.pressed=(int16_t)(PT_LEARN_HIT_ROW+i); update(&old,&s); s.learn.pressed=-1; update(&old,&s); }
            s.learn.cursor=4; SHOT("learn_new"); s.learn.on=0;
        }
        CtrPokeTouchView keepOpt=s;
        /* Llama: the Pokedex (menu, list, habitats, category, page) and the Trainer Card note; each through the incremental path (a cursor
         * move, a press, a page turn) and then written. */
        s.options=0; s.backVisible=1; s.backEnabled=1; memset(s.bar,0,sizeof(s.bar));
        {
            CtrPokeTouchDex *d=&s.dex; memset(d,0,sizeof(*d)); d->on=1; d->state=PT_DEX_STATE_MENU; d->pressed=-1; d->national=1;
            d->seen[0]=142; d->owned[0]=118; d->seen[1]=251; d->owned[1]=203;
            d->menuCount=8; d->menuCursor=1;
            d->menu[0].id=PT_DEX_ID_HEADER; d->menu[1].id=PT_DEX_ID_KANTO; strcpy(d->menu[1].name,"KANTO");
            d->menu[2].id=PT_DEX_ID_NATIONAL; strcpy(d->menu[2].name,"NATIONAL"); d->menu[3].id=PT_DEX_ID_CANCEL; strcpy(d->menu[3].name,"CLOSE POKEDEX");
            d->menu[4].id=PT_DEX_ID_HEADER; d->menu[5].id=0; strcpy(d->menu[5].name,"GRASSLAND POK\xC3\xA9MON"); d->menu[6].id=1; strcpy(d->menu[6].name,"FOREST POK\xC3\xA9MON");
            d->menu[7].id=2; d->menu[7].locked=1; strcpy(d->menu[7].name,"WATER'S EDGE POK\xC3\xA9MON");
            strcpy(s.bar[0],"KANTO"); strcpy(s.bar[1],"Your POKEMON by number");
            SHOT("dex_menu");
            d->menuCursor=2; update(&old,&s); d->pressed=PT_DEX_HIT_ITEM+2; update(&old,&s); d->pressed=-1; d->menuCursor=3; update(&old,&s); SHOT("dex_menu_close");
            d->menuCursor=5; SHOT("dex_habitats"); d->menuCursor=7; update(&old,&s); d->menuCursor=6; update(&old,&s);
            d->state=PT_DEX_STATE_LIST; d->listCount=151; d->listFirst=1; d->listCursor=3; d->order=0;
            static const char *const nm[PT_DEX_ROWS]={"IVYSAUR","VENUSAUR","CHARMANDER","CHARMELEON","CHARIZARD"};
            for(int i=0;i<PT_DEX_ROWS;++i) { d->row[i].number=(uint16_t)(2+i); d->row[i].species=(uint16_t)(2+i); d->row[i].flags=(uint8_t)(i==4?1:3); strcpy(d->row[i].name,nm[i]); }
            d->sel=d->row[2]; d->type[0]=9; d->type[1]=-1; strcpy(d->typeName[0],"FIRE");
            SHOT("dex_list");
            d->listCursor=4; d->type[1]=2; strcpy(d->typeName[1],"FLYING"); d->sel=d->row[3]; update(&old,&s);
            d->pressed=PT_DEX_HIT_ROW+1; update(&old,&s); d->pressed=-1; d->listFirst=0; update(&old,&s);
            d->state=PT_DEX_STATE_CATEGORY; d->slots=4; d->slot=1; d->canPrev=0; d->canNext=1; d->page=1; d->pages=2;
            for(int i=0;i<4;++i) { d->mon[i]=d->row[i]; }
            SHOT("dex_category"); d->slot=2; update(&old,&s); d->pressed=PT_DEX_HIT_SLOT+3; update(&old,&s); d->pressed=-1;
            d->state=PT_DEX_STATE_PAGE; d->sel=d->row[4]; d->sel.flags=3; d->height=17; d->weight=905; strcpy(d->kind,"FLAME POK\xC3\xA9MON");
            strcpy(d->desc,"Spits fire that is hot enough to melt boulders. Known to cause forest fires unintentionally.");
            SHOT("dex_page"); d->pressed=PT_DEX_HIT_AREA; update(&old,&s); d->pressed=-1; d->added=1; update(&old,&s); d->added=0;
        }
        s.dex.on=0;
        {
            CtrPokeTouchCard *c=&s.card; memset(c,0,sizeof(*c)); c->on=1; strcpy(c->name,"RICK"); strcpy(c->label[0],"NAME: "); strcpy(c->label[1],"IDNo."); strcpy(c->label[2],"MONEY");
            strcpy(c->label[3],"\xC2\xA5"); strcpy(c->label[4],"POK\xC3\xA9" "DEX"); strcpy(c->label[5],"TIME"); c->id=12345; c->money=128450; c->hasDex=1; c->caught=118; c->hours=47; c->minutes=5; c->stars=2; c->badges=0x1f;
            strcpy(s.bar[0],"TRAINER CARD"); strcpy(s.bar[1],"Tap the card to turn it.");
            SHOT("card_front"); c->pressed=1; update(&old,&s); c->pressed=0; c->back=1; c->rows=2; strcpy(c->row[0].label,"HALL OF FAME"); strcpy(c->row[0].value,"3"); strcpy(c->row[1].label,"LINK BATTLES"); strcpy(c->row[1].value,"0/0"); update(&old,&s);
            c->on=0;
        }
        s=keepOpt;
        /* The look page: three look rows and the preview card. */
        s.optPage=3; strcpy(s.optPageName,"LOOKS"); s.optCount=4; memset(s.optSwitch,0,sizeof(s.optSwitch)); memset(s.optSegCount,0,sizeof(s.optSegCount));
        strcpy(s.optLabel[0],"EXP SHARE"); s.optSegCount[0]=3; s.optSegValue[0]=1; strcpy(s.optSegName[0][0],"OFF"); strcpy(s.optSegName[0][1],"HALF"); strcpy(s.optSegName[0][2],"ALL");
        strcpy(s.optLabel[1],"RED LOOK"); strcpy(s.optValue[1],"GOLD HGSS"); strcpy(s.optLabel[2],"LEAF LOOK"); strcpy(s.optValue[2],"MAY HGSS"); strcpy(s.optLabel[3],"RIVAL LOOK"); strcpy(s.optValue[3],"SILVER");
        s.optSelected=2; s.optLook.on=1; s.optLook.has=1; strcpy(s.optLook.name,"MAY HGSS"); strcpy(s.optLook.credit,"Made by DiegoWT");
        for(int i=0;i<512;++i) s.optLook.tiles[i]=(uint8_t)(i*7);
        for(int i=1;i<16;++i) s.optLook.palette[i]=(uint16_t)(i*0x0421);
        strcpy(s.bar[0],"LEAF LOOK"); strcpy(s.bar[1],"How Leaf looks."); strcpy(s.bar[2],"Looks from the Builder.");
        SHOT("options_look");
        /* The chosen row moves over the rows above the window (its ring lies on its neighbours, and on the 2 px gap above the window). */
        for(int row=0;row<4;++row) { s.optSelected=(uint8_t)row; update(&old,&s); s.optPressed=(int8_t)row; s.optZone=PT_OPT_ZONE_NEXT; update(&old,&s); s.optPressed=-1; update(&old,&s); }
        s.optSelected=3; update(&old,&s); s.optSelected=0; update(&old,&s); s.optSelected=2; update(&old,&s);
        /* SAVE: the four states of the mockup (the incremental path first: each must equal a full redraw). */
        memset(&old,0,sizeof(old)); old.selected=old.pressed=PT_NONE;
        s.options=0; s.optCount=0; s.optPage=0; s.optPageCount=0; s.optSelected=0; s.optLook.on=0; s.backVisible=0; s.backEnabled=0;
        memset(s.optSwitch,0,sizeof(s.optSwitch)); memset(s.optSegCount,0,sizeof(s.optSegCount)); memset(s.optLabel,0,sizeof(s.optLabel)); memset(s.optValue,0,sizeof(s.optValue));
        s.save=1; s.saveBadges=5; s.saveDex=87; s.saveHasDex=1; s.saveHours=42; s.saveMinutes=7; s.saveMoney=123456; s.saveSel=1; s.savePressed=PT_SAVE_NONE;
        s.playerFemale=0; s.saveState=SAVE_CONFIRM; strcpy(s.bar[0],"Save your game?"); s.bar[1][0]=0; s.bar[2][0]=0;
        SHOT("save_confirm");
        s.saveSel=0; SHOT("save_confirm_yes"); s.savePressed=PT_SAVE_FIRST; SHOT("save_confirm_yes_pressed"); s.savePressed=PT_SAVE_NONE; s.saveSel=1;
        s.saveState=SAVE_WRITING; strcpy(s.bar[0],"Saving..."); strcpy(s.bar[1],"Don't turn off the power."); SHOT("save_writing");
        s.saveState=SAVE_SUCCESS; strcpy(s.bar[0],"Game saved!"); s.bar[1][0]=0; SHOT("save_saved");
        s.saveState=SAVE_ERROR; strcpy(s.bar[0],"Could not save."); strcpy(s.bar[1],"Try again."); SHOT("save_error");
        s.saveMinutes=8; s.saveMoney=999999; s.saveBadges=8; s.playerFemale=1; strcpy(s.playerName,"Leaf"); update(&old,&s); SHOT("save_error_changed");
        /* A choice of the field (3ds_field_choice_impl.h): flame row with the ring and cursor, the question in the window; the cursor moves incrementally. */
        memset(&old,0,sizeof(old)); old.selected=old.pressed=PT_NONE;
        s.save=0; s.saveState=0; s.viewport=0; s.backVisible=0; s.choice.on=1; s.choice.count=2; s.choice.cols=0; s.choice.cursor=0; s.choice.pressed=-1;
        strcpy(s.choice.label[0],"YES"); strcpy(s.choice.label[1],"NO"); strcpy(s.choice.prompt,"Would you like to buy this?");
        strcpy(s.bar[0],""); s.bar[1][0]=0;
        update(&old,&s); SHOT("choice_yesno"); s.choice.cursor=1; update(&old,&s); s.choice.pressed=PT_CHOICE_HIT_ROW; update(&old,&s); s.choice.pressed=-1; s.choice.cursor=0; update(&old,&s);
        s.choice.count=6; for(int k=0;k<6;++k) snprintf(s.choice.label[k],sizeof(s.choice.label[k]),"CHOICE %d",k+1);
        s.choice.cursor=3; update(&old,&s); SHOT("choice_six"); s.choice.cursor=5; update(&old,&s); s.choice.on=0;
        (void)before;
    }
    v.save=0; v.home=1; v.backVisible=1; update(&old,&v);
    v.backEnabled=1; update(&old,&v); v.backPressed=1; update(&old,&v);
#ifndef PT_BASELINE
    {
        /* HOME's pieces over the map (the Llama mockup's): the region ribbon, the player's icon, the chosen destination's ring and its name on a window.
         * Every move, blink and rename goes through the incremental path, which update() compares with a full redraw. */
        CtrPokeTouchView h={0}; CtrPokeTouchRect ring, label;
        int cells[][2]={{10,8},{11,8},{11,5},{0,0},{21,14},{21,0},{0,14},{5,7}};
        iconArt=1; sPlayer.key=sCursor.key=-1; sMap.group=-1;
        h.selected=h.pressed=PT_NONE; h.optPressed=-1; h.home=1; h.playerX=h.playerY=h.cursorX=h.cursorY=-1;
        for(int i=0;i<PT_COUNT;++i) h.enabled[i]=1;
        strcpy(h.playerName,"Rick"); strcpy(h.bar[0],"TEST MAP");
        memset(&old,0,sizeof(old)); old.selected=old.pressed=PT_NONE; old.optPressed=-1;
        update(&old,&h);
        h.playerX=5; h.playerY=5; update(&old,&h);                                      /* the player appears */
        h.cursorX=10; h.cursorY=8; strcpy(h.bar[0],"VERMILION CITY"); update(&old,&h);    /* a destination: ring, icon, name */
        assert(RingRect(&h,&ring) && ring.w==24 && ring.h==24 && LabelRect(&h,&label) && label.h==21 && label.w==UiTextWidth("VERMILION CITY",1)+18);
        assert(label.y+label.h+3==ring.y);                                               /* 3 px above the ring */
        assert(at(ring.x+12,ring.y)==LL_CURSOR || at(ring.x+11,ring.y)==LL_CURSOR);        /* the ring's red, at its top */
        assert(at(ring.x+12,ring.y+12)==Gba(0x7c00));                                    /* the cursor's icon in it */
        if(getenv("PT_PREVIEW_DIR")) ppm(getenv("PT_PREVIEW_DIR"),"home_cursor");
        h.cursorFrame=1; update(&old,&h); h.cursorFrame=0; update(&old,&h);              /* the cursor blinks */
        for(unsigned i=0;i<sizeof(cells)/sizeof(cells[0]);++i) {
            h.cursorX=(int8_t)cells[i][0]; h.cursorY=(int8_t)cells[i][1]; update(&old,&h);
            h.playerX=(int8_t)cells[(i+2)%(sizeof(cells)/sizeof(cells[0]))][0]; h.playerY=(int8_t)cells[(i+2)%(sizeof(cells)/sizeof(cells[0]))][1]; update(&old,&h);
            if(LabelRect(&h,&label)) { /* the name's window stays inside the map */
                assert(label.x>=PT_VIEW_X && label.y>=PT_VIEW_Y && label.x+label.w<=PT_VIEW_X+PT_VIEW_W && label.y+label.h<=PT_VIEW_Y+PT_VIEW_H);
            }
        }
        h.cursorX=3; h.cursorY=3; update(&old,&h);
        h.playerX=3; h.playerY=2; update(&old,&h);                                       /* the player under the name's window and the ring */
        strcpy(h.bar[0],"CELADON CITY, A NAME TOO LONG TO FIT ON THE MAP'S WINDOW"); update(&old,&h);
        strcpy(h.bar[0],"SAFFRON CITY"); update(&old,&h);                                 /* renamed: the old window goes, the new one comes */
        h.playerFemale=1; update(&old,&h);
        h.cursorX=h.cursorY=-1; update(&old,&h);                                         /* the destination goes: ring and window with it */
        h.mapGroup=PT_MAP_SEVII123; h.cursorX=4; h.cursorY=4; update(&old,&h);          /* another map: SEVII on the ribbon */
        iconArt=0; sPlayer.key=sCursor.key=-1; sMap.group=-1;
    }
#endif
#ifndef PT_BASELINE
    {
        /* Llama (the owner's mockup, llama2_mockup.html): the shared pieces of 3ds_ember2_impl.h. Each is drawn on a canvas of a marker colour
         * and must stay inside its rectangle plus the 2 px a piece owns (the canvas redraws only what changed); the ones that do not blend
         * must draw the same twice; colours and shapes are spot-checked against the mockup's CSS. PT_PREVIEW_DIR gets a sheet of them. */
        enum { MARK = 0x1357 };
        const char *dir=getenv("PT_PREVIEW_DIR");
        #define HEX(h) UI_RGB(((h)>>16)&255,((h)>>8)&255,(h)&255)
        #define SENTINEL() do { sCanvas=canvas; for(int i=0;i<W*H;++i) canvas[i]=MARK; } while(0)
        /* Everything outside (x, y, w, h) grown by `grow` is still the marker; returns how many pixels inside it changed. */
        #define CHANGED(x,y,w,h,grow,name) llChanged(x,y,w,h,grow,name)
        struct { const EmPal *p; const char *name; unsigned g1,g2,f1,f2,lip,hl,ts; } pals[] = {
            {&kEmCream,"cream",0xffffff,0xfffdf4,0xfaf4e2,0xece2c6,0xb8a888,0xffffff,0},   /* ts of the cream is the text shadow EM_DARK_SH */
            {&kEmFlame,"flame",0xffc45a,0xff9a38,0xf8642a,0xd8301e,0x8a1610,0xffe9a8,0x7a1410},
            {&kEmBlue,"blue",0xbcdcff,0x86b8ff,0x5c9af4,0x2e68d2,0x1a46a0,0xe0f0ff,0x0e2460},
            {&kEmGold,"gold",0xfff2c0,0xffe07c,0xf8c63e,0xe0a018,0xa06c08,0xfffae0,0x603c04},
            {&kEmGreen,"green",0xccf4b8,0x94de80,0x66c660,0x2c9a48,0x1a6e2e,0xe8ffe0,0x0c4018},
            {&kEmPurple,"purple",0xe6d8ff,0xc6acff,0xa888f2,0x7a58de,0x5238a0,0xf2eaff,0x2c1c60},
            {&kEmSlate,"slate",0xaab6dc,0x8c9aca,0x7080b6,0x4c5a8c,0x2c3558,0xd0d8f0,0x20263f},
            {&kEmPartyBlue,"party blue",0xc4ecff,0x8ed4fa,0x60b8f0,0x3c92dc,0x1e5ea6,0xe8f8ff,0x184a86},
            {&kEmPartyRed,"party red",0xffd0c8,0xf8a498,0xe87c74,0xc85050,0x8a2c30,0xffe8e4,0x6a1c22},
        };
        /* The palettes are the mockup's .cr .fl .bl .go .gr .pu .sl .pb .pr, outline --o #2a2238 on every one. */
        for(unsigned i=0;i<sizeof(pals)/sizeof(pals[0]);++i) {
            const EmPal *p=pals[i].p;
            if(pals[i].name[0]=='c' && pals[i].name[1]=='r') assert(p->inkSh==HEX(0xd0d0c8) && p->ink==HEX(0x484848));
            else assert(p->inkSh==HEX(pals[i].ts) && p->ink==EM_WHITE);
            assert(p->g1==HEX(pals[i].g1) && p->top==HEX(pals[i].g2) && p->f1==HEX(pals[i].f1) && p->bottom==HEX(pals[i].f2));
            assert(p->lip==HEX(pals[i].lip) && p->hl==HEX(pals[i].hl) && p->edge==HEX(0x2a2238));
        }
        assert(EM_DIM==HEX(0x807468) && LlPick(1,&kEmCream)==&kEmFlame && LlPick(0,&kEmCream)==&kEmCream);
        /* Pieces that must stay in their rectangle (+ the 2 px they own), and change something in it. */
        #define PIECE(x,y,w,h,grow,name,call) do { SENTINEL(); call; assert(CHANGED(x,y,w,h,grow,name)>0); } while(0)
        #define STILL(x,y,w,h,grow,name,call) do { PIECE(x,y,w,h,grow,name,call); memcpy(reference,canvas,sizeof(canvas)); call; \
            assert(!memcmp(reference,canvas,sizeof(canvas)) && name " must draw the same twice"); } while(0)
        STILL(20,20,100,60,2,"LlWindow",LlWindow(20,20,100,60));
        STILL(20,20,100,60,0,"LlWindowPlain",LlWindowPlain(20,20,100,60));
        STILL(30,40,50,1,0,"LlRule",LlRule(30,40,50));
        STILL(82,32,216,36,0,"LlHpBox",LlHpBox(82,32,216,36));
        STILL(10,10,136,9,0,"LlHpBar",LlHpBar(10,10,136,319,319));
        STILL(10,10,136,9,0,"LlHpBar half",LlHpBar(10,10,136,100,319));
        STILL(10,10,136,9,0,"LlHpBar low",LlHpBar(10,10,136,1,319));
        STILL(10,10,136,9,0,"LlHpBar empty",LlHpBar(10,10,136,0,319));
        { int w=0; PIECE(20,20,64,13,0,"LlRibbon",w=LlRibbon(20,20,"KANTO",LL_RIB_TOP,LL_RIB_BOT)); assert(w==UiTextWidth("KANTO",1)+18); }
        STILL(20,20,40,11,0,"LlRibbonBox",LlRibbonBox(20,20,40,11,7,LL_TL,LL_BLUE_TOP,LL_BLUE_BOT,"X"));
        STILL(20,20,40,11,2,"LlRibbonBox BL",LlRibbonBox(20,20,40,11,7,LL_BL,LL_RIB_TOP,LL_RIB_BOT,"1\xC2\xBA"));
        { int w=0; PIECE(20,20,80,13,0,"LlTypeBadge",w=LlTypeBadge(20,20,"TIERRA",UI_RGB(200,160,72))); assert(w==UiTextWidth("TIERRA",1)+12); }
        STILL(20,20,7,11,0,"LlCursor",LlCursor(20,20,LL_CURSOR));
        STILL(20,20,8,5,0,"LlArrow up",LlArrow(20,20,8,5,0,EM_WHITE));
        STILL(20,20,8,5,0,"LlArrow down",LlArrow(20,20,8,5,1,EM_WHITE));
        STILL(62,5,196,25,0,"LlBallTray",LlBallTray(62,5,196,25));
        STILL(150,150,24,24,0,"LlBackDisc",LlBackDisc(150,150));
        STILL(150,150,18,4,0,"LlTrayBar",LlTrayBar(150,150,18,10,100));
        STILL(150,150,15,14,0,"LlNub",LlNub(150,150,15,14,-1,0));
        STILL(150,150,15,14,0,"LlNub pressed",LlNub(150,150,15,14,1,1));
        STILL(150,150,40,14,0,"LlFlameFace",LlFlameFace(150,150,40,14,3,0));
        STILL(150,150,10,68,0,"LlScrollRail",LlScrollRail(150,150,10,68,3,28));
        { static const char *const names[3]={"OFF","LOW","HIGH"}; STILL(150,150,118,14,0,"LlSegments",LlSegments(150,150,118,14,names,3,2,1)); }
        /* HOME / OPTIONS' own pieces (3ds_poketouch.c): the mockup's switch, 30 x 15 with the knob at the right when on and at the left when off; the value box. */
        STILL(150,150,30,15,0,"LhSwitch on",LhSwitch(150,150,1));
        { uint16_t k=0, g; SENTINEL(); LhSwitch(150,150,1); g=at(150+6,150+11); k=at(150+22,150+7);
          assert(((g>>5)&63)*4>(g>>11)*8+40 && (k>>11)>26 && (k&31)>26);   /* green track on the left, the pale knob on the right */
          SENTINEL(); LhSwitch(150,150,0); g=at(150+22,150+11); k=at(150+7,150+7);
          assert(abs((int)(g>>11)*8-(int)(g&31)*8)<24 && (k>>11)>26 && (k&31)>26); }   /* grey track on the right, the knob on the left */
        STILL(150,150,30,15,0,"LhSwitch off",LhSwitch(150,150,0));
        PIECE(150,150,120,17,0,"LhValue",LhValue(150,150,120,16,"RELOJ",0));
        PIECE(150,150,120,17,0,"LhValue long",LhValue(150,150,120,16,"A VALUE FAR TOO LONG TO FIT BETWEEN THE NUBS",2));
        PIECE(150,150,150,26,0,"LhPill",LhPill(150,150,150,26,(uint16_t[26]){0}));
        STILL(150,150,30,30,1,"LlPlate",LlPlate(150,150,30,UI_RGB(64,72,104),0,0));
        PIECE(146,146,38,38,0,"LlPlate ring",LlPlate(150,150,30,UI_RGB(24,74,134),UI_RGB(232,248,255),1));   /* a px of edge, one of band, a px of edge: 3 px out */
        PIECE(146,146,50,50,0,"LlPlate ring 2",LlPlate(150,150,42,UI_RGB(138,22,16),UI_RGB(255,216,144),2));
        PIECE(150,150,9,5,0,"LlMiniText",assert(LlMiniText("PS",150,150,EM_WHITE)==9));
        /* Pieces that tint what is there (the marker colour here): inside their rectangle (+ the light row under them), no more. */
        PIECE(150,150,22,23,0,"LlLeadFrame",LlLeadFrame(150,150,22,23));
        PIECE(150,150,14,14,0,"LlTraySlot",LlTraySlot(150,150));
        PIECE(150,150,100,16,0,"LlWell",LlWell(150,150,100,15));
        PIECE(150,150,59,16,0,"LlSubPill",LlSubPill(150,150,59,15));
        /* The ring of a chosen card: round it only (nothing inside its rectangle), 2 px thick, the two colours in order, on any size. */
        {
            static const int sizes[][2]={{304,73},{150,50},{72,34},{31,39},{20,18}};
            for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
                int w=sizes[i][0], h=sizes[i][1], x=6, y=6, ring=0;
                SENTINEL(); LlSelectRing(x,y,w,h);
                for(int cy=0;cy<H;++cy) for(int cx=0;cx<W;++cx) {
                    int inside=cx>=x&&cx<x+w&&cy>=y&&cy<y+h, near=cx>=x-2&&cx<x+w+2&&cy>=y-2&&cy<y+h+2;
                    /* inside the card's box the rounded corners leave a few pixels empty: the ring follows the curve there, so only the shape itself must be untouched */
                    if((inside&&Em2In(cx-x,cy-y,w,h,5))||!near) assert(at(cx,cy)==MARK);
                    else if(at(cx,cy)!=MARK) { ++ring; assert(at(cx,cy)==LL_RING_IN||at(cx,cy)==LL_RING_OUT); }
                }
                assert(ring>0);
                assert(at(x-1,y+h/2)==LL_RING_IN && at(x-2,y+h/2)==LL_RING_OUT && at(x+w,y+h/2)==LL_RING_IN && at(x+w+1,y+h/2)==LL_RING_OUT);
                assert(at(x+w/2,y-1)==LL_RING_IN && at(x+w/2,y-2)==LL_RING_OUT && at(x+w/2,y+h)==LL_RING_IN && at(x+w/2,y+h+1)==LL_RING_OUT);
                assert(at(x-1,y-1)==MARK && at(x-2,y-2)==MARK);                   /* the corners are rounded: nothing in the very corner */
                memcpy(reference,canvas,sizeof(canvas)); LlSelectRing(x,y,w,h); assert(!memcmp(reference,canvas,sizeof(canvas)));
            }
        }
        /* The shapes: the HP box's left edge leans (its top row starts 9 px in, its bottom row at 0), the ribbon's right edge leans 7 px,
         * the tray's two ends lean 9 px; the outline is the slate, the bar's fill widths follow the HP. */
        SENTINEL(); LlHpBox(40,40,216,36);
        assert(at(40+8,40)==MARK && at(40+9,40)==LL_SLATE && at(40,40+35)==LL_SLATE && at(40+215,40)==LL_SLATE && at(40+215,40+35)==LL_SLATE);
        assert(at(40,40)==MARK && at(40+4,40+18)!=MARK && at(40+9,40+18)!=LL_SLATE && at(40+108,40+18)!=MARK && at(40+108,40+18)!=LL_SLATE);
        SENTINEL(); LlRibbon(40,40,"STAB",LL_RIB_TOP,LL_RIB_BOT);
        { int w=UiTextWidth("STAB",1)+18; assert(at(40+w-1,40)!=MARK && at(40+w-1,40+12)==MARK && at(40+w-8,40+12)!=MARK && at(40+w-7,40+12)==MARK && at(40,40)!=MARK && at(40,40+12)!=MARK); }
        SENTINEL(); LlRibbonBox(40,40,40,11,7,LL_TL|LL_BL,LL_RIB_TOP,LL_RIB_BOT,"");
        assert(at(40,40)==MARK && at(40,40+10)==MARK && at(40+4,40)!=MARK && at(40,40+5)!=MARK);   /* the rounded corners are cut */
        SENTINEL(); LlBallTray(62,5,196,25);
        assert(at(62+9,5)==LL_SLATE_DK && at(62+8,5)==MARK && at(62,5+24)==LL_SLATE_DK && at(62+195,5)==LL_SLATE_DK && at(62+195-9,5+24)==LL_SLATE_DK && at(62+195-8,5+24)==MARK);
        assert(at(62+98,5+12)!=MARK && at(62+98,5+12)!=LL_SLATE_DK);
        /* The window: outline, white, two px of sky blue, a px of light blue, then the paper. */
        SENTINEL(); LlWindow(20,20,100,60);
        assert(at(20+50,20)==LL_SLATE && at(20+50,21)==EM_WHITE && at(20+50,22)==HEX(0x7ab4d8) && at(20+50,23)==HEX(0x7ab4d8) && at(20+50,24)==HEX(0xc8e4f0));
        assert(at(20,20+30)==LL_SLATE && at(21,20+30)==EM_WHITE && at(22,20+30)==HEX(0x7ab4d8) && at(24,20+30)==HEX(0xc8e4f0) && at(20+50,25)==EM_WHITE);
        assert(at(20+50,20+60+0)!=MARK && at(20+50,20+60+1)!=MARK && at(20+50,20+60+2)==MARK);         /* its shadow: two rows */
        /* The HP bar: the fill's width follows the HP (green over half, yellow over a fifth, red below) and is clipped by the track's ends. */
        {
            int tx=10+15, ty=12, tw=136-17;
            SENTINEL(); LlHpBar(10,10,136,319,319);
            assert(at(tx+5,ty+3)==HEX(0x30c848) && at(tx+5,ty)==HEX(0xa8f8b8) && at(tx+tw-1,ty+4)==LL_SLATE && at(tx+tw-1,ty+3)==HEX(0x30c848) && at(tx,ty)==LL_SLATE);
            SENTINEL(); LlHpBar(10,10,136,100,319);                         /* 31 percent: yellow, 37 px of a 119 px track */
            assert(at(tx+36,ty+3)==HEX(0xe8b820) && at(tx+37,ty+3)==HEX(0x586080) && at(tx+5,ty)==HEX(0xf8e890));
            SENTINEL(); LlHpBar(10,10,136,1,319);                           /* a sliver is one pixel, red */
            assert(at(tx,ty+3)==HEX(0xe03828) && at(tx+1,ty+3)==HEX(0x586080));
            SENTINEL(); LlHpBar(10,10,136,0,319);
            assert(at(tx,ty+3)==HEX(0x586080) && at(tx+5,ty+3)==HEX(0x586080));
            assert(at(10+3,10+2)==HEX(0xf8b020) || at(10+4,10+2)==HEX(0xf8b020));   /* the orange label */
        }
        /* Text that does not fit ends in "..." (Em2Fit): the ribbon never paints past its slant. */
        SENTINEL(); LlRibbonBox(40,40,30,13,7,0,LL_RIB_TOP,LL_RIB_BOT,"A VERY LONG RIBBON");
        for(int cy=0;cy<H;++cy) for(int cx=70;cx<W;++cx) assert(at(cx,cy)==MARK);
        /* The cursor: red, with the shadow one px right and below; white on a chosen row. */
        SENTINEL(); LlCursor(40,40,EM_WHITE);
        assert(at(40,45)==EM_WHITE && at(40,41)==EM_WHITE && at(40,50)==LL_CURSOR_SH && at(46,45)==LL_CURSOR_SH);
        if(dir) {
            /* A sheet of the pieces, laid out as the mockup lays them (panels 1, 2 and 4), for a look next to the mockup's pictures. */
            static const char *const types[]={"TIERRA","FUEGO","DRAGON","NORMAL"};
            const uint16_t tcol[4]={UI_RGB(200,160,72),UI_RGB(240,128,48),UI_RGB(112,56,248),UI_RGB(168,168,120)};
            memset(sPtFontReady,0,sizeof(sPtFontReady));
            sCanvas=canvas; TsBackdrop();
            /* panel 1: the tray, the FIGHT card, four action cards */
            LlBallTray(62,5,196,25);
            LlLeadFrame(62+13,5+1,22,23);
            for(int i=0;i<5;++i) { LlTrayBar(62+15+28*i,5+18,18,i==3?0:i==2?44:i==1?82:100,100); if(i==4) LlTraySlot(62+157,5+5); }
            EmButton(8,36,304,73+2,&kEmFlame,3,0); LlSelectRing(8,36,304,75);
            LlPlate(8+9+1,36+4+1,63,kEmFlame.lip,UI_RGB(255,216,144),2);
            UiText("LUCHAR",8+88,36+7,EM_WHITE,kEmFlame.inkSh,1,300);
            LlTypeBadge(8+170,36+8,"TIERRA",UI_RGB(200,160,72));
            LlHpBox(8+82,36+32,216,36);
            UiText("Groudon",8+82+16,36+32+4,EM_DARK,EM_DARK_SH,1,300);
            UiText("Nv100",8+82+164,36+32+4,EM_DARK,EM_DARK_SH,1,300);
            LlHpBar(8+82+13,36+32+20,136,319,319);
            UiText("319/319",8+82+156,36+32+18,EM_DARK,EM_DARK_SH,1,300);
            { const EmPal *ps[4]={&kEmBlue,&kEmGold,&kEmGreen,&kEmPurple}; const char *nm[4]={"MOCHILA","POKE BALL","POKEMON","HUIR"};
              for(int i=0;i<4;++i) { int x=8+77*i; EmButton(x,120,73,108,ps[i],3,0); EmLabel(nm[i],x,73,120+61-3+3,9,ps[i]); LlSubPill(x+7,120+85,59,15); UiText("OBJETOS",x+13,120+88,EM_WHITE,ps[i]->inkSh,1,x+66); } }
            ppm(dir,"llama_battle");
            TsBackdrop();
            /* panel 2: the tray, four move cards (the first chosen), a window with a rule and a ribbon, CANCEL with the back disc */
            LlBallTray(62,5,196,25); LlLeadFrame(62+13,5+1,22,23);
            for(int i=0;i<4;++i) {
                int x=i%2?162:8, y=i<2?36:93;
                EmButton(x,y,150,50+3,&kEmCream,3,0);
                if(i==0) LlSelectRing(x,y,150,53);
                UiRoundFill(x+1,y+1,6,49,5,tcol[i]);
                UiText(i==0?"TERREMOTO":i==1?"LANZALLAMAS":i==2?"GARRA DRAGON":"DANZA ESPADA",x+(i==0?21:13),y+5,EM_DARK,EM_DARK_SH,1,x+110);
                if(i==0) LlCursor(x+11,y+6,LL_CURSOR);
                LlTypeBadge(x+150-6-UiTextWidth(types[i],1)-12,y+5,types[i],tcol[i]);
                UiText("POT 100  PRE 100",x+13,y+20,EM_DIM,EM_DARK_SH,1,x+140);
            }
            LlWindow(7,153,228,80);
            UiText("TERREMOTO",8+12,154+9,EM_DARK,EM_DARK_SH,1,200);
            LlRibbon(8+160,154+8,"STAB",LL_RIB_TOP,LL_RIB_BOT);
            LlRule(8+12,154+25,202);
            UiText("Un terremoto que golpea a",8+12,154+31,EM_DIM,EM_DARK_SH,1,230);
            EmButton(240,154,72,75+3,&kEmSlate,3,0); LlBackDisc(240+24,154+14-1);
            EmLabel("CANCELAR",240,72,154+46,9,&kEmSlate);
            ppm(dir,"llama_moves");
            TsBackdrop();
            /* panel 4: the party's six cards (blue, the last fainted, one empty), the note and CANCEL */
            for(int i=0;i<6;++i) {
                int x=i%2?162:8, y=6+53*(i/2), dead=i==3;
                const EmPal *p=dead?&kEmPartyRed:&kEmPartyBlue;
                if(i==5) { UiRoundFill(x,y,150,51,5,UI_RGB(34,6,6)); continue; }
                EmButton(x,y,150,48+3,p,3,0);
                if(i==0) LlSelectRing(x,y,150,51);
                LlPlate(x+5+1,y+4+1,38,p->inkSh,EM_WHITE,1);
                UiText(i==0?"GROUDON":i==1?"CHARIZARD":i==2?"PIKACHU":"ONIX",x+52,y+4,EM_WHITE,p->inkSh,1,x+100);
                LlHpBar(x+52,y+19,92,dead?0:i==2?31:100,i==2?70:100);
                if(i==0) LlRibbonBox(x+1,y+37,UiTextWidth("1\xC2\xBA",1)+18,11,7,LL_BL,LL_RIB_TOP,LL_RIB_BOT,"1\xC2\xBA");
                if(i==2) LlTypeBadge(x+52,y+31,"PAR",UI_RGB(216,168,24));
            }
            LlWindow(7,169,228,65);
            UiText("Elige un POKEMON.",8+12,170+9,EM_DARK,EM_DARK_SH,1,220);
            LlRule(8+12,170+25,202);
            EmButton(240,170,72,60+3,&kEmSlate,3,0); LlBackDisc(240+24,170+8-1);
            EmLabel("CANCELAR",240,72,170+36,9,&kEmSlate);
            ppm(dir,"llama_party");
        }
        #undef PIECE
        #undef STILL
        #undef SENTINEL
        #undef CHANGED
        #undef HEX
    }
#endif
    if(viewports) fclose(viewports);
    if(realPack) fclose(realPack);
    free(realIndex);
    puts("PASS real renderer: decoder, 4bpp, geometry, viewport, incremental states, X/Y/RUN, save/back hit-tests");
    return 0;
}
