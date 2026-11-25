#!/usr/bin/env bash
# Installs the spike's ports (cold or restored), reinstalls them from the binary cache, builds the spike.
#   build.sh <triplet> [extra cmake args]
# Lines starting "FACT:" are the measurements.
set -euo pipefail
triplet=$1
shift
src=spike/webgpu
ws=$(cygpath -m "$GITHUB_WORKSPACE" 2>/dev/null || echo "$GITHUB_WORKSPACE")
root=$(cygpath -m "$VCPKG_ROOT" 2>/dev/null || echo "$VCPKG_ROOT")
py=$(command -v python3 || command -v python)

fact() { echo "FACT: $*"; }
free_gb() { df -Pk . | awk 'NR==2 {printf "%.1f", $4 / 1e6}'; }
vcpkg_install() {
	"$root/vcpkg" install --x-manifest-root=$src --x-install-root=inst --triplet "$triplet" \
		--overlay-ports=$src/ports --overlay-triplets=$src/triplets
}

fact "image ${ImageOS:-?} ${ImageVersion:-?}, $(nproc) CPUs, free disk $(free_gb) GB"
(c++ --version 2>/dev/null || true) | sed -n 's/^/FACT: compiler /;1p'
before=$(find "$VCPKG_ARCHIVES" -name '*.zip' | wc -l)
start=$SECONDS
vcpkg_install | tee install1.log
fact "install 1 (archives before: $before): $((SECONDS - start)) s, free disk $(free_gb) GB"
grep -E 'Elapsed time to handle|Total install time' install1.log | sed 's/^/FACT: /' || true

rm -rf inst
start=$SECONDS
vcpkg_install > install2.log
fact "install 2 (binary cache): $((SECONDS - start)) s"

"$py" $src/ci/archives.py "$VCPKG_ARCHIVES"
fact "installed $triplet: $(du -sm "inst/$triplet" | cut -f1) MB"
find "inst/$triplet" -name '*webgpu_dawn*' -type f -size +100k -exec ls -l {} \; | awk '{print "FACT: " $5, $NF}'

start=$SECONDS
cmake -S $src -B build/spike -G "Ninja Multi-Config" \
	-DCMAKE_TOOLCHAIN_FILE="$root/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET="$triplet" \
	-DVCPKG_OVERLAY_TRIPLETS="$ws/$src/triplets" -DVCPKG_OVERLAY_PORTS="$ws/$src/ports" \
	-DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_INSTALLED_DIR="$ws/inst" "$@"
cmake --build build/spike --config Release
fact "configure + Release build: $((SECONDS - start)) s"
start=$SECONDS
if cmake --build build/spike --config Debug; then
	fact "Debug build: $((SECONDS - start)) s"
else
	fact "Debug build FAILED"
fi
