#include "screenshot.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Write a 32-bit unsigned value as 4 little-endian bytes (no alignment req.). */
static void put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* Write a 32-bit signed value as 4 little-endian bytes (two's-complement). */
static void put_i32le(uint8_t *p, int32_t v)
{
    put_u32le(p, (uint32_t)v);
}

static bool use_tga = false;

/* Immutable header templates; each write copies one into a local buffer and
   patches the per-image fields there, so screenshot_write stays reentrant. */
static const uint8_t bmp_header_template[] = {
    0x42, 0x4D, 0x48, 0x68, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x38, 0x00,
    0x00, 0x00, 0xA0, 0x00, 0x00, 0x00, 0x70, 0xFF,
    0xFF, 0xFF, 0x01, 0x00, 0x20, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x02, 0x68, 0x01, 0x00, 0x12, 0x0B,
    0x00, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t tga_header_template[] = {
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xA0, 0x00, 0x90, 0x00,
    0x20, 0x28,
};

void screenshot_set_format(bool tga)
{
    use_tga = tga;
}

const char *screenshot_extension(void)
{
    return use_tga ? "tga" : "bmp";
}

uint32_t screenshot_rgb_encode(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b)
{
    (void)gb;
#ifdef GB_BIG_ENDIAN
    if (use_tga) {
        return (r << 8) | (g << 16) | (b << 24);
    }
    return (r << 0) | (g << 8) | (b << 16);
#else
    if (use_tga) {
        return (r << 16) | (g << 8) | (b);
    }
    return (r << 24) | (g << 16) | (b << 8);
#endif
}

int screenshot_write(const char *path, const uint32_t *pixels,
                     unsigned w, unsigned h)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Failed to write screenshot '%s'\n", path);
        return -1;
    }

    if (use_tga) {
        uint8_t header[sizeof(tga_header_template)];
        memcpy(header, tga_header_template, sizeof(header));
        header[0xC] = w;
        header[0xD] = w >> 8;
        header[0xE] = h;
        header[0xF] = h >> 8;
        fwrite(header, 1, sizeof(header), f);
    }
    else {
        uint8_t header[sizeof(bmp_header_template)];
        memcpy(header, bmp_header_template, sizeof(header));
        put_u32le(&header[0x2],  (uint32_t)(sizeof(header) + sizeof(pixels[0]) * w * h + 2));
        put_u32le(&header[0x12], w);
        put_i32le(&header[0x16], -(int32_t)h);
        put_u32le(&header[0x22], (uint32_t)(sizeof(pixels[0]) * w * h + 2));
        fwrite(header, 1, sizeof(header), f);
    }
    fwrite(pixels, 1, sizeof(pixels[0]) * w * h, f);
    fclose(f);

    fprintf(stderr, "Wrote screenshot %s\n", path);
    return 0;
}

int screenshot_read(const char *path, uint32_t *pixels, size_t max_pixels,
                    unsigned *width, unsigned *height)
{
    if (!path || !pixels || !width || !height) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    int rc = -1;
    unsigned w = 0, h = 0;
    long header_size = 0;

    if (use_tga) {
        uint8_t hdr[sizeof(tga_header_template)];
        if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) goto done;
        w = (unsigned)hdr[0xC] | ((unsigned)hdr[0xD] << 8);
        h = (unsigned)hdr[0xE] | ((unsigned)hdr[0xF] << 8);
        header_size = (long)sizeof(hdr);
    }
    else {
        uint8_t hdr[sizeof(bmp_header_template)];
        if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) goto done;
        if (hdr[0] != 0x42 || hdr[1] != 0x4D) goto done; /* 'BM' */
        w = (unsigned)hdr[0x12] | ((unsigned)hdr[0x13] << 8)
          | ((unsigned)hdr[0x14] << 16) | ((unsigned)hdr[0x15] << 24);
        /* BMP height is signed (negative = top-down). Take the magnitude in the
           unsigned domain so an on-disk INT32_MIN can't trigger negation UB. */
        uint32_t raw_h = (uint32_t)hdr[0x16] | ((uint32_t)hdr[0x17] << 8)
          | ((uint32_t)hdr[0x18] << 16) | ((uint32_t)hdr[0x19] << 24);
        h = (raw_h & 0x80000000u) ? (unsigned)(0u - raw_h) : (unsigned)raw_h;
        header_size = (long)sizeof(hdr);
    }

    if (w == 0 || h == 0) goto done;
    /* w != 0 here, so the division can't trap; this avoids the overflow a
       direct `w * h > max_pixels` would risk when size_t is 32-bit. */
    if ((size_t)h > max_pixels / w) goto done;

    if (fseek(f, header_size, SEEK_SET) != 0) goto done;
    size_t n = (size_t)w * h;
    if (fread(pixels, sizeof(uint32_t), n, f) != n) goto done;

    *width = w;
    *height = h;
    rc = 0;
done:
    fclose(f);
    return rc;
}
