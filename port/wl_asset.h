/* Resolve DOS asset names on case-sensitive macOS volumes without renaming
 * user data. Only bare WL1 read names participate; save paths are untouched. */
#ifndef WL_ASSET_H
#define WL_ASSET_H
#ifdef __APPLE__
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
static inline const char *WL_AssetName(const char *name)
{
    static char found[256];
    const char *dot = strrchr(name, '.');
    struct dirent *entry;
    DIR *dir;
    if (strchr(name, '/') || !dot || strcasecmp(dot, ".WL1"))
        return name;
    dir = opendir(".");
    if (!dir) return name;
    found[0] = 0;
    while ((entry = readdir(dir))) {
        struct stat st;
        if (!strcasecmp(entry->d_name, name) &&
            !stat(entry->d_name, &st) && S_ISREG(st.st_mode)) {
            snprintf(found, sizeof(found), "%s", entry->d_name);
            if (!strcmp(entry->d_name, name)) {
                closedir(dir);
                return name;
            }
        }
    }
    closedir(dir);
    return found[0] ? found : name;
}
#else
#define WL_AssetName(name) (name)
#endif
#endif
