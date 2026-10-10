/*
 * wl_save.h - Portable serialization and XDG path handling for config,
 * savegames, and high scores.
 */
#ifndef WL_SAVE_H
#define WL_SAVE_H

#include <stdint.h>
#include <stddef.h>


/* Exact serialized sizes in bytes (little-endian fixed layout) */
#define WL_CONFIG_SIZE              522
#define WL_HIGHSCORE_SIZE           66
#define WL_SAVE_HEADER_SIZE         32
#define WL_GAMESTATE_SIZE           66
#define WL_LEVELRATIOS_SIZE         80
#define WL_TILEMAP_SIZE             4096
#define WL_ACTORAT_SIZE             8192
#define WL_AREACONNECT_SIZE         1369
#define WL_AREABYPLAYER_SIZE        74
#define WL_ACTOR_SIZE               60
#define WL_LASTSTATOBJ_SIZE         2
#define WL_STATOBJ_SIZE             8
#define WL_STATOBJLIST_SIZE         (MAXSTATS * WL_STATOBJ_SIZE)  /* 3200 */
#define WL_DOORPOSITION_SIZE        (MAXDOORS * 2)               /* 128 */
#define WL_DOOROBJ_SIZE             10
#define WL_DOOROBJLIST_SIZE         (MAXDOORS * WL_DOOROBJ_SIZE)  /* 640 */
#define WL_PWALL_SIZE               10
#define WL_CHECKSUM_SIZE            4
#define WL_TRAILING_SIZE            (WL_LASTSTATOBJ_SIZE + WL_STATOBJLIST_SIZE + WL_DOORPOSITION_SIZE + WL_DOOROBJLIST_SIZE + WL_PWALL_SIZE + WL_CHECKSUM_SIZE) /* 3984 */
#define WL_SAVE_MIN_SIZE            (WL_GAMESTATE_SIZE + WL_LEVELRATIOS_SIZE + WL_TILEMAP_SIZE + WL_ACTORAT_SIZE + WL_AREACONNECT_SIZE + WL_AREABYPLAYER_SIZE + WL_ACTOR_SIZE + WL_ACTOR_SIZE + WL_TRAILING_SIZE) /* 17981 */

/* State mapping */
uint16_t    WL_StateToID (const statetype *state);
statetype  *WL_IDToState (uint16_t id);

/* Path resolution */
boolean     WL_EnsureDir (const char *path);
void        WL_GetConfigDir (char *dest, size_t dest_len);
void        WL_GetSaveDir (char *dest, size_t dest_len);
void        WL_GetConfigFilePath (char *dest, size_t dest_len);
void        WL_GetSaveFilePath (int slot, char *dest, size_t dest_len);

/* Config serialization / file operations */
size_t      WL_SerializeConfig (uint8_t *buf, size_t max_size);
boolean     WL_DeserializeConfig (const uint8_t *buf, size_t size);
boolean     WL_WriteConfigFile (const char *path);
boolean     WL_ReadConfigFile (const char *path);

/* Savegame serialization / file operations */
size_t      WL_SerializeSaveGame (uint8_t *buf, size_t max_size);
boolean     WL_DeserializeSaveGame (const uint8_t *buf, size_t size);
boolean     WL_WriteSaveGameFile (int fd);
boolean     WL_ReadSaveGameFile (int fd);

#endif /* WL_SAVE_H */
