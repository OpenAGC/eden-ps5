#!/bin/sh
set -eu

: "${PS5_HOST:?set PS5_HOST to the FW 5.50 console address}"
: "${EDEN_PS5_MEMORY_PROBE_EXPECTED_SHA256:?set the exact memory-probe ELF hash}"
: "${EDEN_PS5_CLEANUP_EXPECTED_SHA256:?set the exact cleanup ELF hash}"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(dirname -- "$script_dir")
vulkan_repo=${EDEN_PS5_VULKAN_PS5_DIR:-$repo_dir/../Vulkan-PS5}
runner="$vulkan_repo/examples/run_fw550_swapchain.sh"
elf=${EDEN_PS5_MEMORY_PROBE_ELF:-$repo_dir/build-prospero-full-audit2/bin/eden-ps5-host-memory-probe.elf}
cleanup_elf=${EDEN_PS5_CLEANUP_ELF:-$vulkan_repo/build-prospero-m2/vulkan_ps5_process_cleanup.elf}

if [ ! -x "$runner" ]; then
    echo "missing guarded Vulkan-PS5 runner: $runner" >&2
    exit 2
fi

run=1
while [ "$run" -le 2 ]; do
    VULKAN_PS5_QUALIFICATION_ELF="$elf" \
    VULKAN_PS5_CLEANUP_ELF="$cleanup_elf" \
    VULKAN_PS5_QUALIFICATION_REMOTE_NAME=eden_memory_probe \
    VULKAN_PS5_QUALIFICATION_LABEL="eden-memory-run${run}" \
    VULKAN_PS5_QUALIFICATION_PASS_PATTERN='^eden-ps5 host-memory probe: PASS bytes=0x100000000 chunks=64$' \
    VULKAN_PS5_QUALIFICATION_PASS_DESCRIPTION='4 GiB chunked guest backing allocation, touch, release' \
    VULKAN_PS5_SWAPCHAIN_EXPECTED_SHA256="$EDEN_PS5_MEMORY_PROBE_EXPECTED_SHA256" \
    VULKAN_PS5_CLEANUP_EXPECTED_SHA256="$EDEN_PS5_CLEANUP_EXPECTED_SHA256" \
        "$runner"
    run=$((run + 1))
done

echo "eden-ps5 FW 5.50 host-memory immediate-relaunch gate: PASS"
