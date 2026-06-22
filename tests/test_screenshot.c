#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "screenshot.h"

/* A 2x2 image; values are arbitrary 32-bit pixels. */
static void test_bmp_header_and_size(void)
{
    screenshot_set_format(false); /* BMP */
    uint32_t pixels[4] = {0x11223344u, 0x55667788u, 0x99aabbccu, 0xddeeff00u};
    const char *path = "build/test_out.bmp";
    int rc = screenshot_write(path, pixels, 2, 2);
    assert(rc == 0);

    FILE *f = fopen(path, "rb");
    assert(f);
    unsigned char head[2];
    assert(fread(head, 1, 2, f) == 2);
    assert(head[0] == 0x42 && head[1] == 0x4D); /* 'BM' */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    /* 70-byte header + 4 pixels * 4 bytes = 86. The BMP size *fields* carry a
       phantom +2 (inherited from SameBoy), but no pad bytes are written. */
    assert(size == 70 + 4 * 4);
    fclose(f);
    remove(path);
}

static void test_tga_writes(void)
{
    screenshot_set_format(true); /* TGA */
    uint32_t pixels[4] = {0,0,0,0};
    const char *path = "build/test_out.tga";
    int rc = screenshot_write(path, pixels, 2, 2);
    assert(rc == 0);
    FILE *f = fopen(path, "rb");
    assert(f);
    unsigned char head[3];
    assert(fread(head, 1, 3, f) == 3);
    assert(head[2] == 0x02); /* TGA image type: uncompressed true-color */
    fclose(f);
    remove(path);
}

static void test_bmp_roundtrip(void)
{
    screenshot_set_format(false); /* BMP */
    uint32_t px[6] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u};
    const char *path = "build/test_rt.bmp";
    assert(screenshot_write(path, px, 3, 2) == 0);

    uint32_t out[6] = {0};
    unsigned w = 0, h = 0;
    assert(screenshot_read(path, out, 6, &w, &h) == 0);
    assert(w == 3 && h == 2);
    assert(memcmp(px, out, sizeof(px)) == 0);
    remove(path);
}

static void test_read_buffer_too_small(void)
{
    screenshot_set_format(false); /* BMP */
    uint32_t px[4] = {1, 2, 3, 4};
    const char *path = "build/test_small.bmp";
    assert(screenshot_write(path, px, 2, 2) == 0);

    uint32_t out[2];
    unsigned w = 0, h = 0;
    assert(screenshot_read(path, out, 2, &w, &h) == -1); /* 4 pixels > max 2 */
    remove(path);
}

static void test_tga_roundtrip(void)
{
    screenshot_set_format(true); /* TGA */
    uint32_t px[4] = {0xAABBCCDDu, 0u, 0x12345678u, 0xFFFFFFFFu};
    const char *path = "build/test_rt.tga";
    assert(screenshot_write(path, px, 2, 2) == 0);

    uint32_t out[4] = {0};
    unsigned w = 0, h = 0;
    assert(screenshot_read(path, out, 4, &w, &h) == 0);
    assert(w == 2 && h == 2);
    assert(memcmp(px, out, sizeof(px)) == 0);
    remove(path);
}

int main(void)
{
    test_bmp_roundtrip();
    test_read_buffer_too_small();
    test_tga_roundtrip();
    test_bmp_header_and_size();
    test_tga_writes();
    printf("test_screenshot: OK\n");
    return 0;
}
