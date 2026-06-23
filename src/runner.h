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

typedef enum {
    SETTLE_CONTINUE,  /* keep advancing frames */
    SETTLE_STABLE,    /* screen held steady for `target` frames */
    SETTLE_TIMEOUT,   /* `ceiling` frames elapsed without stabilizing */
} settle_status_t;

typedef struct {
    uint64_t last_hash; /* hash of the current stable run */
    unsigned stable;    /* length of that run, counted inclusively */
    unsigned waited;    /* total frames advanced this settle */
    bool     primed;    /* a frame has been seen */
} settle_tracker_t;

void settle_tracker_init(settle_tracker_t *t);
/* Feed one frame's hash. `target` = consecutive identical frames required,
   counted inclusively so `target` N stabilizes on the Nth such frame (>= 1);
   `ceiling` = max frames to wait (0 disables the timeout). */
settle_status_t settle_tracker_update(settle_tracker_t *t, uint64_t hash,
                                      unsigned target, unsigned ceiling);

typedef struct {
    uint32_t   *framebuffer;          /* GB_set_pixels_output target */
    const char *screenshot_dir;       /* dir for screenshot writes + .actual dumps */
    const char *reference_dir;        /* dir compare reads/writes goldens */
    unsigned    hang_timeout_frames;  /* 0 disables hang detection */
    bool        update_mode;          /* compare writes references instead of asserting */
    bool        fail_fast;            /* stop at first assertion failure */
} runner_config_t;

typedef struct {
    bool     hang_detected;
    unsigned frames_run;
    unsigned screenshots_written;
    unsigned failures;                /* compare + memory + settle + hang */
} runner_result_t;

/* Execute the script against `gb`. Caller must have initialized the core,
   loaded ROM/boot ROM, and called GB_set_pixels_output(gb, config->framebuffer). */
void runner_run(GB_gameboy_t *gb, const script_t *script,
                const runner_config_t *config, runner_result_t *result);

#endif
