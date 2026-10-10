/*
 * persist_test.c - port/wl_persist.c without game data or a display
 * (WOLF-18): the little-endian byte layout of every field width, write ->
 * read round trips through memory and through a file, short reads, the
 * FNV-1a checksum, the state index table and the XDG config directory.
 *
 * The whole save and config formats are checked against real game state
 * by tests/persist_check.py (-savetest), which needs the WL1 data.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "WL_DEF.H"
#include "wl_persist.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

void Quit(char *msg) {
    fprintf(stderr, "Quit: %s\n", msg);
    exit(1);
}

/* a stand-in for the generated port/wl_states.c */
static statetype s_one, s_two, s_three, s_unknown;
statetype *const wl_states[] = {&s_one, &s_two, &s_three};
const int wl_numstates = sizeof(wl_states) / sizeof(wl_states[0]);

static char tmpdir[] = "/tmp/wl_persist_testXXXXXX";

/* every width writes exactly its bytes, low byte first, and reads them back */
static void test_layout(void) {
    static const uint8_t expect[] = {
        0x7f,                   /* W8 0x7f */
        0xff,                   /* W8 -1 */
        0x34, 0x12,             /* W16 0x1234 */
        0xfe, 0xff,             /* W16 -2 */
        0x78, 0x56, 0x34, 0x12, /* W32 0x12345678 */
        0x00, 0x00, 0x00, 0x80, /* W32 INT32_MIN */
        'R', 'L', 'W',          /* WBytes */
        0xcd,                   /* W8 0xabcd: high bits dropped */
        0xef, 0xcd,             /* W16 0x89abcdef: high bits dropped */
    };
    wl_stream s;
    char b[3];

    WS_Init(&s);
    WS_W8(&s, 0x7f);
    WS_W8(&s, (uint32_t)-1);
    WS_W16(&s, 0x1234);
    WS_W16(&s, (uint32_t)-2);
    WS_W32(&s, 0x12345678u);
    WS_W32(&s, (uint32_t)INT32_MIN);
    WS_WBytes(&s, "RLW", 3);
    WS_W8(&s, 0xabcd);
    WS_W16(&s, 0x89abcdefu);
    CHECK(s.len == sizeof(expect));
    CHECK(memcmp(s.buf, expect, sizeof(expect)) == 0);

    CHECK(WS_R8(&s) == 0x7f);
    CHECK((int8_t)WS_R8(&s) == -1);
    CHECK(WS_R16(&s) == 0x1234);
    CHECK(WS_RS16(&s) == -2);
    CHECK(WS_R32(&s) == 0x12345678u);
    CHECK(WS_RS32(&s) == INT32_MIN);
    WS_RBytes(&s, b, 3);
    CHECK(memcmp(b, "RLW", 3) == 0);
    CHECK(WS_R8(&s) == 0xcd);
    CHECK(WS_R16(&s) == 0xcdef);
    CHECK(!s.bad && s.pos == s.len);
    WS_Free(&s);
    CHECK(s.buf == NULL && s.len == 0 && s.cap == 0 && s.pos == 0 && !s.bad);
    printf("PASS: little-endian layout\n");
}

/* a read past the end returns zeros, sets bad, and every later read fails */
static void test_short_reads(void) {
    wl_stream s;
    uint8_t b[4] = {1, 2, 3, 4};

    WS_Init(&s);
    WS_W8(&s, 0xaa);
    WS_W8(&s, 0xbb);
    WS_W8(&s, 0xcc);
    CHECK(WS_R32(&s) == 0); /* 3 bytes left */
    CHECK(s.bad && s.pos == 0);
    CHECK(WS_R8(&s) == 0); /* sticky, even though a byte is there */
    WS_RBytes(&s, b, sizeof(b));
    CHECK(b[0] == 0 && b[3] == 0);

    s.bad = 0;
    CHECK(WS_R16(&s) == 0xbbaa);
    CHECK(WS_R16(&s) == 0 && s.bad); /* 1 byte left */
    WS_Free(&s);

    WS_Init(&s);
    CHECK(WS_R8(&s) == 0 && s.bad); /* empty stream */
    WS_Free(&s);
    printf("PASS: short reads\n");
}

/* WS_WriteFile then WS_ReadFile gives back the same bytes, across the
 * 8 KiB read chunks and buffer growth */
static void test_file_roundtrip(void) {
    char path[sizeof(tmpdir) + 16];
    wl_stream out, in;
    size_t i;
    int fd;

    snprintf(path, sizeof(path), "%s/roundtrip", tmpdir);
    WS_Init(&out);
    for (i = 0; i < 20000; i++)
        WS_W32(&out, (uint32_t)(i * 2654435761u));
    CHECK(out.len == 80000);

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK(fd != -1);
    CHECK(write(fd, "header", 6) == 6);
    CHECK(WS_WriteFile(fd, &out));
    CHECK(close(fd) == 0);

    fd = open(path, O_RDONLY);
    CHECK(fd != -1);
    CHECK(lseek(fd, 6, SEEK_SET) == 6); /* like a save after its 32-byte name */
    WS_Init(&in);
    CHECK(WS_ReadFile(fd, &in));
    close(fd);
    CHECK(in.len == out.len && in.pos == 0 && !in.bad);
    CHECK(memcmp(in.buf, out.buf, out.len) == 0);
    for (i = 0; i < 20000; i++)
        CHECK(WS_R32(&in) == (uint32_t)(i * 2654435761u));
    CHECK(!in.bad && in.pos == in.len);

    /* reading into a used stream starts over */
    fd = open(path, O_RDONLY);
    CHECK(WS_ReadFile(fd, &in));
    close(fd);
    CHECK(in.len == out.len + 6 && in.pos == 0 && memcmp(in.buf, "header", 6) == 0);

    WS_Free(&in);
    WS_Free(&out);
    unlink(path);
    printf("PASS: file round trip\n");
}

static void test_checksum(void) {
    wl_stream s;

    WS_Init(&s);
    CHECK(WS_Checksum(&s, 0, 0) == 0x811c9dc5u); /* FNV-1a offset basis */
    WS_WBytes(&s, "xfoobar", 7);
    CHECK(WS_Checksum(&s, 1, 7) == 0xbf9cf968u); /* "foobar" */
    CHECK(WS_Checksum(&s, 1, 100) == 0xbf9cf968u); /* clipped to len */
    s.buf[3] ^= 1;
    CHECK(WS_Checksum(&s, 1, 7) != 0xbf9cf968u);
    WS_Free(&s);

    WS_Init(&s);
    WS_W8(&s, 'a');
    CHECK(WS_Checksum(&s, 0, 1) == 0xe40c292cu);
    WS_Free(&s);
    printf("PASS: FNV-1a checksum\n");
}

static void test_states(void) {
    CHECK(WL_StateIndex(NULL) == -1);
    CHECK(WL_StateIndex(&s_one) == 0);
    CHECK(WL_StateIndex(&s_three) == 2);
    CHECK(WL_StateFromIndex(1) == &s_two);
    CHECK(WL_StateFromIndex(-1) == NULL);
    CHECK(WL_StateFromIndex(wl_numstates) == NULL);
    CHECK(WL_StateFromIndex(32767) == NULL);
    (void)s_unknown; /* WL_StateIndex(&s_unknown) Quit()s; not exercised */
    printf("PASS: state index table\n");
}

/* WL_UserFile caches its directory, so each environment runs in a child */
static void user_file_case(const char *xdg, const char *home, const char *expect_dir) {
    pid_t pid = fork();
    int status;

    CHECK(pid != -1);
    if (pid == 0) {
        char expect[1100];
        struct stat st;

        if (xdg)
            setenv("XDG_CONFIG_HOME", xdg, 1);
        else
            unsetenv("XDG_CONFIG_HOME");
        setenv("HOME", home, 1);

        snprintf(expect, sizeof(expect), "%s/CONFIG.WL1", expect_dir);
        CHECK(strcmp(WL_UserFile("CONFIG.WL1"), expect) == 0);
        CHECK(stat(expect_dir, &st) == 0 && S_ISDIR(st.st_mode)); /* created */
        snprintf(expect, sizeof(expect), "%s/SAVEGAM3.WL1", expect_dir);
        CHECK(strcmp(WL_UserFile("SAVEGAM3.WL1"), expect) == 0);
        _exit(0);
    }
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void test_user_file(void) {
    char xdg[sizeof(tmpdir) + 16], home[sizeof(tmpdir) + 16], dir[sizeof(tmpdir) + 64];

    snprintf(xdg, sizeof(xdg), "%s/xdg", tmpdir);
    snprintf(home, sizeof(home), "%s/home", tmpdir);
    CHECK(mkdir(xdg, 0755) == 0 && mkdir(home, 0755) == 0);

    snprintf(dir, sizeof(dir), "%s/raylib_wolf3d", xdg);
    user_file_case(xdg, home, dir);

    /* unset, empty or relative XDG_CONFIG_HOME: ~/.config (XDG spec) */
    snprintf(dir, sizeof(dir), "%s/.config/raylib_wolf3d", home);
    user_file_case(NULL, home, dir);
    user_file_case("", home, dir);
    user_file_case("relative/dir", home, dir);
    printf("PASS: XDG config directory\n");
}

int main(void) {
    char cmd[sizeof(tmpdir) + 16];

    CHECK(mkdtemp(tmpdir) != NULL);
    test_layout();
    test_short_reads();
    test_file_roundtrip();
    test_checksum();
    test_states();
    test_user_file();
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    CHECK(system(cmd) == 0);
    return 0;
}
