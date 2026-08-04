#!/bin/sh
set -eu

: "${PS5_HOST:?set PS5_HOST to the FW 5.50 console address}"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(dirname -- "$script_dir")
vulkan_repo="$repo_dir/../Vulkan-PS5"
runner="$vulkan_repo/examples/run_fw550_swapchain.sh"
process_helper="$vulkan_repo/examples/ps5debug_kill_process.py"
pyps4debug_dir=/Users/bizkut/Downloads/PS5/homebrew/PyPS4debug
pyps4debug_lock="$pyps4debug_dir/uv.lock"
elf=${EDEN_PS5_ELF:-$repo_dir/build-prospero-full-audit2/bin/eden-ps5.elf}
cleanup_elf=${EDEN_PS5_CLEANUP_ELF:-$vulkan_repo/build-prospero-msaa/vulkan_ps5_process_cleanup.elf}
homebrew=${EDEN_PS5_2048_NRO:-$repo_dir/../2048.nro}
sidecar="$repo_dir/src/ps5/eden-2048.launch"
pinned_eden_sha256=ad5160147212771bb43b98aea8f4a835bcb735c315b4c84e362761d9cdf956cb
pinned_cleanup_sha256=9fd6b41cf2ea87989c4217234c6f34c96a1ca5dc482355af1258539db77d4d76
pinned_runner_sha256=1c2da402df3ca3eb30e7121e91abeb83c7da06aa4ff9c4e48e18fac5ec778552
pinned_process_helper_sha256=c46e8b9f1095599498763e1a9e3923cfa47f787d48c2b952a1a90ab6feaaabe5
pinned_pyps4debug_commit=8f1443bb97bd6e2a77ed5ea2cc9145975d3152eb
pinned_pyps4debug_lock_sha256=c9eb85e0f0bc1bde6c4e00f1112a1aea982dc7eed024eb973fca91e436051033
sidecar_sha256=e5c10f0d91bcb683f8e9f41a1bce44228d07317ff1f07236fcfabf702f4a4bac
homebrew_sha256=cd7e7f343830920196590d99c82a9f1ab8a375eeaeb943fa6c671aa68250a20d
cache_identity=cd7e7f3438309201
websrv_timeout=${EDEN_PS5_WEBSRV_TIMEOUT:-900}

verify_file_sha256() {
    file=$1
    expected=$2
    label=$3
    if [ ! -f "$file" ]; then
        echo "missing $label: $file" >&2
        return 1
    fi
    actual=$(shasum -a 256 "$file" | awk '{print $1}')
    if [ "$actual" != "$expected" ]; then
        echo "$label SHA-256 mismatch: expected=$expected actual=$actual" >&2
        return 1
    fi
}

if ! command -v shasum >/dev/null 2>&1 || \
   ! command -v curl >/dev/null 2>&1 || \
   ! command -v uv >/dev/null 2>&1; then
    echo "shasum, curl, and uv are required for the pinned qualification gate" >&2
    exit 2
fi
if ! command -v git >/dev/null 2>&1 || \
   [ "$(git -C "$pyps4debug_dir" rev-parse --verify HEAD 2>/dev/null || true)" != \
     "$pinned_pyps4debug_commit" ] || \
   ! git -C "$pyps4debug_dir" diff --quiet HEAD -- src/ps4debug pyproject.toml || \
   [ -n "$(git -C "$pyps4debug_dir" ls-files --others --exclude-standard -- \
       ':(glob)src/ps4debug/**/*.py')" ]; then
    echo "PyPS4debug source does not match the pinned exact-process toolchain" >&2
    exit 2
fi
verify_file_sha256 "$runner" "$pinned_runner_sha256" 'guarded Vulkan-PS5 runner'
verify_file_sha256 "$process_helper" "$pinned_process_helper_sha256" \
    'exact-process helper'
verify_file_sha256 "$pyps4debug_lock" "$pinned_pyps4debug_lock_sha256" \
    'PyPS4debug lockfile'
if [ -n "${EDEN_PS5_EXPECTED_SHA256:-}" ] && \
   [ "$EDEN_PS5_EXPECTED_SHA256" != "$pinned_eden_sha256" ]; then
    echo "EDEN_PS5_EXPECTED_SHA256 does not match the pinned current ELF" >&2
    exit 2
fi
if [ -n "${EDEN_PS5_CLEANUP_EXPECTED_SHA256:-}" ] && \
   [ "$EDEN_PS5_CLEANUP_EXPECTED_SHA256" != "$pinned_cleanup_sha256" ]; then
    echo "EDEN_PS5_CLEANUP_EXPECTED_SHA256 does not match the pinned cleanup ELF" >&2
    exit 2
fi
case "$websrv_timeout" in
    ''|*[!0-9]*) echo "EDEN_PS5_WEBSRV_TIMEOUT must be 1-1200 seconds" >&2; exit 2 ;;
esac
if [ "$websrv_timeout" -lt 1 ] || [ "$websrv_timeout" -gt 1200 ]; then
    echo "EDEN_PS5_WEBSRV_TIMEOUT must be 1-1200 seconds" >&2
    exit 2
fi
live_klog_timeout=$((websrv_timeout + 120))
if [ "${EDEN_PS5_QUALIFICATION_REJECT_PATTERN+x}" = x ]; then
    echo "EDEN_PS5_QUALIFICATION_REJECT_PATTERN cannot replace mandatory failures; use EDEN_PS5_QUALIFICATION_EXTRA_REJECT_PATTERN" >&2
    exit 2
fi

run=1
native_present_600_pattern='^vulkan-ps5: native present 600-frame gate complete successes=600 frame=[0-9]+ index=[0-2]$'
firmware_pattern='^\[openagc\] system software raw=0x05500008 string= 5\.500\.008$'
input_cycle_pattern='PS5 qualification input cycle: enabled=true interval_ms=50'
reject_pattern='allocation failed|mapping failed|mmap failed|mprotect failed|^eden-ps5 dynarmic .* failed:|terminating without executing an invalid JIT mapping|Failed to present|GPU thread failure|^vulkan-ps5: .*failed'
reject_pattern="$reject_pattern|Failed to derive the Prospero shader-cache identity"
if [ -n "${EDEN_PS5_QUALIFICATION_EXTRA_REJECT_PATTERN:-}" ]; then
    reject_pattern="$reject_pattern|$EDEN_PS5_QUALIFICATION_EXTRA_REJECT_PATTERN"
fi

while [ "$run" -le 2 ]; do
    required_pattern="$native_present_600_pattern"
    required_pattern_2="$firmware_pattern"
    required_pattern_3="$input_cycle_pattern"
    required_pattern_4="EdenMain: Prospero shader-cache identity: $cache_identity"
    required_pattern_5='^\[psbc\] Parameter exports: stage=0 count=1$'
    VULKAN_PS5_QUALIFICATION_ELF="$elf" \
    VULKAN_PS5_CLEANUP_ELF="$cleanup_elf" \
    VULKAN_PS5_QUALIFICATION_REMOTE_NAME=eden_ps5 \
    VULKAN_PS5_QUALIFICATION_LABEL="eden-2048-run${run}" \
    VULKAN_PS5_QUALIFICATION_PASS_PATTERN='^eden-ps5: GAME PASS 600 frames$' \
    VULKAN_PS5_QUALIFICATION_PASS_DESCRIPTION='2048 homebrew, 600 presented frames, and bounded teardown' \
    VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN="$required_pattern" \
    VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_2="$required_pattern_2" \
    VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_3="$required_pattern_3" \
    VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_4="$required_pattern_4" \
    VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_5="$required_pattern_5" \
    VULKAN_PS5_QUALIFICATION_REJECT_PATTERN="$reject_pattern" \
    VULKAN_PS5_WEBSRV_TIMEOUT="$websrv_timeout" \
    VULKAN_PS5_LIVE_KLOG_TIMEOUT="$live_klog_timeout" \
    VULKAN_PS5_CONTINUOUS_KLOG=1 \
    VULKAN_PS5_SWAPCHAIN_EXPECTED_SHA256="$pinned_eden_sha256" \
    VULKAN_PS5_CLEANUP_EXPECTED_SHA256="$pinned_cleanup_sha256" \
    VULKAN_PS5_QUALIFICATION_SIDECAR="$sidecar" \
    VULKAN_PS5_QUALIFICATION_SIDECAR_REMOTE_NAME=eden.launch \
    VULKAN_PS5_QUALIFICATION_SIDECAR_EXPECTED_SHA256="$sidecar_sha256" \
    VULKAN_PS5_QUALIFICATION_ASSET="$homebrew" \
    VULKAN_PS5_QUALIFICATION_ASSET_REMOTE_NAME=2048.nro \
    VULKAN_PS5_QUALIFICATION_ASSET_EXPECTED_SHA256="$homebrew_sha256" \
    PYPS4DEBUG_DIR="$pyps4debug_dir" \
        "$runner"
    run=$((run + 1))
done

echo "eden-ps5 FW 5.50 2048 immediate-relaunch gate: PASS"
