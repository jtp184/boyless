#!/usr/bin/env bash
# Functional integration tests: run boyless against the assembly test ROMs and
# assert observable behavior. Requires `make && make testroms` first.
set -euo pipefail
cd "$(dirname "$0")/.."

BOYLESS=build/bin/boyless
ROMS=build/testroms
WORK=build/integration
rm -rf "$WORK"; mkdir -p "$WORK"

# 1. fill (DMG, static) must trigger hang detection. A hang is now a failure by
# default, so boyless exits non-zero with no special flag. The final auto
# screenshot lands in the screenshot dir ($WORK).
_hang_rc=0
printf 'wait 1200\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/fill.gb" --screenshot-dir "$WORK" - \
    2>"$WORK/hang_stderr.txt" || _hang_rc=$?
if [ "$_hang_rc" -eq 0 ]; then
    echo "FAIL: fill ROM did not exit non-zero (hang not detected)"; exit 1
fi
if ! grep -q "Hang detected" "$WORK/hang_stderr.txt"; then
    echo "FAIL: fill ROM exited non-zero but stderr did not contain 'Hang detected'"; exit 1
fi
echo "PASS: fill ROM triggers hang detection"

# 2. fill produces a non-blank screenshot (hang detection off).
printf 'wait 300\nscreenshot\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/fill.gb" --hang-timeout 0 \
        --screenshot-dir "$WORK" -
test -s "$WORK/screenshot_000.bmp" || { echo "FAIL: no fill screenshot"; exit 1; }
echo "PASS: fill screenshot written"

# 3. input (CGB): every button yields a distinct screen.
buttons="a b start select up down left right"
for k in $buttons; do
    printf 'wait 300\ndown %s\nwait 60\nscreenshot\n' "$k" \
        | "$BOYLESS" --model cgb --rom "$ROMS/input.gb" --hang-timeout 0 \
            --screenshot-dir "$WORK" -
    mv "$WORK/screenshot_000.bmp" "$WORK/shot_$k.bmp"
done
distinct=$(for k in $buttons; do sha1sum "$WORK/shot_$k.bmp" | cut -d' ' -f1; done | sort -u | wc -l)
if [ "$distinct" -ne 8 ]; then
    echo "FAIL: buttons not all distinct ($distinct/8 unique screens)"; exit 1
fi
echo "PASS: all 8 buttons produce distinct screens"

# 4. memory: mem.gb writes $42 to $C000. Asserting the right value passes;
# asserting a wrong value fails non-zero and prints expected-vs-actual.
# wait 200 lets the DMG boot ROM finish (~154 frames) before user code runs.
printf 'wait 200\nmemory C000 $42\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/mem.gb" --hang-timeout 0 -
echo "PASS: memory assertion matches"

_mem_rc=0
printf 'wait 200\nmemory C000 $99\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/mem.gb" --hang-timeout 0 - \
    2>"$WORK/mem_stderr.txt" || _mem_rc=$?
if [ "$_mem_rc" -eq 0 ]; then
    echo "FAIL: wrong memory assertion did not exit non-zero"; exit 1
fi
if ! grep -q "expected" "$WORK/mem_stderr.txt"; then
    echo "FAIL: memory mismatch did not report expected value"; exit 1
fi
echo "PASS: memory mismatch fails"

# memory print form (no value) never fails and reports the byte.
printf 'wait 200\nmemory C000\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/mem.gb" --hang-timeout 0 - \
    2>"$WORK/mem_print.txt"
grep -q 'memory \$C000 = \$42' "$WORK/mem_print.txt" \
    || { echo "FAIL: memory print form wrong output"; exit 1; }
echo "PASS: memory print form reports value"

# 5. compare: mint a reference from fill.gb under --update, then re-run to
# assert it matches. fill.gb is static so the screen is reproducible.
REF="$WORK/refs"; mkdir -p "$REF"
printf 'wait 300\ncompare 0\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/fill.gb" --hang-timeout 0 \
        --reference-dir "$REF" --update -
test -s "$REF/screenshot_000.bmp" || { echo "FAIL: --update did not write reference"; exit 1; }
echo "PASS: compare --update writes reference"

printf 'wait 300\ncompare 0\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/fill.gb" --hang-timeout 0 \
        --reference-dir "$REF" -
echo "PASS: compare matches the minted reference"

# Mismatch: compare input.gb (CGB, different screen) against fill's reference.
# Different dimensions/pixels => failure, non-zero exit, and an .actual dump.
_cmp_rc=0
printf 'wait 300\ncompare 0\n' \
    | "$BOYLESS" --model cgb --rom "$ROMS/input.gb" --hang-timeout 0 \
        --reference-dir "$REF" --screenshot-dir "$WORK" - \
    2>"$WORK/cmp_stderr.txt" || _cmp_rc=$?
if [ "$_cmp_rc" -eq 0 ]; then
    echo "FAIL: mismatched compare did not exit non-zero"; exit 1
fi
test -s "$WORK/screenshot_000.actual.bmp" || { echo "FAIL: no .actual dump on mismatch"; exit 1; }
echo "PASS: compare mismatch fails and dumps actual frame"

# 6. shared id counter advances forward only: an explicit lower id must not
# rewind it. After `screenshot 5` then `screenshot 2`, a bare `screenshot`
# auto-numbers to 6 (max seen + 1), not 3.
CNT="$WORK/counter"; mkdir -p "$CNT"
printf 'wait 300\nscreenshot 5\nscreenshot 2\nscreenshot\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/fill.gb" --hang-timeout 0 \
        --screenshot-dir "$CNT" -
test -s "$CNT/screenshot_005.bmp" || { echo "FAIL: explicit id 5 not written"; exit 1; }
test -s "$CNT/screenshot_002.bmp" || { echo "FAIL: explicit id 2 not written"; exit 1; }
test -s "$CNT/screenshot_006.bmp" || { echo "FAIL: auto id did not advance to 6 (counter rewound)"; exit 1; }
test ! -e "$CNT/screenshot_003.bmp" || { echo "FAIL: auto id rewound to 3"; exit 1; }
echo "PASS: shared id counter advances forward only"

# 7. settle: fill.gb is static after boot, so `settle` must stabilize and let
# the script proceed to write a screenshot (exit 0, no failures).
STL="$WORK/settle"; mkdir -p "$STL"
printf 'wait 200\nsettle 10\nscreenshot\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/fill.gb" --screenshot-dir "$STL" -
test -s "$STL/screenshot_000.bmp" || { echo "FAIL: settle did not reach the screenshot"; exit 1; }
echo "PASS: settle stabilizes on a static screen"

echo "integration: OK"
