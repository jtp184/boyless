# boyless

A standalone, headless Game Boy automation harness built on the
[SameBoy](https://github.com/LIJI32/SameBoy) core. It loads a ROM, runs a
script of timed button inputs, and captures screenshots — for automated,
reproducible testing and capture without a GUI.

It links the SameBoy core as a static library and uses only SameBoy's public
API. SameBoy is included as a pinned git submodule.

## Build

    git clone <this repo>
    cd boyless
    git submodule update --init --recursive
    make

This builds SameBoy's library and boot ROMs, then `build/bin/boyless`.

## Usage

    build/bin/boyless --rom game.gb --script examples/example.script
    # or pipe a script via stdin:
    cat examples/example.script | build/bin/boyless --rom game.gb -

Options:

| Option | Meaning |
|--------|---------|
| `--rom <path>` | ROM to load (required) |
| `--script <path>` | script file (`-` or omitted reads stdin) |
| `--boot <path>` | boot ROM (defaults per model) |
| `--model <dmg\|cgb\|sgb>` | emulated model (default cgb) |
| `--tga` | write TGA screenshots instead of BMP |
| `--screenshot-dir <dir>` | directory for screenshots and `.actual` dumps (default `.`) |
| `--reference-dir <dir>` | directory `compare` reads references from / writes under `--update` (default `.`) |
| `--update` | make `compare` write references instead of asserting |
| `--fail-fast` | stop at the first failed assertion (default: run to completion) |
| `--report-only` | print failures but always exit 0 |
| `--hang-timeout <sec>` | seconds of frozen video before flagging (default 5, 0 disables) |

## Script format

One command per line; `#` starts a comment.

| Command | Effect |
|---------|--------|
| `wait <frames>` | advance N rendered frames |
| `settle <frames>` | advance until the screen is unchanged for N consecutive frames; fails if it can't stabilize within the hang-timeout |
| `press <key> [frames]` | hold key for N frames (default 2), then release |
| `down <key>` | press and hold a key |
| `up <key>` | release a key |
| `screenshot [number]` | write `screenshot_NNN.<ext>`; auto-numbered unless an id is given |
| `compare <number>` | compare the live screen exactly to reference `screenshot_NNN.<ext>`; mismatch is a failure |
| `memory <addr> [value]` | read the byte at 16-bit hex `addr`; with `value` assert equality, without it print the byte |

Keys: `a b start select up down left right`.

Screenshots and references are named `screenshot_NNN.<ext>` (`<ext>` is `bmp`,
or `tga` with `--tga`), where `NNN` is a zero-padded counter shared by
`screenshot` and `compare`. Pass a number to pin a specific id.

Memory addresses are 16-bit hex (a leading `$` is optional). A `memory` value
is decimal by default, or hex with a `$` prefix; both forms are a single byte.

### Assertions and exit codes

`compare`, `memory`, a `settle` that never stabilizes, and a detected hang are
all **failures**. boyless runs the whole script, reports every failure, and
exits non-zero if any occurred — so it works directly as a test in CI. Use
`--fail-fast` to stop at the first failure, or `--report-only` to inspect
failures without affecting the exit code.

To create golden references, run the script once with `--update` (which makes
`compare` write `screenshot_NNN` into the reference dir instead of asserting),
then run it again without `--update` to check against them.

`settle` uses the hang-timeout as its stabilization ceiling: it fails (a
timeout) if the screen hasn't held steady for N frames within that window.
**`--hang-timeout 0` disables that ceiling**, so a `settle` on a screen that
never stabilizes will run forever — only disable the hang-timeout when your
script doesn't rely on `settle` to bound itself.

### Boot timing

Frame counts include the boot ROM. Every `wait`/`press` advances real emulated
frames starting from reset, and the boot ROM animation runs for roughly 300
frames before the cartridge's own code begins. Scripts that capture game output
should `wait` at least ~300 frames before the first `screenshot`, as
`examples/example.script` does.

## Tests

    make test          # unit tests (parser, screenshot writer, hang detector) — no ROM
    make integration   # functional tests against the bundled assembly test ROMs

`make test` needs no ROM. `make integration` assembles the test ROMs in
`testroms/` (requires RGBDS) and runs `boyless` against them to prove the DMG
path + hang detection (`fill.asm`), per-button input dispatch on CGB
(`input.asm`), and the `compare` and `memory` assertion paths (`mem.asm`).

## Continuous integration

`.github/workflows/ci.yml` builds the project and runs both `make test` and
`make integration` on every push and pull request (Ubuntu, with RGBDS installed
for the assembly test ROMs).

## License

boyless is released under the MIT (Expat) License — see [`LICENSE`](LICENSE).

It builds on [SameBoy](https://github.com/LIJI32/SameBoy), included as a git
submodule under `third_party/SameBoy` and itself licensed under the Expat
License. SameBoy is the property of its authors; see
`third_party/SameBoy/LICENSE` for its terms.
