#include "wl_data.h"
uint16_t WL_ReadLE16(const void *p) {
    const uint8_t *b = p;
    return (uint16_t)(b[0] | (uint16_t)b[1] << 8);
}
uint32_t WL_ReadLE24(const void *p) {
    const uint8_t *b = p;
    return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16;
}
uint32_t WL_ReadLE32(const void *p) {
    const uint8_t *b = p;
    return WL_ReadLE24(b) | (uint32_t)b[3] << 24;
}
int WL_HuffExpand(const uint8_t *src, size_t n, uint8_t *dst, size_t len,
                  const WL_HuffNode nodes[255]) {
    size_t pos = 0, out = 0;
    unsigned mask = 1, node = 254;
    while (out < len) {
        unsigned code;
        if (pos >= n)
            return 0;
        code = nodes[node].bit[(src[pos] & mask) != 0];
        mask <<= 1;
        if (mask == 256) {
            mask = 1;
            pos++;
        }
        if (code < 256) {
            dst[out++] = (uint8_t)code;
            node = 254;
        } else {
            if (code > 510)
                return 0;
            node = code - 256;
        }
    }
    return 1;
}
int WL_CarmackExpand(const uint8_t *src, size_t n, uint16_t *dst, size_t words) {
    size_t in = 0, out = 0;
    while (out < words) {
        uint16_t value;
        unsigned tag, count;
        size_t from;
        if (n - in < 2)
            return 0;
        value = WL_ReadLE16(src + in);
        in += 2;
        tag = value >> 8;
        count = value & 255;
        if (tag != 0xa7 && tag != 0xa8) {
            dst[out++] = value;
            continue;
        }
        if (!count) {
            if (in == n)
                return 0;
            dst[out++] = (uint16_t)((tag << 8) | src[in++]);
            continue;
        }
        if (count > words - out)
            return 0;
        if (tag == 0xa7) {
            unsigned distance;
            if (in == n)
                return 0;
            distance = src[in++];
            if (!distance || distance > out)
                return 0;
            from = out - distance;
        } else {
            if (n - in < 2)
                return 0;
            from = WL_ReadLE16(src + in);
            in += 2;
            if (from >= out)
                return 0;
        }
        while (count--)
            dst[out++] = dst[from++]; /* overlapping copies are intentional */
    }
    return 1;
}
int WL_RLEWExpand(const uint16_t *src, size_t words, uint16_t *dst, size_t count, uint16_t tag) {
    size_t in = 0, out = 0;
    while (out < count) {
        uint16_t value;
        size_t run;
        if (in == words)
            return 0;
        value = src[in++];
        if (value != tag) {
            dst[out++] = value;
            continue;
        }
        if (words - in < 2)
            return 0;
        run = src[in++];
        value = src[in++];
        if (run > count - out)
            return 0;
        while (run--)
            dst[out++] = value;
    }
    return 1;
}
int32_t WL_FixedPartial(int32_t step, uint16_t fraction) {
    return (int32_t)((int64_t)step * fraction / 65536);
}
int32_t WL_FixedByFrac(int32_t a, int32_t b) {
    int32_t result = WL_FixedPartial(a, (uint16_t)b);
    return b < 0 ? (int32_t)(0u - (uint32_t)result) : result;
}
