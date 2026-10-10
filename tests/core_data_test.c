#include "WL_DEF.H"
#include "wl_data.h"
#include <assert.h>
pictabletype *pictable;
SDMode SoundMode;
int viewwidth = 320, viewheight = 200;
unsigned wallheight[MAXVIEWWIDTH];
static unsigned pixelcount;
void Quit(char *message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}
void VL_Plot(int x, int y, int c) {
    (void)c;
    if (x < 0 || x >= viewwidth || y < 0 || y >= viewheight)
        Quit("scaler outside view");
    pixelcount++;
}
int VW_MarkUpdateBlock(int x, int y, int xx, int yy) {
    (void)x;
    (void)y;
    (void)xx;
    (void)yy;
    return 0;
}
long filelength(int fd) {
    struct stat st;
    return fstat(fd, &st) ? -1 : st.st_size;
}
static uint32_t crc(const byte *p, size_t n) {
    uint32_t c = ~0u;
    size_t i;
    while (n--) {
        c ^= *p++;
        for (i = 0; i < 8; i++)
            c = (c >> 1) ^ ((0u - (c & 1)) & 0xedb88320u);
    }
    return ~c;
}
static uint32_t planecrc(const word *p) {
    byte disk[8192];
    size_t i;
    for (i = 0; i < 4096; i++) {
        disk[i * 2] = p[i];
        disk[i * 2 + 1] = p[i] >> 8;
    }
    return crc(disk, sizeof disk);
}
static void require(int condition, const char *message) {
    if (!condition)
        Quit((char *)message);
}
int main(int argc, char **argv) {
    memptr discard = NULL, locked = NULL, keep = NULL;
    byte *wall, *sprite;
    size_t i;
    if (argc != 2 || chdir(argv[1])) {
        puts("SKIP: shareware directory missing (scripts/fetch_shareware.sh)");
        return 77;
    }
    if (access("MAPHEAD.WL1", R_OK)) {
        puts("SKIP: shareware data missing (scripts/fetch_shareware.sh)");
        return 77;
    }
    strcpy(extension, "WL1");
    MM_Startup();
    MM_GetPtr(&discard, 16);
    MM_GetPtr(&locked, 16);
    MM_GetPtr(&keep, 16);
    MM_SetPurge(&discard, 3);
    MM_SetPurge(&locked, 3);
    MM_SetLock(&locked, true);
    MM_SortMem();
    require(!discard && locked && keep, "MM purge/lock failure");
    MM_SetLock(&locked, false);
    MM_SortMem();
    require(!locked && keep, "MM unlock failure");
    MM_FreePtr(&keep);
    CA_Startup();
    PM_Startup();
    CA_CacheMap(0);
    require(mapheaderseg[0]->width == 64 && mapheaderseg[0]->height == 64, "E1M1 dimensions");
    printf("E1M1 CRC32: %08x %08x\n", planecrc(mapsegs[0]), planecrc(mapsegs[1]));
    /* Golden values independently decoded from the pinned WL1 set. */
    require(planecrc(mapsegs[0]) == 0xb8d06305u && planecrc(mapsegs[1]) == 0x52ef76a0u,
            "E1M1 plane checksum");
    wall = PM_GetPage(0);
    sprite = PM_GetSpritePage(0);
    printf("wall/sprite CRC32: %08x %08x\n", crc(wall, PMPages[0].length),
           crc(sprite, PMPages[PMSpriteStart].length));
    require(PMPages[0].length == 4096 && crc(wall, 4096) == 0xc078b0d7u, "wall checksum");
    require(WL_ReadLE16(sprite) <= WL_ReadLE16(sprite + 2) && WL_ReadLE16(sprite + 2) < 64,
            "sprite bounds");
    require(crc(sprite, PMPages[PMSpriteStart].length) == 0xf816eee1u, "sprite checksum");
    CA_CacheGrChunk(TITLEPIC);
    require(pictable[TITLEPIC - STARTPICS].width == 320 &&
                pictable[TITLEPIC - STARTPICS].height == 200,
            "title dimensions");
    printf("title CRC32: %08x\n", crc(grsegs[TITLEPIC], 64000));
    require(crc(grsegs[TITLEPIC], 64000) == 0x359d6b7au, "title checksum");
    for (i = 0; i < NUMCHUNKS; i++)
        if (grstarts[i] >= 0)
            CA_CacheGrChunk(i);
    for (i = 0; i < NUMSNDCHUNKS; i++)
        CA_CacheAudioChunk(i);
    for (i = 0; i < 10; i++)
        CA_CacheMap(i);
    SetupScaling(480);
    for (i = PMSpriteStart; i < PMSoundStart; i++)
        if (PMPages[i].offset) {
            SimpleScaleShape(160, (int)i - PMSpriteStart, 64);
            SimpleScaleShape(0, (int)i - PMSpriteStart, 200);
            ScaleShape(160, (int)i - PMSpriteStart, 256);
        }
    require(pixelcount > 1000, "shareware sprite rendering");
    CA_CacheScreen(TITLEPIC);
    CA_SetAllPurge();
    MM_SortMem();
    require(!grsegs[TITLEPIC] && mapsegs[0], "cache purge");
    CA_CacheGrChunk(TITLEPIC);
    require(crc(grsegs[TITLEPIC], 64000) == 0x359d6b7au, "cache reload");
    PM_Shutdown();
    CA_Shutdown();
    MM_Shutdown();
    /* Repeated startup catches stale slots and shutdown ordering. */
    strcpy(PageFileName, "VSWAP.WL1");
    MM_Startup();
    CA_Startup();
    PM_Startup();
    CA_CacheMap(0);
    PM_Shutdown();
    CA_Shutdown();
    MM_Shutdown();
    puts("PASS: WL1 maps, wall, sprite, pictures, fonts, audio and memory lifecycle");
    return 0;
}
