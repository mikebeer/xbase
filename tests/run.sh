#!/bin/sh
# Run every tests/*.prg and compare with tests/NAME.out.
# A tests/NAME.in file, if present, is fed to the program as standard input.
# Each test runs in a scratch directory, so tables and indexes it creates vanish.
#   tests/run.sh            run all tests
#   tests/run.sh --update   rewrite the .out files from the current output
#   XBDOT=1 also runs tests/dot/*.in at the dot prompt (C version)
#   XBGDSM=1 also runs tests/gdsm/* full screen, headless (C version with GDSM)
HERE=$(cd "$(dirname "$0")" && pwd)
XB=${XB:-"$HERE/../xbase"}   # XB=c/xbase-c tests the C version
pass=0; fail=0
for src in "$HERE"/*.prg; do
  name=$(basename "$src" .prg)
  if [ -f "$HERE/$name.in" ]; then in="$HERE/$name.in"; else in=/dev/null; fi
  work=$(mktemp -d)
  out=$(cd "$work" && "$XB" "$src" < "$in" 2>&1)
  rm -rf "$work"
  if [ "$1" = "--update" ]; then
    printf '%s\n' "$out" > "$HERE/$name.out"
    echo "updated $name"
    continue
  fi
  if [ "$out" = "$(cat "$HERE/$name.out")" ]; then
    pass=$((pass+1)); echo "ok    $name"
  else
    fail=$((fail+1)); echo "FAIL  $name"
    printf '%s\n' "$out" | diff - "$HERE/$name.out" | head -10
  fi
done
# the dot prompt (C version only, XBDOT=1): tests/dot/NAME.in typed at the
# prompt, the screen compared with tests/dot/NAME.out
if [ "${XBDOT:-0}" = 1 ]; then
  for in in "$HERE"/dot/*.in; do
    name=dot/$(basename "$in" .in)
    work=$(mktemp -d)
    out=$(cd "$work" && "$XB" < "$in" 2>&1)
    rm -rf "$work"
    if [ "$1" = "--update" ]; then
      printf '%s\n' "$out" > "$HERE/$name.out"; echo "updated $name"; continue
    fi
    if [ "$out" = "$(cat "$HERE/$name.out")" ]; then
      pass=$((pass+1)); echo "ok    $name"
    else
      fail=$((fail+1)); echo "FAIL  $name"
      printf '%s\n' "$out" | diff - "$HERE/$name.out" | head -10
    fi
  done
fi
# the GDSM full screen (C version built with GDSM, XBGDSM=1), headless:
# GDSM's null driver, keys from tests/gdsm/NAME.keys; NAME.prg runs as a
# program, NAME.in is typed at the dot prompt
if [ "${XBGDSM:-0}" = 1 ]; then
  for k in "$HERE"/gdsm/*.keys; do
    b=$(basename "$k" .keys); name=gdsm/$b
    work=$(mktemp -d)
    if [ -f "$HERE/gdsm/$b.prg" ]; then
      out=$(cd "$work" && GDSM_DRIVER=null GDSM_KEYS="$(cat "$k")" "$XB" "$HERE/gdsm/$b.prg" < /dev/null 2>&1)
    else
      out=$(cd "$work" && GDSM_DRIVER=null GDSM_KEYS="$(cat "$k")" "$XB" < "$HERE/gdsm/$b.in" 2>&1)
    fi
    rm -rf "$work"
    if [ "$1" = "--update" ]; then
      printf '%s\n' "$out" > "$HERE/$name.out"; echo "updated $name"; continue
    fi
    if [ "$out" = "$(cat "$HERE/$name.out")" ]; then
      pass=$((pass+1)); echo "ok    $name"
    else
      fail=$((fail+1)); echo "FAIL  $name"
      printf '%s\n' "$out" | diff - "$HERE/$name.out" | head -10
    fi
  done
fi
[ "$1" = "--update" ] && exit 0
echo "$pass passed, $fail failed"
[ $fail = 0 ]
