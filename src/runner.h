#ifndef BOYLESS_RUNNER_H
#define BOYLESS_RUNNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <Core/gb.h>
#include "script.h"

/* Pure helpers (unit-testable without an emulator). */
uint64_t framebuffer_hash(const uint32_t *pixels, size_t pixel_count);

typedef struct {
    uint64_t last_hash;
    unsigned stale_frames;
    bool     primed;
} hang_tracker_t;

void hang_tracker_init(hang_tracker_t *t);
/* Feed one frame's hash. Returns true once `stale_frames >= limit`
   (limit == 0 disables and always returns false). */
bool hang_tracker_update(hang_tracker_t *t, uint64_t hash, unsigned limit);

typedef struct {
    uint32_t   *framebuffer;          /* GB_set_pixels_output target */
    const char *screenshot_basename;  /* for auto-named screenshots */
    unsigned    hang_timeout_frames;  /* 0 disables hang detection */
} runner_config_t;

typedef struct {
    bool     hang_detected;
    unsigned frames_run;
    unsigned screenshots_written;
} runner_result_t;

/* Execute the script against `gb`. Caller must have initialized the core,
   loaded ROM/boot ROM, and called GB_set_pixels_output(gb, config->framebuffer). */
void runner_run(GB_gameboy_t *gb, const script_t *script,
                const runner_config_t *config, runner_result_t *result);

#endif
