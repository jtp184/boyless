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

int main(void)
{
    test_bmp_header_and_size();
    test_tga_writes();
    printf("test_screenshot: OK\n");
    return 0;
}
