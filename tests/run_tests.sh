#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [ -z "${GODOT:-}" ]; then
	echo "Set GODOT to the path of the Godot console executable" >&2
	exit 2
fi

run_scons() {
	(cd "$ROOT/extension_src" && PYTHONUTF8=1 py -m SCons "$@")
}

run_godot_test() {
	local name="$1"
	local log
	log="$(mktemp)"
	timeout 600 "$GODOT" --headless --path "$ROOT" --script "res://tests/$name.gd" 2>&1 | tee "$log"
	local status=${PIPESTATUS[0]}
	local errors
	errors=$(grep -c "SCRIPT ERROR" "$log")
	rm -f "$log"
	if [ "$status" -ne 0 ] || [ "$errors" -ne 0 ]; then
		echo "FAILED: $name (exit=$status, SCRIPT ERROR=$errors)" >&2
		return 1
	fi
}

run_scons || { echo "FAILED: DLL build (make sure the Godot editor is not holding the DLL)" >&2; exit 1; }
run_scons tests || { echo "FAILED: C++ unit tests" >&2; exit 1; }
run_godot_test sfen_selftest || exit 1
run_godot_test hint_selftest || exit 1

echo "ALL TESTS PASSED"
