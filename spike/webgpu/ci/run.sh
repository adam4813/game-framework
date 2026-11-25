#!/usr/bin/env bash
# Runs the native spike on the software adapter, asserts which adapter it got, and checks the packaged folder.
#   run.sh linux|windows <triplet>
set -uo pipefail
platform=$1
triplet=$2
failed=0
out=run.log

fact() { echo "FACT: $*"; }
# try <label> <must|may|mustfail> <adapter regex> <command...>
try() {
	local label=$1 need=$2 want=$3
	shift 3
	local start=$SECONDS code=0
	timeout 120 "$@" > $out 2>&1 || code=$?
	sed 's/^/    /' $out | head -60
	local adapter
	adapter=$(grep -m1 '^adapter:' $out || true)
	local got=no
	if [ $code -eq 0 ] && { [ -z "$want" ] || grep -qE "$want" <<< "$adapter"; }; then got=yes; fi
	fact "$label: exit $code, $((SECONDS - start)) s, ok=$got | ${adapter:-no adapter} | $(grep -m1 '^offscreen:\|^window:' $out || true)"
	if [ "$need" = must ] && [ $got = no ]; then failed=1; echo "::error::$label"; fi
	if [ "$need" = mustfail ] && [ $got = yes ]; then echo "::warning::$label was expected to fail"; fi
}

if [ "$platform" = linux ]; then
	exe=build/spike/Release/webgpu_spike
	debug=build/spike/Debug/webgpu_spike
	args=(--backend vulkan)
	want='backend=vulkan type=[0-9]+ vendor=0x10005 .*llvmpipe'
	fact "preinstalled: $(dpkg -l 2>/dev/null | awk '/^ii/ && $2 ~ /mesa-vulkan|libvulkan1|libgl1-mesa-dri|libegl-mesa/ {print $2 "=" $3}' | tr '\n' ' ')"
	fact "ICDs before: $(ls /usr/share/vulkan/icd.d 2>/dev/null | tr '\n' ' ')"
	try "before mesa, vulkan named" may "$want" $exe --offscreen --backend vulkan
	try "before mesa, no backend named" may "$want" $exe --offscreen
	start=$SECONDS
	sudo apt-get install -y mesa-vulkan-drivers libvulkan1 > apt.log 2>&1 || cat apt.log
	fact "apt mesa: $((SECONDS - start)) s; $(grep -E 'Need to get|newly installed' apt.log | tr '\n' ' ')"
	fact "ICDs after: $(ls /usr/share/vulkan/icd.d | tr '\n' ' ')"
	icd=$(ls /usr/share/vulkan/icd.d/lvp_icd*.json | head -1)
	try "no VK_DRIVER_FILES" may "$want" $exe --offscreen "${args[@]}"
	export VK_DRIVER_FILES=$icd
	try "no backend named, lavapipe only" may "$want" $exe --offscreen
	try "fallback flag" mustfail "$want" $exe --offscreen "${args[@]}" --fallback
	timeout 60 $exe --list 2>&1 | sed 's/^/    list: /'
	try "Debug exe" may "$want" $debug --offscreen "${args[@]}"
	try "window on xvfb (x11)" may "$want" env SDL_VIDEO_DRIVER=x11 xvfb-run -a $exe "${args[@]}" --frames 120 --imgui
else
	exe=build/spike/Release/webgpu_spike.exe
	debug=build/spike/Debug/webgpu_spike.exe
	args=(--backend d3d12 --vendor 0x1414)
	want='backend=d3d12 type=[0-9]+ vendor=0x1414'
	cp inst/$triplet/bin/webgpu_dawn.dll inst/$triplet/bin/abseil_dll.dll build/spike/Debug/ 2>/dev/null
	timeout 60 $exe --list 2>&1 | sed 's/^/    list: /'
	try "no backend named" may "$want" $exe --offscreen
	try "fallback flag" mustfail "$want" $exe --offscreen --backend d3d12 --fallback
	[[ $triplet == *dawnrel ]] && try "Debug exe, Release Dawn" may "$want" $debug --offscreen "${args[@]}"
	try "window" may "$want" $exe "${args[@]}" --frames 120 --imgui
fi

try "software adapter" must "$want" $exe --offscreen "${args[@]}" --shot offscreen.ppm
pass=0
: > hashes.txt
for _ in $(seq 20); do
	if timeout 120 $exe --offscreen "${args[@]}" > $out 2>&1; then pass=$((pass + 1)); fi
	sed -n 's/.*image hash //p' $out >> hashes.txt
done
fact "20 repeats: $pass pass, hashes $(sort hashes.txt | uniq -c | tr '\n' ' ')"
grep -iE 'gpu|compute|draw' $out | sed 's/^/FACT: times /' | head -5
[ $pass -eq 20 ] || failed=1

# What build-games.yml would stage: the binary plus *.dll on Windows, the binary alone on Linux.
rm -rf stage && mkdir stage
cp $exe stage/
if [ "$platform" = linux ]; then
	ls -l stage | awk 'NR>1 {print "FACT: staged " $5, $NF}'
	strip -o stripped stage/webgpu_spike && fact "stripped $(stat -c %s stripped)"
	ldd stage/webgpu_spike | awk '{print "FACT: needs " $1}'
	cd stage || exit 1
	try "staged folder" must "$want" ./webgpu_spike --offscreen "${args[@]}"
	cd ..
else
	find build/spike/Release -maxdepth 1 -name '*.dll' -exec cp {} stage/ \;
	ls stage | sed 's/^/FACT: build left /'
	# The port's CMake target points dxil.dll at the wrong file in Release (ticket 05), so copy it by path.
	cp "$(find "inst/$triplet" -iname dxil.dll | head -1)" stage/
	cp "$(find "inst/$triplet" -iname dxcompiler.dll | head -1)" stage/
	ls -l stage | awk 'NR>1 {print "FACT: staged " $5, $NF}'
	key='HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock'
	dev() { MSYS_NO_PATHCONV=1 reg query "$key" /v AllowDevelopmentWithoutDevLicense 2>&1 | grep -oE '0x[0-9a-f]+' || echo unset; }
	fact "Developer Mode: $(dev)"
	MSYS_NO_PATHCONV=1 reg add "$key" /v AllowDevelopmentWithoutDevLicense /t REG_DWORD /d 0 /f > /dev/null
	fact "Developer Mode now: $(dev)"
	clean=(env -i "SYSTEMROOT=$SYSTEMROOT" "TEMP=$TEMP" "TMP=$TMP" "PATH=/c/Windows/System32:/usr/bin")
	cd stage || exit 1
	try "staged, Developer Mode off" must "$want" "${clean[@]}" ./webgpu_spike.exe --offscreen "${args[@]}"
	mv dxil.dll ../
	try "staged, no dxil.dll, Developer Mode off" may "$want" "${clean[@]}" ./webgpu_spike.exe --offscreen "${args[@]}"
	mv ../dxil.dll . && mv dxcompiler.dll ../
	try "staged, no dxcompiler.dll" mustfail "$want" "${clean[@]}" ./webgpu_spike.exe --offscreen "${args[@]}"
	mv ../dxcompiler.dll .
	cd ..
fi
exit $failed
