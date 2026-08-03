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
homebrew=${EDEN_PS5_FLAPPY_BIRD_NRO:-$repo_dir/../Flappy_Bird_NX.nro}
sidecar="$repo_dir/src/ps5/eden-flappy-bird.launch"
log_dir="$vulkan_repo/examples/qualification-logs/flappy-bird"
pinned_eden_sha256=4fbccbe733095447f6d851110986c58c7eb0ce363be581043e808b3b5628d67d
pinned_cleanup_sha256=ff88ac293a55ec4ba5636a6556b74ffbeaf5d1093e96f86208cc55ce262565c5
pinned_runner_sha256=96e396e42d6b3a73eef0ed7de78fe0e318b1aa51cdcf8ff2c89a54b013452c08
pinned_process_helper_sha256=8dff282cdbc7ac1f4a037ad9e2a0e800fa82838cd1342b804b1eaff65ffd1ef6
pinned_pyps4debug_commit=8f1443bb97bd6e2a77ed5ea2cc9145975d3152eb
pinned_pyps4debug_lock_sha256=c9eb85e0f0bc1bde6c4e00f1112a1aea982dc7eed024eb973fca91e436051033
sidecar_sha256=44f0abd17639f09a237074de03db839c755d7e0767a9d32e1de99f35977ff5f2
homebrew_sha256=6e7cd9a1a22a0102a4f68ba6e434378c9b7381ce4f44a43ca376953f536aa54d
cache_identity=ee7cd9a1a22a0102
websrv_timeout=${EDEN_PS5_WEBSRV_TIMEOUT:-55}

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
   ! command -v uv >/dev/null 2>&1 || \
   ! command -v git >/dev/null 2>&1; then
    echo "shasum, curl, uv, and git are required for the pinned canary" >&2
    exit 2
fi
if [ "$(git -C "$pyps4debug_dir" rev-parse --verify HEAD 2>/dev/null || true)" != \
     "$pinned_pyps4debug_commit" ] || \
   ! git -C "$pyps4debug_dir" diff --quiet HEAD -- src/ps4debug pyproject.toml || \
   [ -n "$(git -C "$pyps4debug_dir" ls-files --others --exclude-standard -- \
       ':(glob)src/ps4debug/**/*.py')" ]; then
    echo "PyPS4debug source does not match the pinned exact-process toolchain" >&2
    exit 2
fi

verify_file_sha256 "$elf" "$pinned_eden_sha256" 'current Eden ELF'
verify_file_sha256 "$cleanup_elf" "$pinned_cleanup_sha256" 'cleanup ELF'
verify_file_sha256 "$runner" "$pinned_runner_sha256" 'guarded Vulkan-PS5 runner'
verify_file_sha256 "$process_helper" "$pinned_process_helper_sha256" \
    'exact-process helper'
verify_file_sha256 "$pyps4debug_lock" "$pinned_pyps4debug_lock_sha256" \
    'PyPS4debug lockfile'
verify_file_sha256 "$sidecar" "$sidecar_sha256" 'Flappy Bird sidecar'
verify_file_sha256 "$homebrew" "$homebrew_sha256" 'Flappy Bird NRO'

case "$websrv_timeout" in
    ''|*[!0-9]*)
        echo "EDEN_PS5_WEBSRV_TIMEOUT must be 1-360 seconds" >&2
        exit 2
        ;;
esac
if [ "$websrv_timeout" -lt 1 ] || [ "$websrv_timeout" -gt 360 ]; then
    echo "EDEN_PS5_WEBSRV_TIMEOUT must be 1-360 seconds" >&2
    exit 2
fi
if [ "${EDEN_PS5_QUALIFICATION_REJECT_PATTERN+x}" = x ]; then
    echo "EDEN_PS5_QUALIFICATION_REJECT_PATTERN cannot replace mandatory failures; use EDEN_PS5_QUALIFICATION_EXTRA_REJECT_PATTERN" >&2
    exit 2
fi

firmware_pattern='^\[openagc\] system software raw=0x05500008 string= 5\.500\.008$'
input_cycle_pattern='PS5 qualification input cycle: enabled=true interval_ms=250 press_limit=2'
input_cycle_stop_pattern='PS5 qualification input cycle: stopped presses=2 limit=2'
telemetry_baseline_pattern='Prospero guest pipeline cache live: reason=baseline .*graphics_discovered=0 compute_discovered=0 graphics_loaded=0 compute_loaded=0 records_rejected=0'
telemetry_pattern='Prospero guest pipeline cache live:'
cache_reload_pattern='Prospero guest pipeline cache live: reason=disk-load-complete .*graphics_discovered=[1-9][0-9]* compute_discovered=[0-9]+ graphics_loaded=[1-9][0-9]* compute_loaded=[0-9]+ records_rejected=[0-9]+'
reject_pattern='(allocation|mapping|mmap|mprotect) failed|^eden-ps5 dynarmic .* failed:'
reject_pattern="$reject_pattern|invalid JIT mapping|Failed to (present|derive|obtain|load)"
reject_pattern="$reject_pattern|GPU thread failure|^vulkan-ps5: .*failed|PS5 presented-frame oracle failed|CPUCore not initialized"
if [ -n "${EDEN_PS5_QUALIFICATION_EXTRA_REJECT_PATTERN:-}" ]; then
    reject_pattern="$reject_pattern|$EDEN_PS5_QUALIFICATION_EXTRA_REJECT_PATTERN"
fi

mkdir -p "$log_dir"
VULKAN_PS5_QUALIFICATION_ELF="$elf" \
VULKAN_PS5_CLEANUP_ELF="$cleanup_elf" \
VULKAN_PS5_QUALIFICATION_REMOTE_NAME=eden_ps5 \
VULKAN_PS5_QUALIFICATION_LABEL=eden-flappy-bird-run1 \
VULKAN_PS5_QUALIFICATION_PASS_PATTERN='^eden-ps5: GAME PASS 300 frames$' \
VULKAN_PS5_QUALIFICATION_PASS_DESCRIPTION='Flappy Bird, 300 presented frames, nonzero graphics-cache reload, and bounded teardown' \
VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN="$firmware_pattern" \
VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_2="$input_cycle_pattern" \
VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_3="EdenMain: Prospero shader-cache identity: $cache_identity" \
VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_4='^\[psbc\] Parameter exports: stage=0 count=1$' \
VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_5="$telemetry_baseline_pattern" \
VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_6='Prospero Dynarmic memory path: core=0 sparse_page_table=true callback_fallback=true' \
VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_7="$cache_reload_pattern" \
VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_8='Prospero audio policy: sink=null fail_soft=true' \
VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_9='^\[openagc\] backend=direct-dev-gc fd_open=true capability=0x[0-9A-Fa-f]+$' \
VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_10="$input_cycle_stop_pattern" \
VULKAN_PS5_QUALIFICATION_REJECT_PATTERN="$reject_pattern" \
VULKAN_PS5_FW550_LOG_DIR="$log_dir" \
VULKAN_PS5_WEBSRV_TIMEOUT="$websrv_timeout" \
VULKAN_PS5_LIVE_KLOG_TIMEOUT="$((websrv_timeout + 30))" \
VULKAN_PS5_CONTINUOUS_KLOG=1 \
VULKAN_PS5_ABSENCE_CHECK_COUNT=2 \
VULKAN_PS5_ABSENCE_CHECK_DELAY=1 \
VULKAN_PS5_SWAPCHAIN_EXPECTED_SHA256="$pinned_eden_sha256" \
VULKAN_PS5_CLEANUP_EXPECTED_SHA256="$pinned_cleanup_sha256" \
VULKAN_PS5_QUALIFICATION_SIDECAR="$sidecar" \
VULKAN_PS5_QUALIFICATION_SIDECAR_REMOTE_NAME=eden.launch \
VULKAN_PS5_QUALIFICATION_SIDECAR_EXPECTED_SHA256="$sidecar_sha256" \
VULKAN_PS5_QUALIFICATION_ASSET="$homebrew" \
VULKAN_PS5_QUALIFICATION_ASSET_REMOTE_NAME=Flappy_Bird_NX.nro \
VULKAN_PS5_QUALIFICATION_ASSET_EXPECTED_SHA256="$homebrew_sha256" \
PYPS4DEBUG_DIR="$pyps4debug_dir" \
    "$runner"

latest_log=$(ls -1t "$log_dir"/*-swapchain-run1.log | head -n 1)
telemetry=$(grep -E "$telemetry_pattern" "$latest_log" | tail -n 1)
echo "Flappy Bird cache observation: $telemetry"
echo "eden-ps5 FW 5.50 Flappy Bird bounded canary: PASS"
