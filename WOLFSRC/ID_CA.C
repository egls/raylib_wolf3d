/* Portable cache manager: disk bytes are never native structs or pointers. */
#include "ID_HEADS.H"
#include "wl_data.h"
#include <stddef.h>
byte *tinf;
int mapon = -1;
word *mapsegs[MAPPLANES];
maptype *mapheaderseg[NUMMAPS];
byte *audiosegs[NUMSNDCHUNKS];
void *grsegs[NUMCHUNKS];
byte grneeded[NUMCHUNKS], ca_levelbit = 1, ca_levelnum;
char *titleptr[8];
int profilehandle = -1, debughandle = -1;
char extension[5], gheadname[10] = "VGAHEAD.", gfilename[10] = "VGAGRAPH.",
                   gdictname[10] = "VGADICT.", mheadname[10] = "MAPHEAD.",
                   mfilename[10] = "GAMEMAPS.", aheadname[10] = "AUDIOHED.",
                   afilename[10] = "AUDIOT.", audioname[13] = "AUDIO.";
int32_t *grstarts;
uint32_t *audiostarts;
static int grhandle = -1, maphandle = -1, audiohandle = -1;
static WL_HuffNode dictionary[255];
static word rlewtag;
static SDMode oldsoundmode = sdm_Off;
static boolean started;
void (*drawcachebox)(char *, unsigned), (*updatecachebox)(void), (*finishcachebox)(void);
static void bad(void) { Quit("CA: invalid, mismatched or truncated " WOLF3D_DATA_EXT " data"); }
boolean CA_FarRead(int fd, byte *p, long len) {
    if (len < 0)
        return false;
    while (len) {
        ssize_t n = read(fd, p, (size_t)len);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        p += n;
        len -= n;
    }
    return true;
}
boolean CA_FarWrite(int fd, byte *p, long len) {
    if (len < 0)
        return false;
    while (len) {
        ssize_t n = write(fd, p, (size_t)len);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        p += n;
        len -= n;
    }
    return true;
}
static int openfile(const char *base) {
    char name[20];
    int fd;
    snprintf(name, sizeof name, "%s%s", base, extension);
    fd = open(name, O_RDONLY | O_BINARY);
    if (fd < 0) {
        char error[80];
        snprintf(error, sizeof error, "Can't open %s", name);
        Quit(error);
    }
    return fd;
}
static byte *readpart(int fd, uint32_t off, size_t n) {
    long total = filelength(fd);
    byte *p;
    if (total < 0 || off > (unsigned long)total || n > (unsigned long)total - off ||
        n > 16 * 1024 * 1024) {
        bad();
        return NULL;
    }
    p = malloc(n ? n : 1);
    if (!p)
        Quit("CA: out of memory");
    if (lseek(fd, off, SEEK_SET) < 0 || !CA_FarRead(fd, p, (long)n))
        bad();
    return p;
}
static byte *whole(const char *base, size_t *n) {
    int fd = openfile(base);
    long len = filelength(fd);
    byte *p;
    if (len < 0)
        bad();
    *n = len;
    p = readpart(fd, 0, *n);
    close(fd);
    return p;
}
boolean CA_ReadFile(char *name, memptr *slot) {
    int fd = open(name, O_RDONLY | O_BINARY);
    long n;
    boolean ok;
    if (fd < 0)
        return false;
    n = filelength(fd);
    ok = CA_FarRead(fd, *slot, n);
    close(fd);
    return ok;
}
boolean CA_LoadFile(char *name, memptr *slot) {
    int fd = open(name, O_RDONLY | O_BINARY);
    long n;
    boolean ok;
    if (fd < 0)
        return false;
    n = filelength(fd);
    if (n < 0) {
        close(fd);
        return false;
    }
    MM_GetPtr(slot, n);
    if (mmerror) {
        close(fd);
        return false;
    }
    ok = CA_FarRead(fd, *slot, n);
    close(fd);
    if (!ok)
        MM_FreePtr(slot);
    return ok;
}
boolean CA_WriteFile(char *name, void *p, long n) {
    int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600);
    boolean ok;
    if (fd < 0)
        return false;
    ok = CA_FarWrite(fd, p, n);
    close(fd);
    return ok;
}
void CA_OpenDebug(void) { debughandle = open("DEBUG.TXT", O_WRONLY | O_CREAT | O_TRUNC, 0600); }
void CA_CloseDebug(void) {
    if (debughandle >= 0)
        close(debughandle);
    debughandle = -1;
}
long CA_RLEWCompress(word *src, long bytes, word *dst, unsigned tag) {
    word *start = dst;
    size_t i = 0, n = bytes / 2;
    while (i < n) {
        size_t run = 1;
        word v = src[i];
        while (i + run < n && src[i + run] == v && run < 65535)
            run++;
        if (run > 3 || v == tag) {
            *dst++ = tag;
            *dst++ = run;
            *dst++ = v;
        } else {
            size_t j;
            for (j = 0; j < run; j++)
                *dst++ = v;
        }
        i += run;
    }
    return (dst - start) * 2;
}
/* Legacy callers have no input length; production map loads use the bounded codec. */
void CA_RLEWexpand(word *src, word *dst, long bytes, unsigned tag) {
    size_t out = 0, n = bytes / 2;
    while (out < n) {
        word v = *src++;
        if (v != tag)
            dst[out++] = v;
        else {
            size_t run = *src++;
            v = *src++;
            if (run > n - out)
                bad();
            while (run--)
                dst[out++] = v;
        }
    }
}
static byte *graphics(int chunk, size_t *expanded) {
    int next;
    size_t compressed, skip;
    byte *src, *dst;
    if (chunk < 0 || chunk >= NUMCHUNKS || grstarts[chunk] < 0) {
        bad();
        return NULL;
    }
    for (next = chunk + 1; next <= NUMCHUNKS && grstarts[next] < 0; next++)
        ;
    if (next > NUMCHUNKS || grstarts[next] < grstarts[chunk]) {
        bad();
        return NULL;
    }
    compressed = (size_t)(grstarts[next] - grstarts[chunk]);
    src = readpart(grhandle, grstarts[chunk], compressed);
    if (chunk == STARTTILE8) {
        *expanded = NUMTILE8 * 64;
        skip = 0;
    } else {
        if (compressed < 4)
            bad();
        *expanded = WL_ReadLE32(src);
        skip = 4;
    }
    if (!*expanded || *expanded > 16 * 1024 * 1024)
        bad();
    dst = malloc(*expanded);
    if (!dst)
        Quit("CA: out of memory");
    if (!WL_HuffExpand(src + skip, compressed - skip, dst, *expanded, dictionary))
        bad();
    free(src);
    return dst;
}
void CA_Startup(void) {
    size_t n, i;
    byte *b, *p;
    if (started)
        return;
    if (!extension[0])
        strcpy(extension, WOLF3D_DATA_EXT);
    b = whole(gdictname, &n);
    if (n != 1024 && n != 1020)
        bad();
    for (i = 0; i < 255; i++) {
        dictionary[i].bit[0] = WL_ReadLE16(b + i * 4);
        dictionary[i].bit[1] = WL_ReadLE16(b + i * 4 + 2);
    }
    free(b);
    b = whole(gheadname, &n);
    if (n != (NUMCHUNKS + 1) * 3)
        bad();
    MM_GetPtr((memptr *)&grstarts, (NUMCHUNKS + 1) * sizeof(*grstarts));
    MM_SetLock((memptr *)&grstarts, true);
    for (i = 0; i <= NUMCHUNKS; i++) {
        uint32_t off = WL_ReadLE24(b + i * 3);
        grstarts[i] = off == 0xffffff ? -1 : (int32_t)off;
    }
    free(b);
    grhandle = openfile(gfilename);
    {
        int32_t previous = -1;
        long end = filelength(grhandle);
        for (i = 0; i <= NUMCHUNKS; i++) {
            if (grstarts[i] < 0)
                continue;
            if (grstarts[i] < previous || grstarts[i] > end)
                bad();
            previous = grstarts[i];
        }
        if (grstarts[NUMCHUNKS] < 0)
            bad();
    }
    p = graphics(STRUCTPIC, &n);
    if (n != NUMPICS * 4)
        bad();
    MM_GetPtr((memptr *)&pictable, NUMPICS * sizeof(*pictable));
    MM_SetLock((memptr *)&pictable, true);
    for (i = 0; i < NUMPICS; i++) {
        pictable[i].width = WL_ReadLE16(p + 4 * i);
        pictable[i].height = WL_ReadLE16(p + 4 * i + 2);
        if (!pictable[i].width || pictable[i].width > 320 ||
            !pictable[i].height || pictable[i].height > 200)
            bad();
    }
    free(p);
    b = whole(mheadname, &n);
    if (n < 2 + NUMMAPS * 4)
        bad();
    rlewtag = WL_ReadLE16(b);
    maphandle = openfile(mfilename);
    for (i = 0; i < NUMMAPS; i++) {
        uint32_t off = WL_ReadLE32(b + 2 + i * 4);
        maptype *h;
        size_t j;
        if (off == 0xffffffff || !off) {
#ifdef WOLF3D_REGISTERED
            bad(); /* All six episodes need their ten maps. */
#endif
            continue;
        }
        p = readpart(maphandle, off, 38);
        MM_GetPtr((memptr *)&mapheaderseg[i], sizeof(maptype));
        MM_SetLock((memptr *)&mapheaderseg[i], true);
        h = mapheaderseg[i];
        for (j = 0; j < 3; j++) {
            h->planestart[j] = WL_ReadLE32(p + j * 4);
            h->planelength[j] = WL_ReadLE16(p + 12 + j * 2);
        }
        h->width = WL_ReadLE16(p + 18);
        h->height = WL_ReadLE16(p + 20);
        memcpy(h->name, p + 22, 16);
        free(p);
        if (h->width != 64 || h->height != 64)
            bad();
        for (j = 0; j < MAPPLANES; j++) {
            long end = filelength(maphandle);
            if (h->planelength[j] < 2 || h->planestart[j] > end ||
                h->planelength[j] > end - h->planestart[j])
                bad();
        }
    }
    free(b);
    for (i = 0; i < MAPPLANES; i++) {
        MM_GetPtr((memptr *)&mapsegs[i], 4096 * sizeof(word));
        MM_SetLock((memptr *)&mapsegs[i], true);
    }
    b = whole(aheadname, &n);
    if (n != (NUMSNDCHUNKS + 1) * 4)
        bad();
    MM_GetPtr((memptr *)&audiostarts, (NUMSNDCHUNKS + 1) * sizeof(*audiostarts));
    MM_SetLock((memptr *)&audiostarts, true);
    for (i = 0; i <= NUMSNDCHUNKS; i++)
        audiostarts[i] = WL_ReadLE32(b + 4 * i);
    free(b);
    audiohandle = openfile(afilename);
    for (i = 0; i <= NUMSNDCHUNKS; i++)
        if (audiostarts[i] > filelength(audiohandle) ||
            (i && audiostarts[i] < audiostarts[i - 1]))
            bad();
    mapon = -1;
    ca_levelbit = 1;
    ca_levelnum = 0;
    started = true;
}
void CA_CacheMap(int num) {
    int plane;
    maptype *h;
    if (num < 0 || num >= NUMMAPS || !mapheaderseg[num])
        bad();
    h = mapheaderseg[num];
    for (plane = 0; plane < MAPPLANES; plane++) {
        size_t n = h->planelength[plane], words;
        byte *src = readpart(maphandle, h->planestart[plane], n);
        word *tmp;
        if (n < 2)
            bad();
        words = WL_ReadLE16(src) / 2;
        if (!words || WL_ReadLE16(src) % 2)
            bad();
        tmp = malloc(words * sizeof(word));
        if (!tmp)
            Quit("CA: out of memory");
        if (!WL_CarmackExpand(src + 2, n - 2, tmp, words) || tmp[0] != 8192 ||
            !WL_RLEWExpand(tmp + 1, words - 1, mapsegs[plane], 4096, rlewtag))
            bad();
        free(tmp);
        free(src);
    }
    mapon = num;
}
void CA_CacheGrChunk(int chunk) {
    byte *p;
    size_t len, i;
    if (chunk < 0 || chunk >= NUMCHUNKS)
        bad();
    grneeded[chunk] |= ca_levelbit;
    if (grsegs[chunk]) {
        MM_SetPurge(&grsegs[chunk], 0);
        return;
    }
    if (grstarts[chunk] < 0)
        return;
    p = graphics(chunk, &len);
    if (chunk >= STARTPICS && chunk < STARTPICM &&
        len != (size_t)pictable[chunk - STARTPICS].width * pictable[chunk - STARTPICS].height)
        bad();
    if (chunk >= STARTFONT && chunk < STARTFONTM) {
        if (len < 770)
            bad();
        {
            size_t height = WL_ReadLE16(p);
            if (!height || height > 200)
                bad();
            for (i = 0; i < 256; i++) {
                size_t offset = WL_ReadLE16(p + 2 + i * 2);
                size_t pixels = height * p[514 + i];
                if (offset > len || pixels > len - offset)
                    bad();
            }
        }
        /* Fixed-size native fields; data offsets stay relative to the chunk. */
        for (i = 0; i < 257; i++) {
            word v = WL_ReadLE16(p + 2 * i);
            memcpy(p + 2 * i, &v, 2);
        }
    }
    MM_GetPtr(&grsegs[chunk], len);
    if (!mmerror) {
        memcpy(grsegs[chunk], p, len);
    }
    free(p);
}
void CA_CacheScreen(int chunk) {
    size_t n;
    byte *p = graphics(chunk, &n);
    unsigned x, y;
    if (n != 64000)
        bad();
    for (y = 0; y < 200; y++)
        for (x = 0; x < 320; x++)
            VL_Plot(x, y, p[(x & 3) * 16000 + y * 80 + (x >> 2)]);
    free(p);
    VW_MarkUpdateBlock(0, 0, 319, 199);
}
void CA_CacheAudioChunk(int chunk) {
    byte *src;
    size_t n, header, payload;
    memptr *slot;
    if (chunk < 0 || chunk >= NUMSNDCHUNKS)
        bad();
    slot = (memptr *)&audiosegs[chunk];
    if (audiosegs[chunk]) {
        MM_SetPurge(slot, 0);
        return;
    }
    if (audiostarts[chunk + 1] < audiostarts[chunk])
        bad();
    n = audiostarts[chunk + 1] - audiostarts[chunk];
    src = readpart(audiohandle, audiostarts[chunk], n);
    if (chunk < STARTDIGISOUNDS) {
        uint32_t length;
        if (n < 6)
            bad();
        length = WL_ReadLE32(src);
        header = chunk < STARTADLIBSOUNDS ? 6 : 23;
        payload = chunk < STARTADLIBSOUNDS ? offsetof(PCSound, data) : offsetof(AdLibSound, data);
        if (n < header || length > n - header)
            bad();
        MM_GetPtr(slot, payload + n - header);
        if (mmerror) {
            free(src);
            return;
        }
        memset(audiosegs[chunk], 0, payload);
        ((SoundCommon *)audiosegs[chunk])->length = length;
        ((SoundCommon *)audiosegs[chunk])->priority = WL_ReadLE16(src + 4);
        if (header == 23) {
            AdLibSound *sound = (AdLibSound *)audiosegs[chunk];
            memcpy(&sound->inst, src + 6, 16);
            sound->block = src[22];
        }
        memcpy((byte *)audiosegs[chunk] + payload, src + header, n - header);
    } else {
        MM_GetPtr(slot, n);
        if (mmerror) {
            free(src);
            return;
        }
        memcpy(audiosegs[chunk], src, n);
        if (chunk >= STARTMUSIC) {
            size_t i;
            if (n < 2 || WL_ReadLE16(src) > n - 2 || WL_ReadLE16(src) % 4)
                bad();
            for (i = 0; i + 1 < n; i += 2) {
                word v = WL_ReadLE16(src + i);
                memcpy((byte *)audiosegs[chunk] + i, &v, 2);
            }
        }
    }
    free(src);
}
void CA_LoadAllSounds(void) {
    int i, start;
    if (oldsoundmode != sdm_Off) {
        start = oldsoundmode == sdm_PC ? STARTPCSOUNDS : STARTADLIBSOUNDS;
        for (i = 0; i < NUMSOUNDS; i++)
            if (audiosegs[start + i])
                MM_SetPurge((memptr *)&audiosegs[start + i], 3);
    }
    oldsoundmode = SoundMode;
    if (SoundMode == sdm_Off)
        return;
    start = SoundMode == sdm_PC ? STARTPCSOUNDS : STARTADLIBSOUNDS;
    for (i = 0; i < NUMSOUNDS; i++)
        CA_CacheAudioChunk(start + i);
}
void CA_ClearMarks(void) {
    int i;
    for (i = 0; i < NUMCHUNKS; i++)
        grneeded[i] &= ~ca_levelbit;
}
void CA_ClearAllMarks(void) {
    memset(grneeded, 0, sizeof grneeded);
    ca_levelbit = 1;
    ca_levelnum = 0;
}
void CA_SetGrPurge(void) {
    int i;
    CA_ClearMarks();
    for (i = 0; i < NUMCHUNKS; i++)
        if (grsegs[i])
            MM_SetPurge(&grsegs[i], 3);
}
void CA_SetAllPurge(void) {
    int i;
    CA_SetGrPurge();
    for (i = 0; i < NUMSNDCHUNKS; i++)
        if (audiosegs[i])
            MM_SetPurge((memptr *)&audiosegs[i], 3);
}
void CA_UpLevel(void) {
    int i;
    if (ca_levelnum == 7)
        Quit("CA_UpLevel: past level 7");
    for (i = 0; i < NUMCHUNKS; i++)
        if (grsegs[i])
            MM_SetPurge(&grsegs[i], 3);
    ca_levelnum++;
    ca_levelbit <<= 1;
}
void CA_DownLevel(void) {
    if (!ca_levelnum)
        Quit("CA_DownLevel: below zero");
    ca_levelnum--;
    ca_levelbit >>= 1;
    CA_CacheMarks();
}
void CA_CacheMarks(void) {
    int i;
    for (i = 0; i < NUMCHUNKS; i++) {
        if (grneeded[i] & ca_levelbit)
            CA_CacheGrChunk(i);
        else if (grsegs[i])
            MM_SetPurge(&grsegs[i], 3);
    }
}
void CA_Shutdown(void) {
    int i;
    if (!started)
        return;
    close(grhandle);
    close(maphandle);
    close(audiohandle);
    grhandle = maphandle = audiohandle = -1;
    for (i = 0; i < NUMCHUNKS; i++)
        if (grsegs[i])
            MM_FreePtr(&grsegs[i]);
    for (i = 0; i < NUMSNDCHUNKS; i++)
        if (audiosegs[i])
            MM_FreePtr((memptr *)&audiosegs[i]);
    for (i = 0; i < NUMMAPS; i++)
        if (mapheaderseg[i])
            MM_FreePtr((memptr *)&mapheaderseg[i]);
    for (i = 0; i < MAPPLANES; i++)
        MM_FreePtr((memptr *)&mapsegs[i]);
    MM_FreePtr((memptr *)&pictable);
    MM_FreePtr((memptr *)&grstarts);
    MM_FreePtr((memptr *)&audiostarts);
    CA_ClearAllMarks();
    started = false;
}
