#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <Core/gb.h>
#include <Core/random.h>

#include "screenshot.h"
#include "script.h"
#include "symbols.h"
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

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [options] [script-file | -]\n"
        "  --rom <path>          ROM to load (required)\n"
        "  --script <path>       script file ('-' or omitted reads stdin)\n"
        "  --sym <path>          .sym file; {symbol} refs in the script expand to addresses\n"
        "  --boot <path>         boot ROM (defaults per model)\n"
        "  --model <dmg|cgb|sgb> emulated model (default cgb)\n"
        "  --tga                 write TGA screenshots instead of BMP\n"
        "  --screenshot-dir <d>  dir for screenshots and .actual dumps (default .)\n"
        "  --reference-dir <d>   dir for compare references (default .)\n"
        "  --update              write compare references instead of asserting\n"
        "  --fail-fast           stop at the first failed assertion\n"
        "  --report-only         print failures but always exit 0\n"
        "  --hang-timeout <sec>  seconds of frozen video before flagging (default 5, 0 disables)\n"
        "  --version             print version and exit\n",
        argv0);
}

int main(int argc, char **argv)
{
    const char *rom_path = NULL;
    const char *script_path = NULL;
    const char *sym_path = NULL;
    const char *boot_path = NULL;
    const char *model_name = "cgb";
    const char *screenshot_dir = ".";
    const char *reference_dir = ".";
    bool use_tga = false;
    bool update_mode = false;
    bool fail_fast = false;
    bool report_only = false;
    double hang_timeout_sec = 5.0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--version") == 0) { printf("boyless %s\n", BOYLESS_VERSION); return 0; }
        else if (strcmp(a, "--tga") == 0) { use_tga = true; }
        else if (strcmp(a, "--update") == 0) { update_mode = true; }
        else if (strcmp(a, "--fail-fast") == 0) { fail_fast = true; }
        else if (strcmp(a, "--report-only") == 0) { report_only = true; }
        /* Consume the next argv as this flag's value, or fail with a clear
           "needs a value" message rather than a misleading "unknown option". */
#define REQUIRE_VALUE(dest) do { \
            if (i + 1 >= argc) { \
                fprintf(stderr, "Option %s needs a value\n", a); \
                usage(argv[0]); \
                return 1; \
            } \
            (dest) = argv[++i]; \
        } while (0)
        else if (strcmp(a, "--screenshot-dir") == 0) { REQUIRE_VALUE(screenshot_dir); }
        else if (strcmp(a, "--reference-dir") == 0)  { REQUIRE_VALUE(reference_dir); }
        else if (strcmp(a, "--rom") == 0)            { REQUIRE_VALUE(rom_path); }
        else if (strcmp(a, "--script") == 0)         { REQUIRE_VALUE(script_path); }
        else if (strcmp(a, "--sym") == 0)            { REQUIRE_VALUE(sym_path); }
        else if (strcmp(a, "--boot") == 0)           { REQUIRE_VALUE(boot_path); }
        else if (strcmp(a, "--model") == 0)          { REQUIRE_VALUE(model_name); }
        else if (strcmp(a, "--hang-timeout") == 0) {
            const char *val;
            REQUIRE_VALUE(val);
            char *end;
            double t = strtod(val, &end);
            if (end == val || *end != '\0' || t < 0.0) {
                fprintf(stderr, "Invalid --hang-timeout value '%s' (must be a non-negative number)\n", val);
                return 1;
            }
            hang_timeout_sec = t;
        }
#undef REQUIRE_VALUE
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

    symbols_t *syms = NULL;
    if (sym_path && !symbols_load(sym_path, &syms)) {
        GB_free(&gb);
        return 1;
    }

    script_t script = {0};
    if (!script_parse_path(script_path, syms, &script)) {
        symbols_free(syms);
        GB_free(&gb);
        return 1;
    }

    runner_config_t cfg = {
        .framebuffer = framebuffer,
        .screenshot_dir = screenshot_dir,
        .reference_dir = reference_dir,
        .hang_timeout_frames = hang_timeout_sec > 0 ? (unsigned)(hang_timeout_sec * 60.0) : 0,
        .update_mode = update_mode,
        .fail_fast = fail_fast,
    };
    runner_result_t result = {0};
    runner_run(&gb, &script, &cfg, &result);

    fprintf(stderr, "Ran %u frames, wrote %u screenshot(s), %u failure(s).\n",
            result.frames_run, result.screenshots_written, result.failures);

    script_free(&script);
    symbols_free(syms);
    GB_free(&gb);

    if (result.failures > 0 && !report_only) return 1;
    return 0;
}
