#include "runner.h"
#include "screenshot.h"
#include <stdio.h>
#include <string.h>
#include <Core/memory.h>

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

/* Advance one rendered frame and return its framebuffer hash. */
static uint64_t advance_one_frame(GB_gameboy_t *gb, const runner_config_t *cfg,
                                  runner_result_t *result)
{
    GB_run_frame(gb);
    result->frames_run++;
    unsigned w = GB_get_screen_width(gb);
    unsigned h = GB_get_screen_height(gb);
    return framebuffer_hash(cfg->framebuffer, (size_t)w * h);
}

/* Write the current frame to <dir>/screenshot_NNN.<ext>. Returns 0 on success. */
static int write_numbered(GB_gameboy_t *gb, const char *dir, unsigned id,
                          const char *suffix, const runner_config_t *cfg)
{
    char path[1200];
    snprintf(path, sizeof(path), "%s/screenshot_%03u%s.%s",
             dir, id, suffix, screenshot_extension());
    unsigned w = GB_get_screen_width(gb);
    unsigned h = GB_get_screen_height(gb);
    return screenshot_write(path, cfg->framebuffer, w, h);
}

/* Returns true if the comparison failed. */
static bool do_compare(GB_gameboy_t *gb, const command_t *cmd,
                       const runner_config_t *cfg, runner_result_t *result)
{
    if (cfg->update_mode) {
        if (write_numbered(gb, cfg->reference_dir, cmd->number, "", cfg) == 0)
            result->screenshots_written++;
        return false;
    }

    unsigned w = GB_get_screen_width(gb);
    unsigned h = GB_get_screen_height(gb);

    char ref_path[1200];
    snprintf(ref_path, sizeof(ref_path), "%s/screenshot_%03u.%s",
             cfg->reference_dir, cmd->number, screenshot_extension());

    static uint32_t ref[256 * 224];
    unsigned rw = 0, rh = 0;
    if (screenshot_read(ref_path, ref, 256 * 224, &rw, &rh) != 0) {
        fprintf(stderr, "compare (line %u): cannot read reference '%s'\n",
                cmd->line, ref_path);
        result->failures++;
        return true;
    }
    if (rw != w || rh != h ||
        memcmp(ref, cfg->framebuffer, (size_t)w * h * sizeof(uint32_t)) != 0) {
        if (write_numbered(gb, cfg->screenshot_dir, cmd->number, ".actual", cfg) == 0)
            result->screenshots_written++;
        fprintf(stderr, "compare (line %u): screen differs from '%s' "
                        "(wrote actual to %s/screenshot_%03u.actual.%s)\n",
                cmd->line, ref_path, cfg->screenshot_dir, cmd->number,
                screenshot_extension());
        result->failures++;
        return true;
    }
    return false;
}

/* Returns true if the assertion failed. */
static bool do_memory(GB_gameboy_t *gb, const command_t *cmd, runner_result_t *result)
{
    uint8_t v = GB_safe_read_memory(gb, cmd->addr);
    if (!cmd->has_value) {
        fprintf(stderr, "memory $%04X = $%02X (%u)\n", cmd->addr, v, v);
        return false;
    }
    if (v != (uint8_t)cmd->value) {
        fprintf(stderr, "memory (line %u): $%04X = $%02X (%u), expected $%02X (%u)\n",
                cmd->line, cmd->addr, v, v, cmd->value, cmd->value);
        result->failures++;
        return true;
    }
    return false;
}

/* Returns true if the screen failed to stabilize. */
static bool do_settle(GB_gameboy_t *gb, const command_t *cmd,
                      const runner_config_t *cfg, runner_result_t *result)
{
    settle_tracker_t st;
    settle_tracker_init(&st);
    for (;;) {
        uint64_t h = advance_one_frame(gb, cfg, result);
        settle_status_t s = settle_tracker_update(&st, h, cmd->count,
                                                  cfg->hang_timeout_frames);
        if (s == SETTLE_STABLE) return false;
        if (s == SETTLE_TIMEOUT) {
            fprintf(stderr, "settle (line %u): screen did not stabilize within %u frames\n",
                    cmd->line, cfg->hang_timeout_frames);
            result->failures++;
            return true;
        }
    }
}

void runner_run(GB_gameboy_t *gb, const script_t *script,
                const runner_config_t *cfg, runner_result_t *result)
{
    result->hang_detected = false;
    result->frames_run = 0;
    result->screenshots_written = 0;
    result->failures = 0;

    hang_tracker_t hang;
    hang_tracker_init(&hang);
    unsigned next_id = 0;
    bool stop = false;

    for (size_t i = 0; i < script->count && !stop; i++) {
        const command_t *cmd = &script->commands[i];
        switch (cmd->type) {
            case CMD_WAIT:
            case CMD_PRESS: {
                if (cmd->type == CMD_PRESS) GB_set_key_state(gb, cmd->key, true);
                for (unsigned f = 0; f < cmd->count; f++) {
                    uint64_t h = advance_one_frame(gb, cfg, result);
                    if (hang_tracker_update(&hang, h, cfg->hang_timeout_frames)) {
                        result->hang_detected = true;
                        result->failures++;
                        fprintf(stderr, "Hang detected (screen unchanged for %u frames).\n",
                                cfg->hang_timeout_frames);
                        if (write_numbered(gb, cfg->screenshot_dir, next_id++, "", cfg) == 0)
                            result->screenshots_written++;
                        stop = true; /* a hang is terminal */
                        break;
                    }
                }
                if (cmd->type == CMD_PRESS) GB_set_key_state(gb, cmd->key, false);
                break;
            }
            case CMD_SETTLE:
                if (do_settle(gb, cmd, cfg, result)) {
                    if (cfg->fail_fast) stop = true;
                } else {
                    /* A settled screen is legitimately static; start hang
                       detection fresh from it so the carried-over streak
                       doesn't trip a spurious hang on the next wait. */
                    hang_tracker_init(&hang);
                }
                break;
            case CMD_DOWN:
                GB_set_key_state(gb, cmd->key, true);
                break;
            case CMD_UP:
                GB_set_key_state(gb, cmd->key, false);
                break;
            case CMD_SCREENSHOT: {
                unsigned id = cmd->has_number ? cmd->number : next_id;
                next_id = id + 1;
                if (write_numbered(gb, cfg->screenshot_dir, id, "", cfg) == 0)
                    result->screenshots_written++;
                break;
            }
            case CMD_COMPARE: {
                bool failed = do_compare(gb, cmd, cfg, result);
                next_id = cmd->number + 1;
                if (failed && cfg->fail_fast) stop = true;
                break;
            }
            case CMD_MEMORY:
                if (do_memory(gb, cmd, result) && cfg->fail_fast) stop = true;
                break;
        }
    }
}
