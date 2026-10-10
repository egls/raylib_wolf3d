/*
 * save_test.c - Unit tests for XDG path resolution and explicit
 * little-endian serialization of config, savegames, and high scores.
 */
#include "WL_DEF.H"
#define WL_STATES_DEFINE
#include "wl_states.h"
#include "wl_save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define CHECK(x)                                                               \
    do {                                                                       \
        if (!(x)) {                                                            \
            fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #x);     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

/* -------------------------------------------------------------------------
 * Required game globals & stubs for headless unit testing
 * ------------------------------------------------------------------------- */

char extension[5] = "WL1";
HighScore Scores[MaxScores];

SDMode SoundMode = sdm_Off;
SMMode MusicMode = smm_Off;
SDSMode DigiMode = sds_Off;

boolean mouseenabled = false;
boolean joystickenabled = false;
boolean joypadenabled = false;
boolean joystickprogressive = false;
int joystickport = 0;

int dirscan[4];
int buttonscan[NUMBUTTONS];
int buttonmouse[4];
int buttonjoy[4];

int viewsize = 15;
int mouseadjustment = 5;

boolean MousePresent = true;
boolean JoysPresent[MaxJoys];
boolean SoundBlasterPresent = true;
boolean AdLibPresent = true;
boolean SoundSourcePresent = false;

gametype gamestate;
LRstruct LevelRatios[8];
byte tilemap[MAPSIZE][MAPSIZE];
byte spotvis[MAPSIZE][MAPSIZE];
objtype *actorat[MAPSIZE][MAPSIZE];
boolean areabyplayer[NUMAREAS];
byte areaconnect[NUMAREAS][NUMAREAS];

statobj_t statobjlist[MAXSTATS];
statobj_t *laststatobj;

doorobj_t doorobjlist[MAXDOORS];
unsigned doorposition[MAXDOORS];

unsigned pwallx;
unsigned pwally;
int pwalldir;
unsigned pwallstate;
unsigned pwallpos;

objtype objlist[MAXACTORS];
objtype *objfreelist = NULL;
objtype *lastobj = NULL;
objtype *new = NULL;
objtype *player = NULL;
int objcount = 0;

CP_itemtype MainMenu[20];
CP_iteminfo MainItems;

boolean SD_SetSoundMode (SDMode mode)
{
    SoundMode = mode;
    return true;
}

boolean SD_SetMusicMode (SMMode mode)
{
    MusicMode = mode;
    return true;
}

void SD_SetDigiDevice (SDSMode mode)
{
    DigiMode = mode;
}

void SetupGameLevel (void)
{
    /* Headless stub: savegame deserialization overwrites map/actor state */
}

long DoChecksum (byte far *source, unsigned size, long checksum)
{
    unsigned i;
    for (i = 0; i < size - 1; i++)
        checksum += source[i] ^ source[i + 1];
    return checksum;
}

void InitActorList (void)
{
    int i;
    for (i = 0; i < MAXACTORS; i++)
    {
        objlist[i].prev = &objlist[i + 1];
        objlist[i].next = NULL;
    }
    objlist[MAXACTORS - 1].prev = NULL;

    objfreelist = &objlist[0];
    lastobj = NULL;
    objcount = 0;

    GetNewActor();
    player = new;
}

void GetNewActor (void)
{
    if (!objfreelist)
    {
        fprintf(stderr, "GetNewActor: No free spots!\n");
        exit(1);
    }

    new = objfreelist;
    objfreelist = new->prev;
    memset(new, 0, sizeof(*new));

    if (lastobj)
        lastobj->next = new;
    new->prev = lastobj;
    new->next = NULL;
    lastobj = new;
    objcount++;
}

/* -------------------------------------------------------------------------
 * Test 1: XDG Path Resolution
 * ------------------------------------------------------------------------- */

static int test_xdg_paths (void)
{
    char buf[512];
    const char *orig_home = getenv("HOME");
    const char *orig_xdg_cfg = getenv("XDG_CONFIG_HOME");
    const char *orig_xdg_data = getenv("XDG_DATA_HOME");

    /* Default resolution without XDG variables */
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");
    setenv("HOME", "/tmp/wolf_mock_home", 1);

    WL_GetConfigDir(buf, sizeof(buf));
    CHECK(strcmp(buf, "/tmp/wolf_mock_home/.config/raylib_wolf3d") == 0);

    WL_GetSaveDir(buf, sizeof(buf));
    CHECK(strcmp(buf, "/tmp/wolf_mock_home/.local/share/raylib_wolf3d") == 0);

    WL_GetConfigFilePath(buf, sizeof(buf));
    CHECK(strcmp(buf, "/tmp/wolf_mock_home/.config/raylib_wolf3d/CONFIG.WL1") == 0);

    WL_GetSaveFilePath(0, buf, sizeof(buf));
    CHECK(strcmp(buf, "/tmp/wolf_mock_home/.local/share/raylib_wolf3d/SAVEGAM0.WL1") == 0);

    WL_GetSaveFilePath(5, buf, sizeof(buf));
    CHECK(strcmp(buf, "/tmp/wolf_mock_home/.local/share/raylib_wolf3d/SAVEGAM5.WL1") == 0);

    /* Slot clamp test: negative slot -> 0, >9 -> 9 */
    WL_GetSaveFilePath(-1, buf, sizeof(buf));
    CHECK(strcmp(buf, "/tmp/wolf_mock_home/.local/share/raylib_wolf3d/SAVEGAM0.WL1") == 0);

    WL_GetSaveFilePath(99, buf, sizeof(buf));
    CHECK(strcmp(buf, "/tmp/wolf_mock_home/.local/share/raylib_wolf3d/SAVEGAM9.WL1") == 0);

    /* Explicit XDG environment variables */
    setenv("XDG_CONFIG_HOME", "/custom/config_path", 1);
    setenv("XDG_DATA_HOME", "/custom/data_path", 1);

    WL_GetConfigDir(buf, sizeof(buf));
    CHECK(strcmp(buf, "/custom/config_path/raylib_wolf3d") == 0);

    WL_GetSaveDir(buf, sizeof(buf));
    CHECK(strcmp(buf, "/custom/data_path/raylib_wolf3d") == 0);

    WL_GetConfigFilePath(buf, sizeof(buf));
    CHECK(strcmp(buf, "/custom/config_path/raylib_wolf3d/CONFIG.WL1") == 0);

    WL_GetSaveFilePath(3, buf, sizeof(buf));
    CHECK(strcmp(buf, "/custom/data_path/raylib_wolf3d/SAVEGAM3.WL1") == 0);

    /* Restore environment */
    if (orig_home) setenv("HOME", orig_home, 1);
    else unsetenv("HOME");
    if (orig_xdg_cfg) setenv("XDG_CONFIG_HOME", orig_xdg_cfg, 1);
    else unsetenv("XDG_CONFIG_HOME");
    if (orig_xdg_data) setenv("XDG_DATA_HOME", orig_xdg_data, 1);
    else unsetenv("XDG_DATA_HOME");

    puts("PASS: XDG path resolution");
    return 0;
}

/* -------------------------------------------------------------------------
 * Test 2: State Mapping
 * ------------------------------------------------------------------------- */

static int test_state_mapping (void)
{
    CHECK(WL_NUM_STATES == 305);
    CHECK(WL_StateToID(NULL) == 0);
    CHECK(WL_IDToState(0) == NULL);
    CHECK(WL_IDToState(WL_NUM_STATES + 1) == NULL);

    /* s_player is first (ID 1) */
    CHECK(WL_StateToID(&s_player) == 1);
    CHECK(WL_IDToState(1) == &s_player);

    /* s_attack is second (ID 2) */
    CHECK(WL_StateToID(&s_attack) == 2);
    CHECK(WL_IDToState(2) == &s_attack);

    /* All states in table must map bijectively */
    for (uint16_t id = 1; id <= WL_NUM_STATES; id++)
    {
        statetype *st = WL_IDToState(id);
        CHECK(st != NULL);
        CHECK(WL_StateToID(st) == id);
    }

    puts("PASS: statetype ID mapping");
    return 0;
}

/* -------------------------------------------------------------------------
 * Test 3: Config Serialization & Byte Layout
 * ------------------------------------------------------------------------- */

static int test_config_roundtrip (void)
{
    uint8_t buf1[WL_CONFIG_SIZE];
    uint8_t buf2[WL_CONFIG_SIZE];

    /* Populate known values */
    memset(Scores, 0, sizeof(Scores));
    for (int i = 0; i < MaxScores; i++)
    {
        snprintf(Scores[i].name, sizeof(Scores[i].name), "Player_%02d", i + 1);
        Scores[i].score = (int32_t)(1000000 + i * 54321);
        Scores[i].completed = (uint16_t)(i + 1);
        Scores[i].episode = (uint16_t)(i % 3);
    }

    SoundMode = sdm_AdLib;
    MusicMode = smm_AdLib;
    DigiMode = sds_SoundBlaster;

    mouseenabled = true;
    joystickenabled = false;
    joypadenabled = true;
    joystickprogressive = false;
    joystickport = 1;

    for (int i = 0; i < 4; i++)
        dirscan[i] = 0x10 + i;
    for (int i = 0; i < NUMBUTTONS; i++)
        buttonscan[i] = 0x20 + i;
    for (int i = 0; i < 4; i++)
        buttonmouse[i] = 0x30 + i;
    for (int i = 0; i < 4; i++)
        buttonjoy[i] = 0x40 + i;

    viewsize = 18;
    mouseadjustment = 9;

    /* Serialize */
    size_t len = WL_SerializeConfig(buf1, sizeof(buf1));
    CHECK(len == WL_CONFIG_SIZE);
    CHECK(len == 522);

    /* Assert exact byte layout and offsets */
    /* Score 0: name (58 bytes), score (4 bytes LE), completed (2 bytes LE), episode (2 bytes LE) */
    CHECK(strcmp((char *)buf1, "Player_01") == 0);
    int32_t s0_score = (int32_t)buf1[58] | ((int32_t)buf1[59] << 8) |
                       ((int32_t)buf1[60] << 16) | ((int32_t)buf1[61] << 24);
    CHECK(s0_score == 1000000);
    uint16_t s0_comp = (uint16_t)buf1[62] | ((uint16_t)buf1[63] << 8);
    CHECK(s0_comp == 1);
    uint16_t s0_ep = (uint16_t)buf1[64] | ((uint16_t)buf1[65] << 8);
    CHECK(s0_ep == 0);

    /* Score 6 at offset 6 * 66 = 396 */
    CHECK(strcmp((char *)(buf1 + 396), "Player_07") == 0);

    /* Sound modes at offset 462 */
    int16_t sd = (int16_t)((uint16_t)buf1[462] | ((uint16_t)buf1[463] << 8));
    int16_t sm = (int16_t)((uint16_t)buf1[464] | ((uint16_t)buf1[465] << 8));
    int16_t sds = (int16_t)((uint16_t)buf1[466] | ((uint16_t)buf1[467] << 8));
    CHECK(sd == sdm_AdLib);
    CHECK(sm == smm_AdLib);
    CHECK(sds == sds_SoundBlaster);

    /* Input flags at offset 468 */
    CHECK(buf1[468] == 1 && buf1[469] == 0);  /* mouseenabled */
    CHECK(buf1[470] == 0 && buf1[471] == 0);  /* joystickenabled */
    CHECK(buf1[472] == 1 && buf1[473] == 0);  /* joypadenabled */
    CHECK(buf1[474] == 0 && buf1[475] == 0);  /* joystickprogressive */
    CHECK(buf1[476] == 1 && buf1[477] == 0);  /* joystickport */

    /* View size and mouse adjustment at offset 518 and 520 */
    int16_t vs = (int16_t)((uint16_t)buf1[518] | ((uint16_t)buf1[519] << 8));
    int16_t ma = (int16_t)((uint16_t)buf1[520] | ((uint16_t)buf1[521] << 8));
    CHECK(vs == 18);
    CHECK(ma == 9);

    /* Wipe globals */
    memset(Scores, 0, sizeof(Scores));
    SoundMode = sdm_Off;
    MusicMode = smm_Off;
    DigiMode = sds_Off;
    mouseenabled = false;
    joystickenabled = true;
    joypadenabled = false;
    joystickprogressive = true;
    joystickport = 0;
    viewsize = 0;
    mouseadjustment = 0;

    /* Deserialize */
    CHECK(WL_DeserializeConfig(buf1, len) == true);

    /* Verify restored globals match */
    for (int i = 0; i < MaxScores; i++)
    {
        char expected[64];
        snprintf(expected, sizeof(expected), "Player_%02d", i + 1);
        CHECK(strcmp(Scores[i].name, expected) == 0);
        CHECK(Scores[i].score == (int32_t)(1000000 + i * 54321));
        CHECK(Scores[i].completed == (uint16_t)(i + 1));
        CHECK(Scores[i].episode == (uint16_t)(i % 3));
    }

    CHECK(SoundMode == sdm_AdLib);
    CHECK(MusicMode == smm_AdLib);
    CHECK(DigiMode == sds_SoundBlaster);
    CHECK(mouseenabled == true);
    CHECK(joystickenabled == false);
    CHECK(joypadenabled == true);
    CHECK(joystickprogressive == false);
    CHECK(joystickport == 1);
    CHECK(viewsize == 18);
    CHECK(mouseadjustment == 9);

    for (int i = 0; i < 4; i++)
        CHECK(dirscan[i] == 0x10 + i);
    for (int i = 0; i < NUMBUTTONS; i++)
        CHECK(buttonscan[i] == 0x20 + i);
    for (int i = 0; i < 4; i++)
        CHECK(buttonmouse[i] == 0x30 + i);
    for (int i = 0; i < 4; i++)
        CHECK(buttonjoy[i] == 0x40 + i);

    /* Idempotent re-serialization check */
    size_t len2 = WL_SerializeConfig(buf2, sizeof(buf2));
    CHECK(len2 == WL_CONFIG_SIZE);
    CHECK(memcmp(buf1, buf2, WL_CONFIG_SIZE) == 0);

    puts("PASS: config serialization layout & roundtrip");
    return 0;
}

/* -------------------------------------------------------------------------
 * Test 4: Savegame Serialization & Roundtrip
 * ------------------------------------------------------------------------- */

static int test_savegame_roundtrip (void)
{
    uint8_t buf1[65536];
    uint8_t buf2[65536];

    /* 1. Setup gamestate */
    memset(&gamestate, 0, sizeof(gamestate));
    gamestate.difficulty = gd_hard;
    gamestate.mapon = 4;
    gamestate.score = 254800;
    gamestate.health = 92;
    gamestate.ammo = 75;
    gamestate.lives = 5;
    gamestate.weapon = wp_chaingun;
    gamestate.bestweapon = wp_chaingun;
    gamestate.chosenweapon = wp_chaingun;
    gamestate.keys = 3;
    gamestate.episode = 1;
    gamestate.attackframe = 2;
    gamestate.attackcount = 1;
    gamestate.weaponframe = 3;
    gamestate.TimeCount = 1234567;
    gamestate.killtotal = 80;
    gamestate.secrettotal = 5;
    gamestate.treasuretotal = 20;
    gamestate.killcount = 60;
    gamestate.secretcount = 3;
    gamestate.treasurecount = 18;

    /* 2. Setup LevelRatios */
    memset(LevelRatios, 0, sizeof(LevelRatios));
    for (int i = 0; i < 8; i++)
    {
        LevelRatios[i].kill = 80 + i;
        LevelRatios[i].secret = 50 + i;
        LevelRatios[i].treasure = 90 + i;
        LevelRatios[i].time = 3600 + i * 100;
    }

    /* 3. Setup tilemap */
    for (int y = 0; y < MAPSIZE; y++)
    {
        for (int x = 0; x < MAPSIZE; x++)
            tilemap[x][y] = (uint8_t)((x * 3 + y * 7) % 64);
    }

    /* 4. Setup areaconnect & areabyplayer */
    for (int a = 0; a < NUMAREAS; a++)
    {
        areabyplayer[a] = (uint8_t)(a % 2);
        for (int b = 0; b < NUMAREAS; b++)
            areaconnect[a][b] = (uint8_t)((a + b) % 2);
    }

    /* 5. Setup static objects */
    memset(statobjlist, 0, sizeof(statobjlist));
    for (int i = 0; i < 5; i++)
    {
        statobjlist[i].shapenum = 20 + i;
        statobjlist[i].tilex = 10 + i;
        statobjlist[i].tiley = 20 + i;
        statobjlist[i].visspot = &spotvis[10 + i][20 + i];
        statobjlist[i].flags = 1;
        statobjlist[i].itemnumber = 100 + i;
    }
    laststatobj = &statobjlist[5];

    /* 6. Setup doors */
    memset(doorobjlist, 0, sizeof(doorobjlist));
    memset(doorposition, 0, sizeof(doorposition));
    for (int i = 0; i < 4; i++)
    {
        doorobjlist[i].tilex = 5 + i;
        doorobjlist[i].tiley = 15 + i;
        doorobjlist[i].vertical = (boolean)(i % 2);
        doorobjlist[i].lock = (byte)(i % 3);
        doorobjlist[i].action = dr_closed;
        doorobjlist[i].ticcount = 50;
        doorposition[i] = (int16_t)(1000 + i);
    }

    /* 7. Setup pushwall */
    pwallx = 12;
    pwally = 34;
    pwalldir = di_north;
    pwallstate = 2;
    pwallpos = 28;

    /* 8. Setup actors chain */
    InitActorList();
    player->active = ac_yes;
    player->x = 0x11223344;
    player->y = 0x55667788;
    player->tilex = 5;
    player->tiley = 5;
    player->areanumber = 0;
    player->angle = 180;
    player->ticcount = 42;
    player->hitpoints = 92;
    player->state = &s_player;

    GetNewActor();
    objtype *guard = new;
    guard->active = ac_yes;
    guard->x = 0x22334455;
    guard->y = 0x66778899;
    guard->tilex = 6;
    guard->tiley = 6;
    guard->areanumber = 1;
    guard->angle = 90;
    guard->ticcount = 12;
    guard->hitpoints = 50;
    guard->speed = 800;
    guard->flags = FL_SHOOTABLE | FL_VISABLE;
    guard->state = &s_grdstand;

    GetNewActor();
    objtype *dog = new;
    dog->active = ac_yes;
    dog->x = 0x33445566;
    dog->y = 0x77889900;
    dog->tilex = 7;
    dog->tiley = 7;
    dog->areanumber = 2;
    dog->angle = 270;
    dog->ticcount = 8;
    dog->hitpoints = 25;
    dog->speed = 1200;
    dog->flags = FL_VISABLE;
    dog->state = &s_dogchase1;

    /* Setup actorat array: mixed tile numbers and actor pointers */
    memset(actorat, 0, sizeof(actorat));
    actorat[10][10] = (objtype *)(uintptr_t)128;   /* Door/wall tile marker */
    actorat[10][11] = (objtype *)(uintptr_t)200;   /* Tile marker */
    actorat[5][5] = player;                         /* Player actor */
    actorat[6][6] = guard;                          /* Guard actor */
    actorat[7][7] = dog;                            /* Dog actor */

    /* Serialize */
    size_t len1 = WL_SerializeSaveGame(buf1, sizeof(buf1));
    CHECK(len1 > 0);

    /* Assert exact serialized byte layout and offsets */
    /* 1. gamestate at offset 0 */
    int16_t diff = (int16_t)((uint16_t)buf1[0] | ((uint16_t)buf1[1] << 8));
    CHECK(diff == gd_hard);
    int16_t mapon = (int16_t)((uint16_t)buf1[2] | ((uint16_t)buf1[3] << 8));
    CHECK(mapon == 4);
    int32_t sc = (int32_t)buf1[8] | ((int32_t)buf1[9] << 8) |
                 ((int32_t)buf1[10] << 16) | ((int32_t)buf1[11] << 24);
    CHECK(sc == 254800);

    /* 2. LevelRatios at offset 66 */
    uint16_t lr0_kill = (uint16_t)buf1[66] | ((uint16_t)buf1[67] << 8);
    CHECK(lr0_kill == 80);

    /* 3. tilemap at offset 66 + 80 = 146 */
    CHECK(buf1[146] == tilemap[0][0]);

    /* 4. actorat at offset 146 + 4096 = 4242 */
    /* actorat[5][5] is player (actor index 0 -> serialized ID 0x8000) */
    size_t actorat_player_offset = 4242 + (5 * MAPSIZE + 5) * 2;
    uint16_t actorat_player_val = (uint16_t)buf1[actorat_player_offset] |
                                  ((uint16_t)buf1[actorat_player_offset + 1] << 8);
    CHECK(actorat_player_val == 0x8000);

    /* actorat[10][10] is tile marker 128 */
    size_t actorat_tile_offset = 4242 + (10 * MAPSIZE + 10) * 2;
    uint16_t actorat_tile_val = (uint16_t)buf1[actorat_tile_offset] |
                                ((uint16_t)buf1[actorat_tile_offset + 1] << 8);
    CHECK(actorat_tile_val == 128);

    /* 5. areaconnect at offset 4242 + 8192 = 12434 */
    CHECK(buf1[12434] == areaconnect[0][0]);

    /* 6. areabyplayer at offset 12434 + 1369 = 13803 */
    CHECK(buf1[13803] == areabyplayer[0]);

    /* 7. actors chain at offset 13803 + 74 = 13877 */
    /* player object: active (2 bytes LE) is ac_yes (2) */
    uint16_t player_active = (uint16_t)buf1[13877] | ((uint16_t)buf1[13878] << 8);
    CHECK(player_active == ac_yes);
    /* player state ID at offset 13877 + 6 is s_player ID (1) */
    uint16_t player_state_id = (uint16_t)buf1[13877 + 6] | ((uint16_t)buf1[13877 + 7] << 8);
    CHECK(player_state_id == 1);

    /* Wipe all game state */
    memset(&gamestate, 0, sizeof(gamestate));
    memset(LevelRatios, 0, sizeof(LevelRatios));
    memset(tilemap, 0, sizeof(tilemap));
    memset(actorat, 0, sizeof(actorat));
    memset(areaconnect, 0, sizeof(areaconnect));
    memset(areabyplayer, 0, sizeof(areabyplayer));
    memset(statobjlist, 0, sizeof(statobjlist));
    laststatobj = NULL;
    memset(doorobjlist, 0, sizeof(doorobjlist));
    memset(doorposition, 0, sizeof(doorposition));
    pwallx = pwally = pwalldir = pwallstate = pwallpos = 0;
    InitActorList();

    /* Deserialize */
    CHECK(WL_DeserializeSaveGame(buf1, len1) == true);

    /* Verify restored state */
    CHECK(gamestate.difficulty == gd_hard);
    CHECK(gamestate.mapon == 4);
    CHECK(gamestate.score == 254800);
    CHECK(gamestate.health == 92);
    CHECK(gamestate.ammo == 75);
    CHECK(gamestate.lives == 5);
    CHECK(gamestate.weapon == wp_chaingun);
    CHECK(gamestate.keys == 3);
    CHECK(gamestate.TimeCount == 1234567);
    CHECK(gamestate.killcount == 60);

    for (int i = 0; i < 8; i++)
    {
        CHECK(LevelRatios[i].kill == 80 + i);
        CHECK(LevelRatios[i].time == 3600 + i * 100);
    }

    for (int y = 0; y < MAPSIZE; y++)
    {
        for (int x = 0; x < MAPSIZE; x++)
            CHECK(tilemap[x][y] == (uint8_t)((x * 3 + y * 7) % 64));
    }

    for (int a = 0; a < NUMAREAS; a++)
    {
        CHECK(areabyplayer[a] == (uint8_t)(a % 2));
        for (int b = 0; b < NUMAREAS; b++)
            CHECK(areaconnect[a][b] == (uint8_t)((a + b) % 2));
    }

    /* Check static objects */
    CHECK(laststatobj == &statobjlist[5]);
    for (int i = 0; i < 5; i++)
    {
        CHECK(statobjlist[i].shapenum == 20 + i);
        CHECK(statobjlist[i].tilex == 10 + i);
        CHECK(statobjlist[i].tiley == 20 + i);
        CHECK(statobjlist[i].visspot == &spotvis[10 + i][20 + i]);
        CHECK(statobjlist[i].flags == 1);
        CHECK(statobjlist[i].itemnumber == 100 + i);
    }

    /* Check doors */
    for (int i = 0; i < 4; i++)
    {
        CHECK(doorobjlist[i].tilex == 5 + i);
        CHECK(doorobjlist[i].tiley == 15 + i);
        CHECK(doorobjlist[i].vertical == (boolean)(i % 2));
        CHECK(doorobjlist[i].lock == (byte)(i % 3));
        CHECK(doorobjlist[i].action == dr_closed);
        CHECK(doorposition[i] == (int16_t)(1000 + i));
    }

    /* Check pushwall */
    CHECK(pwallx == 12);
    CHECK(pwally == 34);
    CHECK(pwalldir == di_north);
    CHECK(pwallstate == 2);
    CHECK(pwallpos == 28);

    /* Check actors */
    CHECK(player != NULL);
    CHECK(player->x == 0x11223344);
    CHECK(player->y == 0x55667788);
    CHECK(player->angle == 180);
    CHECK(player->state == &s_player);

    objtype *a2 = player->next;
    CHECK(a2 != NULL);
    CHECK(a2->x == 0x22334455);
    CHECK(a2->y == 0x66778899);
    CHECK(a2->angle == 90);
    CHECK(a2->hitpoints == 50);
    CHECK(a2->state == &s_grdstand);

    objtype *a3 = a2->next;
    CHECK(a3 != NULL);
    CHECK(a3->x == 0x33445566);
    CHECK(a3->y == 0x77889900);
    CHECK(a3->angle == 270);
    CHECK(a3->hitpoints == 25);
    CHECK(a3->state == &s_dogchase1);
    CHECK(a3->next == NULL);

    /* Check actorat mapping */
    CHECK(actorat[10][10] == (objtype *)(uintptr_t)128);
    CHECK(actorat[10][11] == (objtype *)(uintptr_t)200);
    CHECK(actorat[5][5] == player);
    CHECK(actorat[6][6] == a2);
    CHECK(actorat[7][7] == a3);

    /* Re-serialize and assert byte-for-byte identity */
    size_t len2 = WL_SerializeSaveGame(buf2, sizeof(buf2));
    CHECK(len1 == len2);
    CHECK(memcmp(buf1, buf2, len1) == 0);

    puts("PASS: savegame serialization & roundtrip");
    return 0;
}

/* -------------------------------------------------------------------------
 * Test 5: File I/O Roundtrip (Config and Savegame)
 * ------------------------------------------------------------------------- */

static int test_file_roundtrip (void)
{
    char tmp_dir[] = "/tmp/wolf_test_io_XXXXXX";
    CHECK(mkdtemp(tmp_dir) != NULL);

    char cfg_path[512];
    snprintf(cfg_path, sizeof(cfg_path), "%s/CONFIG.WL1", tmp_dir);

    char save_path[512];
    snprintf(save_path, sizeof(save_path), "%s/SAVEGAM0.WL1", tmp_dir);

    /* Write and read config file */
    viewsize = 19;
    mouseadjustment = 11;
    SoundMode = sdm_AdLib;
    Scores[0].score = 777777;
    CHECK(WL_WriteConfigFile(cfg_path) == true);

    viewsize = 0;
    mouseadjustment = 0;
    SoundMode = sdm_Off;
    Scores[0].score = 0;
    CHECK(WL_ReadConfigFile(cfg_path) == true);

    CHECK(viewsize == 19);
    CHECK(mouseadjustment == 11);
    CHECK(SoundMode == sdm_AdLib);
    CHECK(Scores[0].score == 777777);

    /* Write and read savegame file */
    gamestate.difficulty = gd_medium;
    gamestate.score = 555555;
    gamestate.health = 85;

    int fd = open(save_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    CHECK(fd >= 0);
    CHECK(WL_WriteSaveGameFile(fd) == true);
    close(fd);

    gamestate.score = 0;
    gamestate.health = 0;

    fd = open(save_path, O_RDONLY);
    CHECK(fd >= 0);
    CHECK(WL_ReadSaveGameFile(fd) == true);
    close(fd);

    CHECK(gamestate.score == 555555);
    CHECK(gamestate.health == 85);

    /* Clean up */
    unlink(cfg_path);
    unlink(save_path);
    rmdir(tmp_dir);

    puts("PASS: file I/O roundtrip");
    return 0;
}

/* -------------------------------------------------------------------------
 * Test 6: Actor Holes and Reuse Restoration
 * ------------------------------------------------------------------------- */

static int test_actor_holes_and_reuse (void)
{
    uint8_t buf[65536];

    InitActorList();
    player->active = ac_yes;
    player->state = &s_player;
    GetNewActor(); /* slot 1 */
    GetNewActor(); /* slot 2 */

    objtype *survivor = new;
    survivor->hitpoints = 77;
    survivor->state = &s_grdstand;
    survivor->active = ac_yes;

    /* Simulate removing slot 1 (hole in chain), linking player directly to survivor */
    player->next = survivor;
    survivor->prev = player;
    objcount--;

    memset(actorat, 0, sizeof(actorat));
    actorat[6][6] = survivor;

    size_t len = WL_SerializeSaveGame(buf, sizeof(buf));
    CHECK(len > 0);

    /* Verify that on load, survivor is reallocated in sequential slot 1,
     * and actorat[6][6] correctly points to survivor (slot 1), NOT old slot 2. */
    CHECK(WL_DeserializeSaveGame(buf, len) == true);
    CHECK(player->next != NULL);
    CHECK(player->next->hitpoints == 77);
    CHECK(player->next->state == &s_grdstand);
    CHECK(actorat[6][6] == player->next);
    CHECK((actorat[6][6] - objlist) == 1);

    puts("PASS: actor holes and reuse restoration");
    return 0;
}

/* -------------------------------------------------------------------------
 * Test 7: Truncated Savegame Rejection
 * ------------------------------------------------------------------------- */

static int test_truncated_savegame (void)
{
    uint8_t buf[65536];

    InitActorList();
    player->active = ac_yes;
    player->state = &s_player;

    size_t len = WL_SerializeSaveGame(buf, sizeof(buf));
    CHECK(len > 0);

    /* Test range of truncated lengths */
    CHECK(WL_DeserializeSaveGame(buf, 0) == false);
    CHECK(WL_DeserializeSaveGame(buf, 1) == false);
    CHECK(WL_DeserializeSaveGame(buf, WL_SAVE_MIN_SIZE - 1) == false);
    CHECK(WL_DeserializeSaveGame(buf, len - 1) == false);
    CHECK(WL_DeserializeSaveGame(buf, len - 4) == false);
    CHECK(WL_DeserializeSaveGame(buf, len - 100) == false);
    CHECK(WL_DeserializeSaveGame(buf, len - WL_TRAILING_SIZE) == false);
    CHECK(WL_DeserializeSaveGame(buf, len - WL_TRAILING_SIZE + 1) == false);

    /* Full buffer must succeed */
    CHECK(WL_DeserializeSaveGame(buf, len) == true);

    puts("PASS: truncated save rejection");
    return 0;
}

/* -------------------------------------------------------------------------
 * Test 8: Corrupt Savegame Rejection
 * ------------------------------------------------------------------------- */

static int test_corrupt_savegame (void)
{
    uint8_t buf[65536];
    uint8_t corrupt[65536];

    InitActorList();
    player->active = ac_yes;
    player->state = &s_player;
    memset(actorat, 0, sizeof(actorat));
    actorat[5][5] = player;

    size_t len = WL_SerializeSaveGame(buf, sizeof(buf));
    CHECK(len > 0);
    const size_t P = 13877;
    const size_t ACTORAT = 66 + 80 + 4096;

    /* 1. Missing badobject terminator */
    memcpy(corrupt, buf, len);
    size_t term_offset = 13877 + 60;
    corrupt[term_offset] = 1;
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    /* 2. Invalid actor state ID (> WL_NUM_STATES or == 0) */
    memcpy(corrupt, buf, len);
    corrupt[P + 6] = 0xff;
    corrupt[P + 7] = 0x01; /* ID 511 > 305 */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    memcpy(corrupt, buf, len);
    corrupt[P + 6] = 0;
    corrupt[P + 7] = 0; /* ID 0 (NULL state) */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    /* 3. Out-of-bounds actor coordinates & area */
    memcpy(corrupt, buf, len);
    corrupt[P + 24] = 0xe8; corrupt[P + 25] = 0x03; /* tilex = 1000 >= 64 */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    memcpy(corrupt, buf, len);
    corrupt[P + 26] = 0xe8; corrupt[P + 27] = 0x03; /* tiley = 1000 >= 64 */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    memcpy(corrupt, buf, len);
    corrupt[P + 28] = 255; /* areanumber = 255 >= NUMAREAS */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    /* 4. Out-of-bounds gamestate.episode */
    memcpy(corrupt, buf, len);
    corrupt[38] = 0xf4; corrupt[39] = 0x01; /* episode = 500 */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    memcpy(corrupt, buf, len);
    corrupt[38] = 6; corrupt[39] = 0; /* episode = 6 (maps >= NUMMAPS 60) */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    /* 5. Wild pointer or out-of-range actor index in actorat */
    memcpy(corrupt, buf, len);
    corrupt[ACTORAT + 2] = 0x34; corrupt[ACTORAT + 3] = 0x12; /* raw 0x1234 */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    memcpy(corrupt, buf, len);
    corrupt[ACTORAT + 2] = 0x05; corrupt[ACTORAT + 3] = 0x80; /* 0x8000 | 5 >= 1 actor */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    /* 6. Invalid door index in tilemap (> MAXDOORS) */
    memcpy(corrupt, buf, len);
    corrupt[66 + 80] = 0xff; /* door index 127 >= MAXDOORS */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    /* 7. Out-of-bounds statobj coordinates (>= 64) */
    memcpy(corrupt, buf, len);
    size_t trailing_start = len - WL_TRAILING_SIZE;
    corrupt[trailing_start + 2] = 64; /* tx >= 64 */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    /* 8. Out-of-bounds pushwall coordinates */
    memcpy(corrupt, buf, len);
    size_t pwall_offset = len - WL_CHECKSUM_SIZE - WL_PWALL_SIZE;
    corrupt[pwall_offset + 2] = 65; /* pw_x >= 64 */
    CHECK(WL_DeserializeSaveGame(corrupt, len) == false);

    /* 9. Extra garbage appended past valid save */
    uint8_t extra[65536];
    memcpy(extra, buf, len);
    extra[len] = 0xaa;
    CHECK(WL_DeserializeSaveGame(extra, len + 1) == false);

    puts("PASS: corrupt save rejection");
    return 0;
}

/* -------------------------------------------------------------------------
 * Test 9: Joystick Bound and Presence Validation
 * ------------------------------------------------------------------------- */

static int test_joystick_bound_validation (void)
{
    uint8_t buf[WL_CONFIG_SIZE];

    /* Initialize config */
    JoysPresent[0] = true;
    JoysPresent[1] = true;
    joystickenabled = true;
    joystickport = 0;

    size_t len = WL_SerializeConfig(buf, sizeof(buf));
    CHECK(len == WL_CONFIG_SIZE);

    /* Test joystickport >= MaxJoys (e.g. 2, 255) */
    buf[476] = 2;
    buf[477] = 0;
    buf[470] = 1; /* joystickenabled = true */
    buf[471] = 0;
    CHECK(WL_DeserializeConfig(buf, len) == true);
    CHECK(joystickport == 0);
    CHECK(joystickenabled == false);

    buf[476] = 255;
    buf[477] = 0;
    buf[470] = 1;
    CHECK(WL_DeserializeConfig(buf, len) == true);
    CHECK(joystickport == 0);
    CHECK(joystickenabled == false);

    /* Test joystickport = 1 when JoysPresent[1] == false */
    JoysPresent[1] = false;
    buf[476] = 1;
    buf[477] = 0;
    buf[470] = 1;
    CHECK(WL_DeserializeConfig(buf, len) == true);
    CHECK(joystickenabled == false);

    /* Test joystickport = 0 when JoysPresent[0] == true */
    buf[476] = 0;
    buf[477] = 0;
    buf[470] = 1;
    CHECK(WL_DeserializeConfig(buf, len) == true);
    CHECK(joystickport == 0);
    CHECK(joystickenabled == true);

    puts("PASS: joystick bound and presence validation");
    return 0;
}

/* -------------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------------- */

int main (void)
{
    printf("Running wolf_save_test...\n");
    if (test_xdg_paths() != 0) return 1;
    if (test_state_mapping() != 0) return 1;
    if (test_config_roundtrip() != 0) return 1;
    if (test_savegame_roundtrip() != 0) return 1;
    if (test_file_roundtrip() != 0) return 1;
    if (test_actor_holes_and_reuse() != 0) return 1;
    if (test_truncated_savegame() != 0) return 1;
    if (test_corrupt_savegame() != 0) return 1;
    if (test_joystick_bound_validation() != 0) return 1;

    puts("ALL SAVE & CONFIG TESTS PASSED!");
    return 0;
}
