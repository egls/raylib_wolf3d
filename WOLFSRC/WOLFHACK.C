/* Optional textured floor/ceiling experiment, now using pixel coordinates. */
#include "WL_DEF.H"
extern char planepics[8192];
static int spanstart[100], halfheight;
static fixed basedist[100], psin, pcos;
int mr_rowofs, mr_count, mr_xstep, mr_ystep, mr_xfrac, mr_yfrac, mr_dest;
fixed FixedMul(fixed a, fixed b) { return (int32_t)((uint32_t)(a >> 8) * (uint32_t)(b >> 8)); }
void MapRow(void) {
    uint32_t frac = ((uint32_t)(uint16_t)mr_yfrac << 16) | (uint16_t)mr_xfrac;
    uint32_t step = ((uint32_t)(uint16_t)mr_ystep << 16) | (uint16_t)mr_xstep;
    int i;
    for (i = 0; i < mr_count; i++) {
        unsigned index = ((frac >> 3) & (63 * 128)) | ((frac >> 25) & (63 * 2));
        int pixel = mr_dest + i * 4, bottom = pixel + mr_rowofs;
        VL_Plot(pixel % 320, pixel / 320, (byte)planepics[index]);
        VL_Plot(bottom % 320, bottom / 320, (byte)planepics[index + 1]);
        frac += step;
    }
}
void SetPlaneViewSize(void) {
    int y, x;
    byte *src;
    halfheight = viewheight / 2;
    for (y = 1; y < halfheight; y++)
        basedist[y] = GLOBAL1 / 2 * scale / y;
    src = PM_GetPage(0);
    for (x = 0; x < 4096; x++)
        planepics[x * 2] = src[x];
    src = PM_GetPage(1);
    for (x = 0; x < 4096; x++)
        planepics[x * 2 + 1] = src[x];
}
void DrawSpans(int x1, int x2, int height) {
    fixed length, startx, starty;
    int prestep, lane, xpart, ypart;
    if (height <= 0 || x2 < x1)
        return;
    mr_xstep = psin * 2 / height;
    mr_ystep = pcos * 2 / height;
    /* Original arithmetic shifts round negative lane presteps downward. */
    xpart = mr_xstep / 4 - (mr_xstep < 0 && mr_xstep % 4 != 0);
    ypart = mr_ystep / 4 - (mr_ystep < 0 && mr_ystep % 4 != 0);
    length = basedist[height];
    startx = viewx + FixedMul(length, pcos);
    starty = viewy - FixedMul(length, psin);
    prestep = viewwidth / 2 - x1;
    mr_rowofs = (2 * height + 1) * 320;
    for (lane = 0; lane < 4; lane++, x1++, prestep--) {
        mr_xfrac = startx - xpart * prestep;
        mr_yfrac = starty - ypart * prestep;
        mr_dest = (halfheight - 1 - height) * 320 + x1;
        mr_count = x1 > x2 ? 0 : (x2 - x1) / 4 + 1;
        MapRow();
    }
}
void DrawPlanes(void) {
    int x, h, last;
    if (viewheight / 2 != halfheight)
        SetPlaneViewSize();
    psin = viewsin < 0 ? -(viewsin & 65535) : viewsin;
    pcos = viewcos < 0 ? -(viewcos & 65535) : viewcos;
    last = halfheight;
    for (x = 0; x < viewwidth; x++) {
        h = wallheight[x] >> 3;
        if (h > halfheight)
            h = halfheight;
        while (last > h)
            spanstart[--last] = x;
        while (last < h) {
            DrawSpans(spanstart[last], x - 1, last);
            last++;
        }
    }
    while (last < halfheight) {
        DrawSpans(spanstart[last], x - 1, last);
        last++;
    }
}
