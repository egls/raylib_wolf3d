/* VSWAP pages remain resident. Sparse entries retain NULL addresses. */
#include "ID_HEADS.H"
#include "wl_data.h"
boolean XMSPresent, EMSPresent;
word XMSPagesAvail, EMSPagesAvail;
word ChunksInFile, PMSpriteStart, PMSoundStart;
PageListStruct *PMPages;
char PageFileName[13] = "VSWAP.";
static byte **pages;
static boolean started;
static void exact(int fd, void *dst, size_t n) {
    byte *p = dst;
    while (n) {
        ssize_t got = read(fd, p, n);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            Quit("VSWAP: short read");
        p += got;
        n -= got;
    }
}
void PM_Startup(void) {
    int fd;
    byte h[6], b[4];
    size_t i, filesize;
    char name[20];
    const char *dot;
    if (started)
        return;
    dot = strchr(PageFileName, '.');
    snprintf(name, sizeof name, "%s%s", PageFileName, dot && dot[1] ? "" : extension);
    fd = open(name, O_RDONLY | O_BINARY);
    if (fd < 0)
        Quit("Can't open VSWAP.WL1");
    filesize = filelength(fd);
    exact(fd, h, 6);
    ChunksInFile = WL_ReadLE16(h);
    PMSpriteStart = WL_ReadLE16(h + 2);
    PMSoundStart = WL_ReadLE16(h + 4);
    if (!ChunksInFile || PMSpriteStart > PMSoundStart || PMSoundStart > ChunksInFile ||
        6u + 6u * ChunksInFile > filesize)
        Quit("VSWAP: bad header");
    PMPages = calloc(ChunksInFile, sizeof(*PMPages));
    pages = calloc(ChunksInFile, sizeof(*pages));
    if (!PMPages || !pages)
        Quit("VSWAP: out of memory");
    for (i = 0; i < ChunksInFile; i++) {
        exact(fd, b, 4);
        PMPages[i].offset = WL_ReadLE32(b);
        PMPages[i].mainPage = PMPages[i].emsPage = PMPages[i].xmsPage = -1;
    }
    for (i = 0; i < ChunksInFile; i++) {
        exact(fd, b, 2);
        PMPages[i].length = WL_ReadLE16(b);
    }
    for (i = 0; i < ChunksInFile; i++) {
        size_t off = PMPages[i].offset, len = PMPages[i].length;
        if (!off)
            continue;
        if (off < 6u + 6u * ChunksInFile || off > filesize || len > filesize - off)
            Quit("VSWAP: bad page range");
        /* Pad sound pages for the original audio page consumer. */
        pages[i] = calloc(len < PMPageSize ? PMPageSize : len, 1);
        if (!pages[i])
            Quit("VSWAP: out of memory");
        if (lseek(fd, off, SEEK_SET) < 0)
            Quit("VSWAP: seek failed");
        exact(fd, pages[i], len);
        PMPages[i].mainPage = i;
    }
    close(fd);
    started = true;
}
void PM_Shutdown(void) {
    size_t i;
    if (pages)
        for (i = 0; i < ChunksInFile; i++)
            free(pages[i]);
    free(pages);
    free(PMPages);
    pages = NULL;
    PMPages = NULL;
    ChunksInFile = PMSpriteStart = PMSoundStart = 0;
    started = false;
}
memptr PM_GetPageAddress(int n) {
    if (n < 0 || n >= ChunksInFile)
        Quit("PM: page out of range");
    return pages[n];
}
memptr PM_GetPage(int n) {
    memptr p = PM_GetPageAddress(n);
    if (!p)
        Quit("PM: sparse page requested");
    return p;
}
void PM_SetPageLock(int n, PMLockType lock) {
    (void)PM_GetPageAddress(n);
    PMPages[n].locked = lock;
}
void PM_Preload(boolean (*update)(word, word)) {
    word i;
    if (update)
        for (i = 0; i < ChunksInFile; i++)
            (void)update(i + 1, ChunksInFile);
}
void PM_Reset(void) {
    word i;
    for (i = 0; i < ChunksInFile; i++)
        PMPages[i].locked = pml_Unlocked;
}
void PM_NextFrame(void) {}
void PM_SetMainMemPurge(int level) { (void)level; }
void PM_SetMainPurge(int level) { PM_SetMainMemPurge(level); }
void PM_CheckMainMem(void) {}
