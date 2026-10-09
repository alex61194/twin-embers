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
unsigned CtrTwinFontWidth(unsigned code) { (void)code; return 5; }

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
                fprintf(stderr,"redraw mismatch at %d,%d selected=%u pressed=%u save=%u/%u back=%u\n",
                        x,y,v->selected,v->pressed,v->save,v->saveState,v->backVisible);
                assert(reference[n]==canvas[n]);
            }
        }
    }
#endif
    *old = *v;
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
            {248,2,70,25},{248,30,70,25},{248,58,70,25},{248,86,70,25},
            {248,114,70,25},{248,142,70,25},{248,170,34,33},{284,170,34,33},{248,205,70,33}};
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
    for(int x=4;x<244;++x) for(int y=4;y<164;++y) assert(at(x,y)==Gba(0x5ab3));
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
    v.save=0; v.home=1; v.backVisible=1; update(&old,&v);
    v.backEnabled=1; update(&old,&v); v.backPressed=1; update(&old,&v);
    if(viewports) fclose(viewports);
    if(realPack) fclose(realPack);
    free(realIndex);
    puts("PASS real renderer: decoder, 4bpp, geometry, viewport, incremental states, X/Y/RUN, save/back hit-tests");
    return 0;
}
