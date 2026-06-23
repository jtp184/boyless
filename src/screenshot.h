#ifndef BOYLESS_SCREENSHOT_H
#define BOYLESS_SCREENSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <Core/gb.h>

/* Select output format. false => BMP (default), true => TGA. Affects both the
   rgb_encode byte order and the file written. */
void screenshot_set_format(bool use_tga);

/* File extension for the current format ("bmp" or "tga"), for auto-naming. */
const char *screenshot_extension(void);

/* Register with GB_set_rgb_encode_callback. Encodes a pixel for the current
   output format. */
uint32_t screenshot_rgb_encode(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b);

/* Write `width`x`height` pixels from `pixels` to `path` in the current format.
   Returns 0 on success, -1 on failure (also logs to stderr). */
int screenshot_write(const char *path, const uint32_t *pixels,
                     unsigned width, unsigned height);

/* Read a `width`x`height` image written by screenshot_write back into `pixels`
   (in the current format — set it with screenshot_set_format first). Fails if
   width*height exceeds `max_pixels`. Returns 0 on success, -1 on failure. */
int screenshot_read(const char *path, uint32_t *pixels, size_t max_pixels,
                    unsigned *width, unsigned *height);

#endif
