#!/usr/bin/env bash
# Functional integration tests: run boyless against the assembly test ROMs and
# assert observable behavior. Requires `make && make testroms` first.
set -euo pipefail
cd "$(dirname "$0")/.."

BOYLESS=build/bin/boyless
ROMS=build/testroms
WORK=build/integration
rm -rf "$WORK"; mkdir -p "$WORK"

# 1. fill (DMG, static) must trigger hang detection (exit non-zero, stderr
# contains "Hang detected").  On a hang, boyless writes a final auto-screenshot
# ("boyless_000.bmp") to the current directory; redirect it into $WORK so it
# doesn't litter the repo root.
_hang_rc=0
( cd "$WORK" && printf 'wait 1200\n' \
        | "../../$BOYLESS" --model dmg --rom "../../$ROMS/fill.gb" --fail-on-hang - \
        2>"../../$WORK/hang_stderr.txt" ) || _hang_rc=$?
if [ "$_hang_rc" -eq 0 ]; then
    echo "FAIL: fill ROM did not exit non-zero (hang not detected)"; exit 1
fi
if ! grep -q "Hang detected" "$WORK/hang_stderr.txt"; then
    echo "FAIL: fill ROM exited non-zero but stderr did not contain 'Hang detected'"; exit 1
fi
echo "PASS: fill ROM triggers hang detection"

# 2. fill produces a non-blank screenshot (hang detection off).
printf 'wait 300\nscreenshot %s/fill.bmp\n' "$WORK" \
    | "$BOYLESS" --model dmg --rom "$ROMS/fill.gb" --hang-timeout 0 -
test -s "$WORK/fill.bmp" || { echo "FAIL: no fill screenshot"; exit 1; }
echo "PASS: fill screenshot written"

# 3. input (CGB): every button yields a distinct screen.
# Wait out the boot animation BEFORE pressing, so held directions don't trip
# the CGB boot ROM's DMG-compatibility palette selection (which would leave the
# four directions stuck on the same boot-assigned palette).
buttons="a b start select up down left right"
for k in $buttons; do
    printf 'wait 300\ndown %s\nwait 60\nscreenshot %s/shot_%s.bmp\n' "$k" "$WORK" "$k" \
        | "$BOYLESS" --model cgb --rom "$ROMS/input.gb" --hang-timeout 0 -
done
distinct=$(for k in $buttons; do sha1sum "$WORK/shot_$k.bmp" | cut -d' ' -f1; done | sort -u | wc -l)
if [ "$distinct" -ne 8 ]; then
    echo "FAIL: buttons not all distinct ($distinct/8 unique screens)"; exit 1
fi
echo "PASS: all 8 buttons produce distinct screens"
echo "integration: OK"
