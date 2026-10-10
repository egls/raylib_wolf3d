/* Looping scalers replace generated executable x86 code. Bounds use the
 * same floor(src*height/64) edges as BuildCompScale, including double-step
 * height aliases above viewheight/2. Sprite commands stay disk byte streams. */
#include "WL_DEF.H"
#include "wl_data.h"
int maxscale, maxscaleshl2;
boolean insetupscaling;
static int heights[MAXSCALEHEIGHT + 1];
void SetupScaling(int maximum) {
    int i, limit = maximum / 2, stepbytwo = viewheight / 2;
    if (limit < 1 || limit > MAXSCALEHEIGHT - 2)
        Quit("SetupScaling: invalid height");
    maxscale = limit - 1;
    maxscaleshl2 = maxscale * 4;
    insetupscaling = true;
    memset(heights, 0, sizeof heights);
    for (i = 1; i <= limit; i++) {
        heights[i] = i * 2;
        if (i >= stepbytwo) {
            heights[i + 1] = heights[i];
            heights[i + 2] = heights[i];
            i += 2;
        }
    }
    heights[0] = heights[1];
    insetupscaling = false;
}
void WL_ScaleWallColumn(unsigned x, unsigned width, unsigned fractional, const byte *source) {
    unsigned index = fractional >> 3;
    int h, top, src, y;
    unsigned dx;
    if (index > (unsigned)maxscale)
        index = maxscale;
    h = heights[index];
    if (!h)
        return;
    top = (viewheight - h) / 2;
    for (src = 0; src < 64; src++) {
        int start = top + src * h / 64, end = top + (src + 1) * h / 64;
        if (start < 0)
            start = 0;
        if (end > viewheight)
            end = viewheight;
        for (y = start; y < end; y++)
            for (dx = x; dx < x + width && dx < (unsigned)viewwidth; dx++)
                VL_Plot(dx, y, source[src]);
    }
}
static void column(const byte *shape, size_t len, unsigned offset, int x, int width, int h,
                   unsigned depth, boolean clip) {
    size_t pos = offset;
    int top = (viewheight - h) / 2;
    while (1) {
        unsigned end, start, data;
        int sy;
        if (pos > len || len - pos < 2)
            Quit("Sprite: bad command offset");
        end = WL_ReadLE16(shape + pos);
        if (!end)
            return;
        if (len - pos < 6)
            Quit("Sprite: truncated command");
        data = WL_ReadLE16(shape + pos + 2);
        start = WL_ReadLE16(shape + pos + 4);
        pos += 6;
        if ((end | start) & 1 || end > 128 || start >= end)
            Quit("Sprite: bad pixel range");
        start /= 2;
        end /= 2;
        for (sy = start; sy < (int)end; sy++) {
            int y, dx, y1 = top + sy * h / 64, y2 = top + (sy + 1) * h / 64;
            /* Virtual pixel bases are 16-bit offsets and may wrap below zero. */
            unsigned pixel = (uint16_t)(data + sy);
            if (pixel >= len)
                Quit("Sprite: bad pixel offset");
            if (y1 < 0)
                y1 = 0;
            if (y2 > viewheight)
                y2 = viewheight;
            for (dx = x; dx < x + width; dx++)
                if (dx >= 0 && dx < viewwidth && (!clip || wallheight[dx] < depth))
                    for (y = y1; y < y2; y++)
                        VL_Plot(dx, y, shape[pixel]);
        }
    }
}
static void shape(int center, int number, unsigned depth, boolean clip) {
    byte *p;
    size_t len;
    int left, right, src, x, h, index;
    if (number < 0 || number + PMSpriteStart >= PMSoundStart)
        Quit("Sprite: invalid page");
    p = PM_GetSpritePage(number);
    len = PMPages[number + PMSpriteStart].length;
    index = clip ? (depth >> 3) : (depth >> 1);
    if (!index || (clip && index > maxscale))
        return;
    if (index > MAXSCALEHEIGHT || !(h = heights[index]))
        Quit("Sprite: invalid scale");
    if (len < 4)
        Quit("Sprite: truncated header");
    left = WL_ReadLE16(p);
    right = WL_ReadLE16(p + 2);
    if (left > right || right > 63 || len < 4u + 2u * (right - left + 1))
        Quit("Sprite: bad columns");
    /* Original left half starts at source 31 and accumulates widths backwards;
     * the right half begins at center even for shapes wholly right of 31. */
    x = center;
    for (src = 31; src >= left; src--) {
        int width = (src + 1) * h / 64 - src * h / 64;
        x -= width;
        if (src <= right && width)
            column(p, len, WL_ReadLE16(p + 4 + 2 * (src - left)), x, width, h, depth, clip);
    }
    x = center;
    for (src = left >= 31 ? left : 32; src <= right; src++) {
        int width = (src + 1) * h / 64 - src * h / 64;
        if (width)
            column(p, len, WL_ReadLE16(p + 4 + 2 * (src - left)), x, width, h, depth, clip);
        x += width;
    }
}
void ScaleShape(int center, int number, unsigned height) { shape(center, number, height, true); }
void SimpleScaleShape(int center, int number, unsigned height) {
    shape(center, number, height, false);
}
