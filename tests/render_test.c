/* Production raycaster/hit/scaler code with an in-memory pixel sink. */
#include "WL_DEF.H"
#include "wl_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
int viewwidth = 64, viewheight = 64, centerx = 32, scale = 64;
int32_t heightnumerator = 65536;
byte tilemap[MAPSIZE][MAPSIZE], spotvis[MAPSIZE][MAPSIZE];
unsigned doorposition[MAXDOORS], pwallpos;
doorobj_t doorobjlist[MAXDOORS];
word PMSpriteStart = 16, PMSoundStart = 17;
PageListStruct *PMPages;
static PageListStruct pageinfo[17];
static byte wall[4096], sprite[128], pixels[64][64];
extern int midangle, focaltx, focalty, xtile, ytile, xtilestep, ytilestep, lastside;
extern unsigned xpartialup, xpartialdown, ypartialup, ypartialdown, tilehit;
extern int32_t xintercept, yintercept, xstep, ystep;
void AsmRefresh(void), US_InitRndT(boolean), MapRow(void);
extern int mr_rowofs, mr_count, mr_xstep, mr_ystep, mr_xfrac, mr_yfrac, mr_dest;
extern char planepics[8192];
objtype *actorat[MAPSIZE][MAPSIZE];
word *mapsegs[MAPPLANES];
unsigned farmapylookup[MAPSIZE];
static word mapplane[4096];
static int opened = -1;
void OpenDoor(int n) { opened = n; }
void Quit(char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}
void VL_Plot(int x, int y, int color) {
    CHECK(x >= 0 && x < viewwidth && y >= 0 && y < viewheight);
    pixels[y][x] = color;
}
memptr PM_GetPage(int n) {
    CHECK(n >= 0 && n <= 16);
    return n == 16 ? sprite : wall;
}
static void setup(int angle) {
    int i;
    memset(tilemap, 0, sizeof tilemap);
    memset(spotvis, 0, sizeof spotvis);
    memset(pixels, 0, sizeof pixels);
    for (i = 0; i < 64; i++)
        tilemap[0][i] = tilemap[63][i] = tilemap[i][0] = tilemap[i][63] = 1;
    viewx = viewy = 32 * 65536 + 32768;
    focaltx = focalty = 32;
    xpartialup = xpartialdown = ypartialup = ypartialdown = 32768;
    midangle = angle;
    lastside = -1;
    viewsin = 0;
    viewcos = 65535;
    for (i = 0; i < 64; i++)
        pixelangle[i] = 0;
}
static void cast(int angle) {
    /* CalcHeight uses view trig in signed-magnitude format. */
    int32_t s = (int32_t)(65535 * sin(angle * 3.141592653589793 / 1800));
    int32_t c = (int32_t)(65535 * cos(angle * 3.141592653589793 / 1800));
    viewsin = s < 0 ? (int32_t)(0x80000000u + (uint32_t)-s) : s;
    viewcos = c < 0 ? (int32_t)(0x80000000u + (uint32_t)-c) : c;
    AsmRefresh();
    FarScalePost();
}
int main(void) {
    int i;
    unsigned count = 0;
    PMPages = pageinfo;
    pageinfo[16].length = sizeof sprite;
    for (i = 0; i < 900; i++)
        finetangent[i] = (int32_t)(65536 * tan((i + 0.5) * 3.141592653589793 / 1800));
    for (i = 0; i < 4096; i++)
        wall[i] = i & 63;
    SetupScaling(96);
    for (i = 0; i < 3600; i += 25) {
        setup(i);
        cast(i);
        CHECK(tilehit == 1 && wallheight[0] > 0);
    }
    setup(0);
    cast(0);
    CHECK(xtile == 63 && xtilestep == 1);
    setup(900);
    cast(900);
    CHECK(ytile == 0 && ytilestep == -1);
    setup(1800);
    cast(1800);
    CHECK(xtile == 0 && xtilestep == -1);
    setup(2700);
    cast(2700);
    CHECK(ytile == 63 && ytilestep == 1);
    setup(0);
    tilemap[33][32] = 0x80;
    doorposition[0] = 0;
    cast(0);
    CHECK(tilehit == 0x80 && xintercept == 33 * 65536 + 32768);
    setup(0);
    tilemap[33][32] = 0x80;
    doorposition[0] = 65535;
    cast(0);
    CHECK(tilehit == 1 && spotvis[33][32]);
    setup(2700);
    tilemap[32][33] = 0x80;
    doorposition[0] = 0;
    cast(2700);
    CHECK(tilehit == 0x80 && yintercept == 33 * 65536 + 32768);
    setup(2700);
    tilemap[32][33] = 0x80;
    doorposition[0] = 65535;
    cast(2700);
    CHECK(tilehit == 1 && spotvis[32][33]);
    setup(0);
    tilemap[33][32] = 0xc1;
    pwallpos = 32;
    cast(0);
    CHECK(tilehit == 0xc1 && xintercept == 33 * 65536 + 32768);
    setup(2700);
    tilemap[32][33] = 0xc1;
    cast(2700);
    CHECK(tilehit == 0xc1 && yintercept == 33 * 65536 + 32768);
    memset(pixels, 255, sizeof pixels);
    WL_ScaleWallColumn(3, 2, 256, wall);
    for (i = 0; i < 64; i++)
        CHECK(pixels[i][3] == i && pixels[i][4] == i && pixels[i][2] == 255);
    /* Single column at source x=32: opaque y=0..63, color 42. */
    memset(sprite, 0, sizeof sprite);
    sprite[0] = sprite[2] = 32;
    sprite[4] = 6;
    sprite[6] = 128;
    sprite[8] = 16;
    memset(sprite + 16, 42, 64);
    memset(wallheight, 0, sizeof wallheight);
    memset(pixels, 0, sizeof pixels);
    SimpleScaleShape(32, 0, 64);
    for (i = 0; i < 64; i++)
        CHECK(pixels[i][32] == 42 && pixels[i][31] == 0);
    memset(pixels, 0, sizeof pixels);
    wallheight[32] = 256;
    ScaleShape(32, 0, 256);
    for (i = 0; i < 64; i++)
        CHECK(pixels[i][32] == 0);
    wallheight[32] = 0;
    ScaleShape(32, 0, 256);
    for (i = 0; i < 64; i++)
        CHECK(pixels[i][32] == 42);
    /* Oversized weapon image is clipped vertically and at window edges. */
    SimpleScaleShape(0, 0, 96);
    US_InitRndT(false);
    CHECK(US_RndT() == 8 && US_RndT() == 109);
    for (i = 0; i < 254; i++)
        count += US_RndT();
    CHECK(US_RndT() == 8 && count > 0);
    memset(planepics, 0, sizeof planepics);
    planepics[0] = 7;
    planepics[1] = 8;
    mr_dest = 0;
    mr_rowofs = 320;
    mr_count = 1;
    mr_xstep = mr_ystep = mr_xfrac = mr_yfrac = 0;
    MapRow();
    CHECK(pixels[0][0] == 7 && pixels[1][0] == 8);
    /* Texture address packs x bits 10..15 and y bits 26..31. */
    planepics[(2 * 64 + 3) * 2] = 11;
    planepics[(2 * 64 + 3) * 2 + 1] = 12;
    mr_xfrac = 2 * 1024;
    mr_yfrac = 3 * 1024;
    MapRow();
    CHECK(pixels[0][0] == 11 && pixels[1][0] == 12);
    {
        memptr a = NULL, b = NULL, c = NULL;
        MM_Startup();
        MM_GetPtr(&a, 16);
        MM_GetPtr(&b, 16);
        MM_GetPtr(&c, 16);
        MM_SetPurge(&a, 3);
        MM_SetPurge(&b, 3);
        MM_SetLock(&b, true);
        MM_SortMem();
        CHECK(!a && b && c);
        MM_SetLock(&b, false);
        MM_SortMem();
        CHECK(!b && c);
        MM_Shutdown();
        CHECK(!c && !bufferseg);
    }
    {
        objtype mover = {0}, blocker = {0};
        mover.tilex = mover.tiley = 32;
        mover.dir = east;
        mover.obclass = guardobj;
        blocker.flags = FL_SHOOTABLE;
        actorat[33][32] = &blocker;
        CHECK(!TryWalk(&mover) && mover.tilex == 32);
        mover.dir = northeast;
        actorat[33][31] = &blocker;
        CHECK(!TryWalk(&mover));
        mover.dir = east;
        actorat[33][32] = (objtype *)(uintptr_t)128;
        CHECK(TryWalk(&mover) && opened == 0 && mover.distance == -1);
        actorat[33][32] = NULL;
        mover.tilex = 32;
        mover.tiley = 32;
        mapsegs[0] = mapplane;
        for (i = 0; i < 64; i++)
            farmapylookup[i] = i * 64;
        mapplane[32 * 64 + 33] = AREATILE;
        CHECK(TryWalk(&mover) && mover.tilex == 33 && mover.areanumber == 0);
    }
    puts("PASS: DDA quadrants, doors, pushwalls, wall/sprite clipping, RNG and floor mapper");
    return 0;
}
