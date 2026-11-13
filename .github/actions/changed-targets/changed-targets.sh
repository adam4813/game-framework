#!/usr/bin/env bash
# Decides what CI has to build from the files a push or pull request changed.
#
# Usage: changed-targets.sh <base> <head>   compare two commits
#        changed-targets.sh --all            build everything (manual runs, unknown base)
#
# Writes to $GITHUB_OUTPUT (or stdout when unset):
#   engine=true|false   build and test the engine (src/, tests/, CMake, vcpkg, shared assets, CI setup)
#   games=["a","b"]     Game Targets to build: every game when the engine changed, otherwise the games
#                       whose own files changed (games/<name>/ or assets/<type>/<name>/)
#
# Files nothing builds from (docs, Markdown, .scratch/, tools/, editor config) change neither.
set -euo pipefail

out="${GITHUB_OUTPUT:-/dev/stdout}"

# A game is any games/<name>/ with a CMakeLists.txt; its executable target shares the folder name.
mapfile -t all_games < <(find games -mindepth 2 -maxdepth 2 -name CMakeLists.txt 2>/dev/null \
	| sed -E 's#^games/([^/]+)/CMakeLists.txt$#\1#' | sort)

is_game() {
	local candidate="$1" game
	for game in "${all_games[@]}"; do
		[ "$game" = "$candidate" ] && return 0
	done
	return 1
}

emit() {
	local engine="$1"
	shift
	# Game names are folder names, so quoting each one is enough to make a JSON array.
	local json
	json="[$(printf '%s\n' "$@" | sed '/^$/d' | sort -u | sed 's/.*/"&"/' | paste -sd, -)]"
	echo "engine=$engine" >> "$out"
	echo "games=$json" >> "$out"
	echo "engine=$engine games=$json" >&2
}

if [ "${1:-}" = "--all" ]; then
	emit true "${all_games[@]}"
	exit 0
fi

base="$1"
head="$2"
if ! files=$(git diff --name-only "$base...$head" 2>/dev/null); then
	echo "Can't diff $base...$head; building everything" >&2
	emit true "${all_games[@]}"
	exit 0
fi

engine=false
games=()
while IFS= read -r file; do
	[ -n "$file" ] || continue
	case "$file" in
		# Nothing builds from these.
		*.md | .scratch/* | */.scratch/* | docs/* | games/*/docs/* | tools/* | games/*/tools/* | plan/* \
			| scene_test_output/* | .claude/* | .idea/* | .vscode/* | .mcp.json | .clangd | .clang-format \
			| .editorconfig | .gitignore | .gitattributes | msvc.natvis | LICENSE*)
			;;
		# Each workflow file only affects its own runs.
		.github/workflows/build-games.yml) games+=("${all_games[@]}") ;;
		.github/workflows/deploy-wasm.yml) games+=(sandbox) ;;
		.github/workflows/*) engine=true ;;
		# A path in a folder that isn't a Game Target (a removed game, say) builds nothing.
		games/*)
			name="${file#games/}"
			name="${name%%/*}"
			if is_game "$name"; then games+=("$name"); fi
			;;
		# assets/<type>/<game>/... belongs to that game; anything else in assets/ is shared.
		assets/*/*/*)
			name=$(cut -d/ -f3 <<< "$file")
			if is_game "$name"; then games+=("$name"); else engine=true; fi
			;;
		# Engine source, tests, CMake, vcpkg, triplets, shared assets, CI actions, and anything unknown.
		*) engine=true ;;
	esac
done <<< "$files"

if [ "$engine" = true ]; then
	emit true "${all_games[@]}"
else
	emit false "${games[@]}"
fi
