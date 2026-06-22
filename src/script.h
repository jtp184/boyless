#ifndef BOYLESS_SCRIPT_H
#define BOYLESS_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <Core/gb.h>

typedef enum {
    CMD_WAIT,        /* count = frames to advance */
    CMD_PRESS,       /* key held for `count` frames, then released */
    CMD_DOWN,        /* key pressed and left down */
    CMD_UP,          /* key released */
    CMD_SCREENSHOT,  /* filename != NULL => explicit name, else auto */
} cmd_type_t;

typedef struct {
    cmd_type_t type;
    GB_key_t   key;       /* press/down/up */
    unsigned   count;     /* wait: frames; press: hold frames */
    char      *filename;  /* screenshot: explicit name, or NULL for auto */
    unsigned   line;      /* 1-based source line, for diagnostics */
} command_t;

typedef struct {
    command_t *commands;
    size_t     count;
} script_t;

/* Parse a script from an open stream. `label` is used in error messages.
   On success fills `out` and returns true. On failure logs to stderr and
   returns false (out is left zeroed). */
bool script_parse_stream(FILE *stream, const char *label, script_t *out);

/* Open `path` and parse it. If `path` is NULL or "-", reads from stdin. */
bool script_parse_path(const char *path, script_t *out);

void script_free(script_t *script);

#endif
