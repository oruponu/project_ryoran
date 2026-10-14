#!/usr/bin/env bash
set -euo pipefail

if [ $# -ne 2 ]; then
	echo "usage: build_engine.sh <commit|.> <name>" >&2
	exit 2
fi
commit=$1
name=$2
repo=$(git rev-parse --show-toplevel)
out="$repo/tools/selfplay/engines/$name"
worktree="$repo/tools/selfplay/engines/.worktree-$name"

remove_worktree() {
	if [ -d "$worktree" ]; then
		git -C "$repo" worktree remove --force "$worktree"
	fi
}
trap remove_worktree EXIT

if [ "$commit" = "." ]; then
	source_root=$repo
else
	remove_worktree
	git -C "$repo" worktree add --detach "$worktree" "$commit" >/dev/null
	source_root=$worktree
fi

sources=()
for file in "$source_root"/extension_src/src/*.cpp; do
	case "$(basename "$file")" in
	shogi_engine.cpp | register_types.cpp) ;;
	*) sources+=("$file") ;;
	esac
done

if [ "$(uname -s)" = Darwin ]; then
	cxx=${CXX:-clang++}
	link_flags=()
else
	cxx=g++
	link_flags=(-static -lstdc++exp)
fi

mkdir -p "$out"
"$cxx" -std=c++23 -O3 "-DRYORAN_BUILD_NAME=\"$name\"" -I"$source_root/extension_src/src" \
	"${sources[@]}" "$repo/extension_src/test/usi_main.cpp" "$repo/extension_src/test/usi_protocol.cpp" \
	${link_flags[@]+"${link_flags[@]}"} -o "$out/shogi_usi.exe"
cp "$source_root/assets/data/zobrist_params.bin" "$out/"
echo "$out/shogi_usi.exe"
