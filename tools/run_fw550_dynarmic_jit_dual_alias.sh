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
elf=${EDEN_PS5_JIT_DUAL_ALIAS_PROBE_ELF:-$repo_dir/build-prospero-full-audit2/bin/eden-ps5-dynarmic-jit-dual-alias-probe.elf}
cleanup_elf=${EDEN_PS5_CLEANUP_ELF:-$vulkan_repo/build-prospero-msaa/vulkan_ps5_process_cleanup.elf}
pinned_probe_sha256=5f8510d1b0612dc46910b1c381cb98d4757ffe15b90b5386c8cb9f8b22c7b5c6
pinned_cleanup_sha256=9fd6b41cf2ea87989c4217234c6f34c96a1ca5dc482355af1258539db77d4d76
pinned_runner_sha256=1c2da402df3ca3eb30e7121e91abeb83c7da06aa4ff9c4e48e18fac5ec778552
pinned_process_helper_sha256=8dff282cdbc7ac1f4a037ad9e2a0e800fa82838cd1342b804b1eaff65ffd1ef6
pinned_pyps4debug_commit=8f1443bb97bd6e2a77ed5ea2cc9145975d3152eb
pinned_pyps4debug_lock_sha256=c9eb85e0f0bc1bde6c4e00f1112a1aea982dc7eed024eb973fca91e436051033
qualification_runs=${EDEN_PS5_JIT_DUAL_ALIAS_RUNS:-20}
reject_pattern='dynarmic-jit-dual-alias probe: FAIL|result=-?[1-9][0-9]*'

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

case "$qualification_runs" in
    ''|*[!0-9]*) echo "EDEN_PS5_JIT_DUAL_ALIAS_RUNS must be 1-100" >&2; exit 2 ;;
esac
if [ "$qualification_runs" -lt 1 ] || [ "$qualification_runs" -gt 100 ]; then
    echo "EDEN_PS5_JIT_DUAL_ALIAS_RUNS must be 1-100" >&2
    exit 2
fi
if ! command -v shasum >/dev/null 2>&1; then
    echo "shasum is required for the pinned qualification gate" >&2
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
verify_file_sha256 "$elf" "$pinned_probe_sha256" 'Dynarmic JIT dual-alias probe'
verify_file_sha256 "$cleanup_elf" "$pinned_cleanup_sha256" 'cleanup ELF'
verify_file_sha256 "$runner" "$pinned_runner_sha256" 'guarded Vulkan-PS5 runner'
verify_file_sha256 "$process_helper" "$pinned_process_helper_sha256" \
    'exact-process helper'
verify_file_sha256 "$pyps4debug_lock" "$pinned_pyps4debug_lock_sha256" \
    'PyPS4debug lockfile'

run=1
while [ "$run" -le "$qualification_runs" ]; do
    VULKAN_PS5_QUALIFICATION_ELF="$elf" \
    VULKAN_PS5_CLEANUP_ELF="$cleanup_elf" \
    VULKAN_PS5_QUALIFICATION_REMOTE_NAME=eden_jit_dual_alias_probe \
    VULKAN_PS5_QUALIFICATION_LABEL="eden-jit-dual-alias-run${run}" \
    VULKAN_PS5_QUALIFICATION_PASS_PATTERN='^eden-ps5 dynarmic-jit-dual-alias probe: PASS size=0x4000 aliases=2$' \
    VULKAN_PS5_QUALIFICATION_PASS_DESCRIPTION='distinct RW and RX JIT shared-memory aliases, known-return execution, and teardown' \
    VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN='op=create-shared-RWX-maximum address=0 size=0x4000 result=0 errno=0' \
    VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_2='op=create-alias-RW address=0 size=0x4000 result=0 errno=0' \
    VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_3='op=mmap-writer-RW address=[0-9a-f]+ size=0x4000 result=0 errno=0' \
    VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN_4='op=jit-map-executor-RX address=[0-9a-f]+ size=0x4000 result=0 errno=0' \
    VULKAN_PS5_QUALIFICATION_REJECT_PATTERN="$reject_pattern" \
    VULKAN_PS5_SWAPCHAIN_EXPECTED_SHA256="$pinned_probe_sha256" \
    VULKAN_PS5_CLEANUP_EXPECTED_SHA256="$pinned_cleanup_sha256" \
    PYPS4DEBUG_DIR="$pyps4debug_dir" \
        "$runner"
    run=$((run + 1))
done

echo "eden-ps5 FW 5.50 Dynarmic JIT dual-alias ${qualification_runs}-relaunch gate: PASS"
