#!/bin/sh
set -eu

: "${PS5_HOST:?set PS5_HOST to the FW 5.50 console address}"
: "${EDEN_PS5_EXPECTED_SHA256:?set EDEN_PS5_EXPECTED_SHA256 to the exact frontend ELF hash}"
: "${EDEN_PS5_CLEANUP_EXPECTED_SHA256:?set EDEN_PS5_CLEANUP_EXPECTED_SHA256 to the exact cleanup ELF hash}"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(dirname -- "$script_dir")
vulkan_repo=${EDEN_PS5_VULKAN_PS5_DIR:-$repo_dir/../Vulkan-PS5}
runner="$vulkan_repo/examples/run_fw550_swapchain.sh"
elf=${EDEN_PS5_ELF:-$repo_dir/build-prospero-full-audit2/bin/eden-ps5.elf}
cleanup_elf=${EDEN_PS5_CLEANUP_ELF:-$vulkan_repo/build-prospero-m2/vulkan_ps5_process_cleanup.elf}
homebrew=${EDEN_PS5_2048_NRO:-$repo_dir/../2048.nro}
sidecar="$repo_dir/src/ps5/eden-2048.launch"
sidecar_sha256=9f85dcac310c0031ca32bd735a8e6a93d04bfb81c9d60aedc3a659b09c2c5e2b
homebrew_sha256=cd7e7f343830920196590d99c82a9f1ab8a375eeaeb943fa6c671aa68250a20d

if [ ! -x "$runner" ]; then
    echo "missing guarded Vulkan-PS5 runner: $runner" >&2
    exit 2
fi

run=1
while [ "$run" -le 2 ]; do
    required_pattern=
    if [ "$run" -eq 2 ]; then
        required_pattern='Total Pipeline Count: [1-9][0-9]*'
    fi
    VULKAN_PS5_QUALIFICATION_ELF="$elf" \
    VULKAN_PS5_CLEANUP_ELF="$cleanup_elf" \
    VULKAN_PS5_QUALIFICATION_REMOTE_NAME=eden_ps5 \
    VULKAN_PS5_QUALIFICATION_LABEL="eden-2048-run${run}" \
    VULKAN_PS5_QUALIFICATION_PASS_PATTERN='^eden-ps5: GAME PASS 600 frames$' \
    VULKAN_PS5_QUALIFICATION_PASS_DESCRIPTION='2048 homebrew, 600 presented frames, and bounded teardown' \
    VULKAN_PS5_QUALIFICATION_REQUIRED_PATTERN="$required_pattern" \
    VULKAN_PS5_WEBSRV_TIMEOUT=120 \
    VULKAN_PS5_SWAPCHAIN_EXPECTED_SHA256="$EDEN_PS5_EXPECTED_SHA256" \
    VULKAN_PS5_CLEANUP_EXPECTED_SHA256="$EDEN_PS5_CLEANUP_EXPECTED_SHA256" \
    VULKAN_PS5_QUALIFICATION_SIDECAR="$sidecar" \
    VULKAN_PS5_QUALIFICATION_SIDECAR_REMOTE_NAME=eden.launch \
    VULKAN_PS5_QUALIFICATION_SIDECAR_EXPECTED_SHA256="$sidecar_sha256" \
    VULKAN_PS5_QUALIFICATION_ASSET="$homebrew" \
    VULKAN_PS5_QUALIFICATION_ASSET_REMOTE_NAME=2048.nro \
    VULKAN_PS5_QUALIFICATION_ASSET_EXPECTED_SHA256="$homebrew_sha256" \
        "$runner"
    run=$((run + 1))
done

echo "eden-ps5 FW 5.50 2048 immediate-relaunch gate: PASS"
