#include "runner.h"
#include "screenshot.h"
#include <stdio.h>

uint64_t framebuffer_hash(const uint32_t *pixels, size_t pixel_count)
{
    /* FNV-1a over the pixel bytes. */
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < pixel_count; i++) {
        uint32_t p = pixels[i];
        for (int b = 0; b < 4; b++) {
            h ^= (uint8_t)(p >> (b * 8));
            h *= 1099511628211ull;
        }
    }
    return h;
}

void hang_tracker_init(hang_tracker_t *t)
{
    t->last_hash = 0;
    t->stale_frames = 0;
    t->primed = false;
}

bool hang_tracker_update(hang_tracker_t *t, uint64_t hash, unsigned limit)
{
    if (limit == 0) return false;
    if (!t->primed) {
        t->primed = true;
        t->last_hash = hash;
        t->stale_frames = 0;
        return false;
    }
    if (hash == t->last_hash) {
        t->stale_frames++;
    }
    else {
        t->last_hash = hash;
        t->stale_frames = 0;
    }
    return t->stale_frames >= limit;
}

void settle_tracker_init(settle_tracker_t *t)
{
    hang_tracker_init(&t->inner);
    t->waited = 0;
}

settle_status_t settle_tracker_update(settle_tracker_t *t, uint64_t hash,
                                      unsigned target, unsigned ceiling)
{
    t->waited++;
    if (hang_tracker_update(&t->inner, hash, target)) return SETTLE_STABLE;
    if (ceiling != 0 && t->waited >= ceiling) return SETTLE_TIMEOUT;
    return SETTLE_CONTINUE;
}

/* Advance one rendered frame; update hang tracking. Returns true on hang. */
static bool advance_frame(GB_gameboy_t *gb, const runner_config_t *cfg,
                          hang_tracker_t *tracker, runner_result_t *result)
{
    GB_run_frame(gb);
    result->frames_run++;
    unsigned w = GB_get_screen_width(gb);
    unsigned h = GB_get_screen_height(gb);
    uint64_t hash = framebuffer_hash(cfg->framebuffer, (size_t)w * h);
    return hang_tracker_update(tracker, hash, cfg->hang_timeout_frames);
}

static void write_auto_screenshot(GB_gameboy_t *gb, const runner_config_t *cfg,
                                  runner_result_t *result, const char *explicit_name)
{
    char auto_name[1200];
    const char *path = explicit_name;
    if (!path) {
        snprintf(auto_name, sizeof(auto_name), "%s_%03u.%s",
                 cfg->screenshot_basename, result->screenshots_written,
                 screenshot_extension());
        path = auto_name;
    }
    unsigned w = GB_get_screen_width(gb);
    unsigned h = GB_get_screen_height(gb);
    if (screenshot_write(path, cfg->framebuffer, w, h) == 0) {
        result->screenshots_written++;
    }
}

void runner_run(GB_gameboy_t *gb, const script_t *script,
                const runner_config_t *cfg, runner_result_t *result)
{
    result->hang_detected = false;
    result->frames_run = 0;
    result->screenshots_written = 0;

    hang_tracker_t tracker;
    hang_tracker_init(&tracker);

    for (size_t i = 0; i < script->count && !result->hang_detected; i++) {
        const command_t *cmd = &script->commands[i];
        switch (cmd->type) {
            case CMD_WAIT:
                for (unsigned f = 0; f < cmd->count && !result->hang_detected; f++) {
                    result->hang_detected = advance_frame(gb, cfg, &tracker, result);
                }
                break;
            case CMD_PRESS:
                GB_set_key_state(gb, cmd->key, true);
                for (unsigned f = 0; f < cmd->count && !result->hang_detected; f++) {
                    result->hang_detected = advance_frame(gb, cfg, &tracker, result);
                }
                GB_set_key_state(gb, cmd->key, false);
                break;
            case CMD_DOWN:
                GB_set_key_state(gb, cmd->key, true);
                break;
            case CMD_UP:
                GB_set_key_state(gb, cmd->key, false);
                break;
            case CMD_SCREENSHOT:
                write_auto_screenshot(gb, cfg, result, cmd->filename);
                break;
        }
    }

    if (result->hang_detected) {
        fprintf(stderr, "Hang detected (screen unchanged for %u frames); "
                        "writing final screenshot and stopping.\n",
                cfg->hang_timeout_frames);
        write_auto_screenshot(gb, cfg, result, NULL);
    }
}
