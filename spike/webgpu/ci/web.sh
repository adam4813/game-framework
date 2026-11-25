#!/usr/bin/env bash
# Runs the web build in the runner's Chrome, with and without the software-adapter flags.
set -uo pipefail
fact() { echo "FACT: $*"; }
dist=build/spike/Release
export CHROME=$(command -v google-chrome || command -v chromium)
fact "chrome $("$CHROME" --version), node $(node --version)"
ls -l $dist | awk '/webgpu_spike\.(wasm|js|html)$/ {print "FACT: " $5, $NF}'
for f in wasm js; do fact "$f gz $(gzip -9c $dist/webgpu_spike.$f | wc -c) brotli $(brotli -c $dist/webgpu_spike.$f 2>/dev/null | wc -c)"; done
failed=0
run() {
	local label=$1 need=$2 query=$3
	shift 3
	local start=$SECONDS
	node spike/webgpu/web/drive.mjs $dist run --query "$query" -- "$@" > web.log 2>&1
	sed 's/^/    /' web.log | head -40
	local ok=no
	grep -q 'offscreen: PASS' web.log && ok=yes
	fact "$label: $((SECONDS - start)) s, ok=$ok | $(grep -m1 -o 'adapter:.*' web.log) | $(grep -m1 -o 'offscreen:.*' web.log) | $(grep -m1 -- '-- exit' web.log)"
	if [ "$need" = must ] && [ $ok = no ]; then failed=1; echo "::error::$label"; fi
}
run "no flags" may --offscreen
run "unsafe-webgpu only" may --offscreen --enable-unsafe-webgpu
run "swiftshader + unsafe" must --offscreen --use-webgpu-adapter=swiftshader --enable-unsafe-webgpu
run "swiftshader + unsafe + --no-sandbox" may --offscreen --use-webgpu-adapter=swiftshader --enable-unsafe-webgpu --no-sandbox
node spike/webgpu/web/drive.mjs $dist run --query "--imgui&--frames&60" --shot window.png -- \
	--use-webgpu-adapter=swiftshader --enable-unsafe-webgpu > web.log 2>&1
sed 's/^/    /' web.log | head -40
fact "window + imgui: $(grep -m1 -o 'window:.*' web.log) | $(grep -m1 -- '-- exit' web.log)"
exit $failed
