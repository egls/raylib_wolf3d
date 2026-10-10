#ifndef WL_DATA_H
#define WL_DATA_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint16_t bit[2];
} WL_HuffNode;
uint16_t WL_ReadLE16(const void *p);
uint32_t WL_ReadLE24(const void *p);
uint32_t WL_ReadLE32(const void *p);
int WL_HuffExpand(const uint8_t *src, size_t n, uint8_t *dst, size_t len,
                  const WL_HuffNode nodes[255]);
int WL_CarmackExpand(const uint8_t *src, size_t n, uint16_t *dst, size_t words);
int WL_RLEWExpand(const uint16_t *src, size_t words, uint16_t *dst, size_t count, uint16_t tag);
/* Multiply with DOS truncation toward zero, not arithmetic right shift. */
int32_t WL_FixedPartial(int32_t step, uint16_t fraction);
int32_t WL_FixedByFrac(int32_t a, int32_t sign_magnitude);
#endif
