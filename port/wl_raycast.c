/* Direct C translation of WL_DR_A.ASM's DDA. Equality ordering and signed
 * magnitude partial multiplication match the 16-bit code. */
#include "WL_DEF.H"
#include "wl_data.h"
extern int midangle, focaltx, focalty, xtile, ytile, xtilestep, ytilestep;
extern unsigned xpartialup, xpartialdown, ypartialup, ypartialdown, tilehit, pixx;
extern int32_t xintercept, yintercept, xstep, ystep;
void HitVertWall(void), HitHorizWall(void), HitVertDoor(void), HitHorizDoor(void),
    HitVertPWall(void), HitHorizPWall(void);
static int32_t add(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t floor_div(int64_t a, int divisor) {
    return (int32_t)(a / divisor - (a < 0 && a % divisor != 0));
}
void AsmRefresh(void) {
    for (pixx = 0; pixx < (unsigned)viewwidth; pixx++) {
        int a = midangle + pixelangle[pixx], vx, hy, xi, yi, steps = 0;
        uint16_t xp, yp;
        int32_t candidate;
        if (a < 0)
            a += FINEANGLES;
        if (a >= FINEANGLES)
            a -= FINEANGLES;
        if (a < 900) {
            xtilestep = 1;
            ytilestep = -1;
            xstep = finetangent[899 - a];
            ystep = -finetangent[a];
            xp = xpartialup;
            yp = ypartialdown;
        } else if (a < 1800) {
            xtilestep = -1;
            ytilestep = -1;
            xstep = -finetangent[a - 900];
            ystep = -finetangent[1799 - a];
            xp = xpartialdown;
            yp = ypartialdown;
        } else if (a < 2700) {
            xtilestep = -1;
            ytilestep = 1;
            xstep = -finetangent[2699 - a];
            ystep = finetangent[a - 1800];
            xp = xpartialdown;
            yp = ypartialup;
        } else {
            xtilestep = 1;
            ytilestep = 1;
            xstep = finetangent[a - 2700];
            ystep = finetangent[3599 - a];
            xp = xpartialup;
            yp = ypartialup;
        }
        yintercept = add(viewy, WL_FixedPartial(ystep, xp));
        vx = focaltx + xtilestep;
        xintercept = add(viewx, WL_FixedPartial(xstep, yp));
        hy = focalty + ytilestep;
    vertcheck:
        yi = floor_div(yintercept, 65536);
        if (ytilestep < 0 ? yi <= hy : yi >= hy)
            goto horizentry;
    vertentry:
        if (++steps > 256)
            Quit("Raycaster: unenclosed map");
        yi = floor_div(yintercept, 65536);
        if (vx < 0 || vx >= MAPSIZE || yi < 0 || yi >= MAPSIZE)
            Quit("Raycaster: ray outside map");
        tilehit = tilemap[vx][yi];
        if (tilehit) {
            xtile = vx;
            ytile = yi;
            if (!(tilehit & 0x80)) {
                xintercept = vx * 65536;
                HitVertWall();
                goto nextpix;
            }
            if (tilehit & 0x40)
                candidate = add(yintercept, floor_div((int32_t)((uint32_t)ystep * pwallpos), 64));
            else
                candidate = add(yintercept, floor_div(ystep, 2));
            if (floor_div(candidate, 65536) == yi &&
                ((tilehit & 0x40) || (uint16_t)candidate >= doorposition[tilehit & 0x7f])) {
                yintercept = candidate;
                xintercept = vx * 65536 + ((tilehit & 0x40) ? 0 : 32768);
                if (tilehit & 0x40)
                    HitVertPWall();
                else
                    HitVertDoor();
                goto nextpix;
            }
        }
        spotvis[vx][yi] = 1;
        vx += xtilestep;
        yintercept = add(yintercept, ystep);
        goto vertcheck;
    horizcheck:
        xi = floor_div(xintercept, 65536);
        if (xtilestep < 0 ? xi <= vx : xi >= vx)
            goto vertentry;
    horizentry:
        if (++steps > 256)
            Quit("Raycaster: unenclosed map");
        xi = floor_div(xintercept, 65536);
        if (hy < 0 || hy >= MAPSIZE || xi < 0 || xi >= MAPSIZE)
            Quit("Raycaster: ray outside map");
        tilehit = tilemap[xi][hy];
        if (tilehit) {
            xtile = xi;
            ytile = hy;
            if (!(tilehit & 0x80)) {
                yintercept = hy * 65536;
                HitHorizWall();
                goto nextpix;
            }
            if (tilehit & 0x40)
                candidate = add(xintercept, floor_div((int32_t)((uint32_t)xstep * pwallpos), 64));
            else
                candidate = add(xintercept, floor_div(xstep, 2));
            if (floor_div(candidate, 65536) == xi &&
                ((tilehit & 0x40) || (uint16_t)candidate >= doorposition[tilehit & 0x7f])) {
                xintercept = candidate;
                yintercept = hy * 65536 + ((tilehit & 0x40) ? 0 : 32768);
                if (tilehit & 0x40)
                    HitHorizPWall();
                else
                    HitHorizDoor();
                goto nextpix;
            }
        }
        spotvis[xi][hy] = 1;
        hy += ytilestep;
        xintercept = add(xintercept, xstep);
        goto horizcheck;
    nextpix:;
    }
}
