/* Flat allocator. Owners are stable pointer slots; purge invalidates the slot.
 * Locked blocks never move or purge. There is no DOS address-space budget. */
#include "ID_HEADS.H"
#include <limits.h>
typedef struct MMBlock {
    memptr *owner;
    size_t size;
    int purge;
    boolean locked;
    struct MMBlock *next;
} MMBlock;
static MMBlock *blocks;
static boolean bombonerror = true, started;
mminfotype mminfo;
memptr bufferseg;
boolean mmerror;
void (*beforesort)(void), (*aftersort)(void);
/* Callers also pass slots typed byte ** or word ** through the historical API.
 * Copy pointer representations instead of aliasing those slots as void **. */
static memptr owner_value(memptr *owner) {
    memptr value;
    memcpy(&value, owner, sizeof value);
    return value;
}
static void owner_set(memptr *owner, memptr value) { memcpy(owner, &value, sizeof value); }
static MMBlock *find(memptr *owner) {
    MMBlock *b;
    for (b = blocks; b; b = b->next)
        if (b->owner == owner)
            return b;
    Quit("MM: unowned pointer slot");
    return NULL;
}
void MM_BombOnError(boolean bomb) { bombonerror = bomb; }
void MM_FreePtr(memptr *owner) {
    MMBlock **link = &blocks, *b;
    while ((b = *link) != NULL) {
        if (b->owner == owner) {
            *link = b->next;
            free(owner_value(owner));
            owner_set(owner, NULL);
            free(b);
            return;
        }
        link = &b->next;
    }
    Quit("MM_FreePtr: unowned pointer slot");
}
void MM_SortMem(void) {
    MMBlock *b, *next;
    if (beforesort)
        beforesort();
    for (b = blocks; b; b = next) {
        next = b->next;
        if (b->purge && !b->locked)
            MM_FreePtr(b->owner);
    }
    if (aftersort)
        aftersort();
}
void MM_GetPtr(memptr *owner, unsigned long size) {
    MMBlock *b;
    void *ptr;
    mmerror = false;
    for (b = blocks; b; b = b->next)
        if (b->owner == owner) {
            Quit("MM_GetPtr: slot already allocated");
            return;
        }
    owner_set(owner, NULL);
    b = malloc(sizeof(*b));
    ptr = malloc(size ? size : 1);
    if (!b || !ptr) {
        free(b);
        free(ptr);
        MM_SortMem();
        b = malloc(sizeof(*b));
        ptr = malloc(size ? size : 1);
    }
    if (!b || !ptr) {
        free(b);
        free(ptr);
        mmerror = true;
        if (bombonerror)
            Quit("MM_GetPtr: out of memory");
        return;
    }
    b->owner = owner;
    b->size = size;
    b->purge = 0;
    b->locked = false;
    b->next = blocks;
    blocks = b;
    owner_set(owner, ptr);
}
void MM_SetPurge(memptr *owner, int purge) { find(owner)->purge = purge; }
void MM_SetLock(memptr *owner, boolean locked) { find(owner)->locked = locked; }
void MM_Startup(void) {
    if (started)
        return;
    started = true;
    mmerror = false;
    memset(&mminfo, 0, sizeof mminfo);
    mminfo.mainmem = MM_UnusedMemory();
    MM_GetPtr(&bufferseg, BUFFERSIZE);
    MM_SetLock(&bufferseg, true);
}
void MM_Shutdown(void) {
    while (blocks)
        MM_FreePtr(blocks->owner);
    started = false;
}
long MM_UnusedMemory(void) {
    long pages = sysconf(_SC_AVPHYS_PAGES), size = sysconf(_SC_PAGESIZE);
    return pages > 0 && size > 0 && pages <= LONG_MAX / size ? pages * size : 64L * 1024 * 1024;
}
long MM_TotalFree(void) {
    long available = MM_UnusedMemory();
    MMBlock *b;
    for (b = blocks; b; b = b->next)
        if (b->purge && !b->locked && b->size < (size_t)(LONG_MAX - available))
            available += b->size;
    return available;
}
void MM_ShowMemory(void) { /* no segmented address-space visualization */ }
void MM_MapEMS(void) { /* all pages are flat */ }
void MML_UseSpace(unsigned start, unsigned length) {
    (void)start;
    (void)length;
}
