#include "wl_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    uint8_t le[] = {0x78, 0x56, 0x34, 0x12};
    uint8_t stream[] = {0x34, 0x12, 0x03, 0xa7, 0x01, 0x00, 0xa7, 0x42, 0x02, 0xa8, 0x00, 0x00};
    uint16_t out[7], rle[] = {1, 0xabcd, 3, 2, 0xabcd, 1, 0xabcd}, expanded[5];
    WL_HuffNode nodes[255] = {0};
    uint8_t bits[] = {5}, decoded[3];
    CHECK(WL_ReadLE16(le) == 0x5678 && WL_ReadLE24(le) == 0x345678 &&
          WL_ReadLE32(le) == 0x12345678);
    CHECK(WL_CarmackExpand(stream, sizeof stream, out, 7));
    CHECK(out[0] == 0x1234 && out[3] == 0x1234 && out[4] == 0xa742 && out[5] == 0x1234 &&
          out[6] == 0x1234);
    CHECK(!WL_CarmackExpand(stream, 1, out, 7));
    CHECK(!WL_CarmackExpand(stream + 2, 3, out, 3));
    CHECK(!WL_CarmackExpand(stream, sizeof stream, out, 2));
    CHECK(WL_RLEWExpand(rle, 7, expanded, 5, 0xabcd));
    CHECK(expanded[0] == 1 && expanded[3] == 2 && expanded[4] == 0xabcd);
    CHECK(!WL_RLEWExpand(rle, 2, expanded, 5, 0xabcd));
    CHECK(!WL_RLEWExpand(rle, 7, expanded, 3, 0xabcd));
    nodes[254].bit[0] = 'A';
    nodes[254].bit[1] = 'B';
    CHECK(WL_HuffExpand(bits, 1, decoded, 3, nodes));
    CHECK(!memcmp(decoded, "BAB", 3));
    CHECK(!WL_HuffExpand(bits, 0, decoded, 3, nodes));
    nodes[254].bit[1] = 511;
    CHECK(!WL_HuffExpand(bits, 1, decoded, 3, nodes));
    CHECK(WL_FixedPartial(-65537, 32768) == -32768);
    CHECK(WL_FixedPartial(INT32_MIN, 65535) == -2147450880);
    CHECK(WL_FixedByFrac(65537, (int32_t)0x80008000) == -32768);
    CHECK(WL_FixedByFrac(-65537, (int32_t)0x80008000) == 32768);
    CHECK(WL_FixedByFrac(12345, 0x10000) == 0); /* table's 1.0 encoding clamps to 65535 */
    puts("PASS: endian, overlapping Carmack, escapes, RLEW, Huffman, signed fixed point");
    return 0;
}
