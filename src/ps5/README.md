# Eden PS5 bootstrap

This directory contains the Prospero-only integration executable used to
bring Eden's production components onto Vulkan-PS5 in controlled slices. It is
not a replacement renderer and uses no OpenAGC or VideoOut entrypoints
directly.

Configure against installed packages or the adjacent source checkout:

```sh
cmake -S . -B build-prospero-bootstrap \
  -DCMAKE_TOOLCHAIN_FILE="$PS5_PAYLOAD_SDK/toolchain/prospero.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DEDEN_PS5_BOOTSTRAP_ONLY=ON \
  -DEDEN_PS5_VULKAN_PS5_SOURCE_DIR=../Vulkan-PS5
cmake --build build-prospero-bootstrap \
  --target eden-ps5-vulkan-bootstrap.elf
```

`EDEN_PS5_VMA_INCLUDE_DIR` may name an installed directory containing
`vk_mem_alloc.h`. When unset, CMake accepts Eden's pinned CPM 3.3.0 cache or
the adjacent `VulkanMemoryAllocator` checkout.
`EDEN_PS5_VULKAN_UTILITY_INCLUDE_DIR` may likewise name installed Vulkan
Utility Libraries headers; the adjacent checkout is accepted for development.
A host
`glslangValidator` is required at build time to regenerate the embedded
SPIR-V from Eden's production `vulkan_quad_indexed.comp`; it is not a PS5
runtime dependency.

The current gate performs:

1. Vulkan instance, PS5 surface, device, and three-image FIFO swapchain
   creation.
2. Eden's production `MemoryAllocator` and `vk::Buffer` wrappers over its VMA
   implementation, with mapped upload/readback and a device-local buffer.
3. A 4,096-byte upload→device→readback copy and exact CPU oracle.
4. Compilation and execution of Eden's production quad-index compute shader
   with two storage descriptors, a 12-byte push-constant range, and an exact
   six-index readback oracle.
5. Vulkan pipeline-cache serialization, recreation from those bytes, and a
   second compute dispatch with a different base-vertex oracle. The current
   Vulkan-PS5 cache payload is the standard cache header; compiled PSBC reuse
   remains a later cache implementation step.
6. 600 bounded acquire, submit, clear, and present frames.
7. Explicit VMA allocation-count/byte recovery, Vulkan teardown, and
   system-service self-exit.

Hardware runs must use the guarded Vulkan-PS5 runner with the cleanup ELF and
local/remote SHA-256 verification. FW 5.50 is the development endpoint; replay
the final identical bytes on FW 11.60 only during final qualification.

After the qualified two-run 2048 lifecycle gate, the active workload is a
bounded `Flappy_Bird_NX.nro` canary. Its purpose is to determine whether this
Mesa GLES2 title reaches Eden's guest Maxwell graphics/compute pipeline and
transferable shader-cache paths; native PSBC presentation activity alone is
not sufficient. The canary pins every local and tooling identity, performs
cleanup plus repeated exact-process absence before launch, requires visible
output and exact bounded teardown, and treats SDL2_mixer initialization as
fail-soft only. Do not promote Flappy Bird to the two-run cache gate until run
one proves guest pipeline creation and writes a transferable record.

Run the canary with `tools/run_fw550_flappy_bird.sh`. It requests exactly 300
presented frames, cycles sustained A/B/directional input without another input
thread, defaults to a 55-second host deadline, and performs two independently
separated absence checks. Run one has already created two guest graphics
pipelines and written two transferable records. The current wrapper is the
immediate-relaunch gate: it requires a nonzero discovered and successfully
loaded graphics-record count from the new `disk-load-complete` telemetry under
the same derived cache identity. Runtime creation/write counts remain separate
from disk discovery/load/rejection counts so a warm-cache replay cannot be
mistaken for a fresh compile.

The cleanup-first FW 5.50 replay in
`20260803T230453Z-swapchain-run1.log` discovered and successfully loaded five
graphics records with zero rejections under the pinned identity. It reached
native present sequence 63 and left no exact `eboot.bin` after cleanup. This
qualifies immediate cache reload, but not the separate 300-present gate: the
45-second bound expired while the title remained at its static intro.

Qualification sidecars never synthesize controller or keyboard input. They may
select a game and an optional bounded `frames=N` lifetime only. On Prospero,
interactive qualification uses the native `libScePad` bridge; obsolete
`input_cycle` and qualification `input_profile` options fail closed as malformed
sidecars.

Earlier 48-press and two-press logs remain historical renderer evidence only.
They predate removal of synthetic SDL3 input and cannot qualify the current
manual-only ELF. New interactive evidence must contain `libScePad` transition
telemetry and operator confirmation.

The first Flappy hardware attempt reached its accelerated GLES2 renderer and
one native present, then exposed an eager allocation in the guest GPU
`MultiLevelPageTable`: a 37-bit `nvhost-as-gpu` address space tried to commit a
128 MiB logical table through Prospero flexible memory. Current builds retain
the flat logical interface but allocate zeroed 64 KiB first-level chunks on
demand. This is separate from the sparse 39-bit process page table described
below. The failed run was cleanup-safe and is not qualification evidence; use
only the ELF SHA pinned by `tools/run_fw550_flappy_bird.sh` for its retry.

The first sparse-table retry passed that assertion and identified the next
blocker precisely: format 51, usage `0x1f`, flags `0x108` was a mutable,
extended-usage A8B8G8R8 image whose storage capability came from its declared
view formats. Vulkan-PS5 now evaluates usage against the union of that format
list for both creation and the properties2 query, while the same request
without extended usage remains rejected. The retry log ends with cleanup and
two PID/global absence observations; it is diagnostic evidence, not a pass.

The full Prospero build also provides `eden-ps5-host-memory-probe.elf`. It
exercises Eden's PS5 guest-memory backend independently of Vulkan: fastmem is
disabled because the host uses 16 KiB pages, a contiguous 4 GiB guest backing
range is mapped from 64 tracked 64 MiB direct-memory allocations, both ends of
every chunk are verified, and teardown unmaps the range and releases every
physical allocation. Run it twice through the cleanup-first gate with:

```sh
PS5_HOST=10.0.1.41 \
EDEN_PS5_MEMORY_PROBE_EXPECTED_SHA256=<probe-sha256> \
EDEN_PS5_CLEANUP_EXPECTED_SHA256=<cleanup-sha256> \
EDEN_PS5_CLEANUP_ELF=<cleanup-elf> \
  tools/run_fw550_host_memory.sh
```

This is an eager chunked backing implementation. True sparse commitment would
require a Prospero fault handler or a larger Eden memory-model refactor; it is
not claimed by this gate.

The full build also provides `eden-ps5-virtual-buffer-probe.elf` for Eden's
large CPU-side address maps. On Prospero, the 39-bit process page table keeps
its full logical 128-million-entry contract but allocates zero-initialized
64 KiB chunks only when an entry is touched. Dynarmic therefore uses Eden's
memory callbacks instead of its contiguous page-table fast path on PS5.
Ordinary `VirtualBuffer` allocations use 16 KiB-aligned CPU flexible memory
and release the exact mapping during destruction. The probe checks sparse
entries across chunk and address-space boundaries, a 64 MiB flexible mapping,
move/resize behavior, teardown, and immediate relaunch:

```sh
PS5_HOST=10.0.1.41 \
EDEN_PS5_VIRTUAL_BUFFER_EXPECTED_SHA256=<probe-sha256> \
EDEN_PS5_CLEANUP_EXPECTED_SHA256=<cleanup-sha256> \
EDEN_PS5_CLEANUP_ELF=<cleanup-elf> \
  tools/run_fw550_virtual_buffer.sh
```

The FW 5.50-qualified probe SHA-256 is
`ba68ab06b05540868cdd828c94c41d47ec8c022861fe3ae1eae50617ca4290a5`.
The two cleanup-first runs are recorded in the Vulkan-PS5 qualification logs
at `20260802T045445Z-swapchain-run1` and
`20260802T045456Z-swapchain-run1`; both returned the exact PASS oracle,
retired the process, and left websrv/FTP available for immediate relaunch.

The full Eden boot path also requires executable memory for Dynarmic's Xbyak
code cache. On Prospero, each cache uses JIT shared memory with distinct
OS-chosen RW and RX aliases. Xbyak exposes RX addresses for branches and
execution while routing emission and patch writes through the RW alias;
constant-pool writes follow the same rule. Allocation ownership is kept in a
fixed out-of-band registry, and mapping, lookup, or teardown failures stop
execution. A32 and A64 each use a 32 MiB cache. Sixteen MiB left no space
beyond Dynarmic's prelude reservation, while 64 MiB per cache exhausted
native-app memory.

Vulkan instance creation must use the version returned by Eden's normal
version negotiation. It must not request Vulkan 1.3 unconditionally while the
Vulkan-PS5 ICD advertises Vulkan 1.2. After forwarding the negotiated version,
the FW 5.50 run `20260802T051216Z-swapchain-run1` reached physical-device
selection, OpenAGC initialization, and PSBC compilation. The tested ELF SHA-256
was `d7aacb715a7ec61e83d0fcecfd2d5965529d03d5cd062ab739c5bd74fe7945e3`.
It currently stops at a later `VK_ERROR_FEATURE_NOT_PRESENT`; this is a
diagnostic milestone, not a renderer-pass claim.

For an Eden run, set the runner's `VULKAN_PS5_QUALIFICATION_ELF`,
`VULKAN_PS5_QUALIFICATION_REMOTE_NAME`, `VULKAN_PS5_QUALIFICATION_LABEL`,
`VULKAN_PS5_QUALIFICATION_PASS_PATTERN`, and
`VULKAN_PS5_QUALIFICATION_PASS_DESCRIPTION` controls as documented in
`../Vulkan-PS5/README.md`, and pin both application and cleanup ELF hashes.

After an address32/compiler change, do not start with the long gate. Run
`tools/run_fw550_2048_sequence0.sh` first. It uses the eight-frame sidecar but
accepts the run only when sequence zero reads back exact BGRA magenta from both
the renderer intermediate and the swapchain image, FW 5.50 is identified, the
bounded teardown completes, and no exact `eboot.bin` remains. Obtain visual
confirmation before advancing to the authoritative long gate,
`tools/run_fw550_2048.sh`, which performs two cleanup-first 600-frame runs with
identical ELF, sidecar, and ROM hashes. Both active 2048 wrappers pin the
current Eden ELF, cleanup ELF, guarded
Vulkan runner, exact-process helper hashes, canonical PyPS4debug source
revision, and exact PyPS4debug lockfile bytes. Their mandatory failure pattern
cannot be replaced; callers may only append additional rejection patterns.
The eight-frame preflight keeps its 60-second default and 120-second ceiling.
The measured PS5 rate is about 0.75 presented 2048 frames per second, so the
authoritative 600-frame wrapper defaults to 900 seconds and permits 1-1200;
its continuous target-klog deadline is kept 120 seconds beyond the web request.

Zero-ID NRO homebrew uses a Prospero-only shader-cache namespace derived from
the full SHA-256 of the exact loader backing file. This does not alter the
guest program ID, and real nonzero title IDs remain unchanged. Hash/read
failure disables disk caching for that launch. Teardown drains queued
transferable pipeline records before destroying cache state. The pinned 2048
workload uses SDL software rendering and creates no guest Maxwell pipeline, so
its wrapper requires the derived identity, native PSBC host-pipeline activity,
600 native presents, visible output, and teardown but does not require or claim
a transferable `vulkan.bin`. Guest-cache persistence must be qualified with a
workload that logs guest graphics/compute pipeline creation and writes a
nonempty transferable cache before a nonzero relaunch count is accepted.

The address32 build passed that 2048 sequence-zero gate and was repeatedly
confirmed visible on the console: magenta appeared before the 2048 board,
whose colors were correct but faint. The same exact ELF then passed two
cleanup-first 600-present runs with immediate relaunch, bounded teardown, and
repeated exact-process absence. Host input-cycle progress remains diagnostic
only and never substitutes for the native 600-present oracle. Vulkan-PS5
commit `93c7325` subsequently removed the temporary first-eight
end/submit/acquire/present checkpoints while preserving its committed-success
100/600 markers and fail-only diagnostics. The post-retirement ELF is a new,
unlaunched artifact and does not replace the qualified pair. The next
shader-cache slice uses a workload that actually creates guest pipelines;
Flappy Bird is the first bounded homebrew canary. InvadersNX remains available
for guest-specific investigation but is not accepted as evidence for the 2048
completion goal.

Before rerunning the long gate after a Dynarmic or payload-SDK JIT change, run
`tools/run_fw550_dynarmic_jit_dual_alias.sh`. It pins the GPU-free probe,
cleanup ELF, guarded runner, exact-process helper, and PyPS4debug toolchain,
then requires 20 fresh cleanup-first processes. Each process creates shared
JIT backing, maps a writable alias with non-executable `mmap`, maps the
executable alias once through `sceKernelJitMapSharedMemory`, emits only through
RW, executes a known-return stub only through RX, and checks exact teardown and
process absence. Any failure prevents the pass oracle. This is a JIT primitive
preflight, not full Dynarmic or renderer evidence. The active allocator uses
the same hybrid mapping mechanism and never performs RW-to-RX `mprotect`
transitions.
