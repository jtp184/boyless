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
| `--hang-timeout <sec>` | seconds of frozen video before flagging (default 5, 0 disables) |
| `--fail-on-hang` | exit non-zero if a hang is detected |

## Script format

One command per line; `#` starts a comment.

| Command | Effect |
|---------|--------|
| `wait <frames>` | advance N rendered frames |
| `press <key> [frames]` | hold key for N frames (default 2), then release |
| `down <key>` | press and hold a key |
| `up <key>` | release a key |
| `screenshot [name]` | capture; auto-named `<script>_NNN.bmp` if no name |

Keys: `a b start select up down left right`.

Screenshots are written to the current directory; auto-named shots use the
script's basename and a zero-padded counter.

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
path + hang detection (`fill.asm`) and per-button input dispatch on CGB
(`input.asm`).

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
