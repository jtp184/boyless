#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <Core/gb.h>
#include <Core/random.h>

#include "screenshot.h"
#include "script.h"
#include "runner.h"

#define BOYLESS_VERSION "0.1.0"

/* 256x224 is SameBoy's maximum screen (SGB borders); GB/GBC use a subregion. */
static uint32_t framebuffer[256 * 224];

static void log_callback(GB_gameboy_t *gb, const char *string, GB_log_attributes_t attributes)
{
    (void)gb; (void)attributes;
    fprintf(stderr, "%s", string);
}

static const char *executable_folder(void)
{
    static char path[1024] = {0};
    if (path[0]) return path;
#ifdef __linux__
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n < 0) {
        /* Exe path unavailable (unusual container/mount): fall back to the
           current directory rather than risk an out-of-bounds write. */
        if (!getcwd(path, sizeof(path) - 1)) path[0] = 0;
        return path;
    }
    path[n] = 0;
#else
    if (!getcwd(path, sizeof(path) - 1)) path[0] = 0;
    return path;
#endif
    for (size_t pos = strlen(path); pos--;) {
        if (path[pos] == '/') { path[pos] = 0; break; }
    }
    return path;
}

static char *executable_relative_path(const char *filename)
{
    static char path[1024];
    snprintf(path, sizeof(path), "%s/%s", executable_folder(), filename);
    return path;
}

/* Derive the auto-screenshot basename: strip directory and extension.
   For stdin ("-"/NULL) use "boyless". */
static void derive_basename(const char *script_path, char *out, size_t out_sz)
{
    const char *src = (!script_path || strcmp(script_path, "-") == 0) ? "boyless" : script_path;
    strncpy(out, src, out_sz - 1);
    out[out_sz - 1] = 0;
    char *slash = strrchr(out, '/');
    char *base = slash ? slash + 1 : out;
    char *dot = strrchr(base, '.');
    if (dot) *dot = 0;
    if (base != out) memmove(out, base, strlen(base) + 1);
}

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [options] [script-file | -]\n"
        "  --rom <path>          ROM to load (required)\n"
        "  --script <path>       script file ('-' or omitted reads stdin)\n"
        "  --boot <path>         boot ROM (defaults per model)\n"
        "  --model <dmg|cgb|sgb> emulated model (default cgb)\n"
        "  --tga                 write TGA screenshots instead of BMP\n"
        "  --hang-timeout <sec>  seconds of frozen video before flagging (default 5, 0 disables)\n"
        "  --fail-on-hang        exit non-zero if a hang is detected\n"
        "  --version             print version and exit\n",
        argv0);
}

int main(int argc, char **argv)
{
    const char *rom_path = NULL;
    const char *script_path = NULL;
    const char *boot_path = NULL;
    const char *model_name = "cgb";
    bool use_tga = false;
    bool fail_on_hang = false;
    double hang_timeout_sec = 5.0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--version") == 0) { printf("boyless %s\n", BOYLESS_VERSION); return 0; }
        else if (strcmp(a, "--tga") == 0) { use_tga = true; }
        else if (strcmp(a, "--fail-on-hang") == 0) { fail_on_hang = true; }
        else if (strcmp(a, "--rom") == 0 && i + 1 < argc) { rom_path = argv[++i]; }
        else if (strcmp(a, "--script") == 0 && i + 1 < argc) { script_path = argv[++i]; }
        else if (strcmp(a, "--boot") == 0 && i + 1 < argc) { boot_path = argv[++i]; }
        else if (strcmp(a, "--model") == 0 && i + 1 < argc) { model_name = argv[++i]; }
        else if (strcmp(a, "--hang-timeout") == 0 && i + 1 < argc) {
            const char *val = argv[++i];
            char *end;
            double t = strtod(val, &end);
            if (end == val || *end != '\0' || t < 0.0) {
                fprintf(stderr, "Invalid --hang-timeout value '%s' (must be a non-negative number)\n", val);
                return 1;
            }
            hang_timeout_sec = t;
        }
        else if (strcmp(a, "-") == 0) { script_path = a; }
        else if (a[0] == '-') { fprintf(stderr, "Unknown option: %s\n", a); usage(argv[0]); return 1; }
        else if (!script_path) { script_path = a; }
        else { fprintf(stderr, "Unexpected argument: %s\n", a); usage(argv[0]); return 1; }
    }

    if (!rom_path) { fprintf(stderr, "A --rom is required.\n"); usage(argv[0]); return 1; }

    GB_model_t model;
    const char *default_boot;
    if (strcmp(model_name, "dmg") == 0)      { model = GB_MODEL_DMG_B; default_boot = "dmg_boot.bin"; }
    else if (strcmp(model_name, "sgb") == 0) { model = GB_MODEL_SGB2;  default_boot = "sgb2_boot.bin"; }
    else if (strcmp(model_name, "cgb") == 0) { model = GB_MODEL_CGB_E; default_boot = "cgb_boot.bin"; }
    else { fprintf(stderr, "Unknown model '%s' (use dmg|cgb|sgb)\n", model_name); return 1; }

    screenshot_set_format(use_tga);
    GB_random_set_enabled(false);

    GB_gameboy_t gb;
    GB_init(&gb, model);
    GB_set_log_callback(&gb, log_callback);

    const char *boot = boot_path ? boot_path : executable_relative_path(default_boot);
    if (GB_load_boot_rom(&gb, boot)) {
        fprintf(stderr, "Failed to load boot ROM from '%s'\n", boot);
        GB_free(&gb);
        return 1;
    }
    if (GB_load_rom(&gb, rom_path)) {
        fprintf(stderr, "Failed to load ROM '%s'\n", rom_path);
        GB_free(&gb);
        return 1;
    }

    GB_set_pixels_output(&gb, framebuffer);
    GB_set_rgb_encode_callback(&gb, screenshot_rgb_encode);
    GB_set_color_correction_mode(&gb, GB_COLOR_CORRECTION_MODERN_BALANCED);
    GB_set_rtc_mode(&gb, GB_RTC_MODE_ACCURATE);
    GB_set_emulate_joypad_bouncing(&gb, false);
    GB_set_turbo_mode(&gb, true, true); /* run fast, never skip rendering */

    script_t script = {0};
    if (!script_parse_path(script_path, &script)) {
        GB_free(&gb);
        return 1;
    }

    char basename[1024];
    derive_basename(script_path, basename, sizeof(basename));

    runner_config_t cfg = {
        .framebuffer = framebuffer,
        .screenshot_basename = basename,
        .hang_timeout_frames = hang_timeout_sec > 0 ? (unsigned)(hang_timeout_sec * 60.0) : 0,
    };
    runner_result_t result = {0};
    runner_run(&gb, &script, &cfg, &result);

    fprintf(stderr, "Ran %u frames, wrote %u screenshot(s).\n",
            result.frames_run, result.screenshots_written);

    script_free(&script);
    GB_free(&gb);

    if (result.hang_detected && fail_on_hang) return 1;
    return 0;
}
