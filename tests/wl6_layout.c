#include "ID_HEADS.H"
int main(void) {
    printf("{\"graphics\":%d,\"pictures\":%d,\"tile\":%d,\"tiles\":%d,"
           "\"font\":%d,\"startpics\":%d,\"maps\":%d,\"audio\":%d,"
           "\"adlib\":%d,\"digi\":%d,\"music\":%d,\"lastmusic\":%d}\n",
           NUMCHUNKS, NUMPICS, STARTTILE8, NUMTILE8, STARTFONT, STARTPICS,
           NUMMAPS, NUMSNDCHUNKS, STARTADLIBSOUNDS, STARTDIGISOUNDS, STARTMUSIC, LASTMUSIC);
    return 0;
}
