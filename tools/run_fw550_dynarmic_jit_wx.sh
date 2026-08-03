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
elf=${EDEN_PS5_JIT_WX_PROBE_ELF:-$repo_dir/build-prospero-full-audit2/bin/eden-ps5-dynarmic-jit-wx-probe.elf}
cleanup_elf=${EDEN_PS5_CLEANUP_ELF:-$vulkan_repo/build-prospero-msaa/vulkan_ps5_process_cleanup.elf}
pinned_probe_sha256=7905e56f44fd419900258b247372c0885f305ec562b9111df47c070483bdcbc8
pinned_cleanup_sha256=9fd6b41cf2ea87989c4217234c6f34c96a1ca5dc482355af1258539db77d4d76
pinned_runner_sha256=1daccfac01b61a2b00e610dd15403cde844ed399a27d512eb8646b07f7d6102e
pinned_process_helper_sha256=8dff282cdbc7ac1f4a037ad9e2a0e800fa82838cd1342b804b1eaff65ffd1ef6
pinned_pyps4debug_commit=8f1443bb97bd6e2a77ed5ea2cc9145975d3152eb
pinned_pyps4debug_lock_sha256=c9eb85e0f0bc1bde6c4e00f1112a1aea982dc7eed024eb973fca91e436051033
qualification_runs=20
reject_pattern='allocation failed|mapping failed|mmap failed|mprotect failed'
reject_pattern="$reject_pattern|dynarmic-jit-wx probe: FAIL|RW-to-RX.*result=-1"
reject_pattern="$reject_pattern|RX-to-RW.*result=-1|initial-RW-demotion.*result=-1"
reject_pattern="$reject_pattern|munmap.*result=-1|map-mutator.*result=FAIL"
reject_pattern="$reject_pattern|terminating without executing an invalid JIT mapping"

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
verify_file_sha256 "$elf" "$pinned_probe_sha256" 'Dynarmic JIT W^X probe'
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
    VULKAN_PS5_QUALIFICATION_REMOTE_NAME=eden_jit_wx_probe \
    VULKAN_PS5_QUALIFICATION_LABEL="eden-jit-wx-run${run}" \
    VULKAN_PS5_QUALIFICATION_PASS_PATTERN='^eden-ps5 dynarmic-jit-wx probe: PASS caches=4 size=0x2004000 cycles=4$' \
    VULKAN_PS5_QUALIFICATION_PASS_DESCRIPTION='four exact-size Dynarmic caches, concurrent bounded map mutation, W^X cycles, and teardown' \
    VULKAN_PS5_QUALIFICATION_REJECT_PATTERN="$reject_pattern" \
    VULKAN_PS5_SWAPCHAIN_EXPECTED_SHA256="$pinned_probe_sha256" \
    VULKAN_PS5_CLEANUP_EXPECTED_SHA256="$pinned_cleanup_sha256" \
    PYPS4DEBUG_DIR="$pyps4debug_dir" \
        "$runner"
    run=$((run + 1))
done

echo "eden-ps5 FW 5.50 Dynarmic JIT W^X ${qualification_runs}-relaunch gate: PASS"
