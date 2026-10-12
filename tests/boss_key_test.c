/* Exercise the actual registered BossKey with scripted input, no game assets. */
#include "WL_DEF.H"
#include <assert.h>
boolean Keyboard[NumCodes];
byte gamepal;
static int pumps, cleared, hidden, presented, muted, resumed, restored;
void SD_MusicOff(void) { muted++; }
void SD_MusicOn(void) { assert(muted == 1 && pumps == 3); resumed++; }
void VL_Bar(int x, int y, int w, int h, int color) {
    assert(x == 0 && y == 0 && w == 320 && h == 200 && color == 0);
    hidden++;
}
void VW_UpdateScreen(void) { assert(hidden == 1); presented++; }
void IN_ClearKeysDown(void) {
    memset(Keyboard, 0, sizeof Keyboard);
    cleared++;
}
void IN_SpinWait(void) {
    assert(presented == 1 && muted == 1 && cleared == 1);
    assert(++pumps <= 3);
    if (pumps == 3) Keyboard[sc_Escape] = true;
}
void VL_SetVGAPlaneMode(void) { assert(resumed == 1); }
void VL_TestPaletteSet(void) {}
void VL_SetPalette(byte *palette) { assert(palette == &gamepal); }
void LoadLatchMem(void) { restored++; }
void geninterrupt(int number) { (void)number; abort(); }
int main(void) {
    Keyboard[sc_Escape] = true; /* A stale key must not immediately dismiss it. */
    BossKey();
    assert(pumps == 3 && cleared == 2 && !Keyboard[sc_Escape]);
    assert(hidden == 1 && resumed == 1 && restored == 1);
    puts("PASS: registered F1 hides video, pumps input until Escape, and restores music/video");
    return 0;
}
