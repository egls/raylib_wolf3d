/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SD_AUDIO_H
#define SD_AUDIO_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* All calls on an engine must be serialized. Input buffers are borrowed and
 * must remain alive until stopped/replaced. mix outputs interleaved stereo. */
typedef struct sd_audio sd_audio;
typedef void (*sd_register_hook)(void *user, uint64_t frame, uint8_t reg, uint8_t value);
sd_audio *sd_audio_create(unsigned sample_rate);
void sd_audio_destroy(sd_audio *s);
void sd_audio_set_register_hook(sd_audio *s, sd_register_hook hook, void *user);
/* IMF type 0 (first register/value word must be zero) or type 1
 * (little-endian length prefix), using the original engine detection. Returns 0 on
 * malformed input; failed starts leave current playback unchanged. */
int sd_audio_music(sd_audio *s, const uint8_t *data, size_t bytes, int loop);
void sd_audio_stop_music(sd_audio *s);
/* Raw on-disk SoundCommon + instrument/block/data, not host structs.
 * Return 1 accepted, 0 rejected (bad input or lower priority). */
int sd_audio_adlib(sd_audio *s, const uint8_t *chunk, size_t bytes);
int sd_audio_pc(sd_audio *s, const uint8_t *chunk, size_t bytes);
void sd_audio_stop_effect(sd_audio *s);
/* Unsigned 8-bit 7042 Hz PCM. Attenuations match SD_PositionSound: 0..15.
 * Both 15 is invalid. PCM priority is independent from synthesized effects. */
int sd_audio_pcm(sd_audio *s, const uint8_t *data, size_t bytes,
                 unsigned priority, unsigned left, unsigned right);
void sd_audio_stop_pcm(sd_audio *s);
void sd_audio_mix(sd_audio *s, int16_t *out, size_t frames);
unsigned sd_audio_music_hz(void);
#ifdef __cplusplus
}
#endif
#endif
