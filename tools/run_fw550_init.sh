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
sidecar="$repo_dir/src/ps5/eden-init.launch"
sidecar_sha256=06111c6797d5e5932b792d0c3afdf13bc912ba44af806058bbca4770d8915305

if [ ! -x "$runner" ]; then
    echo "missing guarded Vulkan-PS5 runner: $runner" >&2
    exit 2
fi

run=1
while [ "$run" -le 2 ]; do
    VULKAN_PS5_QUALIFICATION_ELF="$elf" \
    VULKAN_PS5_CLEANUP_ELF="$cleanup_elf" \
    VULKAN_PS5_QUALIFICATION_REMOTE_NAME=eden_ps5 \
    VULKAN_PS5_QUALIFICATION_LABEL="eden-init-run${run}" \
    VULKAN_PS5_QUALIFICATION_PASS_PATTERN='^eden-ps5: INIT PASS$' \
    VULKAN_PS5_QUALIFICATION_PASS_DESCRIPTION='full frontend init and bounded teardown' \
    VULKAN_PS5_SWAPCHAIN_EXPECTED_SHA256="$EDEN_PS5_EXPECTED_SHA256" \
    VULKAN_PS5_CLEANUP_EXPECTED_SHA256="$EDEN_PS5_CLEANUP_EXPECTED_SHA256" \
    VULKAN_PS5_QUALIFICATION_SIDECAR="$sidecar" \
    VULKAN_PS5_QUALIFICATION_SIDECAR_REMOTE_NAME=eden.launch \
    VULKAN_PS5_QUALIFICATION_SIDECAR_EXPECTED_SHA256="$sidecar_sha256" \
        "$runner"
    run=$((run + 1))
done

echo "eden-ps5 FW 5.50 init immediate-relaunch gate: PASS"
