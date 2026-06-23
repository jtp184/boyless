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

# 8. a value-taking flag with no value fails with a clear "needs a value"
# message (not a misleading "unknown option") and a non-zero exit.
_arg_rc=0
"$BOYLESS" --rom "$ROMS/fill.gb" --screenshot-dir 2>"$WORK/arg_stderr.txt" || _arg_rc=$?
if [ "$_arg_rc" -eq 0 ]; then
    echo "FAIL: missing flag value did not exit non-zero"; exit 1
fi
grep -q "needs a value" "$WORK/arg_stderr.txt" \
    || { echo "FAIL: missing flag value did not report 'needs a value'"; exit 1; }
echo "PASS: missing flag value reports a clear error"

# 9. --report-only: a failing assertion is still reported but the run exits 0.
_ro_rc=0
printf 'wait 10\nmemory C000 99\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/mem.gb" --hang-timeout 0 --report-only - \
    2>"$WORK/ro_stderr.txt" || _ro_rc=$?
if [ "$_ro_rc" -ne 0 ]; then
    echo "FAIL: --report-only did not exit 0 on a failed assertion"; exit 1
fi
grep -q "expected" "$WORK/ro_stderr.txt" \
    || { echo "FAIL: --report-only suppressed the failure message"; exit 1; }
echo "PASS: --report-only reports failures but exits 0"

# 10. symbols: {wValue} from mem.sym resolves to $C000, so `memory {wValue} $42`
# passes exactly like the literal-address form.
printf 'wait 200\nmemory {wValue} $42\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/mem.gb" --sym "$ROMS/mem.sym" \
        --hang-timeout 0 -
echo "PASS: symbol resolves to the right address"

# unknown symbol is a parse error (non-zero exit, clear message).
_sym_rc=0
printf 'wait 200\nmemory {nope} $42\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/mem.gb" --sym "$ROMS/mem.sym" \
        --hang-timeout 0 - 2>"$WORK/sym_unknown.txt" || _sym_rc=$?
if [ "$_sym_rc" -eq 0 ]; then
    echo "FAIL: unknown symbol did not exit non-zero"; exit 1
fi
grep -q "unknown symbol" "$WORK/sym_unknown.txt" \
    || { echo "FAIL: unknown symbol message missing"; exit 1; }
echo "PASS: unknown symbol fails clearly"

# a {symbol} reference without --sym is a parse error.
_nosym_rc=0
printf 'wait 200\nmemory {wValue} $42\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/mem.gb" --hang-timeout 0 - \
    2>"$WORK/sym_missing.txt" || _nosym_rc=$?
if [ "$_nosym_rc" -eq 0 ]; then
    echo "FAIL: {symbol} without --sym did not exit non-zero"; exit 1
fi
grep -q "requires a --sym" "$WORK/sym_missing.txt" \
    || { echo "FAIL: missing --sym message wrong"; exit 1; }
echo "PASS: {symbol} without --sym fails clearly"

# 11. differ: capture a baseline, change the screen with a held button, then
# `differ` passes (screen changed). input.gb renders distinct screens per button.
DIF="$WORK/differ"; mkdir -p "$DIF"
printf 'wait 300\nscreenshot 0\ndown a\nwait 60\ndiffer 0\n' \
    | "$BOYLESS" --model cgb --rom "$ROMS/input.gb" --hang-timeout 0 \
        --screenshot-dir "$DIF" -
echo "PASS: differ passes when the screen changed"

# differ fails when the screen is unchanged. fill.gb is static after boot, so a
# baseline captured then re-checked without input is identical => failure.
DIFS="$WORK/differ_same"; mkdir -p "$DIFS"
_dif_rc=0
printf 'wait 300\nscreenshot 0\nwait 60\ndiffer 0\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/fill.gb" --hang-timeout 0 \
        --screenshot-dir "$DIFS" - 2>"$WORK/differ_same.txt" || _dif_rc=$?
if [ "$_dif_rc" -eq 0 ]; then
    echo "FAIL: differ on an unchanged screen did not exit non-zero"; exit 1
fi
grep -q "requires a change" "$WORK/differ_same.txt" \
    || { echo "FAIL: differ-unchanged message wrong"; exit 1; }
echo "PASS: differ fails when the screen is unchanged"

# differ fails when the baseline file is missing.
DIFM="$WORK/differ_missing"; mkdir -p "$DIFM"
_difm_rc=0
printf 'wait 300\ndiffer 0\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/fill.gb" --hang-timeout 0 \
        --screenshot-dir "$DIFM" - 2>"$WORK/differ_missing.txt" || _difm_rc=$?
if [ "$_difm_rc" -eq 0 ]; then
    echo "FAIL: differ with no baseline did not exit non-zero"; exit 1
fi
grep -q "cannot read baseline" "$WORK/differ_missing.txt" \
    || { echo "FAIL: differ-missing-baseline message wrong"; exit 1; }
echo "PASS: differ fails when the baseline is missing"

# 12. noblank: mem.gb renders a non-blank screen after boot (the DMG boot ROM
# leaves mixed VRAM content visible), so `noblank` passes (exit 0).
printf 'wait 200\nnoblank\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/mem.gb" --hang-timeout 0 -
echo "PASS: noblank passes on a non-blank screen"

# noblank fails on blank.gb, which renders a single flat colour.
_nb_rc=0
printf 'wait 200\nnoblank\n' \
    | "$BOYLESS" --model dmg --rom "$ROMS/blank.gb" --hang-timeout 0 - \
    2>"$WORK/noblank.txt" || _nb_rc=$?
if [ "$_nb_rc" -eq 0 ]; then
    echo "FAIL: noblank on a blank screen did not exit non-zero"; exit 1
fi
grep -q "screen is blank" "$WORK/noblank.txt" \
    || { echo "FAIL: noblank-blank message wrong"; exit 1; }
echo "PASS: noblank fails on a blank screen"

echo "integration: OK"
