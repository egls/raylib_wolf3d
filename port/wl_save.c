/*
 * wl_save.c - Portable serialization and XDG path handling for config,
 * savegames, and high scores.
 */
#include "WL_DEF.H"
#include "wl_save.h"
#include "wl_states.h"
#include <sys/stat.h>
#include <errno.h>

#ifndef NUMMAPS
#define NUMMAPS 60
#endif

/* -------------------------------------------------------------------------
 * Little-endian serialization primitives
 * ------------------------------------------------------------------------- */

static inline void wr_u8 (uint8_t **p, uint8_t v)
{
    *(*p)++ = v;
}

static inline uint8_t rd_u8 (const uint8_t **p)
{
    return *(*p)++;
}

static inline void wr_u16_le (uint8_t **p, uint16_t v)
{
    (*p)[0] = (uint8_t)(v & 0xff);
    (*p)[1] = (uint8_t)((v >> 8) & 0xff);
    *p += 2;
}

static inline uint16_t rd_u16_le (const uint8_t **p)
{
    uint16_t v = (uint16_t)(*p)[0] | ((uint16_t)(*p)[1] << 8);
    *p += 2;
    return v;
}

static inline void wr_i16_le (uint8_t **p, int16_t v)
{
    wr_u16_le(p, (uint16_t)v);
}

static inline int16_t rd_i16_le (const uint8_t **p)
{
    return (int16_t)rd_u16_le(p);
}

static inline void wr_u32_le (uint8_t **p, uint32_t v)
{
    (*p)[0] = (uint8_t)(v & 0xff);
    (*p)[1] = (uint8_t)((v >> 8) & 0xff);
    (*p)[2] = (uint8_t)((v >> 16) & 0xff);
    (*p)[3] = (uint8_t)((v >> 24) & 0xff);
    *p += 4;
}

static inline uint32_t rd_u32_le (const uint8_t **p)
{
    uint32_t v = (uint32_t)(*p)[0]
               | ((uint32_t)(*p)[1] << 8)
               | ((uint32_t)(*p)[2] << 16)
               | ((uint32_t)(*p)[3] << 24);
    *p += 4;
    return v;
}

static inline void wr_i32_le (uint8_t **p, int32_t v)
{
    wr_u32_le(p, (uint32_t)v);
}

static inline int32_t rd_i32_le (const uint8_t **p)
{
    return (int32_t)rd_u32_le(p);
}

static inline void wr_bytes (uint8_t **p, const void *src, size_t n)
{
    memcpy(*p, src, n);
    *p += n;
}

static inline void rd_bytes (const uint8_t **p, void *dst, size_t n)
{
    memcpy(dst, *p, n);
    *p += n;
}

/* -------------------------------------------------------------------------
 * State mapping
 * ------------------------------------------------------------------------- */

uint16_t WL_StateToID (const statetype *state)
{
    if (!state)
        return 0;
    for (uint16_t i = 0; i < WL_NUM_STATES; i++)
    {
        if (wl_state_table[i] == state)
            return (uint16_t)(i + 1);
    }
    return 0;
}

statetype *WL_IDToState (uint16_t id)
{
    if (id == 0 || id > WL_NUM_STATES)
        return NULL;
    return (statetype *)wl_state_table[id - 1];
}

/* -------------------------------------------------------------------------
 * XDG directory resolution & creation
 * ------------------------------------------------------------------------- */

boolean WL_EnsureDir (const char *path)
{
    char tmp[512];
    char *p;
    size_t len;

    if (!path || !*path)
        return false;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return false;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return false;
    return true;
}

void WL_GetConfigDir (char *dest, size_t dest_len)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
    {
        snprintf(dest, dest_len, "%s/raylib_wolf3d", xdg);
        return;
    }
    const char *home = getenv("HOME");
    if (home && *home)
    {
        snprintf(dest, dest_len, "%s/.config/raylib_wolf3d", home);
        return;
    }
    snprintf(dest, dest_len, ".");
}

void WL_GetSaveDir (char *dest, size_t dest_len)
{
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && *xdg)
    {
        snprintf(dest, dest_len, "%s/raylib_wolf3d", xdg);
        return;
    }
    const char *home = getenv("HOME");
    if (home && *home)
    {
        snprintf(dest, dest_len, "%s/.local/share/raylib_wolf3d", home);
        return;
    }
    WL_GetConfigDir(dest, dest_len);
}

void WL_GetConfigFilePath (char *dest, size_t dest_len)
{
    char dir[512];
    WL_GetConfigDir(dir, sizeof(dir));
    if (extension[0])
        snprintf(dest, dest_len, "%s/CONFIG.%s", dir, extension);
    else
        snprintf(dest, dest_len, "%s/CONFIG.WL1", dir);
}

void WL_GetSaveFilePath (int slot, char *dest, size_t dest_len)
{
    char save_dir[512];
    char config_dir[512];
    char ext[16];

    if (slot < 0) slot = 0;
    if (slot > 9) slot = 9;

    snprintf(ext, sizeof(ext), "%s", extension[0] ? extension : "WL1");

    WL_GetSaveDir(save_dir, sizeof(save_dir));
    snprintf(dest, dest_len, "%s/SAVEGAM%d.%s", save_dir, slot, ext);

    /* If the file doesn't exist in save_dir, check config_dir fallback */
    if (access(dest, F_OK) != 0)
    {
        WL_GetConfigDir(config_dir, sizeof(config_dir));
        if (strcmp(save_dir, config_dir) != 0)
        {
            char fallback[1024];
            snprintf(fallback, sizeof(fallback), "%s/SAVEGAM%d.%s", config_dir, slot, ext);
            if (access(fallback, F_OK) == 0)
            {
                snprintf(dest, dest_len, "%s", fallback);
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * Config Serialization
 * ------------------------------------------------------------------------- */

size_t WL_SerializeConfig (uint8_t *buf, size_t max_size)
{
    if (!buf || max_size < WL_CONFIG_SIZE)
        return 0;

    uint8_t *p = buf;

    /* 1. Scores (7 entries * 66 bytes = 462 bytes) */
    for (int i = 0; i < MaxScores; i++)
    {
        char name[MaxHighName + 1];
        memset(name, 0, sizeof(name));
        strncpy(name, Scores[i].name, sizeof(name) - 1);
        wr_bytes(&p, name, MaxHighName + 1);          /* 58 bytes */
        wr_i32_le(&p, (int32_t)Scores[i].score);       /* 4 bytes */
        wr_u16_le(&p, (uint16_t)Scores[i].completed);  /* 2 bytes */
        wr_u16_le(&p, (uint16_t)Scores[i].episode);    /* 2 bytes */
    }

    /* 2. Sound modes (3 * 2 = 6 bytes) */
    wr_i16_le(&p, (int16_t)SoundMode);
    wr_i16_le(&p, (int16_t)MusicMode);
    wr_i16_le(&p, (int16_t)DigiMode);

    /* 3. Input flags (5 * 2 = 10 bytes) */
    wr_i16_le(&p, (int16_t)mouseenabled);
    wr_i16_le(&p, (int16_t)joystickenabled);
    wr_i16_le(&p, (int16_t)joypadenabled);
    wr_i16_le(&p, (int16_t)joystickprogressive);
    wr_i16_le(&p, (int16_t)joystickport);

    /* 4. Controls (4*2 + 8*2 + 4*2 + 4*2 = 40 bytes) */
    for (int i = 0; i < 4; i++)
        wr_i16_le(&p, (int16_t)dirscan[i]);
    for (int i = 0; i < NUMBUTTONS; i++)
        wr_i16_le(&p, (int16_t)buttonscan[i]);
    for (int i = 0; i < 4; i++)
        wr_i16_le(&p, (int16_t)buttonmouse[i]);
    for (int i = 0; i < 4; i++)
        wr_i16_le(&p, (int16_t)buttonjoy[i]);

    /* 5. View size & mouse adjustment (2 + 2 = 4 bytes) */
    wr_i16_le(&p, (int16_t)viewsize);
    wr_i16_le(&p, (int16_t)mouseadjustment);

    return (size_t)(p - buf);
}

boolean WL_DeserializeConfig (const uint8_t *buf, size_t size)
{
    if (!buf || size < WL_CONFIG_SIZE)
        return false;

    const uint8_t *p = buf;

    for (int i = 0; i < MaxScores; i++)
    {
        rd_bytes(&p, Scores[i].name, MaxHighName + 1);
        Scores[i].name[MaxHighName] = '\0';
        Scores[i].score = rd_i32_le(&p);
        Scores[i].completed = rd_u16_le(&p);
        Scores[i].episode = rd_u16_le(&p);
    }

    SDMode sd = (SDMode)rd_i16_le(&p);
    SMMode sm = (SMMode)rd_i16_le(&p);
    SDSMode sds = (SDSMode)rd_i16_le(&p);

    mouseenabled = (boolean)rd_i16_le(&p);
    joystickenabled = (boolean)rd_i16_le(&p);
    joypadenabled = (boolean)rd_i16_le(&p);
    joystickprogressive = (boolean)rd_i16_le(&p);
    joystickport = (int)rd_i16_le(&p);

    for (int i = 0; i < 4; i++)
        dirscan[i] = (int)rd_i16_le(&p);
    for (int i = 0; i < NUMBUTTONS; i++)
        buttonscan[i] = (int)rd_i16_le(&p);
    for (int i = 0; i < 4; i++)
        buttonmouse[i] = (int)rd_i16_le(&p);
    for (int i = 0; i < 4; i++)
        buttonjoy[i] = (int)rd_i16_le(&p);

    viewsize = (int)rd_i16_le(&p);
    mouseadjustment = (int)rd_i16_le(&p);

    /* Hardware capability checks per ReadConfig */
    if (sd == sdm_AdLib && !AdLibPresent && !SoundBlasterPresent)
    {
        sd = sdm_PC;
        sm = smm_Off;
    }
    if ((sds == sds_SoundBlaster && !SoundBlasterPresent) ||
        (sds == sds_SoundSource && !SoundSourcePresent))
    {
        sds = sds_Off;
    }
    if (!MousePresent)
        mouseenabled = false;
    if (joystickport < 0 || joystickport >= MaxJoys)
    {
        joystickport = 0;
        joystickenabled = false;
    }
    else if (!JoysPresent[joystickport])
    {
        joystickenabled = false;
    }

    MainMenu[6].active = 1;
    MainItems.curpos = 0;

    SD_SetMusicMode(sm);
    SD_SetSoundMode(sd);
    SD_SetDigiDevice(sds);

    return true;
}

boolean WL_WriteConfigFile (const char *path)
{
    char filepath[512];
    if (path && *path)
        snprintf(filepath, sizeof(filepath), "%s", path);
    else
        WL_GetConfigFilePath(filepath, sizeof(filepath));

    char dir[512];
    snprintf(dir, sizeof(dir), "%s", filepath);
    char *slash = strrchr(dir, '/');
    if (slash)
    {
        *slash = '\0';
        WL_EnsureDir(dir);
    }

    uint8_t buf[WL_CONFIG_SIZE];
    size_t len = WL_SerializeConfig(buf, sizeof(buf));
    if (len != WL_CONFIG_SIZE)
        return false;

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
    if (fd < 0)
        return false;

    ssize_t written = write(fd, buf, len);
    close(fd);
    return written == (ssize_t)len;
}

boolean WL_ReadConfigFile (const char *path)
{
    char filepath[512];
    if (path && *path)
        snprintf(filepath, sizeof(filepath), "%s", path);
    else
        WL_GetConfigFilePath(filepath, sizeof(filepath));

    int fd = open(filepath, O_RDONLY | O_BINARY);
    if (fd < 0)
        return false;

    uint8_t buf[WL_CONFIG_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);

    if (n != (ssize_t)WL_CONFIG_SIZE)
        return false;

    return WL_DeserializeConfig(buf, (size_t)n);
}

/* -------------------------------------------------------------------------
 * SaveGame Serialization
 * ------------------------------------------------------------------------- */

extern int32_t DoChecksum (byte far *source, unsigned size, int32_t checksum);

size_t WL_SerializeSaveGame (uint8_t *buf, size_t max_size)
{
    if (!buf || max_size < 32768)
        return 0;

    /* Build sequential list of active actors in the chain */
    objtype *chain_ptrs[MAXACTORS];
    int chain_count = 0;
    for (objtype *ob = player; ob && chain_count < MAXACTORS; ob = ob->next)
    {
        chain_ptrs[chain_count++] = ob;
    }

    uint8_t *p = buf;
    int32_t checksum = 0;
    const uint8_t *p_start;

    /* 1. gamestate (66 bytes) */
    p_start = p;
    wr_i16_le(&p, (int16_t)gamestate.difficulty);
    wr_i16_le(&p, (int16_t)gamestate.mapon);
    wr_i32_le(&p, (int32_t)gamestate.oldscore);
    wr_i32_le(&p, (int32_t)gamestate.score);
    wr_i32_le(&p, (int32_t)gamestate.nextextra);
    wr_i16_le(&p, (int16_t)gamestate.lives);
    wr_i16_le(&p, (int16_t)gamestate.health);
    wr_i16_le(&p, (int16_t)gamestate.ammo);
    wr_i16_le(&p, (int16_t)gamestate.keys);
    wr_i16_le(&p, (int16_t)gamestate.bestweapon);
    wr_i16_le(&p, (int16_t)gamestate.weapon);
    wr_i16_le(&p, (int16_t)gamestate.chosenweapon);
    wr_i16_le(&p, (int16_t)gamestate.faceframe);
    wr_i16_le(&p, (int16_t)gamestate.attackframe);
    wr_i16_le(&p, (int16_t)gamestate.attackcount);
    wr_i16_le(&p, (int16_t)gamestate.weaponframe);
    wr_i16_le(&p, (int16_t)gamestate.episode);
    wr_i16_le(&p, (int16_t)gamestate.secretcount);
    wr_i16_le(&p, (int16_t)gamestate.treasurecount);
    wr_i16_le(&p, (int16_t)gamestate.killcount);
    wr_i16_le(&p, (int16_t)gamestate.secrettotal);
    wr_i16_le(&p, (int16_t)gamestate.treasuretotal);
    wr_i16_le(&p, (int16_t)gamestate.killtotal);
    wr_i32_le(&p, (int32_t)gamestate.TimeCount);
    wr_i32_le(&p, (int32_t)gamestate.killx);
    wr_i32_le(&p, (int32_t)gamestate.killy);
    wr_i16_le(&p, (int16_t)gamestate.victoryflag);
    checksum = DoChecksum((byte far *)p_start, WL_GAMESTATE_SIZE, checksum);

    /* 2. LevelRatios (80 bytes) */
    p_start = p;
    for (int i = 0; i < 8; i++)
    {
        wr_i16_le(&p, (int16_t)LevelRatios[i].kill);
        wr_i16_le(&p, (int16_t)LevelRatios[i].secret);
        wr_i16_le(&p, (int16_t)LevelRatios[i].treasure);
        wr_i32_le(&p, (int32_t)LevelRatios[i].time);
    }
    checksum = DoChecksum((byte far *)p_start, WL_LEVELRATIOS_SIZE, checksum);

    /* 3. tilemap (4096 bytes) */
    p_start = p;
    wr_bytes(&p, tilemap, sizeof(tilemap));
    checksum = DoChecksum((byte far *)p_start, WL_TILEMAP_SIZE, checksum);

    /* 4. actorat (8192 bytes) */
    p_start = p;
    for (int x = 0; x < 64; x++)
    {
        for (int y = 0; y < 64; y++)
        {
            objtype *ob = actorat[x][y];
            uint16_t actnum;
            if ((uintptr_t)ob > 1024)
            {
                int found_idx = -1;
                for (int i = 0; i < chain_count; i++)
                {
                    if (chain_ptrs[i] == ob)
                    {
                        found_idx = i;
                        break;
                    }
                }
                if (found_idx >= 0)
                    actnum = (uint16_t)(0x8000 | found_idx);
                else
                    actnum = 0;
            }
            else
            {
                actnum = (uint16_t)(uintptr_t)ob;
            }
            wr_u16_le(&p, actnum);
        }
    }
    checksum = DoChecksum((byte far *)p_start, WL_ACTORAT_SIZE, checksum);

    /* 5. areaconnect (1369 bytes) and areabyplayer (74 bytes) */
    wr_bytes(&p, areaconnect, sizeof(areaconnect));
    for (int i = 0; i < NUMAREAS; i++)
        wr_i16_le(&p, (int16_t)areabyplayer[i]);

    /* 6. Actor chain (60 bytes per actor, ending with badobject) */
    for (int i = 0; i < chain_count; i++)
    {
        objtype *ob = chain_ptrs[i];
        wr_i16_le(&p, (int16_t)ob->active);
        wr_i16_le(&p, (int16_t)ob->ticcount);
        wr_i16_le(&p, (int16_t)ob->obclass);
        wr_u16_le(&p, WL_StateToID(ob->state));
        wr_u8(&p, ob->flags);
        wr_u8(&p, 0); /* pad */
        wr_i32_le(&p, (int32_t)ob->distance);
        wr_i16_le(&p, (int16_t)ob->dir);
        wr_i32_le(&p, (int32_t)ob->x);
        wr_i32_le(&p, (int32_t)ob->y);
        wr_u16_le(&p, (uint16_t)ob->tilex);
        wr_u16_le(&p, (uint16_t)ob->tiley);
        wr_u8(&p, ob->areanumber);
        wr_u8(&p, 0); /* pad */
        wr_i16_le(&p, (int16_t)ob->viewx);
        wr_u16_le(&p, (uint16_t)ob->viewheight);
        wr_i32_le(&p, (int32_t)ob->transx);
        wr_i32_le(&p, (int32_t)ob->transy);
        wr_i16_le(&p, (int16_t)ob->angle);
        wr_i16_le(&p, (int16_t)ob->hitpoints);
        wr_i32_le(&p, (int32_t)ob->speed);
        wr_i16_le(&p, (int16_t)ob->temp1);
        wr_i16_le(&p, (int16_t)ob->temp2);
        wr_i16_le(&p, (int16_t)ob->temp3);
        wr_u16_le(&p, 0); /* next */
        wr_u16_le(&p, 0); /* prev */
    }

    /* End-of-file actor marker (60 bytes) */
    wr_i16_le(&p, (int16_t)ac_badobject);
    for (int k = 0; k < 58; k++)
        wr_u8(&p, 0);

    /* 7. laststatobj (2 bytes) and statobjlist (3200 bytes) */
    p_start = p;
    uint16_t last_stat_idx = (uint16_t)(laststatobj ? (laststatobj - statobjlist) : 0);
    wr_u16_le(&p, last_stat_idx);
    checksum = DoChecksum((byte far *)p_start, WL_LASTSTATOBJ_SIZE, checksum);

    p_start = p;
    for (int i = 0; i < MAXSTATS; i++)
    {
        wr_u8(&p, statobjlist[i].tilex);
        wr_u8(&p, statobjlist[i].tiley);
        wr_u16_le(&p, 0); /* dummy visspot pointer placeholder */
        wr_i16_le(&p, (int16_t)statobjlist[i].shapenum);
        wr_u8(&p, statobjlist[i].flags);
        wr_u8(&p, statobjlist[i].itemnumber);
    }
    checksum = DoChecksum((byte far *)p_start, WL_STATOBJLIST_SIZE, checksum);

    /* 8. doorposition (128 bytes) and doorobjlist (640 bytes) */
    p_start = p;
    for (int i = 0; i < MAXDOORS; i++)
        wr_u16_le(&p, (uint16_t)doorposition[i]);
    checksum = DoChecksum((byte far *)p_start, WL_DOORPOSITION_SIZE, checksum);

    p_start = p;
    for (int i = 0; i < MAXDOORS; i++)
    {
        wr_u8(&p, doorobjlist[i].tilex);
        wr_u8(&p, doorobjlist[i].tiley);
        wr_i16_le(&p, (int16_t)doorobjlist[i].vertical);
        wr_u8(&p, doorobjlist[i].lock);
        wr_u8(&p, 0); /* pad */
        wr_i16_le(&p, (int16_t)doorobjlist[i].action);
        wr_i16_le(&p, (int16_t)doorobjlist[i].ticcount);
    }
    checksum = DoChecksum((byte far *)p_start, WL_DOOROBJLIST_SIZE, checksum);

    /* 9. Pushwalls (10 bytes total) */
    p_start = p;
    wr_u16_le(&p, (uint16_t)pwallstate);
    checksum = DoChecksum((byte far *)p_start, 2, checksum);

    p_start = p;
    wr_u16_le(&p, (uint16_t)pwallx);
    checksum = DoChecksum((byte far *)p_start, 2, checksum);

    p_start = p;
    wr_u16_le(&p, (uint16_t)pwally);
    checksum = DoChecksum((byte far *)p_start, 2, checksum);

    p_start = p;
    wr_i16_le(&p, (int16_t)pwalldir);
    checksum = DoChecksum((byte far *)p_start, 2, checksum);

    p_start = p;
    wr_u16_le(&p, (uint16_t)pwallpos);
    checksum = DoChecksum((byte far *)p_start, 2, checksum);

    /* 10. Checksum (4 bytes) */
    wr_i32_le(&p, checksum);

    return (size_t)(p - buf);
}

boolean WL_DeserializeSaveGame (const uint8_t *buf, size_t size)
{
    if (!buf || size < WL_SAVE_MIN_SIZE)
        return false;

    /* ---------------------------------------------------------------------
     * Pass 1: Strict validation (read-only, no modification of live state)
     * --------------------------------------------------------------------- */
    const uint8_t *v = buf;
    const uint8_t *end = buf + size;

    /* 1. gamestate (66 bytes) */
    int16_t v_diff = (int16_t)rd_i16_le(&v);
    int16_t v_map = (int16_t)rd_i16_le(&v);
    if (v_diff < 0 || v_diff > 3 || v_map < 0 || v_map > 9)
        return false;
    v += 34; /* skip to episode at offset 38 */
    int16_t v_episode = (int16_t)rd_i16_le(&v);
    if (v_episode < 0 || v_episode > 5 || (v_map + 10 * v_episode) >= NUMMAPS)
        return false;
    v += 26; /* remainder of gamestate (66 total) */

    /* 2. LevelRatios (80 bytes) */
    v += WL_LEVELRATIOS_SIZE;

    /* 3. tilemap (4096 bytes) */
    for (int i = 0; i < 64 * 64; i++)
    {
        uint8_t tile = rd_u8(&v);
        if ((tile & 0x80) && (tile & 0x7f) >= MAXDOORS)
            return false;
    }

    /* 4. actorat (8192 bytes) - deferred until actor count is known */
    const uint8_t *v_actorat = v;
    v += WL_ACTORAT_SIZE;

    /* 5. areaconnect (1369 bytes) and areabyplayer (74 bytes) */
    v += WL_AREACONNECT_SIZE;
    v += WL_AREABYPLAYER_SIZE;

    /* 6. Actors chain */
    /* Validate player actor record */
    if (v + WL_ACTOR_SIZE > end)
        return false;
    activetype p_act = (activetype)rd_i16_le(&v);
    if (p_act == ac_badobject)
        return false;
    v += 4; /* ticcount, obclass */
    uint16_t p_st = rd_u16_le(&v);
    if (p_st == 0 || p_st > WL_NUM_STATES)
        return false;
    v += 16; /* flags (1), pad (1), distance (4), dir (2), x (4), y (4) */
    uint16_t p_tx = rd_u16_le(&v);
    uint16_t p_ty = rd_u16_le(&v);
    if (p_tx >= MAPSIZE || p_ty >= MAPSIZE)
        return false;
    uint8_t p_area = rd_u8(&v);
    if (p_area >= NUMAREAS)
        return false;
    v += 31; /* pad (1) + remainder of player record (30) */

    /* Validate follower actors until ac_badobject */
    int validated_actors = 1;
    boolean found_term = false;
    while (v + WL_ACTOR_SIZE <= end)
    {
        activetype act = (activetype)rd_i16_le(&v);
        if (act == ac_badobject)
        {
            v += 58;
            found_term = true;
            break;
        }
        validated_actors++;
        if (validated_actors > MAXACTORS)
            return false;
        v += 4; /* ticcount, obclass */
        uint16_t st = rd_u16_le(&v);
        if (st == 0 || st > WL_NUM_STATES)
            return false;
        v += 16; /* flags, pad, distance, dir, x, y */
        uint16_t tx = rd_u16_le(&v);
        uint16_t ty = rd_u16_le(&v);
        if (tx >= MAPSIZE || ty >= MAPSIZE)
            return false;
        uint8_t area = rd_u8(&v);
        if (area >= NUMAREAS)
            return false;
        v += 31; /* pad + remainder of record */
    }

    if (!found_term)
        return false;

    /* Validate actorat entries against actor count and valid tile/door values */
    for (int i = 0; i < 64 * 64; i++)
    {
        uint16_t actval = rd_u16_le(&v_actorat);
        if (actval & 0x8000)
        {
            uint16_t idx = actval & (uint16_t)~0x8000;
            if (idx >= validated_actors)
                return false;
        }
        else
        {
            if (actval >= 256)
                return false;
        }
    }

    /* Trailing section must match WL_TRAILING_SIZE exactly */
    if ((size_t)(end - v) != WL_TRAILING_SIZE)
        return false;

    /* Validate laststatobj */
    uint16_t v_laststat = rd_u16_le(&v);
    if (v_laststat >= MAXSTATS)
        return false;

    /* Validate statobjlist coordinates */
    for (int i = 0; i < MAXSTATS; i++)
    {
        uint8_t tx = rd_u8(&v);
        uint8_t ty = rd_u8(&v);
        if (tx >= 64 || ty >= 64)
            return false;
        v += 6;
    }

    /* Skip doorposition (128 bytes) */
    v += WL_DOORPOSITION_SIZE;

    /* Validate doorobjlist coordinates and action */
    for (int i = 0; i < MAXDOORS; i++)
    {
        uint8_t d_tx = rd_u8(&v);
        uint8_t d_ty = rd_u8(&v);
        if (d_tx >= 64 || d_ty >= 64)
            return false;
        v += 4; /* vertical, lock, pad */
        int16_t d_act = rd_i16_le(&v);
        if (d_act < 0 || d_act > 3)
            return false;
        v += 2; /* ticcount */
    }

    /* Validate pushwall coordinates */
    v += 2; /* pwallstate */
    uint16_t pw_x = rd_u16_le(&v);
    uint16_t pw_y = rd_u16_le(&v);
    if (pw_x >= 64 || pw_y >= 64)
        return false;
    v += 4; /* pwalldir, pwallpos */
    v += 4; /* checksum */

    if (v != end)
        return false;

    /* ---------------------------------------------------------------------
     * Pass 2: Deserialization into live game state
     * --------------------------------------------------------------------- */
    const uint8_t *p = buf;
    int32_t checksum = 0;
    const uint8_t *p_start;

    /* 1. gamestate (66 bytes) */
    p_start = p;
    gamestate.difficulty = rd_i16_le(&p);
    gamestate.mapon = rd_i16_le(&p);
    gamestate.oldscore = rd_i32_le(&p);
    gamestate.score = rd_i32_le(&p);
    gamestate.nextextra = rd_i32_le(&p);
    gamestate.lives = rd_i16_le(&p);
    gamestate.health = rd_i16_le(&p);
    gamestate.ammo = rd_i16_le(&p);
    gamestate.keys = rd_i16_le(&p);
    gamestate.bestweapon = (weapontype)rd_i16_le(&p);
    gamestate.weapon = (weapontype)rd_i16_le(&p);
    gamestate.chosenweapon = (weapontype)rd_i16_le(&p);
    gamestate.faceframe = rd_i16_le(&p);
    gamestate.attackframe = rd_i16_le(&p);
    gamestate.attackcount = rd_i16_le(&p);
    gamestate.weaponframe = rd_i16_le(&p);
    gamestate.episode = rd_i16_le(&p);
    gamestate.secretcount = rd_i16_le(&p);
    gamestate.treasurecount = rd_i16_le(&p);
    gamestate.killcount = rd_i16_le(&p);
    gamestate.secrettotal = rd_i16_le(&p);
    gamestate.treasuretotal = rd_i16_le(&p);
    gamestate.killtotal = rd_i16_le(&p);
    gamestate.TimeCount = rd_i32_le(&p);
    gamestate.killx = rd_i32_le(&p);
    gamestate.killy = rd_i32_le(&p);
    gamestate.victoryflag = (boolean)rd_i16_le(&p);
    checksum = DoChecksum((byte far *)p_start, WL_GAMESTATE_SIZE, checksum);

    /* 2. LevelRatios (80 bytes) */
    p_start = p;
    for (int i = 0; i < 8; i++)
    {
        LevelRatios[i].kill = rd_i16_le(&p);
        LevelRatios[i].secret = rd_i16_le(&p);
        LevelRatios[i].treasure = rd_i16_le(&p);
        LevelRatios[i].time = rd_i32_le(&p);
    }
    checksum = DoChecksum((byte far *)p_start, WL_LEVELRATIOS_SIZE, checksum);

    /* Setup level geometry */
    SetupGameLevel();

    /* 3. tilemap (4096 bytes) */
    p_start = p;
    rd_bytes(&p, tilemap, sizeof(tilemap));
    checksum = DoChecksum((byte far *)p_start, WL_TILEMAP_SIZE, checksum);

    /* 4. actorat raw entries (8192 bytes) */
    uint16_t actorat_raw[64][64];
    p_start = p;
    for (int x = 0; x < 64; x++)
        for (int y = 0; y < 64; y++)
            actorat_raw[x][y] = rd_u16_le(&p);
    checksum = DoChecksum((byte far *)p_start, WL_ACTORAT_SIZE, checksum);

    /* 5. areaconnect (1369 bytes) and areabyplayer (74 bytes) */
    rd_bytes(&p, areaconnect, sizeof(areaconnect));
    for (int i = 0; i < NUMAREAS; i++)
        areabyplayer[i] = (boolean)rd_i16_le(&p);

    /* 6. Actors chain */
    InitActorList();

    objtype *chain_ptrs[MAXACTORS];
    int chain_count = 0;

    /* Player is the first actor */
    chain_ptrs[chain_count++] = player;
    player->active = (activetype)rd_i16_le(&p);
    player->ticcount = rd_i16_le(&p);
    player->obclass = (classtype)rd_i16_le(&p);
    player->state = WL_IDToState(rd_u16_le(&p));
    player->flags = rd_u8(&p);
    rd_u8(&p);
    player->distance = rd_i32_le(&p);
    player->dir = (dirtype)rd_i16_le(&p);
    player->x = rd_i32_le(&p);
    player->y = rd_i32_le(&p);
    player->tilex = rd_u16_le(&p);
    player->tiley = rd_u16_le(&p);
    player->areanumber = rd_u8(&p);
    rd_u8(&p);
    player->viewx = rd_i16_le(&p);
    player->viewheight = rd_u16_le(&p);
    player->transx = rd_i32_le(&p);
    player->transy = rd_i32_le(&p);
    player->angle = rd_i16_le(&p);
    player->hitpoints = rd_i16_le(&p);
    player->speed = rd_i32_le(&p);
    player->temp1 = rd_i16_le(&p);
    player->temp2 = rd_i16_le(&p);
    player->temp3 = rd_i16_le(&p);
    rd_u16_le(&p);
    rd_u16_le(&p);

    while (1)
    {
        activetype act = (activetype)rd_i16_le(&p);
        if (act == ac_badobject)
        {
            p += 58;
            break;
        }

        GetNewActor();
        chain_ptrs[chain_count++] = new;
        new->active = act;
        new->ticcount = rd_i16_le(&p);
        new->obclass = (classtype)rd_i16_le(&p);
        new->state = WL_IDToState(rd_u16_le(&p));
        new->flags = rd_u8(&p);
        rd_u8(&p);
        new->distance = rd_i32_le(&p);
        new->dir = (dirtype)rd_i16_le(&p);
        new->x = rd_i32_le(&p);
        new->y = rd_i32_le(&p);
        new->tilex = rd_u16_le(&p);
        new->tiley = rd_u16_le(&p);
        new->areanumber = rd_u8(&p);
        rd_u8(&p);
        new->viewx = rd_i16_le(&p);
        new->viewheight = rd_u16_le(&p);
        new->transx = rd_i32_le(&p);
        new->transy = rd_i32_le(&p);
        new->angle = rd_i16_le(&p);
        new->hitpoints = rd_i16_le(&p);
        new->speed = rd_i32_le(&p);
        new->temp1 = rd_i16_le(&p);
        new->temp2 = rd_i16_le(&p);
        new->temp3 = rd_i16_le(&p);
        rd_u16_le(&p);
        rd_u16_le(&p);
    }

    /* Fixup actorat using chain_ptrs */
    for (int x = 0; x < 64; x++)
    {
        for (int y = 0; y < 64; y++)
        {
            uint16_t val = actorat_raw[x][y];
            if (val & 0x8000)
            {
                int idx = val & ~0x8000;
                if (idx >= 0 && idx < chain_count)
                    actorat[x][y] = chain_ptrs[idx];
                else
                    actorat[x][y] = NULL;
            }
            else
            {
                actorat[x][y] = (objtype *)(uintptr_t)val;
            }
        }
    }

    /* 7. laststatobj (2 bytes) and statobjlist (3200 bytes) */
    p_start = p;
    uint16_t last_stat_idx = rd_u16_le(&p);
    laststatobj = &statobjlist[last_stat_idx];
    checksum = DoChecksum((byte far *)p_start, WL_LASTSTATOBJ_SIZE, checksum);

    p_start = p;
    for (int i = 0; i < MAXSTATS; i++)
    {
        statobjlist[i].tilex = rd_u8(&p);
        statobjlist[i].tiley = rd_u8(&p);
        rd_u16_le(&p);
        statobjlist[i].visspot = &spotvis[statobjlist[i].tilex][statobjlist[i].tiley];
        statobjlist[i].shapenum = rd_i16_le(&p);
        statobjlist[i].flags = rd_u8(&p);
        statobjlist[i].itemnumber = rd_u8(&p);
    }
    checksum = DoChecksum((byte far *)p_start, WL_STATOBJLIST_SIZE, checksum);

    /* 8. doorposition (128 bytes) and doorobjlist (640 bytes) */
    p_start = p;
    for (int i = 0; i < MAXDOORS; i++)
        doorposition[i] = rd_u16_le(&p);
    checksum = DoChecksum((byte far *)p_start, WL_DOORPOSITION_SIZE, checksum);

    p_start = p;
    for (int i = 0; i < MAXDOORS; i++)
    {
        doorobjlist[i].tilex = rd_u8(&p);
        doorobjlist[i].tiley = rd_u8(&p);
        doorobjlist[i].vertical = (boolean)rd_i16_le(&p);
        doorobjlist[i].lock = rd_u8(&p);
        rd_u8(&p);
        doorobjlist[i].action = rd_i16_le(&p);
        doorobjlist[i].ticcount = rd_i16_le(&p);
    }
    checksum = DoChecksum((byte far *)p_start, WL_DOOROBJLIST_SIZE, checksum);

    /* 9. Pushwalls (10 bytes) */
    p_start = p;
    pwallstate = rd_u16_le(&p);
    checksum = DoChecksum((byte far *)p_start, 2, checksum);

    p_start = p;
    pwallx = rd_u16_le(&p);
    checksum = DoChecksum((byte far *)p_start, 2, checksum);

    p_start = p;
    pwally = rd_u16_le(&p);
    checksum = DoChecksum((byte far *)p_start, 2, checksum);

    p_start = p;
    pwalldir = rd_i16_le(&p);
    checksum = DoChecksum((byte far *)p_start, 2, checksum);

    p_start = p;
    pwallpos = rd_u16_le(&p);
    checksum = DoChecksum((byte far *)p_start, 2, checksum);

    /* 10. Checksum (4 bytes) */
    int32_t oldchecksum = rd_i32_le(&p);

    if (oldchecksum != checksum)
    {
        /* Checksum mismatch: reset stats per original game */
        gamestate.score = 0;
        gamestate.lives = 1;
        gamestate.weapon = gamestate.chosenweapon = gamestate.bestweapon = wp_pistol;
        gamestate.ammo = 8;
    }

    return true;
}

boolean WL_WriteSaveGameFile (int fd)
{
    size_t buf_size = 131072;
    uint8_t *buf = (uint8_t *)malloc(buf_size);
    if (!buf)
        return false;

    size_t len = WL_SerializeSaveGame(buf, buf_size);
    if (!len)
    {
        free(buf);
        return false;
    }

    ssize_t written = write(fd, buf, len);
    free(buf);
    return written == (ssize_t)len;
}

boolean WL_ReadSaveGameFile (int fd)
{
    struct stat st;
    size_t file_size = 131072;
    if (fstat(fd, &st) == 0 && st.st_size > 0)
        file_size = (size_t)st.st_size;

    uint8_t *buf = (uint8_t *)malloc(file_size);
    if (!buf)
        return false;

    size_t total = 0;
    while (total < file_size)
    {
        ssize_t n = read(fd, buf + total, file_size - total);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            free(buf);
            return false;
        }
        if (n == 0) break;
        total += n;
    }

    boolean ok = WL_DeserializeSaveGame(buf, total);
    free(buf);
    return ok;
}
