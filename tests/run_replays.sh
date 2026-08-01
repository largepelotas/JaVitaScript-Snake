#!/usr/bin/env bash
# Runs every replay in tests/replays/ against the harness (PLAN.md 4.7).
#
# Usage: run_replays.sh <path-to-replay-binary> [--bless]
#
# --bless rewrites each file's expect_ lines from the current build. Only do
# that when a behavior change is intended and understood; blessing is how a
# regression gets silently accepted.
set -u

BIN="${1:?usage: run_replays.sh <replay-binary> [--bless]}"
MODE="${2:-verify}"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/replays"

shopt -s nullglob
files=("$DIR"/*.txt)

if [ ${#files[@]} -eq 0 ]; then
  echo "no replay files in $DIR" >&2
  exit 1
fi

echo
echo "replays"
fail=0
for f in "${files[@]}"; do
  if [ "$MODE" = "--bless" ]; then
    "$BIN" --bless "$f" || fail=$((fail + 1))
  else
    "$BIN" "$f" || fail=$((fail + 1))
  fi
done

echo
if [ "$fail" -ne 0 ]; then
  echo "$fail replay(s) FAILED"
  exit 1
fi
echo "${#files[@]} replay(s) ok"
