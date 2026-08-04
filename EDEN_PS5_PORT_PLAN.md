# Eden PS5 Port Plan

## Current active slice (2026-08-04)

### Active goal: qualify the SDL package dependency baseline

Before FFmpeg or any other package that consumes SDL is rebuilt, the sibling
`../SDL` tree must qualify against the current `../OpenAGC` and
`../Vulkan-PS5` trees. The direct SDL renderer must consume only the installed
`OpenAGC::openagc` contract, while the Zink path must use the current
`libvulkan_ps5.so`; no installed SDL metadata may add `libSceAgcDriver` to the
direct `/dev/gc` process. SDL must also build and export correct consumer
metadata with libsamplerate disabled, and optionally consume pacbrew's
`ps5-payload-libsamplerate` only when explicitly enabled. FFmpeg and later SDL
consumers remain blocked until this baseline and an installed-package consumer
link pass.

The current OpenAGC and Vulkan-PS5 Prospero trees build successfully. A fresh
combined SDL2 Release configuration with `SDL_PS5_OPENAGC=ON`,
`SDL_PS5_ZINK=ON`, and tests enabled compiled the complete SDL library and test
set against a freshly staged OpenAGC package. It exposed and fixed two stale
package assumptions: PS5 audio unconditionally emitted `-lsamplerate` even
when `SDL_LIBSAMPLERATE=OFF`, and installed OpenAGC metadata injected
`-lSceAgcDriver` despite the selected direct backend. The corrected CMake,
pkg-config, and `sdl2-config` metadata use OpenAGC 0.2.0, contain neither
forbidden dependency in the disabled-samplerate configuration, and pass a
focused metadata regression. A fresh installed CMake consumer links SDL2,
OpenAGC, `kernel`, and `SceVideoOut` without libsamplerate or
libSceAgcDriver. Vulkan-PS5's shared-ICD verifier also passes with 204 exports
and only the qualified relocation set. The pacbrew libsamplerate recipe is
`/Users/bizkut/Downloads/PS5/homebrew/pacbrew-repo/libsamplerate/PKGBUILD`.

After this package slice is committed, resume the paused Flappy diagnostic
goal below, then build FFmpeg and downstream SDL packages in dependency order.

### Paused goal: eliminate remaining Flappy runtime diagnostics

The visual Flappy Bird canary is complete. The active goal is now to remove
the remaining actionable non-Vulkan diagnostics without regressing the proven
direct `/dev/gc` renderer path. In order: preserve NVMap pin ownership through
session cleanup and prove balanced pin/unpin telemetry; implement the NVDRV
error notifier contract; remove the two avoidable BSD-before-initialization
errors; audit the remaining NVDRV, VI, AM, and GetInfo stubs and either
implement workload-required behavior or document why each retained stub is
safe; and classify or avoid ps5debug's post-exit suspend race. Each slice needs
focused host coverage and a meaningful commit. The final gate is a fresh
cleanup-first, 300-frame Flappy run on exact FW 5.50 using only direct
`/dev/gc`, with visible intro, zero Vulkan/critical errors, balanced NVMap
telemetry, bounded teardown, and repeated exact-process absence.

The first slice identified the teardown imbalance: `NvMap::FreeHandle`
force-unmapped a last-user handle and reset its shared pin count even though
AS-GPU/NVDEC mapping records still owned pins. Their later orderly unpins then
underflowed the count. The fix preserves ownership until those owners release
it, uses a saturating pin-state transition so a genuine unmatched unpin cannot
make the count negative, and emits destructor totals for pin calls, unpin
calls, unmatched unpins, and outstanding pins. The focused host regression
passes 10 assertions. The clean host tests target builds successfully; its
unfiltered run still encounters the unrelated existing `HostMemory: Simple
unmap` SIGSEGV, so that failure is not evidence against the focused NVMap
state-machine test and remains a separate host-test issue.

### Completed goal: Flappy Bird guest-pipeline canary

The completed 2048 gate proves the FW 5.50 renderer lifecycle, native
presentation, immediate relaunch, and teardown, but its SDL software renderer
cannot prove guest Maxwell pipeline serialization. The next active goal is one
cleanup-first, bounded `Flappy_Bird_NX.nro` canary using the post-checkpoint
Eden ELF. The local NRO is SHA-256
`6e7cd9a1a22a0102a4f68ba6e434378c9b7381ce4f44a43ca376953f536aa54d`
and its path-independent Prospero cache identity is `ee7cd9a1a22a0102`.
The current pinned guest diagnostic Eden ELF is SHA-256
`ff3c252883753a41ba3a27cb7aea3bd9b392cb208f6672c2e3b816d21e74fc82`.
It includes OpenAGC command-state span and capacity fixes through `d7ed7f2`,
Vulkan-PS5 storage-color support through `236687c`, and explicit
Eden disk-cache discovery/load/rejection telemetry. The guarded wrapper pins
these exact bytes for the immediate cache-reload replay.

Before launch, pin the ELF, NRO, sidecar, cleanup ELF, guarded runner,
exact-process helper, and PyPS4debug revision/lockfile. Run only the direct
`/dev/gc` backend in this boot. Launch the pinned cleanup ELF, wait for the
settle interval, and independently prove exact `eboot.bin` absence twice.
Reject the forbidden fixed-address ELF and every allocation, mapping, W^X,
GPU-thread, Vulkan/OpenAGC, native-submit, presentation, teardown, or exact
process failure.

The canary must be bounded by a presented-frame sidecar and host deadline. It
passes only with exact FW `5.500.008`, the derived cache identity, native PSBC
activity, the exact `GAME PASS` frame count, bounded teardown, a responsive
console, repeated PID/global exact-process absence, and operator-confirmed
visible Flappy Bird output. Audio is not part of the emulation qualification:
the NRO's SDL2_mixer path must either initialize without destabilizing Eden or
fail softly while Eden's explicit null-audio fallback remains active. An
audio failure, hang, or crash fails this canary.

Shader-cache promotion requires separate evidence. Telemetry must first prove
that the guest creates at least one Maxwell graphics or compute pipeline. Only
then may a nonempty transferable cache record written by run one and a nonzero
successfully loaded graphics/compute count on an identical immediate relaunch
be accepted. `Total Pipeline Count` alone records parsed entries and is not a
load-success oracle.
Host ACO/PSBC presentation pipelines and the standard Vulkan cache header do
not satisfy this oracle. If Flappy Bird remains entirely inside its packaged
Mesa/SDL software or GLES translation path without guest Maxwell pipelines,
record that result and select a different workload rather than manufacturing a
cache pass.

Implementation is now staged for hardware qualification. Eden commit
`b5dcddd` adds an A/B/directional input cycle suitable for Flappy's title and
tutorial flow and emits a teardown summary with separate runtime guest
graphics, guest compute, transferable-record-written, and record-skipped
counts. `SerializePipeline` now reports whether it wrote a complete eligible
record rather than merely creating the 12-byte header. The host `video_core`
target and full Prospero build pass. Vulkan-PS5 commit `b41393a` adds bounded,
configurable repeated PID/global absence checks; its guarded-runner regression
passes and default count one preserves existing callers.

`tools/run_fw550_flappy_bird.sh` pins the telemetry ELF, 120-frame sidecar,
NRO, cleanup ELF, updated runner, exact-process helper, and PyPS4debug source
and lockfile. It uses a 30-second default and 360-second hard ceiling, forces
continuous klog, requires two absence observations separated by one second,
and accepts the lifecycle canary only with exact firmware, cache identity,
input-cycle, PSBC, telemetry-summary, frame, teardown, and process oracles. A
successful canary prints the observed counters but does not claim cache
persistence; only positive guest creation and record-write counts make an
identical immediate relaunch eligible.

The cleanup-first PID 239 run is preserved at Vulkan-PS5 log
`examples/qualification-logs/flappy-bird/20260803T211403Z-swapchain-run1.log`.
It proved that removing the false four-byte DMA restriction advances through
the earlier native copy rejection, but precise telemetry then located the
remaining `VK_ERROR_FEATURE_NOT_PRESENT` in the source buffer's whole-range
state query. Eden uses disjoint ranges of its shared staging buffer, so asking
OpenAGC for one uniform state across the entire allocation was invalid.
Vulkan-PS5 `edba96d` computes the exact Vulkan copy footprint, including BC
block geometry, and prepares only the referenced range. Its host regression
deliberately gives an unused part of the source buffer a different state; the
copy records successfully, and both the focused host command test and
Prospero ICD build pass.

The next cleanup-first PID 242 run is preserved at
`examples/qualification-logs/flappy-bird/20260803T211906Z-swapchain-run1.log`.
It passed exact-range preparation, reached the native 576x576 buffer-to-image
copy at staging offset 4,223,488, and increased the recorded draw count from
two to three. The new first failure is `AGC_ERROR_COMMAND_SPACE_EXHAUSTED`
(`0x8089000c`), translated to `VK_ERROR_OUT_OF_HOST_MEMORY`, because encoding
one DMA packet per image row no longer fits the remaining 64-KiB Vulkan native
DCB after the preceding guest work. The apparent image-view destruction text
is stale debug state and is not the result associated with this return path.
The next implementation slice must give this workload a bounded larger native
command capacity (or a safely chunked submission path), add a regression that
exhausts the old 64-KiB budget, and retain fail-closed capacity errors. Do not
paper over the result by translating it to a feature error.

That capacity slice is now implemented. Vulkan-PS5 uses a bounded 256-KiB
native DCB, and its focused host regression records 3,000 buffer copies in one
command buffer so the test necessarily exceeds the former 64-KiB budget.
OpenAGC reports buffer-image row-packet exhaustion through its debug channel;
all 20,120 runtime assertions, the Vulkan focused command test, and the
Prospero ICD build pass. Cleanup-first PID 245 is preserved at
`examples/qualification-logs/flappy-bird/20260803T212357Z-swapchain-run1.log`.
It passed the 576x576 upload and the capacity point, then advanced to three
draws. The new first failure is dynamic `vkCmdSetScissor` during an active
render pass (`VK_ERROR_FEATURE_NOT_PRESENT`); the render pass therefore stays
open when Eden reaches `vkEndCommandBuffer`. The next slice is to log the exact
signed rectangle, implement Vulkan-valid clipping into gfx1013's unsigned
scissor domain, add negative/partially clipped regression cases, rebuild, and
repeat the cleanup-first Flappy canary.

That scissor slice passed on cleanup-first PID 248, log
`examples/qualification-logs/flappy-bird/20260803T213104Z-swapchain-run1.log`.
It advanced from three to eight native draws, closed the active render pass,
and reached a later buffer-to-image operation. PID 251 with exact copy
validation telemetry is preserved at
`examples/qualification-logs/flappy-bird/20260803T213321Z-swapchain-run1.log`.
It identifies the request as `VK_FORMAT_D32_SFLOAT_S8_UINT` (130), depth aspect
`0x2`, mip/layer zero, and extent 1280x720x1. Vulkan-PS5 currently rejects all
depth/stencil aspects in `native_image_copy_layers`, so OpenAGC is not reached.
The next slice must implement a tiled D32 depth-plane upload through a graphics
meta path that reads the staging buffer and writes `gl_FragDepth`; raw linear
DMA rows are not valid for OpenAGC's gfx1013 `64KB_Z_X` depth layout. Keep the
stencil plane fail-closed until an independently correct per-fragment stencil
write path exists. Add exact D32/S8 aspect, row-pitch, offset, transition,
descriptor, attachment, and teardown regressions before the next hardware
retry.

Vulkan-PS5 `dab93b4` now implements that D32 plane as a graphics meta draw:
the fragment shader reads the exact staging-buffer span and writes
`gl_FragDepth` into the public OpenAGC depth target. The host command regression
records a valid D32 copy and proves that an S8-only copy remains fail-closed;
the strict Prospero ICD build also passes. Cleanup-first PID 254 is preserved
at `examples/qualification-logs/flappy-bird/20260803T214720Z-swapchain-run1.log`.
It again reached eight native draws and the depth upload, but the real Eden
command contains two buffer-image regions rather than PID 251's initially
reported first region. The combined command returned
`VK_ERROR_FEATURE_NOT_PRESENT` before a native upload because the new helper
requires every region to be the depth aspect. This narrows the next slice to
the separate S8 plane: log both regions exactly, implement packed-byte staging
reads plus fragment stencil export through a stencil-write meta pipeline, and
retain fail-closed validation for malformed or unsupported aspect mixtures.
The guarded runner cleaned PID 254 and proved PID-specific and global exact
`eboot.bin` absence twice.

Vulkan-PS5 `dbe9973` completes the two-region path with a second meta pipeline.
Its fragment shader performs packed 32-bit loads, extracts the addressed S8
byte, and exports `FragStencilRefEXT`; the Vulkan stencil state writes that
per-fragment value. Depth and stencil use independent exact aligned staging
ranges so disjoint buffer state remains valid. The host regression records
both regions together, includes a deliberately unaligned stencil offset, and
rejects a malformed combined-aspect region. SPIR-V inspection confirms
`StencilExportEXT` and `FragStencilRefEXT`; the focused host test and strict
Prospero ICD build pass.

The first guarded retry with those bytes did not execute Eden and is not a
renderer result. Pre-launch cleanup proved global exact `eboot.bin` absence
twice, then PID 257 stopped at `mDBG: Waiting for debug subsystem .. 1` in
`examples/qualification-logs/flappy-bird/20260803T215522Z-swapchain-run1.klog`.
There is no Eden, Vulkan-PS5, OpenAGC, or guest GPU output after process exec.
The web request timed out, port 744 became unreachable, the host stopped
responding to ping, and post-run cleanup could not prove process absence. On
the next boot, run the pinned cleanup ELF and independently prove exact global
absence twice before retrying this same pinned canary. Do not classify this
debug-service/console loss as either an S8 pass or an S8 failure.

While the console remained offline, the canary proof audit closed two missing
positive oracles. Prospero startup now emits
`Prospero audio policy: sink=null fail_soft=true` immediately after forcing
the qualified null sink, and OpenAGC emits
`[openagc] backend=direct-dev-gc fd_open=true capability=...` only after the
direct device opens and its context query succeeds. Vulkan-PS5 `b5f5f96`
extends the guarded runner to ten independent required patterns and tests both
presence and absence of the tenth oracle; its complete runner safety test
passes. The Flappy wrapper pins the updated runner SHA-256
`96e396e42d6b3a73eef0ed7de78fe0e318b1aa51cdcf8ff2c89a54b013452c08`
and requires both new markers without dropping the existing firmware, input,
cache identity, PSBC, baseline, Dynarmic, guest-pipeline creation, or
transferable-record gates. The Prospero OpenAGC target and full Eden
`yuzu-cmd` build pass. Hardware evidence is still pending a fresh boot,
cleanup, and exact-process absence proof.

The fresh-boot FW 5.50 sequence on 2026-08-04 cleared three additional
driver gates under the pinned cleanup-first runner. PID 89, preserved at
`examples/qualification-logs/flappy-bird/20260803T222224Z-swapchain-run1.log`,
reached the first guest render pass and failed because Vulkan-PS5 rejected
Flappy's legal negative-height viewport. OpenAGC `c1773c6` now emits the
negative gfx1013 Y scale, Vulkan-PS5 `30d683b` accepts the Vulkan 1.1 form and
checks both Y endpoints, and the packet, runtime, and command-recording
regressions pass. PID 92, log
`examples/qualification-logs/flappy-bird/20260803T222751Z-swapchain-run1.log`,
passed that point and exposed exact depth-state equality. Vulkan's
write-capable attachment state was rejected for a read-only pipeline;
OpenAGC `9e3f0e4` now documents and tests depth/stencil-write as a valid
superset for read access while still requiring write state for a writer.

PID 95, log
`examples/qualification-logs/flappy-bird/20260803T223034Z-swapchain-run1.log`,
passed viewport and depth binding, completed three native presentations, and
then found a fragmented shared vertex allocation. Vulkan-PS5 `41b83db`
corrects the first range error by preparing from the bound vertex offset
rather than byte zero and adds a deliberately fragmented-prefix regression.
PID 98, log
`examples/qualification-logs/flappy-bird/20260803T223329Z-swapchain-run1.log`,
proved that the represented tail itself is fragmented: binding 0 uses offset
20,928 and a 44,608-byte tail, and OpenAGC correctly returns
`AGC_ERROR_NOT_SUPPORTED` when asked to describe that mixed range as one
uniform state. Every run used only direct `/dev/gc`, began with two exact
global absence proofs, and ended with repeated PID/global absence proofs; no
`eboot.bin` remains.

The current pinned Eden ELF is SHA-256
`6e04da04e3caea7ce35ceebde45128b729027b98f0dd90377add2c8275986102`.
The bounded visual canary is extended from 120 to 300 presented frames so a
successful run captures several seconds of gameplay while retaining the
30-second host deadline. The next implementation slice is a public OpenAGC
command-local buffer-span query that returns the first uniform state and byte
length inside a requested range. Vulkan-PS5 must use it to issue exact ordered
transitions for each span of the bound vertex tail, aborting on any query,
capacity, ownership, or transition failure. Add mixed-prefix and mixed-tail
host regressions, rebuild both Prospero libraries and Eden, then repeat the
same cleanup-first Flappy canary. Do not weaken OpenAGC's uniform range query
or make vertex binding perform an implicit resource transition.

`InvadersNX.nro` remains the second workload and long-running presentation
canary, not a substitute for the current diagnosis. Switch to it after Flappy
records this upload and reaches command-buffer end/presentation; changing the
guest earlier could merely avoid the 576-row transfer and leave the shared
driver defect latent.

The first hardware canary, PID 115, is preserved at Vulkan-PS5 log
`examples/qualification-logs/flappy-bird/20260803T125249Z-swapchain-run1.log`.
It reached Flappy's accelerated SDL2/GLES2 path, compiled native PSBC
pipelines, and completed the first native present before failing at
`common/multi_level_page_table.inc:36` because its dynamically created
`nvhost-as-gpu` address space attempted to allocate the entire logical GPU
page table. On Prospero, anonymous `mmap` commits flexible memory, so the
37-bit table requested 128 MiB eagerly; 2048's software-rendered path never
created this guest GPU address space. The guarded runner rejected the run,
executed cleanup, and proved PID-specific and global exact-process absence
twice. It is failure evidence, not a qualification pass.

Eden commit `1c581b6` fixes this by preserving the logical flat GPU page-table
interface while allocating zeroed 64 KiB first-level chunks only on mutable
access. Missing const reads return zero, move assignment releases prior
ownership, range arithmetic is overflow and bounds checked, and allocation
failure is fatal before dereference. The dedicated host regression covers
cross-level isolation, move ownership, and zero-sized reservation (3 cases,
7 assertions), and both it and the full Prospero build pass. The next action
was one cleanup-first retry; no cache or lifecycle claim carried over from
PID 115.

That retry, PID 118, is preserved at
`examples/qualification-logs/flappy-bird/20260803T130455Z-swapchain-run1.log`.
The sparse GPU page table passed its former failure point. The run completed
the magenta calibration present but its source buffer was still zero, then at
98.7 seconds Flappy requested a 480x480 optimal
`VK_FORMAT_A8B8G8R8_UNORM_PACK32` image with usage `0x1f` and flags `0x108`.
Those flags mean mutable format plus extended usage; the declared compatible
view-format list contained storage-capable formats, but Vulkan-PS5 validated
storage against only the non-storage base format and returned
`VK_ERROR_FORMAT_NOT_SUPPORTED`. The later uncaught Eden exception ended the
process. The one `Unmapped Read64 @ 0x8` was a fail-soft guest CPU read and was
not the terminal fault. Cleanup again proved PID-specific and global exact
absence twice.

Vulkan-PS5 commit `2d84b89` now validates every requested image usage bit
against the union of a mutable extended image's declared view formats and
uses the same rule for `vkGetPhysicalDeviceImageFormatProperties2`. It still
rejects the exact request without extended usage and retains the base native
AGC format. The command-recording regression covers the exact Flappy request,
properties query, storage view, missing-storage-format list, and no-extended
negative case. That regression and the full Prospero cross-build pass; the
unrelated full host build remains blocked by pre-existing stale calls in
`tests/pipeline.c` to the expanded meta-attachment helper. The newly rebuilt
Eden ELF above is the only artifact eligible for the next cleanup-first
retry.

The Vulkan-PS5 cross-build relinked the standalone cleanup ELF even though
`examples/process_cleanup/main.c` and its CMake linkage are unchanged between
`b41393a` and `2d84b89`. The two independent configured Prospero build trees
produce identical replacement bytes, SHA-256
`ff88ac293a55ec4ba5636a6556b74ffbeaf5d1093e96f86208cc55ce262565c5`.
The Flappy wrapper now pins those exact bytes; it rejected the stale hash
before making network contact, so no unpinned cleanup or Eden payload was sent.

The next Eden retry, PID 121, proved the format-51 correction: the former
480x480 allocation rejection did not recur. It still produced only the
magenta calibration present, then failed at 98.9 seconds on a different,
fail-closed request: `VK_FORMAT_D24_UNORM_S8_UINT` (format 129), 1280x720,
usage `0x27`. A graphics-pipeline build had also returned
`VK_ERROR_FEATURE_NOT_PRESENT` immediately beforehand. Cleanup proved the PID
and global exact-process absence twice.

Before changing Eden again, the exact format-51 path was isolated through
public Vulkan-PS5/OpenAGC. Vulkan-PS5 commit `0833663` extends the existing
storage probe; pinned ELF SHA-256
`215169ea600dac81901ab423d36d342ee7d9df98537e5119b0fb591c1e09f96e`
passed on FW 5.500.008 as PID 124. It repeated all 30 storage formats and 480
exact-bit readbacks, created and bound the 480x480 optimal extended image,
dispatched through its listed SNORM storage view, completed the fence, exited,
and passed two PID plus two global absence checks. The accepted log is
`examples/qualification-logs/extended-storage/20260803T132208Z-swapchain-run1.log`.
This closes the format-51/OpenAGC question. The next slice is the existing
D24 fail-closed policy: qualify a supported D32/S8 substitute through a direct
probe or reject Flappy as unsuitable; do not advertise D24 or retry Eden until
that evidence exists.

The combined D32/S8 sampled-image probe did not qualify. PID 126 returned
native queue-wait status `0x80890007`, queue-submit result `-4`, and left its
command buffer pending with status `0x80890003`. It exited without leaving an
exact `eboot.bin` process, and cleanup passed two PID and two global absence
checks. The accepted failure log is
`examples/qualification-logs/d32s8-sampling/20260803T132723Z-swapchain-run1.log`.
Vulkan-PS5 commit `63d48a1`, which temporarily advertised sampled combined
D32/S8, was therefore reverted by `ec32e66`. Combined D32/S8 sampling remains
fail-closed.

Existing direct depth probes qualify combined D32/S8 for depth/stencil
attachment and transfer. Eden commit `b4c6734` now uses that narrower evidence:
on Prospero only, a requested D24/S8 optimal image omits sampled-image support
from format selection and image usage, allowing the existing D24-to-D32/S8
alternative to be selected strictly as an attachment/transfer fallback. Other
platforms are unchanged, and an attempted sampled use is not advertised. The
Prospero `video_core` target and full `yuzu-cmd` ELF build pass. The rebuilt ELF
embeds exact revision `b4c67341777ae9104a4eb62d27c5aea8701a763c` and has
SHA-256 `92b4066369d31ecb6b6bd540cddba6273d72eac66388776d3b7192eadb442c74`.
The Flappy wrapper pins these bytes for the next cleanup-first canary.

That canary, PID 130, proved the attachment-only format selection: the former
format-129 image-allocation rejection did not recur, and Eden reached creation
of the fallback format-130 view. Presentation remained on magenta because
Vulkan-PS5 then rejected the legal one-layer `VK_IMAGE_VIEW_TYPE_2D_ARRAY`
combined depth/stencil view (`aspect=0x6`). The bounded runner cleaned up and
proved PID and global exact-process absence twice. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T133448Z-swapchain-run1.log`.

Vulkan-PS5 commit `dbc4fc2` removes its blanket rejection of 2D-array depth
views and defers native `AgcImageView` creation when an image has no sampled,
storage, or input-attachment usage. This matches the OpenAGC contract:
framebuffer depth binding consumes the underlying `AgcImage`, while shader
descriptors still create and require a native image view. The exact regression
uses a one-layer D32/S8 2D-array attachment view and verifies that it succeeds
without a native shader view. The focused host command-recording test passes,
and the Vulkan-PS5 Prospero static library plus full Eden ELF cross-build pass.
The rebuilt ELF embeds Eden revision
`002a34174482b6f0e7d0cead1fe6928fcc4abce6` and has SHA-256
`23a025593bd3e244a7f2815d127f09ecb30fcf2397894732f92e4eaa2bb38755`.
The wrapper pins these bytes for the next cleanup-first retry.

The exact attachment-view signal was then sent through public Vulkan-PS5 and
OpenAGC before another Eden launch. Vulkan-PS5 commit `ac03c81` changes the
existing depth example to the same one-layer `VK_IMAGE_VIEW_TYPE_2D_ARRAY`
D32/S8 view. Pinned ELF SHA-256
`13e110421882f2511801c358280086489872c8598643443a272d84e62994af70`
ran as PID 133 on FW 5.500.008 and produced the exact hardware oracle
`depth: PASS green=12288 red=9830 raw=54145/12288/9830 stencil=22118`.
This proves the intended depth/stencil attachment, draw, synchronization, and
readback signal crosses Vulkan-PS5/OpenAGC with the patched array view.

The guarded run is retained as a graphics PASS but not a clean full-process
qualification: after the oracle, the scoped kernel log recorded the known
`SceCloudClientAppMain` user-process SIGSEGV during system-service app exit.
The runner rejected the dirty teardown, relaunched the pinned cleanup ELF, and
proved exact absence. Similar exit signatures exist in earlier unrelated
qualification logs, so this evidence does not implicate the D32/S8 command
stream, but it must not be counted as clean teardown. The accepted graphics log
is `examples/qualification-logs/depth-array/20260803T134154Z-swapchain-run1.log`;
the rejected teardown evidence is its `-target.klog` companion.

The cleanup-first Flappy retry then advanced past both the format-130
allocation and the one-layer D32/S8 array view. PID 136 remained on magenta
because all three guest graphics pipelines failed creation with
`VK_ERROR_FEATURE_NOT_PRESENT`; command-buffer closure then rejected the
latched `-8` record error after `vkCmdPipelineBarrier`. The accepted failure
log is
`examples/qualification-logs/flappy-bird/20260803T134307Z-swapchain-run1.log`.
This supersedes D32/S8 as the active blocker: Eden always declares core
`DEPTH_BOUNDS`, `STENCIL_COMPARE_MASK`, and `STENCIL_WRITE_MASK` dynamic state,
while Vulkan-PS5 previously rejected those enums and implemented their command
entry points as no-ops.

OpenAGC commit `32aef72` now exposes those three dynamic states through its
public runtime and emits exact depth-bound and state-preserving front/back
stencil register packets. Vulkan-PS5 commit `e048d47` accepts the Vulkan enums,
propagates them into the native pipeline mask, records/replays their values,
and enables the already-supported static depth-bounds state. Its lifecycle
regression also pins format enum 130 (`VK_FORMAT_D32_SFLOAT_S8_UINT`) to
attachment/transfer features for both reported tilings, rejects sampled use,
and accepts the qualified optimal attachment/transfer usage. OpenAGC runtime
and API-reference tests pass; all 20 focused Vulkan command-recording tests
and the lifecycle test pass; both Prospero static libraries cross-build. The
rebuilt Eden ELF embeds revision
`e0cbb5ab8cf3bd9bf0c5ea190873ad6c21b5ce1d`, has SHA-256
`d76f2440c04f53ec500724d93361c2bf3df7e4cfe7fb3c91f942dd42d27b8c08`,
and is pinned by the Flappy wrapper for the next cleanup-first hardware retry.

That retry, PID 139, still remained on magenta and failed all three guest
graphics pipelines with `VK_ERROR_FEATURE_NOT_PRESENT`; it therefore does not
yet prove that the three newly supported dynamic states were the only pipeline
gap. It did prove clean process handling: the bounded runner relaunched the
pinned cleanup ELF and both PID-scoped and global exact-name checks found no
`eboot.bin`. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T135622Z-swapchain-run1.log`.
Vulkan-PS5 commit `0860274` adds a temporary Prospero-only request fingerprint
covering dynamic enum values, topology, rasterization, multisampling,
alpha-to-coverage/alpha-to-one, blend attachment count, and depth/stencil
enables. An attempted unconditional alpha-to-coverage acceptance was rejected
by the host regression because OpenAGC deliberately keeps that state outside
its qualified multisample subset; that unsafe change was reverted. The next
cleanup-first retry must use the diagnostic build to identify the remaining
exact `-8` branch before extending support.
The diagnostic ELF embeds revision
`63ba472eb23df8b1760421e2175a12ebf892c256`, has SHA-256
`933be07ee5a9a044db74bd01c6dd476891012e377cfb6bebfb713688e62f4234`,
and is pinned by the Flappy wrapper.

The global-barrier retry, PID 161, passed the new public dependency and reached
a native draw at about 97 seconds. It then failed viewport/scissor resolution
with `VK_ERROR_INITIALIZATION_FAILED` while a render pass remained active; the
last accepted command was `vkCmdDraw`. The operator still saw magenta. The
accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T143649Z-swapchain-run1.log`.
PID 161 exited, and both PID-scoped and global exact-absence checks passed
twice.

The failing rule required dynamic scissor right/bottom edges to fit the current
framebuffer. Vulkan permits a scissor to extend beyond the attachment and clips
rasterization to framebuffer bounds. Vulkan-PS5 commit `2af3a28` performs that
clipping for nonempty intersections and retains fail-closed handling for a
fully disjoint scissor until an empty-rasterization path is qualified. The
oversized-scissor regression, neighboring command/lifecycle/validation tests,
and Prospero static build pass.

Diagnostic runs now default to 110 seconds because every useful Flappy blocker
has appeared by 97-98 seconds. Longer windows are reserved for candidates that
have already demonstrated guest-frame progress and need the full 120-frame
pass oracle.

The integrated clipped-scissor retry ELF embeds Eden revision
`da88637d8f9f4a2e7673f3149075d6c97de57551`, has SHA-256
`19004f8dfa8db66e3672aeaf6bd8e5c0d22614f78ca499cd5cd6612da2d9e018`,
and is pinned by the Flappy wrapper for the next cleanup-first 110-second
diagnostic.

The descriptor-preparation retry, PID 167, proved the descriptor fix with
`descriptors=1` and advanced to vertex binding. It then rejected missing
binding 3 because the pipeline's required mask was `0xffffffff`, even though
the guest draw had not supplied or consumed all 32 bindings. Raw guest samples
remained opaque black and the operator-visible result remained magenta. The
accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T144835Z-swapchain-run1.log`.
PID 167 exited, and both PID-scoped and global exact-absence checks passed
twice. Orderly teardown, final telemetry, and visible guest presentation remain
open.

The all-bits mask came from treating every declared
`VkVertexInputBindingDescription` as required. Vulkan only requires buffers for
vertex inputs statically consumed by the compiled shader. Vulkan-PS5 commit
`262fc83` now derives the mask from the PSBC vertex reflection already shared
with OpenAGC. Its regression declares all 32 legal bindings, provides an
attribute only on binding 0, binds only buffer 0, and completes the native draw.
Command-recording, lifecycle, validation, and the Prospero static build pass.

The integrated reflected-binding retry ELF embeds Eden revision
`e6b201f59a39e2375df8b23861b1175549fc55f8`, has SHA-256
`42e76eb3b6d1628286d8338c80cf37d8ab35015eab9d79b3e189bdda3a1e287d`,
and is pinned by the Flappy wrapper for the next cleanup-first 110-second
diagnostic.

That retry, PID 170, proved the reflected-binding fix: both guest graphics
pipelines compiled and two native draws recorded. The render pass closed, but
the first command-side occlusion resolve then failed at
`vkEndCommandBuffer` with `record_error=-8`, `draws=2`, and last labelled
entry `vkCmdPipelineBarrier`. Eden does not issue timestamp commands; its
query cache calls `vkCmdCopyQueryPoolResults` after leaving the render pass
with `VK_QUERY_RESULT_WAIT_BIT|VK_QUERY_RESULT_64_BIT`, followed by a transfer
write-to-read buffer barrier. Vulkan-PS5 previously rejected every such copy
unconditionally. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T145423Z-swapchain-run1.log`.
Two raw guest samples remained opaque black and only the magenta calibration
present was visible. The wrapper retired PID 170 and passed both PID-scoped
and global exact-absence checks twice.

Vulkan-PS5 commit `ce2fdde` records bounded query-copy operations, prepares
the exact destination range as `CopyDestination`, and uses the driver's
already-synchronous native submit completion to reduce OpenAGC's opaque
per-RB occlusion records into Vulkan results before signaling completion.
Eden's host-visible result-buffer path supports 32/64-bit values, WAIT,
PARTIAL, and availability output; unsupported flags, ranges, strides,
operation overflow, and device-local destinations fail closed. The exact
zero-count no-op, WAIT|64-bit copy, and following barrier regression, command-
recording, lifecycle, validation, and Prospero static-library build pass. The next action
is an integrated rebuild, identity pin, and one cleanup-first 110-second retry;
visible guest presentation, orderly telemetry, audio fail-soft behavior, and
cache persistence remain unproven.

Mesa RADV under `../mesa/src/amd/vulkan` is the AMD Vulkan semantic and packet-
strategy reference for this work. In particular, `radv_query.c` implements
GFX10.3 occlusion copies by waiting on the last enabled RB's availability word
and running a query-reduction shader over per-RB begin/end counters. Vulkan-
PS5's synchronous CPU reducer is intentionally narrower and suitable for
Eden's host-visible result buffer; the RADV-style GPU reducer is required
before device-local query-copy destinations can be qualified. Linux winsys
code and generation-specific packets must not be copied without translating
them through public OpenAGC and PS5 hardware tests.

The integrated query-copy retry ELF embeds Eden revision
`86814b384834e788a0bb8b9f2d15a44645ad553f`, includes Vulkan-PS5
`ce2fdde`, and has SHA-256
`10cc52d7005e16bda2944f2ea1c871f4eab2acc2acd77bc877f225fbe36147da`.
The Flappy wrapper pins those exact bytes for the next cleanup-first,
direct-`/dev/gc`, 110-second hardware diagnostic.

That diagnostic, PID 173, did not exercise the query-copy fix. It remained
alive until the 110-second host deadline with no Vulkan rejection, but it also
never entered Flappy's guest-render sequence: the input counter reached 1,056,
both raw guest samples remained opaque black, and only the magenta calibration
present completed. The accepted timeout log is
`examples/qualification-logs/flappy-bird/20260803T150800Z-swapchain-run1.log`.
Cleanup proved PID-specific and global exact-process absence twice.

Flappy's source advances its title on `KEY_A`. The old qualification injector
held each keyboard key for only 50 ms and offered A once per 600 ms cycle; a
slow guest input poll can miss every such pulse. The next input slice uses
250 ms holds and four consecutive A slots before B and directions, preserving
the 110-second deadline while making title/tutorial progression overlap slow
guest polls. Rebuild and identity pinning are required before another launch.

The sustained-input retry ELF embeds Eden revision
`0b2d29bdd4aa9ffc9ace6cdbb993410b749c5dc4` and has SHA-256
`47906f4f9a1c377f237a296b4c1dadeb8097e47c1568938b7f4fcc56213a5b41`.
The wrapper pins these bytes for the next cleanup-first 110-second diagnostic.

That retry, PID 176, confirmed the sustained input reaches gameplay: it began
guest pipeline work at about 98 seconds, compiled two pipelines, and recorded
two draws. The operator still saw only magenta. Command-buffer finalization
again returned `record_error=-8`, but the embedded query-copy validation
diagnostic did not fire, so the prior attribution to
`vkCmdCopyQueryPoolResults` validation was incomplete. The last labelled entry
remained its following `vkCmdPipelineBarrier`, with zero dispatches. The
accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T151330Z-swapchain-run1.log`;
cleanup proved PID-specific and global absence twice.

The next diagnostic labels `vkCmdFillBuffer` and occlusion reset/begin/end,
prints their exact OpenAGC result codes, and logs successful query-copy
recording. No feature is being accepted on inference. Rebuild, pin, and one
cleanup-first replay are required to name the actual post-barrier failure.

The exact-origin diagnostic ELF embeds Eden revision
`b1f21b8b05e2f995d4dfb92151735035f6a42f58`, includes Vulkan-PS5
`58f7e0e`, and has SHA-256
`69a61f08c4c515cb21746073494bea1d533229f4eb0ded7a1b3568209854ddf6`.
The wrapper pins these bytes for the next cleanup-first 110-second replay.

PID 95 did not reproduce the earlier command-buffer `-8`: it completed guest
composition and one native present, then emitted the same deterministic
`Unmapped Read64 @ 0x8` seen in every Flappy replay at 13.1-13.8 seconds. The
first qualification compositor sequence deliberately clears magenta and skips
guest layers, so this guest CPU stop explains why scanout remains magenta
instead of advancing to the second frame. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T153821Z-swapchain-run1.log`;
cleanup passed both PID-specific and global exact-process absence checks twice.
The wrapper's required input-policy marker is corrected from the obsolete
50-millisecond value to the active 250-millisecond sustained-input policy.

Eden now records the AArch64 PC, SP, LR, and selected registers before an
invalid guest `Read64`, allowing the repeatable null-adjacent access to be
attributed to Flappy code, Horizon service code, or an emulator defect without
changing abort policy. The diagnostic ELF embeds Eden revision
`a45fc5553573404f866919aeda280b2bcaff5862` and has SHA-256
`6bb364b7ce83e56055509e308f704060e7b39a75bf7546d6356d55d884d90adc`.
The wrapper pins these bytes for the next cleanup-first replay.

The 30-second PID 98 replay captured `pc=0x80f7b6d4`, `lr=0x80f50054`,
`x0=0`, and the invalid `x0+8` read at 13.98 seconds. It again completed one
native present first, and cleanup passed two PID-specific plus two global
absence checks. Because the NRO uses randomized page-aligned ASLR, the next
diagnostic also records the process entry point so the PC and LR can be mapped
unambiguously to NRO offsets. It embeds Eden revision
`8e3dfb5dba5a24dad69124ecdde66cd2d1d0f701` and has SHA-256
`5665857fa32867fffb9dc1fcd8ecfc663f9060be5d44b705571b86c8a1aa5706`.

PID 101 fixed the module-relative location: entry `0x808e7000`, basic block
`+0x1666d4`, and caller return `+0x13b054`. Disassembly shows the callee
walking `[object+0x90] -> [+0x60] -> [+0x8]` after an indirect call; the
intermediate pointer is null. Eden now invokes its existing guest backtrace
and symbolication at this exact invalid read. The backtrace diagnostic embeds
revision `e9a550e7327916a4d3090e3e2f8f95018bc47ace` and has SHA-256
`4198a0080a1e3f271c333a471d32a661b50282ab3b0af7c90fd650cf43986c8e`.

PID 104 resolved that backtrace against Flappy Bird's unstripped ELF and map:
module offset `+0x1666d4` is `SWITCHAUDIO_OpenDevice`, caller `+0x13b054`
is `SDL_RunAudio`, `+0x1535ac` is adjacent to `SDL_CreateThreadInternal`, and
`+0x6494ac` is `threadCreate`. The Switch audout initialization and start calls
had already succeeded; the subsequent null-adjacent read occurs while the SDL2
audio worker allocates its aligned buffers with newlib `memalign`. This is not
evidence of a generic Eden audout defect: the active hypothesis is a
Prospero-specific guest-thread TPIDRRO/TLS/reentrancy, mapping, or Dynarmic
context regression.

Eden revision `f380df9` now reports Dynarmic's TPIDRRO_EL0, the scheduler's
expected TLS address, and libnx's `ThreadVars` magic, thread pointer, newlib
reentrancy pointer, and handle whenever this invalid read occurs. The strict
integrated Prospero build passes. Its diagnostic ELF has SHA-256
`48c198d924175419da055af0f5a066ca87a69f37611ee0c01f989cdf8ba85a2c` and is
pinned for one bounded cleanup-first capture. A mismatch between TPIDRRO and
the scheduler address identifies context loading; a valid magic with null
reentrancy identifies guest thread setup; an invalid TLS range identifies
mapping or page-table state. No audio contract or native sink behavior is
changed by this diagnostic.

The bounded FW 5.50 PID 107 capture
`examples/qualification-logs/flappy-bird/20260803T160643Z-swapchain-run1.log`
proved the guest audio worker has a coherent thread context at the fault:
TPIDRRO_EL0 and the scheduler TLS are both `0x81a04400`, the libnx magic is
`0x21545624`, and both the thread pointer and newlib reentrancy pointer are
nonzero. The invalid read remains `0x8` after one native present. This rules
out a missing TLS mapping, an unloaded TPIDRRO context, and a null `_reent`
pointer. The run did not satisfy the 120-frame gate; cleanup nevertheless
proved PID-specific absence twice and global exact `eboot.bin` absence twice.

The capture also exposed why the reported PC was only the translated block
entry: the A64 callback requested `PrefetchAbort` for an invalid data access,
while Dynarmic's per-access generated exit checks recognize `MemoryAbort`.
Eden revision `cda5bb57ecb422efe5d043e3287248213649106c` now requests the correct
data-abort reason, enables immediate post-access halt checks for Prospero's
callback-only page-table path, and records context only after Dynarmic has
written the exact faulting PC. Host `core` and strict integrated Prospero
builds pass. The resulting ELF has SHA-256
`a4f911c2d50441719b3c0150975b95939bcc8a33d6e3a966b72fd443e1b79e8e` and is
pinned for one cleanup-first exact-instruction capture. This is a fail-closed
diagnostic correction, not an audio workaround.

PID 110 repeated the null `Read64` after one native present in
`examples/qualification-logs/flappy-bird/20260803T161221Z-swapchain-run1.log`.
The post-abort PC became module offset `+0x654b18`, the `svcGetInfo`
instruction, which cannot itself perform a guest data read; it is therefore a
halt boundary rather than the faulting load. The fault address `0x8` and audio
worker ancestry match libnx `rmutexUnlock`'s `ldr w1, [x0,#8]` when passed a
null mutex. SDL2 stores and verifies a non-null mixer mutex before starting
this worker, so a later audio-device/heap corruption is now more likely than
normal SDL allocation failure. Cleanup again proved PID 110 absence twice and
global exact-process absence twice.

Revision `ecd77c1d3c0bbe27c6b05984a9ad4c831ba4b376` adds a guarded read-only
snapshot of the candidate SDL audio device's buffer, mixer mutex, audio
thread, lock owner, and hidden-driver fields, plus mutex contents when mapped.
The strict integrated Prospero build passes. Its ELF has SHA-256
`172c7c2a088164c01f7fbad7dc50ebaa8362af21b3b469b5659547e10ad54050` and is
pinned for a bounded cleanup-first capture. A null device mutex proves object
corruption after successful open; a valid mutex rules that out and redirects
the fault to another null-base load in the worker.

PID 113 proved the SDL candidate is intact in
`examples/qualification-logs/flappy-bird/20260803T162156Z-swapchain-run1.log`:
device `0x2103144ea0` holds buffer `0x2103156010`, mixer mutex
`0x2103144f50`, audio thread `0x210315fc90`, and hidden driver
`0x2103144f70`; the mutex is mapped and contains its valid unlocked state.
The null read therefore does not come from a destroyed or overwritten mixer
mutex. Cleanup proved PID-specific and global exact-process absence twice.

Dynarmic revision `40308332cbb3cc9c6cf3b646eab856c06a87da95` adds an
abort-only A64 callback carrying the translated memory instruction's guest PC.
It is invoked only after `check_halt_on_memory_access` observes `MemoryAbort`,
so successful accesses retain the existing fast path. Eden associates that PC
with its pending invalid `Read64`. Host `core` and strict integrated Prospero
builds pass. The resulting diagnostic ELF has SHA-256
`bfeec8c83655464d9680b122ac7b6595c24f682fe1d07dc4959cd9910478dd8b` and is
pinned for one cleanup-first capture that must identify the actual null-base
instruction before any behavioral fix is attempted.

PID 116 repeated the exact SVC-boundary PC in
`examples/qualification-logs/flappy-bird/20260803T162612Z-swapchain-run1.log`;
the new abort callback therefore does not yet identify the generating load.
The intact device snapshot repeated, and all four cleanup absence checks
passed. The unstripped Switch SDL2 disassembly exposes a stronger immediate
oracle: `SWITCHAUDIO_OpenDevice` stores two unchecked `memalign(0x1000, size)`
returns at hidden-driver offsets `0` and `8`, then immediately calls `memset`.
A null allocation can therefore produce the observed low-address access while
leaving the device and mixer mutex intact.

Revision `e8759e8` extends the guarded snapshot to those exact two audout
buffer pointers. The strict integrated Prospero build passes. Its ELF has
SHA-256
`16c79dbd5878bfef10845b7786c65150008397224680f7d00d52fed36add7076` and is
pinned for a bounded cleanup-first capture. If either pointer is null, the
next slice owns guest heap expansion/aligned-allocation failure rather than
audout IPC or SDL mutex state.

PID 119 ruled out that allocation theory in
`examples/qualification-logs/flappy-bird/20260803T163026Z-swapchain-run1.log`:
the two hidden audout buffers are both mapped, non-null aligned addresses
(`0x210314d000` and `0x2103152000`). Cleanup again proved PID-specific and
global exact-process absence twice.

Revision `c1c412f` adds an A64 callback-read variant which receives the guest
PC at the same generated call site as the runtime address. Prospero uses it
only for checked callback-mode `Read64`; other platforms and unchecked access
retain the original callback. This avoids correlating a later halt boundary
with an earlier invalid access. Host `core` and strict integrated Prospero
builds pass. The resulting ELF has SHA-256
`9fbcc11c6775011dcd4fc5df53fb3488beab102d3e069cece6ad6ab51e2a68e6` and is
pinned for one cleanup-first capture.

PID 122 finally correlated the callback itself in
`examples/qualification-logs/flappy-bird/20260803T163609Z-swapchain-run1.log`:
the invalid `Read64` is attached to module offset `+0x13afb4`, the
`SDL_RunAudio` call to `SDL_UnlockMutex`. Immediately before that call,
`+0x13afb0` loads the mixer mutex which the same capture proved is non-null and
mapped. The linked callee nevertheless computes address `0x8`. This proves a
Prospero Dynarmic cross-block register-handoff failure rather than an audout,
TLS, heap, or SDL object failure. PID 122 and global `eboot.bin` were absent
twice after cleanup.

Revision `283c785` disables Dynarmic `BlockLinking` only on Prospero, forcing
translated blocks through the dispatcher so architectural registers are
committed at the boundary. Other platforms and all other optimization flags
are unchanged. Host `core` and strict integrated Prospero builds pass. The A/B
ELF has SHA-256
`d35245af271860f444635a7ca2ea1ff5a710840773787db3089836bb15bb3938` and is
pinned for one cleanup-first 30-second replay. Success requires the prior
`Read64 @ 0x8` to disappear; advancing beyond it is evidence for the fix but
does not by itself satisfy the full 120-frame canary.

PID 125 disproved block linking as the sole cause in
`examples/qualification-logs/flappy-bird/20260803T163958Z-swapchain-run1.log`:
the same invalid read recurred at 13.94 seconds with all four cleanup absence
checks passing. The direct callback PC is the address after the guest
`ldr x0,[x19,#0x70]`, so the access itself computed `0x8`: the live generated
copy of guest `x19` was null while the architectural JIT-state snapshot still
held the valid audio-device pointer. This narrows ownership to optimization or
register preservation within callback-mode generated code, not only the
linked terminal.

Revision `099a032` disables unsafe transforms and all optional Dynarmic IR
optimizations only on Prospero. This is a diagnostic quarantine, not the final
performance policy; flags must be re-enabled individually after correctness is
proven. The strict integrated Prospero build passes. Its ELF has SHA-256
`e532431659976cf507f33bb221dd166ce618e070c477638de31beed27636e321` and is
pinned for one cleanup-first replay. If the low read persists, the remaining
owner is the x64 callback ABI/register allocator rather than an IR pass.

PID 128 confirmed that the unoptimized Prospero build still performs the same
invalid callback-mode `Read64` from address `0x8` after about 14.58 seconds.
The accepted capture is
`examples/qualification-logs/flappy-bird/20260803T164432Z-swapchain-run1.log`;
cleanup again proved both PID-specific and global exact `eboot.bin` absence
twice. Disabling block linking, unsafe transforms, and optional IR passes has
therefore ruled out those layers. The remaining evidence points at host
register preservation across x64 callback calls: the live generated register
for guest `x19` is lost although the architectural JIT-state copy and the SDL
audio device, mutex, TLS, reentrancy state, and audout buffers remain valid.

Revision `81b1e26` makes Prospero spill all Dynarmic caller-save and callee-save
host registers around callbacks, excluding the stack and JIT-state pointers
and the normal argument/return locations. Other targets retain the established
SysV caller-save set. The strict integrated Prospero build passes. The A/B ELF
has SHA-256
`5647fee5b9c7485e757ac71364b97a45f7ceba71a660a67e7c4eb37a6c61d464`
and is pinned for a cleanup-first 30-second replay. The immediate success
criterion is that the `Read64 @ 0x8` disappears and execution advances beyond
the SDL audio callback; that result alone will not satisfy the required
120-presented-frame, cache-telemetry, or bounded-teardown qualification gates.

PID 131 rejected the allocator-wide spill implementation before it reached the
previous 14-second audio fault. At 2.706 seconds Dynarmic asserted that every
candidate register was allocated, then attempted to spill a non-register and
stopped code generation. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T165005Z-swapchain-run1.log`.
The wrapper ran the pinned cleanup first, and after termination proved both
PID-specific and global exact `eboot.bin` absence twice. Because this run ended
earlier, the absence of `Read64 @ 0x8` does not yet prove that issue fixed.

Revision `5f17d4b` replaces allocator-wide spilling with ABI-boundary
preservation. Prospero-generated calls now push and restore Dynarmic's six
SysV callee-save GPRs with its existing alignment-aware helpers; callback
arguments and return registers retain the established allocation contract, and
other platforms retain their existing generated call sequence. The generic
guest host-call emitter now uses the same guarded call path. Host `dynarmic`
and `core` targets and the strict integrated Prospero build pass. The rebuilt
ELF embeds `5f17d4b` and has SHA-256
`81eb9588446977b24a3b22a406c2c5c3d7d37f5c0158b6e7da298acc8f8f8f05`.
It is pinned for a cleanup-first 30-second A/B; success first requires reaching
beyond the prior 14.58-second fault without either the low read or register
allocator assertions.

PID 134 rejected the first ABI-boundary implementation during construction of
the first Dynarmic code cache: Xbyak threw `label is too far` before Eden
loaded the game. Adding save/restore instructions to calls emitted inside the
fixed prelude exceeded one of its compact branch displacements. The accepted
failure log is
`examples/qualification-logs/flappy-bird/20260803T165556Z-swapchain-run1.log`;
both PID-specific and global exact-process absence checks again passed twice.

Revision `dc7b95b` leaves the fixed prelude unchanged and emits the Prospero
callee-save guard only after `PreludeComplete()`, which covers translated guest
blocks where the corrupt live guest register was observed. Host `dynarmic` and
`core` targets and the strict integrated Prospero build pass. The rebuilt ELF
embeds `dc7b95b` and has SHA-256
`bf79993144501f03169f61c87d3cea64d93139c99e4de6c04ac857a1dba639d7`.
It is pinned for the next cleanup-first 30-second A/B.

PID 140 crossed the old fault boundary and disproved callee-save register
clobbering: despite guarding both allocatable SysV callee-save registers, the
same `Read64 @ 0x8` recurred at 13.999 seconds. Architectural guest `x19`, TLS,
the SDL audio device and mutex, and both audout buffers remained valid. The
accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T170251Z-swapchain-run1.log`.
This matches Dynarmic's existing allocator warning that overspill can produce
zero reads: the guest-derived address value is lost while spanning earlier
callback-mode memory accesses. Cleanup again proved PID-specific and global
exact-process absence twice.

Revision `e5c3c08` removes the disproven register guard and adds an opt-in A64
translation fallback that ends a block after a guest instruction emitting a
data-memory access. Both x64 and arm64 backends honor the public option.
Prospero enables it only for its callback-only memory path, ensuring the next
block reloads architectural state instead of carrying allocator spill values
across multiple host callbacks. A focused regression proves an `LDR` block
reads only that instruction, advances PC by four, and returns the correct
loaded value. The complete Dynarmic suite passes 201,927 assertions in 130 test
cases; host `dynarmic` and `core` targets and the strict integrated Prospero
build also pass. The rebuilt ELF embeds `e5c3c08` and has SHA-256
`397b905f0356d691cc27e26ff1f1278b3df74242e2aa35c333374cfdc9bf685b`.
It is pinned for a cleanup-first bounded replay. This fallback prioritizes
correctness; its performance remains to be measured before it can qualify as
the final allocator solution.

PID 143 crossed the same boundary with one memory-emitting guest instruction
per block and still produced `Read64 @ 0x8` at 14.169 seconds. This rules out
both cross-callback and cross-instruction IR lifetimes: the address becomes
invalid while the checked `Read64` itself is prepared. The accepted failure
log is
`examples/qualification-logs/flappy-bird/20260803T171219Z-swapchain-run1.log`;
the wrapper again proved both PID-specific and global exact-process absence
twice.

The remaining checked-read diagnostic had changed the normal devirtualized
callback ABI by moving the address from `RSI` to a third argument in `RDX` so
the compile-time PC could occupy `RSI`. Revision `c3ea295` removes that custom
callback and restores the established one-address `MemoryRead64` path. Exact
fault attribution remains available from `MemoryAccessAbort`, which records
the instruction PC and writes it to JIT state before the forced return. The
complete Dynarmic suite again passes 201,927 assertions in 130 test cases, and
the strict integrated Prospero build passes. The rebuilt ELF embeds `c3ea295`
and has SHA-256
`109c0140e3c390420383436496ce34f7e6bbbba1ef76140afed5ecbbd13f8826`.
It is pinned for a cleanup-first bounded A/B of the standard callback ABI.

PID 146 disproved the custom checked-read ABI as the source of the failure.
The standard one-address callback still received `Read64 @ 0x8` at 14.126
seconds after guest graphics-pipeline creation and one completed native
present. Guest `x19`, TLS, the SDL audio object and mutex, and both audout
buffers remained valid. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T171731Z-swapchain-run1.log`;
cleanup again proved both PID-specific and global exact-process absence twice.

That replay also exposed a validation mismatch: Prospero configured Dynarmic
to inspect `MemoryAbort` after every callback access, while Eden could disable
the callback's address validation through `cpuopt_ignore_memory_aborts`. The
unmapped read consequently reached `Memory::Read64`, and
`MemoryAccessAbort` never received the current instruction PC; the reported
`0x807ffb18` was only the last committed JIT-state PC. Revision `7330a52`
forces checked memory access for the Prospero callback-only path, independent
of the unsafe setting. Invalid reads now request a synchronous abort, preserve
the exact guest PC, and return from the translated block before subsequent
guest execution. The host `core` target and strict integrated Prospero build
pass. The rebuilt ELF embeds `7330a527e0` and has SHA-256
`b6fc1ad3c5b05cfe0f73b1164293609ec9d077b2f2dfc483a713a6f101e1ede0`.
It is pinned for the next cleanup-first 30-second diagnostic; the live-klog
deadline is bounded to 60 seconds rather than retaining the former 150-second
diagnostic window.

The cleanup-first PID 149 replay is preserved at
`examples/qualification-logs/flappy-bird/20260803T172708Z-swapchain-run1.log`.
It verified exact FW `5.500.008`, created guest graphics pipelines, and
completed one native present before the same low read at 19.365 seconds. The
new fail-closed path requested `MemoryAbort` and reported the exact guest PC:
module offset `+0x1666e4`. The pinned NRO bytes there are
`ldr x0, [x0,#8]`, immediately after `ldr x0, [x0,#0x60]`. Their 12-byte tail
matches the local mapped build's `SWITCHAUDIO_GetDeviceBuf`, which calls
`audoutWaitPlayFinish` and returns the sample pointer from the released
`AudioOutBuffer`. This supersedes the earlier custom-ABI attribution to
`SDL_UnlockMutex`: the audio device and mutex are intact, but the released
buffer pointer returned to SDL is null. PID-specific and global exact-process
absence again passed twice after cleanup.

The next diagnostic records both sides of that audout contract without
changing behavior: the service logs every appended client tag and the count,
capacity, and first tag returned by `GetReleasedAudioOutBuffers`; the guarded
fault snapshot records SDL's two sample buffers plus its released-buffer
pointer and count. If the service returns count zero, ownership is audio
buffer release/event scheduling. If it returns a nonzero matching tag while
SDL still holds null, ownership is CMIF output-array serialization or libnx
guest memory delivery. A behavioral workaround is not permitted until this
A/B identifies which side lost the pointer.

Revision `4fcb755` implements that read-only instrumentation. The host `core`
target and strict integrated Prospero build pass. The rebuilt ELF embeds
`4fcb755f21`, has SHA-256
`d1b7d250d677cefddc8a4319b04fdb9ff4ca9ee425cf38630f13172500cb3230`, and
is pinned for one cleanup-first 30-second capture.

The cleanup-first PID 152 capture is preserved at
`examples/qualification-logs/flappy-bird/20260803T173552Z-swapchain-run1.log`.
Eden accepted the two nonzero SDL client tags `0x2102344f80` and
`0x2102344fa8`, then `GetReleasedAudioOutBuffers` returned `count=0`,
`capacity=1`, and `first=0`. The simultaneous guest snapshot held
`released=0` and `released_count=0`. This rules out CMIF output delivery and
proves the buffer event woke libnx before any tag was released. One native
present completed, and cleanup again proved PID-specific and global exact
process absence twice.

`AudioBuffers::ReleaseBuffers` historically reported success both after a
real release and whenever the registered queue was already empty. That
combined predicate is retained explicitly for AudioIn, which uses the empty
notification to request more capture buffers. AudioOut now opts out of empty
queue notifications: its event is signalled only when at least one client tag
actually moves to the released queue. This prevents the startup manager tick
from leaving a stale signalled event that makes `audoutWaitPlayFinish` return
zero buffers. The next replay must return one of the appended tags, eliminate
the null `SWITCHAUDIO_GetDeviceBuf` read, and continue toward the 120-frame
oracle; merely surviving the old boundary is not the completion gate.

Revision `eff9304` implements the split AudioOut/AudioIn empty-queue policy.
Host `audio_core` and `core` targets and the strict integrated Prospero build
pass. The rebuilt ELF embeds `eff93045eb`, has SHA-256
`f3e642af9d8fd04d88f5be37c6a5e739e6594e6f8e1210d2282665be3b32d28c`, and
is pinned for the cleanup-first hardware A/B.

PID 155 crossed the old fault boundary in
`examples/qualification-logs/flappy-bird/20260803T174051Z-swapchain-run1.log`.
Both tags were appended at 19.12 seconds, no zero-count release occurred, and
the former `Read64 @ 0x8` was absent for the remainder of the 30-second
window. This proves the stale-event correction. It also exposed the second
half of the null-audio contract: `NullSinkStreamImpl::AppendBuffer` discards
samples, but the inherited played-sample estimator is capped by a maximum
sample count which the null sink never advances. The two registered buffers
therefore never become releasable, so SDL waits indefinitely. Cleanup still
proved PID-specific and global exact-process absence twice.

The null sink now overrides the played-sample query to report discarded
output consumed. DeviceSession's existing 5 ms manager tick then moves the
real client tags into the released queue and signals AudioOut; there is no
native PS5 audio dependency and no synthetic zero tag. The base query becomes
virtual so hardware sinks retain their timed estimator. The next canary must
show a nonzero returned tag, repeated append/release progress, continued
graphics presentation, and bounded teardown.

Revision `8071f06` implements null-sink consumption. Host `audio_core` and
`core` targets and the strict integrated Prospero build pass. The rebuilt ELF
embeds `8071f06ed6`, has SHA-256
`de844f685bd0bf5e9769e871eb9537a4b1a2c3f514fb21224140c302a962b28f`, and
is pinned for the cleanup-first hardware replay.

PID 158 proved end-to-end tag delivery in
`examples/qualification-logs/flappy-bird/20260803T174533Z-swapchain-run1.log`:
the first two releases returned the exact alternating nonzero tags, and the
old null read did not recur. Returning the maximum played-sample count made
the null sink recycle 4,096-frame buffers every 5 ms manager tick, however,
rather than at their 48 kHz duration. That produced thousands of guest IPC
calls and diagnostic lines, starving useful graphics progress until the
30-second bound. All four cleanup absence checks still passed.

The null sink consumption clock is therefore paced by `steady_clock` at the
Switch target rate. `Start` and `Stop` preserve the accumulated frame count,
and the virtual played-sample query reports only elapsed 48 kHz frames. The
first two append and release records remain at info level as bounded evidence;
later audio cycles are no longer logged. This retains fail-soft audio timing
without a PS5 hardware sink and prevents diagnostic logging from becoming the
workload.

Revision `b9e05b3` implements the paced null sink and bounded tag telemetry.
Host `audio_core` and `core` targets and the strict integrated Prospero build
pass. The rebuilt ELF embeds `b9e05b3793`, has SHA-256
`72196063c6bf2a6c53854ca8c7eeb375ef01c70833740d1d7f6906c838897eec`, and
is pinned for the cleanup-first replay.

PID 161 confirmed the final fail-soft audio timing in
`examples/qualification-logs/flappy-bird/20260803T175021Z-swapchain-run1.log`.
The appended tags were released in order after 85.2 ms and 84.3 ms, matching
4,096 frames at 48 kHz within scheduler tolerance. Only the first two records
were logged, the low read remained absent, and cleanup proved PID-specific
and global exact-process absence twice. Audio is no longer the active blocker.

The run still completed only one native present before the 30-second bound.
The remaining throughput quarantine came from the disproven CPU diagnosis:
Prospero ended every translated block after a memory access and disabled block
linking, return-stack, fast-dispatch, context-elimination, constant-propagation,
and miscellaneous IR optimizations. The exact-PC and audout-tag captures now
prove the null read was a real SDL dereference caused by a premature AudioOut
event, not register loss. Prospero therefore returns to Eden's selected CPU
accuracy policy while retaining callback-only memory, synchronous
`MemoryAbort`, strict W^X, and fail-closed mapping transitions. The next replay
must preserve the audio evidence while materially increasing guest/native
frame progress.

Revision `90db104` removes the disproven Prospero translation and optimization
quarantines. The host `core` target and strict integrated Prospero build pass.
The rebuilt ELF embeds `90db104f0a`, has SHA-256
`50b914312a42af0eb6cf7fc395f3f24cf210bac51a4ea69c9f008db8409e0552`, and
is pinned for the cleanup-first throughput A/B.

That A/B ran as PID 164 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T175438Z-swapchain-run1.log`.
It retained the corrected AudioOut contract: two nonzero tags were appended,
then released in order at approximately 85 ms intervals without an invalid
guest read. It also had no Dynarmic, W^X, Vulkan, OpenAGC, or GPU-thread
failure. Restoring normal JIT block formation and optimization did not change
the presentation result, however: the run entered
`RendererVulkan::Composite` exactly once, completed exactly one native
present, and produced no second composite entry before the 30-second bound.
The wrapper rejected the missing 120-frame oracle, ran cleanup, and proved
both PID-scoped and global exact `eboot.bin` absence twice. CPU optimization
is therefore no longer the active explanation for the one-present stall.

The next diagnostic traces the immediately preceding display boundary rather
than extending the timeout. On Prospero, `GPU::Impl::RequestComposite` now
records a bounded sequence of request entries, layer and acquire-fence counts,
sync-dispatch, each fence's expected and current guest syncpoint values, fence
completion, and the direct or fence-gated renderer call. It uses the same
first-16-then-power-of-two trace policy as the renderer/present path. One
30-second cleanup-first replay can therefore distinguish no second guest
display request from a later request held behind an unsignaled acquire fence,
without changing presentation or synchronization behavior. The host `core`
target and strict integrated Prospero build pass. Revision `ae950bf` contains
the diagnostic and PID 164 evidence. Its rebuilt ELF embeds `ae950bfbee`, has
SHA-256
`cbb3ddae0aadfb2f0c5aad97e8720e7a429665034b9561b2644dd85c2c5299fb`, and
is pinned for exactly one cleanup-first 30-second replay.

That replay ran as PID 167 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T180355Z-swapchain-run1.log`.
The new boundary trace closed the acquire-fence branch: display request
sequence zero contained one layer and zero fences, synchronously entered the
renderer, completed the native present, and returned to the producer. No
sequence-one request entered `GPU::Impl::RequestComposite` during the
remainder of the bound. Correct 48 kHz AudioOut tag release continued, and no
CPU, JIT/W^X, Vulkan, OpenAGC, or GPU-thread failure appeared. Cleanup proved
PID 167 and the global exact process name absent twice each.

The stall is upstream in the Nvnflinger producer wakeup contract. Consumer
release changed an acquired slot to free and notified only Eden's host
condition variable. It did not signal the producer binder's guest-visible
`BufferQueue:WaitEvent`; a GLES/EGL producer that clears and waits on that
native handle can therefore sleep after its first swap even though the slot
is free. The producer is now held weakly by its paired consumer, and a
successful release signals that wait event after dropping the queue mutex and
before the optional producer-listener callback. The weak ownership makes a
late release safe if the producer binder has already been destroyed. This
does not manufacture frames or bypass buffer state: it wakes the guest only
after a valid acquired-to-free transition. The host `core` target passes; the
strict Prospero build also passes. Revision `a166af2` contains the fix and PID
167 evidence. Its rebuilt ELF embeds `a166af2753`, has SHA-256
`a5cc0b995caa8e930657af080f4c75e293b4ef7bdfac305513bc43470d8e91e4`, and
is pinned for one cleanup-first 30-second replay.

That replay ran as PID 170 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T181023Z-swapchain-run1.log`.
It again completed request and native-present sequence zero with no fences,
then produced no sequence-one request. Audio tags remained ordered and paced,
no fatal diagnostic appeared, and cleanup proved PID-specific and global
exact-process absence twice. Signaling the producer wait event therefore did
not by itself restore progress. The acquired-to-free wakeup remains a valid
BufferQueue contract repair, but it is not sufficient evidence that Flappy
was sleeping on that handle; the earlier root-cause wording is narrowed
accordingly.

The next diagnostic observes the full upstream lifecycle. A shared constexpr
trace policy preserves the existing first-16-then-power-of-two bound. VI now
reports sparse vsync/composition progress through the 30-second window.
BufferQueue reports producer dequeue entry/return, queue commit and depth,
successful consumer acquire, and release with the producer-event signal
result. This distinguishes a stopped VI conductor, a released slot followed
by no second dequeue, a producer blocked inside dequeue, and a dequeued frame
that never reaches queue commit. Host `core` and the strict Prospero build
pass. The monolithic host test target still cannot link because its configured
`_deps/ffmpeg-build/libavcodec/libavcodec.a` is absent; no focused
Nvnflinger test target is registered. Revision `60db38b` contains the bounded
trace and PID 170 evidence. Its rebuilt ELF embeds `60db38bdca`, has SHA-256
`9e323f6be18db7ddbcc5be5b52b36943b5534118db6daaa0d01c80686e1cd49b`, and
is pinned for exactly one cleanup-first 30-second replay.

That replay ran as PID 173 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T181636Z-swapchain-run1.log`.
It localizes the missing frame beyond the BufferQueue consumer. VI continued
at 60 Hz through sparse sequence 1,023. The guest dequeued slot zero, queued
frame one, and immediately dequeued slot one for frame two. VI then acquired,
presented, and released frame one, including a successful producer-wait-event
signal. No second dequeue was pending and no queue commit for slot one ever
arrived. Thus VI, acquisition, release, the producer event, and native
presentation all remain live; the guest stalls while producing frame two
after a successful dequeue. Audio remained correctly paced, no fatal
diagnostic appeared, and cleanup proved PID 173 and global `eboot.bin`
absence twice each.

The next boundary is the guest GLES/NVDRV submission and completion contract.
Prospero now sparsely traces each GPFIFO submit's channel syncpoint, flags,
input fence, output target, increment, and current host/guest values. NVHOST
control waits report their target, timeout/allocation mode, cached minimum,
live host/guest counters, immediate-completion path, registered event slot,
and callback. This can prove whether frame two is waiting on a target that was
never incremented, waiting correctly for queued GPU work, or never reaching
NVDRV at all. Host `core` passes; strict Prospero build and one cleanup-first
30-second replay remain required. Revision `7ccde60` contains the diagnostic
and PID 173 evidence. Its rebuilt ELF embeds `7ccde6073f`, has SHA-256
`cc7c8e861d7f0c7c8962327091fa1ca097eda6140da4a3f413712c7ce73a797b`, and
is pinned for exactly one cleanup-first replay.

That replay ran as PID 176 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T182116Z-swapchain-run1.log`.
The guest dequeued its second buffer at about 7.55 seconds but did not submit
the associated GPU work until 17.205 seconds. Its only GPFIFO submission had
two entries and flags `0x104`: it reserved output fence `1:1`, with increment
one, while both live host and guest values remained zero. No NVHOST control
wait, second GPFIFO submission, or second BufferQueue commit followed. Audio
remained correctly paced and cleanup proved PID-specific and global exact
`eboot.bin` absence twice each.

The sibling baseline Eden checkout uses the same `increment_value` handling:
flag `0x100` contributes the input fence value to the reserved maximum but
does not synthesize a fence-action command. Changing that contract without
observing the submitted methods would be speculative. The next diagnostic
therefore traces the bounded queue-to-completion path: GPU-thread dispatch,
DMA dispatch and individual headers, puller fence actions, the immediate guest
syncpoint increment, and the deferred host increment callback. Host `core` and
the strict Prospero build pass. Revision `5b60c5b` contains this telemetry and
the PID 176 evidence. Its rebuilt ELF embeds `5b60c5b251`, has SHA-256
`d6e8da3e9627e881d4d5f9505a286fbc1f6f83526bac080efd59aec031067126`, and
is pinned for exactly one cleanup-first 30-second replay.

That replay ran as PID 179 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T182949Z-swapchain-run1.log`.
It rules out a stalled GPFIFO or syncpoint completion path. The GPU thread
dispatched both submitted headers; the 1,011-word main header and three-word
fence header completed. Syncpoint one advanced guest `0 -> 1`, its deferred
host callback advanced host `0 -> 1`, DMA flush returned, and the GPU-thread
dispatch completed, all within about 1.9 milliseconds. No second BufferQueue
commit followed. The run emitted no fatal diagnostic, audio remained correctly
paced, and cleanup proved PID 179 and global `eboot.bin` absence twice each.

The next boundary is the guest-visible NVDRV response. Prospero now sparsely
traces `SubmitGPFIFOBase2` return, the enclosing `Ioctl2` device return,
output-buffer copy-back, and response construction. This proves whether the
guest receives the successful output fence or remains inside the service call
despite completed GPU work. Host `core` and the strict Prospero build pass.
Revision `0f5ef44` contains the response telemetry and PID 179 evidence. Its
rebuilt ELF embeds `0f5ef44ea7`, has SHA-256
`73a69a5dd91237dbdd8cca6c4ed12a57c4cf5f49e9f27c58cc4ed2322ad4f64f`, and
is pinned for exactly one cleanup-first 30-second replay.

That replay ran as PID 182 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T183453Z-swapchain-run1.log`.
It closes the guest-visible NVDRV response boundary: `SubmitGPFIFOBase2`
returned success with fence `1:1` and cleared flags, the enclosing `Ioctl2`
copied back its 24-byte output, and the IPC response was ready before the GPU
thread completed the already-proven guest and host syncpoint increments. No
second BufferQueue commit followed. The run emitted no fatal diagnostic,
audio remained correctly paced, and cleanup proved PID 182 and global
`eboot.bin` absence twice each.

Flappy's source has no deliberate inter-frame delay: every
`appletMainLoop()` iteration draws and calls `SDL_RenderPresent`. The next
boundary is therefore a guest kernel wait or render-thread scheduling stall,
not NVDRV submission. After the first successful GPFIFO response, Prospero now
sparsely traces each `WaitSynchronization` entry and return with guest thread
ID, timeout, handle count, first four handles, result, and selected index. Host
`core` and the strict Prospero build pass. Revision `4b1b616` contains the
wait telemetry and PID 182 evidence. Its rebuilt ELF embeds `4b1b61673d`, has
SHA-256
`10e96740ff3dcafa8c819ecbe325e8ab3485ddde42d0697a181222621c2870e9`, and
is pinned for exactly one cleanup-first 30-second replay.

That replay ran as PID 185 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T184034Z-swapchain-run1.log`.
It rules out a post-submit `WaitSynchronization` stall on the render thread.
All observed waits belonged to guest thread 80, used the same handle
`0x1483d8` with an infinite timeout, and returned successfully about every
85 milliseconds. Their timing matches the healthy AudioOut release cadence;
no other guest thread entered this SVC after the GPU response. No second
BufferQueue commit followed. The run emitted no fatal diagnostic, audio
remained correctly paced, and cleanup proved PID 185 and global `eboot.bin`
absence twice each.

The next diagnostic moves to the central SVC dispatcher. It records the SVC
whose return first observes the completed GPFIFO response and up to 128
subsequent SVC entries/returns with guest thread ID and leading arguments.
This distinguishes a render thread entering another IPC/SVC from one remaining
entirely in guest code. The checked-in generated dispatcher and its generator
template carry the same instrumentation; no unrelated generated rewrite is
included. Host `core` and the strict Prospero build pass. Revision `20c63eb`
contains the SVC telemetry and PID 185 evidence. Its rebuilt ELF embeds
`20c63eb8e2`, has SHA-256
`4de8c78f5335069a39c5438f82f694539d2c912f670ab11fc8690966350edbe6`, and
is pinned for exactly one cleanup-first 30-second replay.

That replay ran as PID 188 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T184748Z-swapchain-run1.log`.
The first GPFIFO response returned through `SendSyncRequest` on guest thread
77. That same thread then continued through system-tick reads, IPC requests,
memory mappings, thread creation/start, core-mask setup, and a process-wide
condition-variable handshake that completed when the new thread signalled it.
It subsequently initialized AudioOut and continued issuing successful IPC
requests. This is active `SDL_HelperInit` work, not a render-thread deadlock.
The global 128-SVC budget was exhausted around 20 seconds after the audio
worker joined the trace, before initialization ended. No second BufferQueue
commit followed. The run emitted no fatal diagnostic, audio remained correctly
paced, and cleanup proved PID 188 and global `eboot.bin` absence twice each.

The next diagnostic binds its SVC budget to the submit-owning guest thread 77
and excludes the audio worker. It records up to 256 subsequent entries/returns,
which should preserve the render/initialization path through the full bounded
window and identify the last guest service boundary before timeout. The
checked-in dispatcher and generator template remain identical. Host `core` and
the strict Prospero build pass. Revision `7a3042e` contains the focused trace
and PID 188 evidence. Its rebuilt ELF embeds `7a3042e989`, has SHA-256
`3719f5906c46b5f6a833e1813475230947572297ef3081c8a0ec51e5bc9b2af7`, and
is pinned for exactly one cleanup-first 30-second replay.

That replay ran as PID 191 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T185206Z-swapchain-run1.log`.
The focused trace follows guest thread 77 throughout the bounded window. It
continued successful IPC, memory-management, synchronization, and thread
operations through 19.878 seconds, then spent about 7.2 seconds entirely in
guest code before resuming successful service calls at 27.092 seconds. It
mapped two additional `0xf0000` regions and returned to guest work at 27.206
seconds. No call remained blocked and no fatal diagnostic appeared. Audio
continued at the correct cadence, and cleanup proved PID 191 and global
`eboot.bin` absence twice each.

This timing matches Flappy's synchronous `SDL_HelperInit`: after creating the
accelerated renderer and opening SDL_mixer it rasterizes the same shared font
at four sizes before `SceneManager::Start` and the real applet render loop.
The first GPFIFO is therefore an initialization upload, not a frame-two fence,
and the 30-second host deadline currently truncates CPU-bound font startup.
Do not change GPFIFO, syncpoint, BufferQueue, or Vulkan behavior based on this
timeout. The next offline slice must profile or improve the guest/JIT font
rasterization path while retaining the pinned 30-second diagnostic contract;
if that cannot make startup fit, a separately justified bounded startup window
must be agreed before final 120-frame qualification.

The first offline startup optimization is revision `eabe4a7`. Prospero cannot
reserve the 39-bit contiguous fastmem or page-table arenas under the native-app
VM ceiling, so Dynarmic continues using the qualified sparse callback path.
Previously every ordinary scalar callback first walked the sparse page table
through `IsValidVirtualAddressRange` and then walked it again through
`Memory::Read*` or `Memory::Write*`. The PS5 non-debug callbacks now use the
status-returning block access to fuse validity, sparse translation, rasterizer
cache handling, and the scalar copy into one page-table walk. Little-endian
conversion remains explicit. Invalid accesses still set Dynarmic
`MemoryAbort`; debug/watchpoint accesses retain the original path; and writes
that cross a guest page retain range prevalidation so an invalid second page
cannot cause a partial write. `MemoryRead64` retains the focused TLS/audio
diagnostic only on the invalid path.

Host and Prospero `core` builds and the full Prospero `yuzu-cmd` link pass.
The five available relevant host tests (`dynarmic_tests`,
`eden.multi_level_page_table`, `eden.ps5_thread_budget`,
`eden_ps5.launch_config`, and `eden_ps5.shader_cache_identity`) pass. The
unrelated aggregate host target remains unavailable because its stale
Dynarmic test generator expects the removed `A64TestEnv::interrupts` member
and the tree lacks its generated FFmpeg archive; neither failure involves the
two changed A64 callback files. The rebuilt ELF embeds full revision
`eabe4a770f97bb6187636831cf93451817a1110f`, has SHA-256
`d7899430bb4d64b2d73029ad8ee558b45dd126b9739f2ad175c5bfeca93f42ef`,
and is not the banned fixed-address diagnostic. The cleanup-first Flappy
wrapper now pins these bytes and requires the exact core-zero marker
`sparse_callbacks=true single_lookup_scalars=true fastmem=false
address_space_bits=39` before accepting any runtime result. Hardware timing
and presentation evidence remain pending; this static optimization alone does
not advance the visible-frame or 120-frame gates.

The first cleanup-first hardware replay of those exact bytes ran as PID 194
and is preserved at
`examples/qualification-logs/flappy-bird/20260803T190905Z-swapchain-run1.log`.
All four cores emitted the required sparse/single-lookup marker. Relative to
PID 191, the first BufferQueue commit moved from 7.678 to 4.924 seconds, the
first GPFIFO submit from 17.544 to 8.934 seconds, and the first AudioOut append
from 19.107 to 9.785 seconds. This is a 36% improvement to the initial queue
and about a 49% improvement through submit/audio startup, confirming that the
double sparse translation was a real bottleneck. Guest thread 77 subsequently
completed more shared-font allocation/mapping work at 13.894, 18.560, 24.546,
24.723, 27.962, and 28.020 seconds. It remained active and every recorded SVC
returned successfully; audio retained its approximately 85 ms release cadence,
and no invalid memory access, fatal diagnostic, second BufferQueue commit, or
120-frame verdict occurred before the 30-second bound. Cleanup proved PID 194
and global exact `eboot.bin` absence twice each.

The result narrows rather than removes the startup bound. A one-time
cleanup-first 45-second replay is justified for these identical bytes: it is
only 15 seconds beyond the observed final active font mapping and remains far
below the rejected 150/300-second diagnostics. It must retain every existing
identity, failure, teardown, and exact-process gate. If it still does not enter
the render loop, return to offline profiling instead of extending the timeout
again.

That single 45-second replay ran as PID 197 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T191158Z-swapchain-run1.log`.
It reproduced the same startup boundary rather than reaching frame two: the
first BufferQueue commit occurred at 4.899 seconds, GPFIFO submission at 8.869
seconds, and AudioOut append at 9.722 seconds. Guest thread 77 remained active
through successful shared-font allocation/mapping work at 13.811, 13.857,
18.499, 24.437, 24.614, 27.855, 27.914, 34.604, 36.932, 36.958, and 39.903
seconds. Every recorded SVC returned, audio remained paced, and there was no
invalid access, fatal diagnostic, second BufferQueue commit, or 120-frame
verdict. The wrapper cleanup proved PID 197 and global exact `eboot.bin`
absence twice each. This exhausts the one-time extension: do not lengthen the
canary again; improve startup offline and retain the 30-second contract.

Revision `8055761` replaces the general `WalkBlock` scalar path with
width-specific checked memory operations. Same-page 8/16/32/64/128-bit
accesses now validate the complete range, perform one sparse entry lookup,
preserve rasterizer download/write coherency, and copy with explicit guest
little-endian conversion. Cross-page reads retain the safe block walker;
cross-page writes validate the complete range before changing memory, so an
invalid second page cannot leave a partial scalar. Debug/watchpoint accesses
retain their original path, while all non-debug Prospero failures still halt
Dynarmic with `MemoryAbort`. The public range validator now also rejects
overflow and out-of-address-space ranges before indexing the sparse table.

Host `core`, strict Prospero `yuzu-cmd`, and the five focused host tests
(`dynarmic_tests`, `eden.multi_level_page_table`, `eden.ps5_thread_budget`,
`eden_ps5.launch_config`, and `eden_ps5.shader_cache_identity`) pass. The
rebuilt ELF embeds exact revision
`805576154b8a530be9d927d8f5addf139fe932ef`, has SHA-256
`ed101d98e076a3b0fe2ec6e08943a2baf7bddaef33fbeb14af40c9c3366a78d8`,
and is distinct from the banned fixed-address diagnostic. The pinned Flappy
NRO remains SHA-256
`6e7cd9a1a22a0102a4f68ba6e434378c9b7381ce4f44a43ca376953f536aa54d`,
the launch sidecar remains
`27fe1881da2e24df050ce7a896676835d95f1d6be1e9f9b67bffc6d0f881757c`,
and the cleanup ELF, Vulkan-PS5 runner, process helper, PyPS4debug revision,
and lockfile remain pinned by the wrapper. Wrapper SHA-256 is
`09f2c9b9b74b35544c2a0738e6f8bc3e18ec04c4415cbb1685fbf47c7cbe0eb4`;
it requires the exact core-zero `checked_width_scalars=true` marker.

The cleanup-first 30-second replay of those bytes ran as PID 200 and is
preserved at
`examples/qualification-logs/flappy-bird/20260803T192714Z-swapchain-run1.log`.
All four cores emitted the sparse/single-lookup/checked-width marker. The first
BufferQueue commit occurred at 5.423 seconds, GPFIFO submission at 10.478
seconds, and AudioOut append at 11.476 seconds. Audio releases continued at
the expected roughly 85 ms cadence, and guest thread 77 completed further
font-region mappings at 16.499, 16.553, 22.240, and 22.472 seconds; every
recorded focused SVC returned. There was no checked-memory abort, fatal
diagnostic, second BufferQueue commit, or 120-frame verdict. Cleanup proved
PID 200 and global exact `eboot.bin` absence twice each.

The raw OpenAGC graphics-pipeline requests in this log are calibration and
native compositor activity and therefore do not prove a guest Maxwell
pipeline. Eden's current guest graphics/compute and transferable-record
counters are emitted only from `PipelineCache` destruction. Bounded cleanup
terminated PID 200 before that destructor ran, so this replay contains no
authoritative final guest-cache snapshot. Guest pipeline creation and
transferable record creation remain unproven, rather than zero.

Revision `f9b3704` closes that observability gap. The Prospero pipeline cache
now emits an authoritative zero baseline when it becomes ready, then logs
every guest graphics/compute pipeline creation and every transferable-record
write/skip transition immediately; the destructor uses the same snapshot
format when orderly teardown is reached. This is deliberately inside Eden's
guest Maxwell `PipelineCache`, not OpenAGC, so native calibration/compositor
pipeline requests cannot increment it. The Flappy wrapper now requires the
zero baseline, an actual guest pipeline-created transition, and a nonzero
record-written transition in addition to the existing 120-frame, identity,
failure, and cleanup gates. Host `video_core`, strict Prospero `yuzu-cmd`, the
wrapper syntax check, and all five focused tests pass.

The rebuilt ELF embeds exact revision
`f9b37041ed9296acfb0366e64bd6f1ef2d755282`, has SHA-256
`f866212fd9554e6d819cffd63c6e4807255cfe8dfdacab1bd9d387b0cc72954b`,
and remains distinct from the banned fixed-address diagnostic. The updated
wrapper has SHA-256
`49de418942663da73931adfdfe701050072ccf991d367a08cd7536e68c6ea899`;
all other pinned NRO, sidecar, cleanup, runner, process-helper, PyPS4debug, and
lockfile identities remain unchanged.

The cleanup-first 30-second replay ran as PID 203 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T193417Z-swapchain-run1.log`.
It emitted exactly one guest-cache snapshot: the authoritative baseline at
1.410 seconds with `graphics_created=0 compute_created=0 records_written=0
records_skipped=0`. No creation or record transition followed. Because every
counter mutation now emits a live transition, this proves that Flappy had
created no guest Maxwell graphics/compute pipeline and written no transferable
record before the bound; raw OpenAGC pipeline requests in the same log are
native activity only. The first BufferQueue commit occurred at 5.400 seconds,
GPFIFO submission at 10.465 seconds, and fail-soft AudioOut append at 11.461
seconds. Guest thread 77 remained active through successful font mappings at
16.506, 16.562, 22.332, and 22.568 seconds, with every focused SVC returning.
There was one committed buffer, no checked-memory abort or fatal diagnostic,
no second commit, and no 120-frame verdict. The stricter cache patterns
therefore failed exactly as intended. Pinned cleanup then proved PID 203 and
global exact `eboot.bin` absence twice each. The direct `/dev/gc` boot-cycle
invariant was preserved throughout.

Revisions `a8845f8`, `4f488a8`, and `4a399be` accelerate this measured CPU
startup bottleneck without weakening checked guest-memory semantics. The
Prospero scalar checked path now has a per-thread, direct-mapped cache for
ordinary `PageType::Memory` translations. It never caches debug or
rasterizer-coherent pages, and cross-page operations retain their checked slow
path. A globally unique atomic odd/even translation epoch identifies each
mapping generation; process page-table changes, mappings, debug marking, and
rasterizer-cache marking invalidate entries around the mutation so stale host
pointers cannot be used. The initial 256-entry cache covered the observed font
working set at 8 KiB per participating thread; its final compact layout is
recorded below. The Dynarmic marker now
requires `scalar_page_cache=true`. Host core, strict Prospero `yuzu-cmd`, and
the five focused tests (`dynarmic_tests`, `eden.multi_level_page_table`,
`eden.ps5_thread_budget`, `eden_ps5.launch_config`, and
`eden_ps5.shader_cache_identity`) pass for this implementation.

Three cleanup-first, direct-`/dev/gc`, 30-second FW 5.50 measurements isolate
the cache-size effect. PID 206 with 8 entries reached the `0x144000` font
mapping at 27.627 seconds. PID 209 with 64 entries reached it at 27.268 seconds
and the first final `0x90000` mapping at 29.017 seconds. PID 212 with 256
entries reached `0x144000` at 26.965 seconds and two `0x90000` mappings at
28.710 and 28.729 seconds. Their accepted logs are respectively
`examples/qualification-logs/flappy-bird/20260803T194302Z-swapchain-run1.log`,
`20260803T194607Z-swapchain-run1.log`, and
`20260803T194904Z-swapchain-run1.log`. The older PID 197 45-second reference
did not reach the same `0x144000` and first two `0x90000` milestones until
34.604, 36.932, and 36.958 seconds, so the checked scalar cache provides a
material startup speedup rather than merely changing diagnostics.

PID 212 is the current authoritative run. It committed the first BufferQueue
buffer at 4.402 seconds, submitted the first GPFIFO at 7.315 seconds, and
started fail-soft AudioOut at 8.029 seconds; releases continued without a
fatal error. Every recorded guest SVC returned through the final observation
at 28.729 seconds. It emitted exactly the authoritative zero guest-cache
baseline at 1.403 seconds and no graphics-created, compute-created, or
record-written transition. It therefore still produced no guest Maxwell
pipeline or transferable shader record before the bound, no second
BufferQueue commit, and no 120-frame verdict. There was no invalid checked
access, memory/JIT failure, or fatal diagnostic. Pinned cleanup again proved
PID 212 and global exact `eboot.bin` absence twice each before returning.

The current ELF embeds exact revision
`4a399be8b34996ed3d2d1cc83e3b053272607dee`, has SHA-256
`89bbf0d6af026a40f1c73808c3013f0ad56e968cb6f3f51a69bba654a825d7e4`,
and remains distinct from the banned fixed-address diagnostic. The wrapper
pins that identity and has SHA-256
`57762768a7a84efc7c4ef00a133a3d56e76b040da041d06816a22c8ac8625c45`;
it also defines the live-telemetry selector used after a fully successful run,
avoiding an unbound-variable failure under `set -u`. The NRO and sidecar remain
SHA-256 `6e7cd9a1a22a0102a4f68ba6e434378c9b7381ce4f44a43ca376953f536aa54d`
and `27fe1881da2e24df050ce7a896676835d95f1d6be1e9f9b67bffc6d0f881757c`;
the cleanup ELF, guarded runner, exact-process helper, PyPS4debug revision, and
lockfile remain pinned in the wrapper.

The matching Flappy source explains the remaining deterministic work:
`SDL_HelperInit` eagerly creates four SDL_FontCache instances at point sizes
25, 30, 20, and 50, and `SDL_FontCache.c` rasterizes standard ASCII plus the
Nintendo-symbol suffix one glyph at a time with `TTF_RenderUTF8_Blended`
before gameplay begins. The pinned NRO and 30-second gate will not be changed.
The next slice must reduce the emulator's checked-memory overhead enough to
finish that guest workload, create a real guest pipeline/record, and present
repeated frames inside the existing bound; further timeout or blind cache-size
increases are not qualification evidence.

Revision `169b72a` removes two remaining steady-state costs without relaxing
that contract. It replaces each entry's separate `Memory::Impl` identity and
generation with one globally unique even translation epoch (odd while a
mutation is active), and caches the current address-space end rather than
recomputing `1 << address_space_bits` for every scalar access. Revisions
`78efc23` then move the common epoch into one thread-local cache header and
clear exact guest-page tags only when that rare epoch changes. A steady cache
entry is now exactly the guest-page tag and host-page pointer, so all 256
entries occupy 4 KiB per thread instead of 8 KiB. Debug, rasterizer-coherent,
cross-page, unmapped, and mutation paths remain fail-closed slow paths.

Two byte-identical cleanup-first runs of the `169b72a` ELF prove the speedup is
reproducible. PID 215 in
`examples/qualification-logs/flappy-bird/20260803T200153Z-swapchain-run1.log`
and PID 218 in `20260803T200305Z-swapchain-run1.log` reached the third
`0x90000` allocation at 21.734 and 22.242 seconds, then reached two subsequent
`0x3c0000` allocations by 25.003 and 25.621 seconds. The older PID 212 did not
finish the third `0x90000` allocation inside 30 seconds. The final 4 KiB layout
ran as PID 221 in `20260803T200922Z-swapchain-run1.log`; it reached the same
milestones at 22.184, 25.397, and 25.464 seconds, preserving the gain without a
material additional speedup. All three runs committed one BufferQueue frame,
submitted GPFIFO, kept fail-soft AudioOut active, returned every recorded SVC,
and contained no invalid checked access, JIT/memory failure, or fatal
diagnostic. Each emitted only the authoritative zero guest-cache baseline:
there was still no guest graphics/compute creation, transferable record,
second commit, or 120-frame verdict. Pinned cleanup proved the PID-specific
and global exact `eboot.bin` absence twice after every run.

The current ELF embeds exact revision
`78efc23c4968b1b18debf6e74d56c5c5432c1f0b`, has SHA-256
`973f161105ab009259340ce75051516c655383e89ca42917ab63a935984cd76f`,
and remains distinct from the banned diagnostic. The Flappy wrapper pins those
bytes and has SHA-256
`09333074292af160033efe72ccdc3b91fd6a1651acda63ba11b1cb83b55c7a49`.
Host core and strict Prospero `yuzu-cmd` builds pass, as do the same five
focused tests. A broader pre-existing host build target remains independently
broken because Dynarmic's generated `test_reader`/`test_generator` expect an
`A64TestEnv::interrupts` member absent from their checked-in test environment;
the built `dynarmic_tests` target itself passes.

The user-selected InvadersNX diagnostic was also repinned and executed rather
than assumed to be a better canary. Its current NRO remains SHA-256
`4ad1a05d7e7edba203d086151bf83d2be02bf2ead8695ce4d21f21b4bdf27433`;
the sidecar remains
`182bbf2e1cf750ffe5b068bc60473eb44ba47e800704a761a5dfe8db122a06e2`,
and the updated wrapper is
`e8d58a6e2a839ce91a3e7bfe82a74fe114ef2135fd0fbcdd11b831a440943033`.
The cleanup-first PID 224 run is preserved at
`examples/qualification-logs/20260803T201149Z-swapchain-run1.log`. The guest
called `ExitProcess` normally at 3.205 seconds before creating any display
buffer, GPFIFO submission, guest pipeline, or transferable record; orderly
destruction confirmed all four guest counters remained zero and the frame
oracle rejected `expected=600 actual=0`. Cleanup then proved PID and global
absence twice. Thus the existing InvadersNX NRO is not presently a substitute
for Flappy: it exits earlier and provides less renderer coverage. Its source
initializes SDL video, timer, and audio in one fatal `SDL_Init` call before
window creation, making fail-soft audio initialization the leading early-exit
hypothesis; a rebuilt diagnostic NRO would need to separate optional audio
from required video/timer initialization and receive a new pinned identity.

Revision `433cd2b71070d5dc399e63a598a5fc0aadf2e04f` removes another
Prospero-only checked-memory cost found by inspecting the generated x86-64
object code. The function-local `thread_local` scalar cache was implemented by
the Prospero toolchain through `__emutls_get_address`, so every checked scalar
read or write paid an emulated-TLS resolver call before examining its cache
entry. The cache now lives in `Memory::Impl` as four core-indexed 256-entry
banks, and each `DynarmicCallbacks64` supplies its immutable CPU core index.
The global odd/even translation epoch still invalidates every bank lazily;
cross-page, special, unmapped, debugger, rasterizer-coherent, and mutation
paths remain fail-closed. The runtime identity marker now includes
`scalar_cache_storage=core-indexed`. The Prospero object retains
`GetCachedNormalScalarPointer` but contains no `__emutls` symbol. Strict host
core and Prospero `yuzu-cmd` builds pass, as do `dynarmic_tests`,
`eden.multi_level_page_table`, `eden.ps5_thread_budget`,
`eden_ps5.launch_config`, and `eden_ps5.shader_cache_identity`.

The matching cleanup-first FW 5.50 run was PID 227 and is preserved at
`examples/qualification-logs/flappy-bird/20260803T202316Z-swapchain-run1.log`
(SHA-256
`500907061bdc37e982511e8ecdcffac6b11eabe36c1bcff781e2411d97608553`).
It committed its first BufferQueue frame at 3.899 seconds, submitted its first
GPFIFO at 5.706 seconds, kept fail-soft AudioOut active, and returned every
recorded SVC. It reached the third `0x90000` mapping at 21.667 seconds and the
two following `0x3c0000` mappings at 24.847 and 24.912 seconds. Against PID
221's 22.184, 25.397, and 25.464 seconds this is an approximately 0.52--0.55
second improvement in the late font workload, but the initial frame and
GPFIFO timings are effectively unchanged. The result is useful but not the
breakthrough needed for the active gate: only the zero guest-cache baseline
appeared, with no graphics/compute creation, transferable shader record,
second BufferQueue commit, or 120-frame verdict before the unchanged
30-second bound. There was no invalid checked access, JIT/memory failure, or
fatal diagnostic. Cleanup proved PID 227 absence twice and global exact
`eboot.bin` absence twice.

The deployed ELF is SHA-256
`b981af2919283e22e7c35afc4277b38d3ffddf4c5795a75d3040efdee95e19c4`,
embeds `433cd2b710`, and is distinct from the banned fixed-address diagnostic.
The repinned Flappy wrapper is SHA-256
`118d0026855d32749b725f3f2554da571f6bac184b72373ec0d976b64d9f8cb3`
and was committed at `8a2e0bb4aaf27f37da6518ed4b670cf4265b2a11` before the run.
Flappy remains the primary renderer canary because it reaches BufferQueue,
native presentation, GPFIFO, and live audio; the current InvadersNX binary
still exits before all of those. Switching binaries now would reduce evidence
unless InvadersNX is first rebuilt with fail-soft optional audio.

Revision `799a596795051d3b50567ec009159e7c5d0bf485` then tested the
remaining out-of-line cache-hit call directly. The common hit check is
force-inlined into each checked width, while epoch reset and page-table miss
resolution remain in one no-inline cold helper. The generated Prospero
`ReadChecked<u8>` body fell from 360 to 308 bytes; its hit path contains no
cache-helper or assertion call, and only an invalid address, cache miss, or
epoch change can call a slow path. The runtime marker adds
`scalar_cache_hit_inline=true`. Host core and Prospero `yuzu-cmd` builds and
the same five focused tests pass.

The cleanup-first PID 230 measurement is preserved at
`examples/qualification-logs/flappy-bird/20260803T203033Z-swapchain-run1.log`
(SHA-256
`ae3bc4dcb807b5f7509ef85206b2423b70e5d27835acd9ed2cb71cb8fc36a582`).
It reached the third `0x90000` mapping at 22.021 seconds and the following
`0x3c0000` mappings at 25.254 and 25.322 seconds. That is approximately
0.35--0.41 seconds slower than PID 227 and close to PID 221, so eliminating
the helper call is currently a neutral hardware result, not a demonstrated
speedup. It again committed one BufferQueue frame, submitted one GPFIFO,
maintained fail-soft AudioOut, returned every recorded SVC, and emitted only
the zero guest-cache baseline. No guest pipeline, transferable record, second
commit, or 120-frame verdict appeared. No invalid checked access, JIT/memory
failure, or fatal diagnostic appeared, and cleanup proved PID-specific and
global exact-process absence twice each.

The PID 230 ELF is SHA-256
`f3bff16b9977820d5391c59088751f7a5c829e451f511102aa79ccd5cbcc0946`
and embeds `799a596795`. Its repinned wrapper is SHA-256
`9d5a624cc0e096b0b2fcfa0e32ee2dd466f990ca749c7b7e6112c2765ae4bcb6`
and was committed at `db9ce2d182d748bf3b026250f821267377b6b9bd` before launch.
The next optimization must target a larger callback-memory cost or avoid this
font-rasterization bottleneck without changing the NRO or 30-second gate;
another micro-change or an Invaders binary that exits before presentation is
not a justified hardware run.

Revision `ff67cb0ed779e7ae55f31742b48209cdd0473d3b` replaces normal
Prospero scalar-memory callbacks with a generated two-level sparse-page-table
walk. The stable 65,536-entry root selects lazily allocated 2,048-entry leaves;
normal memory entries resolve directly in generated x64 code, while absent
roots, null or tagged special entries, and cross-page accesses retain the
checked callback and MemoryAbort fallback. The same slice fixes macOS
single-architecture source wrapping so the x64 backend is actually compiled,
and disambiguates Xbyak displacement operands exposed by that build. An x64
standalone runtime smoke test under Rosetta proves mapped load/store callback
bypass plus null-root, special-entry, and boundary fallback. The native
Dynarmic suite passes 201,927 assertions in 130 cases, the four focused Eden
tests pass, and the strict Prospero `yuzu-cmd` build passes.

The post-commit ELF is SHA-256
`d508d647614ea95a8783ea08afc0362a3d130018c87de0bd27cbbe95477e08e9`,
embeds `ff67cb0ed7`, and is distinct from the banned fixed-address diagnostic.
The bounded required-marker adjustment is committed at `e4b6658`; its wrapper
is SHA-256
`6c7042beb355d8617fdae597bf898ed7d62979b3ad4ac2cabf361241dff542d8`.
The cleanup-first PID 233 run is preserved at
`examples/qualification-logs/flappy-bird/20260803T205619Z-swapchain-run1.log`
(SHA-256
`8fe71b3bbe28ee645518cb5db7389052e98b05d39932839ba0a3ae0bc86c2410`).
It moved the first BufferQueue commit to 3.169 seconds and first GPFIFO to
3.330 seconds. The three `0x90000` allocations moved from PID 230's roughly
22-second boundary to 10.492, 10.493, and 11.194 seconds; the following
`0x3c0000` allocations moved from roughly 25.3 seconds to 12.591 and 12.593
seconds. It reached a second GPFIFO at 22.481 seconds, compiled guest graphics
pipelines, and recorded two native draws. This is a material CPU/JIT progress
result, not a presentation pass.

The next fail-closed owner is now concrete: after the two draws,
`vkEndCommandBuffer` rejected `record_error=-8`
(`VK_ERROR_FEATURE_NOT_PRESENT`) with a complete native stream, then Eden
terminated through its Vulkan exception path. The last successful command
marker was `vkCmdPipelineBarrier-complete`; cleanup proved PID-specific and
global exact `eboot.bin` absence twice. Instrument and qualify the immediately
following Vulkan command before another canary. Do not switch to the current
InvadersNX binary: it still performs fatal combined
`SDL_Init(VIDEO|TIMER|AUDIO)` and its pinned run exits before display/GPU work.
Invaders becomes a useful secondary A/B only after rebuilding it with mandatory
video/timer and optional fail-soft audio; Flappy remains primary because it now
reaches the actionable Vulkan/OpenAGC command-recording boundary.

PID 137 completed the 30-second observation without the low read, allocator
assertion, Xbyak exception, or another fatal error. It created multiple guest
graphics pipelines, sampled two opaque-black raw guest frames, submitted the
calibration composite, and completed one native present. The accepted bounded
log is
`examples/qualification-logs/flappy-bird/20260803T165853Z-swapchain-run1.log`.
The broad six-register guard reduced guest progress to 7.79 seconds, however,
so this run did not cross the old 14.58-second audio-fault boundary and cannot
yet prove the register loss fixed. It also ended by bounded cleanup rather than
the 120-frame oracle, so final cache telemetry and orderly teardown remain
unproven. Both PID-specific and global exact-process absence passed twice.

Revision `8b77584` narrows the translated-block guard to `RBX` and `R12`, the
only SysV callee-save GPRs that this register allocator can assign to live IR
values; `RBP` and `R13`-`R15` are excluded by allocation policy or reserved.
Two pushes and two pops retain call-stack alignment while avoiding the larger
generic ABI frame. Host `dynarmic` and `core` targets and the strict integrated
Prospero build pass. The rebuilt ELF embeds `8b77584` and has SHA-256
`1ada1f47aea2a2ca7421c0757585a63f9bd0a3330581cb336a10b9b42400c33b`.
It is pinned for the next cleanup-first 30-second A/B.

PID 179 again reached two draws and failed with `record_error=-8` at the
barrier entry, while none of the new query-copy, fill, reset, begin, or end
labels appeared. This proves those commands were not reached and returns the
active blocker to the barrier itself. The remaining unlabelled barrier paths
are OpenAGC buffer-range and image-subresource state queries. Vulkan-PS5 now
prints the exact native result and resource range for both. The accepted log is
`examples/qualification-logs/flappy-bird/20260803T151849Z-swapchain-run1.log`;
all four exact-process absence checks passed after PID 179 exited.

The barrier-state diagnostic ELF embeds Eden revision
`199c9736c9da902629446a89d649dacc1b43da17`, includes Vulkan-PS5
`641dc9d`, and has SHA-256
`af0101f437f31172240cf65cff66ef70424c2a9620ca6b96b884ec096d8edc19`.
The wrapper pins these bytes for one cleanup-first 110-second replay.

PID 92 proved `vkCmdPipelineBarrier` completed and then ruled out regular
indirect draw: the final label was `vkCmdPipelineBarrier-complete`, with no
indirect rejection. A subsequent render-pass begin can set the same `-8`
without activating the pass, matching final `render_pass=0`. Vulkan-PS5 now
labels `vkCmdBeginRenderPass` and prints exact color/depth attachment, usage,
layer, native-object, and layout failures. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T153315Z-swapchain-run1.log`;
cleanup again passed two PID and two global exact-absence checks.

The render-pass-begin diagnostic ELF embeds Eden revision
`460bf5a25d2f45e8b8f534209b2f14dea84fc90f`, includes Vulkan-PS5
`62d9108`, and has SHA-256
`2defe2e06a4a314e24fce44353278543053a2c55f19e44c452f046c7703447ad`.
The wrapper pins these bytes for the next cleanup-first 110-second replay.

The replay after console recovery ran as PID 89. Neither timestamp nor
indirect-count diagnostics fired; it again reached two draws and exited with
`-8`. Regular direct/indexed indirect draws remain an unlabelled validation
path capable of producing that exact result before incrementing the draw
count. Vulkan-PS5 now logs their complete contract and marks successful
barrier completion separately. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T152855Z-swapchain-run1.log`;
cleanup passed both PID and global absence checks twice.

The regular-indirect diagnostic ELF embeds Eden revision
`d42c1250255c9750b92796c8bc0e1d89e04cdc2d`, includes Vulkan-PS5
`77adbce`, and has SHA-256
`bd1d24044d5c393a0c2ce6aebf910bbc873948456d46f538c5f964cecdada90b`.
The wrapper pins these bytes for one cleanup-first 110-second replay.

PID 182 produced no buffer-range or image-subresource state-query failure, so
the barrier completed and a later unlabelled command latched `-8`. Eden has no
timestamp call in this path but does have a Maxwell indirect-count draw path.
Vulkan-PS5 now labels timestamp, indexed draw, and both indirect-count commands
and prints unsupported-call arguments. The accepted log is
`examples/qualification-logs/flappy-bird/20260803T152248Z-swapchain-run1.log`;
PID 182 and global exact-process absence each passed twice after cleanup.

The post-barrier entrypoint diagnostic ELF embeds Eden revision
`784b993457cb12b83a5798c0cebd70b539c37cb2`, includes Vulkan-PS5
`95a43c0`, and has SHA-256
`1d10edced7e3eca95631b0ec291338333e289e65f43f502d33a8140c70a20b63`.
The wrapper pins these bytes for one cleanup-first 110-second replay.

The clipped-scissor retry, PID 164, passed viewport/scissor resolution and
reached the next draw-preparation boundary at about 98 seconds. The guest draw
reported `descriptors=0 vertex_buffers=0`; descriptor preparation had returned
success without marking the bindings ready, so vertex-buffer preparation was
not attempted. The operator still saw magenta. Two sampled raw guest frames
remained opaque black, while the calibration composite submitted and one
native present returned. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T144235Z-swapchain-run1.log`.
PID 164 exited, and the wrapper again passed both PID-scoped and global exact
`eboot.bin` absence checks twice. The crash occurred before orderly teardown,
so final pipeline/cache telemetry and the 120-frame oracle remain unproven.

The descriptor failure exposed a state-contract asymmetry. Graphics image
descriptors and vertex buffers prepare their exact native usage at the command
that consumes them, but graphics buffer descriptors merely returned not-ready
when a preceding global barrier correctly left their typed state unchanged.
Vulkan-PS5 commit `51a263a` now derives ShaderRead or ShaderWrite from reflected
descriptor access and prepares the exact bound buffer range before binding.
The regression forces a storage descriptor through CopyDestination before its
draw and proves use-site recovery. Command-recording, lifecycle, validation,
and the Prospero static build pass.

The integrated descriptor-preparation retry ELF embeds Eden revision
`8eb7a2b3a368c7d0e369d55d7e18971b8a11ee03`, has SHA-256
`8c50d1654ab7cbc10a3e1c9f400e09afd23520528ca8d11d33b48803e5678fdc`,
and is pinned by the Flappy wrapper for the next cleanup-first 110-second
diagnostic.

The zero-stride replay, PID 148, closes the pipeline-creation blocker: all
guest pipelines compile/create, two opaque-black raw guest frames are observed,
the first composite submits, and one native present returns successfully. The
operator still saw the magenta calibration image, and the run later failed a
different `vkCmdPipelineBarrier` with record error `-8` at about 98 seconds.
The process was cleaned and all four exact-absence checks passed. The accepted
failure log is
`examples/qualification-logs/flappy-bird/20260803T141015Z-swapchain-run1.log`.
Vulkan-PS5 commit `8836ed9` now reports whether the failing barrier is an
unsupported memory access pair, buffer access/queue/range contract, image
access/layout/queue/subresource contract, or a request for a native stream
after an earlier unimplemented command. The next retry must use that diagnostic
to solve the barrier; magenta remains a separate visible-output failure until
a non-calibration swapchain readback is proven.
The barrier-diagnostic ELF embeds revision
`1cb7d006e505e1fb1e4cff38610639e1061fd1a0`, has SHA-256
`a4594d1b64c1ee419dc8bb873a69141ffac66965c93497bde5cd79b6b5cbf8fd`,
and is pinned by the wrapper.

The barrier-diagnostic replay, PID 151, identified the exact failure: a
format-51 color image transitions from `VK_IMAGE_LAYOUT_UNDEFINED` to
`VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` with broad source access `0x540` and
destination `VK_ACCESS_TRANSFER_WRITE_BIT`. Vulkan discards prior contents for
an UNDEFINED old layout, so its source access scope is irrelevant; Vulkan-PS5
incorrectly rejected the mixed source roles before applying that rule. Cleanup
and all four exact-absence checks passed. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T141446Z-swapchain-run1.log`.
Vulkan-PS5 commit `5e7aa1d` now maps every UNDEFINED old-layout source directly
to native Undefined while continuing to validate the destination usage, with
an exact broad-source regression. Host command recording and the Prospero
static library pass. The visible magenta blocker remains open pending a rebuilt
cleanup-first replay.
The rebuilt ELF embeds revision
`82bb851d0bcd59278519e021f226e78ba399ef65`, has SHA-256
`9455859598f338c386ea0eae50b1d55a664b8d2e4c962a5b3aef9d222a1363d5`,
and is pinned by the Flappy wrapper.

The undefined-source retry, PID 154, passed that first barrier, submitted one
native present, and still displayed only the magenta calibration image. At
about 98 seconds it reached the next exact boundary: format 51
(`VK_FORMAT_A8B8G8R8_UNORM_PACK32`) transitions from
`VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` to `VK_IMAGE_LAYOUT_GENERAL`, with
`VK_ACCESS_TRANSFER_WRITE_BIT` as the source and conservative destination
access `0x7e0` spanning shader, color-attachment, and depth/stencil roles.
Core Vulkan permits a multi-role access scope; Vulkan-PS5 incorrectly required
one role before considering the image's color aspect and declared usage. The
accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T141911Z-swapchain-run1.log`.
The wrapper retired PID 154 and passed both PID-scoped and global exact-absence
checks twice.

Vulkan-PS5 commit `5f56e1d` resolves a multi-role GENERAL scope against the
concrete image aspect and usage. For this color attachment it emits a real
native `CopyDestination` to `ColorTarget` transition, preserving the transfer
write dependency rather than treating the scope as undefined. The exact Eden
barrier regression, command-recording, lifecycle, and validation host tests,
and the Prospero static-library build pass. A rebuilt cleanup-first Flappy
retry is next; visible output remains unproven.

The integrated retry ELF embeds Eden revision
`52bb02c80d7e6487ea67c14dd623984c5d9de743`, has SHA-256
`160359f69b65933c47cc8dbc02987189413a8009dd7859d5d414eb3a8840b27f`,
and is pinned by the Flappy wrapper.

Format value 122 is `VK_FORMAT_B10G11R11_UFLOAT_PACK32`. The same run shows
Eden's broad optimal-tiling feature probe (`0xc083`) rejecting it and both
configured alternatives because Vulkan-PS5 currently advertises sampled,
color-attachment, transfer, and blit support but not storage-image support for
those formats. This is a separate format-capability qualification item, not
the format-51 barrier failure above; storage must be hardware-qualified before
it is advertised.

The broad-GENERAL retry, PID 158, passed the format-51 image transition and
advanced to a global `VkMemoryBarrier` at about 97 seconds. The exact scope is
source `VK_ACCESS_MEMORY_WRITE_BIT` (`0x10000`) to destination
`VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT` (`0x1800`). A
global Vulkan barrier neither names a resource nor changes its typed usage;
Vulkan-PS5 incorrectly tried to reduce both access masks to single resource
states and rejected the two valid transfer directions. The operator-visible
result remained magenta. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T142729Z-swapchain-run1.log`.
PID 158 exited, and the wrapper passed both PID-scoped and global exact-absence
checks twice.

OpenAGC commit `a79d7ca` adds the public `agcCmdMemoryBarrier` path. It flushes
color/depth metadata, performs an EOP data/cache release, then acquires all GPU
caches without changing tracked per-resource state; non-graphics queues remain
fail-closed. Vulkan-PS5 commit `f9112cd` validates global access masks and emits
one such dependency for the Vulkan barrier instead of inventing resource-state
transitions. OpenAGC's 20,110 direct assertions and native API reference check,
the exact Vulkan `0x10000 -> 0x1800` regression, neighboring lifecycle and
validation tests, and the Prospero Vulkan/OpenAGC static build pass. Rebuild,
hash pinning, and a cleanup-first replay are next.

The integrated global-barrier retry ELF embeds Eden revision
`2b021dc1356696410a7e4986651dd208e430767c`, has SHA-256
`dafec585e364ee79b025bedc06873d8ab15fd8ca4f12c438568aa774813fcdb2`,
and is pinned by the Flappy wrapper.

The diagnostic replay, PID 142, again cleaned up with two PID-scoped and two
global absence checks. Its request fingerprint rules out alpha-to-coverage,
MSAA, depth/stencil tests, and unknown dynamic enums. Each rejected guest
pipeline uses topology 4, the exact nine core dynamic states `0..8`, one
sample, one blend attachment, and `depthClampEnable=1`; the successfully
created presentation/meta pipelines use only viewport/scissor and leave depth
clamp disabled. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T140203Z-swapchain-run1.log`.
Because the request dump occurs before several later validation branches,
depth clamp is a correlation rather than a proven cause. Vulkan-PS5 commit
`8265c13` now labels each remaining post-request feature rejection so the next
replay can identify the precise validator without inference.
The reason-labelled ELF embeds revision
`3c4e16bf31de839579773fcafb70004ed3df4d82`, has SHA-256
`0668814c2b46d33d72b06e76a1a476bf30ed52405ce4e0be1ea9305c206a7a8d`,
and is pinned by the wrapper.

The reason-labelled replay, PID 145, identified the exact remaining validator:
`vertex binding index, rate, stride, or duplicate`. The three failing Eden
pipelines use zero-stride bindings, which core Vulkan permits to fetch the same
element for every vertex. The other binding constraints were unchanged. The
runner cleaned the process and passed both PID-scoped and global absence checks
twice. The accepted failure log is
`examples/qualification-logs/flappy-bird/20260803T140637Z-swapchain-run1.log`.
OpenAGC commit `118841f` now binds zero-stride reflected vertex inputs using a
structured descriptor with zero hardware stride and a nonzero range-derived
record bound. Vulkan-PS5 commit `6a93398` removes its non-Vulkan zero-stride
pipeline rejection. Focused OpenAGC draw/binding and Vulkan pipeline tests pass,
and both Prospero libraries cross-build. The next cleanup-first Flappy retry
must use a rebuilt ELF containing these commits.
That rebuilt ELF embeds revision
`33b3ec35fc47935bb18407ef4ea77e534117a07f`, has SHA-256
`0c34a6cbc8f981ae440d5fa27f579e7be57c39cd5354f69b9bbcfa7b1945c451`,
and is pinned by the Flappy wrapper.

### Payload SDK identity

The current `build-prospero-full-audit2` CMake cache resolves both
`CMAKE_TOOLCHAIN_FILE` and `PS5_PAYLOAD_SDK` through the installed prefix
`/Users/bizkut/ps5-payload-sdk`. That directory is an installed SDK tree, not a
Git checkout or symlink. The pre-hardening artifacts linked into ELF
`18295a780e72d724c4f2eeb4bcf4a868c4ba2fe3c122b7de0dab43b922251f22` were
`target/lib/libc.a` SHA-256
`dc258aae03c1a8c8c7725cfee413c205c7254668966c46eb0b8fc26b289c02c6`
and `target/lib/crt1.o` SHA-256
`04ec94435cdf2dd70e36b61fd0ccd949927c96149d78dfacbd132c1e8c7237d4`.
Those bytes, rather than an assumed source checkout, identify its SDK.

The RW-to-RX hardening source is
`/Users/bizkut/Downloads/PS5/homebrew/ps5debug-NG/ps5-payload-sdk`, owned by
the `ps5debug-NG` repository. Commit `439746c` first serialized each complete
shared kernel-copy pipe transaction and each full VM-entry protection walk.
Commit `5342f3c` adds the process-wide, bootstrap-safe VM-operation lock,
locked libc `mmap`/`mprotect`/`munmap`, locked private runtime-loader and
resolver mappings, and a JIT-only exact-entry protection helper. Executable
`mmap` promotes the exact 16 KiB-rounded entry while the operation lock is
held; generic loader segment protection remains multi-entry-capable. Weak CRT
symbols retain the former shared-object fallback instead of becoming null
calls. Its existing unrelated debugger
changes and untracked `sce_stubs/libSceAgcDriver.c` are outside this work and
remain untouched. The committed sources were rebuilt and only their CRT/libc
and public-header targets installed into `/Users/bizkut/ps5-payload-sdk`. The
active installed artifacts are `target/lib/libc.a` SHA-256
`16ddcdb481c8372fd464075c6a9e7646da267c0a458c4f8e13790dc2da6514b8`,
`target/lib/crt1.o` SHA-256
`9f02746532314b7971e19ebdd3d73ef601d5130b6648147305641003b759adca`,
and `target/include/ps5/kernel.h` SHA-256
`61f008b9b1d3890b399ed9347bd9a4d3e67e05a52636cd5ca5238189d3785d8b`.

Solve the intermittent Dynarmic RW-to-RX `mprotect` `EPERM` without weakening
the Prospero W^X or fail-closed contracts. The active implementation must use
OS-chosen virtual addresses, keep write and execute permissions mutually
exclusive, never execute after an inconclusive or failed transition, and
release every mapping on bounded teardown. In-process retry after a failed
transition is not accepted because the mapping's partial state is then
unknown. The preserved error from the production failure is `EFAULT`; older
artifacts flattened that helper failure to `EPERM`.

The immediate evidence is the cleanup-first InvadersNX run
`Vulkan-PS5/examples/qualification-logs/20260803T022033Z-swapchain-run1.log`.
It allocated and demoted the same four 32 MiB caches and addresses used by
successful 2048 runs, then the first cache's first full-entry promotion failed
at `base=0x303200000 size=0x2004000 errno=1` before guest JIT execution. Eden
terminated fail-closed; the cleanup trap ran and both PID-scoped and global
checks found no exact `eboot.bin`. This is neither an InvadersNX failure nor a
qualification pass.

The correction is cooperative process-map serialization, not a cached kernel
`vm_map_entry *`. Cached entries were rejected because split/merge/delete and
teardown can free or replace them before validation. The SDK always performs a
fresh lookup; Dynarmic's RW-to-RX path then requires one exact entry whose
start/end match the complete owned mapping and fails closed otherwise. Regular
SDK VM syscalls, Eden's direct SCE mappings, and OpenAGC's production direct
SCE mappings all use the same lock with the fixed order VM-operation lock then
kernel-copy pipe lock. OpenAGC commit `47ca983` contains its scoped wrappers.
The lock protects participating Eden/OpenAGC/SDK paths; it is not claimed to
replace the unavailable kernel `vm_map` lock against unknown nonparticipating
mutators.

Eden commit `22d06c5` adds the direct-memory guards, uses the exact-entry helper
for Dynarmic promotions, and adds the concurrent probe. The committed-source
rebuilds are full Eden ELF SHA-256
`32838191b2c611ae7a46318720bae9895f3e45f1fc730ef95a84470830ede1d0`
and GPU-free probe SHA-256
`20bbb80118fa6e4817acc170b1e272c84a7093d8b0d5ef031266b5933a08f1ea`.
The full ELF embeds `22d06c55fd-master`, defines the VM lock, exact helpers,
and libc `munmap`, and has no unresolved ordinary `mmap`/`mprotect`/`munmap`.
These are pinned build artifacts, not target evidence.

The GPU-free probe adds a second thread with exactly 128 bounded anonymous
map/write/unmap cycles while the four Dynarmic-sized mappings execute their W^X
cycles. It uses no GPU API, fixed address, retry, or execution after failure.
The serialization slice required 20 cleanup-first concurrent probe processes
on FW 5.50 with bounded teardown and exact process absence. Its active
production completion gate is two cleanup-first 600-present `2048.nro` runs
using identical bytes, including immediate relaunch. The temporary eight-frame
sequence-zero/stress artifacts remain diagnostic-only; they are not
substitutes for the completed long gate.

The pinned concurrent probe bytes passed all 20 cleanup-first FW 5.50
processes. Logs `20260803T034417Z-swapchain-run1.log` through
`20260803T034825Z-swapchain-run1.log` cover the even PID sequence 190 through
228. Every process reports four exact eligibility mappings/demotions, 16
successful exact RW-to-RX promotions, 16 known-return executions, 16
RX-to-RW demotions, 128 successful concurrent one-page map/write/unmap cycles,
four successful cache unmaps, and the PASS oracle with `errno=0`. This totals
320 promotions, 320 executions, 320 demotions, 2,560 concurrent mutation
cycles, and 80 cache unmaps. Every PID-scoped and global exact-process check
passed. The serial-plus-concurrent GPU-free gate is qualified. The production
2048 gate later completed with the exact evidence recorded below.

The first three processes of the production gate passed completely in logs
`20260803T034913Z-swapchain-run1.log`,
`20260803T034943Z-swapchain-run1.log`, and
`20260803T035013Z-swapchain-run1.log`. Each has all four JIT caches, FW
5.500.008, exact magenta intermediate/swapchain hashes, `GAME PASS 8 frames`,
clean process retirement, and no protection/presentation failure. On the
fourth fresh cleanup-first attempt, log
`20260803T035044Z-swapchain-run1.log` contains only the bounded 60-second
launcher timeout. The console then became unreachable on websrv and the
debugger port, so no target klog or exact-process-absence verdict exists for
that attempt. The wrapper stopped and did not send a fifth process. This is
not a production qualification pass and must not be labeled one.

No further ELF was launched while the console was unavailable. Vulkan-PS5
commit `262b657` adds an opt-in continuous klog path to the guarded runner;
runner SHA-256
`2d8a6d4a0eb20c6fe218c489d0303faef721c79168c8c80cb8ec1f037df63ed8`
starts and verifies exactly one listener after cleanup and exact-process
preflight but before upload/launch, retains bytes across a console disconnect,
and reaps the listener before exit cleanup. Its host regression covers
pre-launch ordering, post-launch evidence, single-listener use, retirement,
and fail-before-launch behavior. The two-run 600-frame wrapper pins that runner
and enables continuous mode; the 180-second `nc` value is a bounded socket
timeout, not an added post-run wait, because the listener is stopped as soon as
the application settle interval completes. On the next fresh direct-backend
boot, run that wrapper from run one. The three successful short runs and the
interrupted fourth attempt do not substitute for either required 600-frame
run.

### Post-panic VideoOut teardown gate

The empty fourth-run klog means the kernel panic has no captured faulting
instruction or proven subsystem. The three preceding processes completed
normal same-app `KillApp` and `All processes exited` handling and contain no
JIT, submit, present, or teardown failure. The investigation therefore does
not claim that Dynarmic, `/dev/gc`, or VideoOut is the proven root cause.

The audit did find one concrete unsafe lifetime that blocks all further
hardware qualification: OpenAGC registered caller-owned scanout buffers with
VideoOut but closed the handle without first calling
`sceVideoOutUnregisterBuffers`. Vulkan then released those image mappings.
That could leave VideoOut or the kernel holding stale scanout addresses across
process teardown and immediate relaunch.

OpenAGC commit `ed02ab8` now performs checked teardown in the required order:
delete flip event, unregister slot zero, close VideoOut, delete the event
queue, and only then release caller-owned image dependencies. Partial-open and
partial-close failures retain the live present chain. The legacy void close
terminates rather than returning after a failure that its caller cannot
observe, and the public hardware sample skips scanout unmap/release when
checked close fails. Its generic suite passes 20,085 assertions with zero
failures; the Prospero static build, public portability ELF build, and source
ownership audit pass.

Vulkan-PS5 commits `07c3fd4` and `38d6e97` consume that checked contract and
fail closed. Failed native teardown quarantines the present chain, images,
memory, surface, and device ownership instead of freeing registered memory.
Replacement unregisters the retired chain before opening another main-display
chain. Present and unregister share the swapchain mutex. Native fence, queue,
and device destruction is retryable phase by phase, with host-only injected
failure coverage; the injection hook is absent from Prospero bytes. Host WSI,
lifecycle, guarded-runner tests and the Prospero static build pass. The
unscoped Vulkan all-target build still has the unrelated pre-existing
`tests/pipeline.c` meta-attachment call-signature mismatch, so it is not
reported as a clean full-suite result.

The rebuilt, never-launched production ELF is SHA-256
`415a8cedd012e2c585fd47d45ada1d85c114a692ca0179eb1421abaf78a923f8`.
It imports `sceVideoOutUnregisterBuffers`, contains the mandatory quarantine
diagnostics, and contains no Prospero teardown-fault injection string. The
2048, sequence-zero, and InvadersNX runners pin these exact bytes; the stress
wrapper pins the revised sequence-zero runner.

The active goal is revised only in ordering, not acceptance: after a fresh FW
5.50 boot using only direct `/dev/gc`, run the pinned cleanup ELF,
independently prove exact `eboot.bin` absence, start continuous klog, then run
one bounded eight-frame 2048 canary with the new bytes. Require checked native
teardown, clean PID-scoped evidence, exact process absence, and a responsive
console. Only after that canary passes may the identical ELF begin the two
cleanup-first 600-present runs, including immediate relaunch. Any unregister,
close, WSI quarantine, exact-process, klog, or responsiveness failure stops
the gate and requires a fresh reboot; no subsequent ELF may be sent in that
boot.

The sequence-zero canary wrapper now forces continuous klog itself, matching
the two-run production wrapper. Callers cannot accidentally omit the
pre-launch listener while still satisfying the wrapper's pinned-hash gate.

The first post-panic canary used ELF `415a8ced...` and log
`20260803T084733Z-swapchain-run1.log`. It presented magenta and then the 2048
game visibly, completed all eight native presents, emitted the exact magenta
readback and `GAME PASS 8 frames`, and retired PID 89 with same-app `KillApp`
and `All processes exited`. Exact-name postflight found no `eboot.bin`, the
continuous klog is nonempty, and the console remained responsive. It is not a
canary pass: native present-chain teardown returned the synthesized OpenAGC
`AGC_ERROR_INTERNAL` `0x8089000a`; Vulkan retained the registered memory,
blocked device/surface release, and the wrapper rejected the run as designed.

The available log could not identify which of delete-flip-event, unregister,
close-handle, or delete-equeue failed because all four raw native results were
collapsed to the same OpenAGC error. OpenAGC commit `7e714a5` adds stage and raw
native-result diagnostics without changing the fail-closed order or ownership
state. Its 20,085-assertion host suite, Prospero static build, and source-order
audit pass. The rebuilt diagnostic Eden ELF is SHA-256
`708f29d48446d5d2d489bfe6c535acd3dab36a068e83d244cf7bdf788036091e`.
It remains ineligible for the 600-frame pair until one cleanup-first canary
identifies and then clears native teardown.

The diagnostic canary in
`20260803T085547Z-swapchain-run1.log` identified the raw failure as
`sceVideoOutUnregisterBuffers(handle, 0) == 0x80290009`, VideoOut
`RESOURCE_BUSY`, immediately after eight successful presents. PID 92 retired,
the exact-name check found no `eboot.bin`, and the console remained responsive.
The same native busy result appears in three earlier FW 5.50 OpenAGC compute
logs, each followed by successful `sceVideoOutClose`.

OpenAGC commit `e6348e2` implements only that evidenced fallback. A successful
unregister clears ownership normally. `RESOURCE_BUSY` leaves ownership marked
live and proceeds to checked handle close; only successful close clears the
registration before image release. Every other unregister result and every
close failure remain quarantined. Host 20,085/20,085 assertions, the Prospero
static build, and the source-order/busy-fallback audit pass. The rebuilt,
never-launched Eden ELF is SHA-256
`c42a553ac9336ea58c31ccf377d36b609b672d33e6bce8df69b27fb44b41fa46`.

That canary (`20260803T085925Z-swapchain-run1.log`) confirmed the busy fallback
reaches successful checked handle close, then reported
`sceKernelDeleteEqueue == 0x80020009` after the queue had already been retired.
The operator also confirmed visible magenta followed by the 2048 game with its
faint but correct color palette, so presentation remained intact through this
teardown diagnostic.
This exact terminal `EBADF` result is recorded across numerous earlier stable
FW 5.50 1,800-flip qualifications. OpenAGC commit `e2427a9` accepts only that
exact already-retired descriptor status after VideoOut ownership has been
released; all other queue-delete results still fail closed. The host suite,
Prospero build, and source audit pass. The resulting Eden ELF was SHA-256
`3b22a7cea17af3fd300dc8d5e8d8160b5bba592fbeeef18cb1b4aa2b83716c2e`.

The cleanup-first sequence-zero canary for that ELF passed all automated gates
in `20260803T090555Z-swapchain-run1.log`, including eight native presents,
exact-magenta readback, checked native teardown, continuous target klog, and
exact PID/global `eboot.bin` absence. The first 600-frame attempt
(`20260803T090629Z-swapchain-run1.log`) remained visibly healthy through the
2048 game but reached the qualification runner's exact 60-second HTTP ceiling
at 544 input-cycle presses before the 600-frame oracle. Cleanup and exact
process-absence checks passed and the loader remained responsive; this is not
a completed qualification.

The bounded retry (`20260803T090925Z-swapchain-run1.log`) disproved timeout as
the limiting factor: the frontend input injector advanced monotonically to 864
presses at 88 seconds, but native presentation never reached the first sparse
100-success marker after the eight detailed checkpoints. No GPU, JIT,
allocation, protection, process, panic, reset, or teardown failure was logged,
and the pinned cleanup again left no exact `eboot.bin`. The input injector runs
on the host event loop and is not a presentation oracle. The active completion
goal continues to require two cleanup-first 600-present `2048.nro` runs with
the identical ELF. InvadersNX is retained only as a separate diagnostic and
cannot replace this gate.

The post-eight audit found no deterministic Eden-side frame-recycle,
semaphore, or readback defect. It did identify an untested native boundary:
captured frames use CopyDestination -> CopySource -> VideoOutScanout, while
normal frames beginning at sequence eight use CopyDestination ->
VideoOutScanout directly. Vulkan-PS5 commit `a52b400` adds direct/captured/direct
command-recording coverage, 16 additional acquire/present recycle iterations,
and bounded WSI checkpoints for ordinals 0-15 plus power-of-two milestones.
Its focused WSI test and full 62-test host suite pass. Eden commit `62899ba`
adds matching Composite, frame-acquire/fence, worker, command-end, queue-submit,
and native-present stage checkpoints, together with a constexpr policy test
that proves readback ends after sequence seven without gating the remaining
592 frames. The rebuilt integrated Prospero ELF is SHA-256
`b65b173188b432ba82528a01ed967aa5d8b1bf0ea86045635042d5b3d2a4ed23`.
The mandatory cleanup-first sequence-zero canary passed in
`20260803T094735Z-swapchain-run1.log`: eight native presents, exact-magenta
readback, bounded native teardown, PID 109 absence, and global exact
`eboot.bin` absence all passed. The immediately following 600-frame run
`20260803T094822Z-swapchain-run1.log` failed before its first Composite call:
the first Dynarmic cache's exact full-map RW-to-RX transition at raw base
`0x303200000`, size `0x2004000`, returned `EFAULT` and terminated fail-closed.
Cleanup retired PID 111 and left no exact process. The repeated unsupported
format messages are capability discovery and are not the cause of this run.

The `0x4000` difference between the logged code base and protected raw base is
intentional allocator metadata, not an unmapped guard. The audit instead found
that initial and RX-to-RW demotions still used ordinary `mprotect` on the full
mapping. Two caches are adjacent, so those calls may structurally merge equal-
protection VM entries and invalidate later per-cache exact ownership checks.
Eden commit `6bf7b4d` now uses `kernel_mprotect_exact` for the initial demotion
and for both directions of every full-map transition; no retry or execution
after failure is permitted. The GPU-free probe mirrors the production policy.
The rebuilt probe is SHA-256
`9e25987d640a5ebbe8ca4e2f1e1623217d4b419196767c095380f6fba2daea9c` and the
rebuilt full ELF is SHA-256
`24aacb2e4198b282b7d30328c7f194705230edc1689dafdc8838d18961519f77`.
All FW 5.50 wrappers pin these bytes. The 20-process GPU-free W^X probe passed,
but the immediately following full Eden sequence-zero run
`20260803T100241Z-swapchain-run1.log` reproduced `EFAULT` on the first cache's
exact full-map RX-to-RW transition at raw base `0x303200000`, size `0x2004000`.
Eden terminated fail-closed and cleanup proved exact process absence. This
rejects further raw VM-tree mutation work: do not retry a failed transition or
use fixed placement.

The replacement path uses PS5 JIT shared memory with independent OS-chosen RW
and RX aliases. Commit `df9e03b` adds the GPU-free
`eden-ps5-dynarmic-jit-dual-alias-probe.elf`; it creates one page of RWX-capable
backing memory, maps distinct RW and RX views, emits only through RW, executes a
known-return stub only through RX, and requires both aliases and descriptors to
tear down cleanly. The source-committed probe is SHA-256
`0fee0f81169bd2ea77d7eed32de037802bfbbfebe90ea5d8960eee63acbc5e24`.
The first cleanup-first canary, `20260803T101454Z-swapchain-run1.log`, proved
that `sceKernelJitMapSharedMemory` does not create distinct OS-chosen aliases
when both destination inputs are null: both handles returned `0x9000d8000`.
The probe rejected equality before writing or executing, then cleanly unmapped
and closed every established resource; exact process absence passed. The next
revision mapped both handles with `mmap(nullptr, ...)`, matching the established
PS4 alias pattern while retaining OS-chosen placement. Commit `5e7ba59` and
SHA-256 `c9932445c3229881baab327cbd51031546e1effe62e7209d3d590e5124784592`
pinned that revision. Its cleanup-first 20-process FW 5.50 gate passed. Only
after that primitive is proven may Xbyak keep executable `getCode`/`getCurr`
pointers while routing all
emission and patch writes through the paired RW alias. Constant-pool storage
must likewise write through RW while retaining RX addresses for generated
RIP-relative targets, and Prospero's separate startup spin-lock generator must
be replaced or moved onto the same alias-safe allocator.

The first distinct-alias canary passed in
`20260803T101712Z-swapchain-run1.log`: RW `0x20006c000` and RX `0x200070000`
were distinct, the RW-emitted known-return stub executed successfully through
RX, and both mappings and both descriptors retired with `errno=0`. PID 160 and
the global exact `eboot.bin` query were absent afterward. The probe's own PASS
oracle is valid; the host wrapper alone rejected this run because the target's
`%p` formatting omitted the `0x` prefix. A second probe execution in
`20260803T101759Z-swapchain-run1.log` produced the same successful distinct
aliases, execution, teardown, and exact absence, but also exposed that the
runner does not accept an optional-group address regex. The corrected wrapper
matches the target's observed bare hexadecimal pointer spelling directly. A
third successful execution in `20260803T101906Z-swapchain-run1.log` exposed
the last host-only mismatch: target null pointers print as `0`, not `(nil)`.
The wrapper now uses the target's exact null spelling as well.

The generic `mmap(PROT_EXEC)` probe still passes through the payload SDK's raw
VM-tree `kernel_mprotect` helper. Its 20-run pass therefore proves shared
backing and alias teardown, but does not by itself remove the full-Eden
`EFAULT` mechanism. The qualifying revision maps the writable handle with
ordinary non-executable `mmap` and maps the executable handle once with
`sceKernelJitMapSharedMemory`, under the payload SDK VM-operation lock. This
hybrid also avoids the helper's observed same-address behavior when it is
called for both handles. The rebuilt hybrid probe is pinned as SHA-256
`5f8510d1b0612dc46910b1c381cb98d4757ffe15b90b5386c8cb9f8b22c7b5c6`.
Its cleanup-first canary passed in
`20260803T103359Z-swapchain-run1.log`: RW `0x20006c000`, direct-JIT RX
`0x9000d8000`, known-return execution, both unmaps, both closes, PID 211
absence, and global exact `eboot.bin` absence all passed. The console web
service became unreachable before the first 20-run command launched, so no
unguarded retry was attempted. After the fresh reboot, the complete hybrid
gate passed in 20 cleanup-first processes, logs
`20260803T104158Z-swapchain-run1.log` through
`20260803T104559Z-swapchain-run1.log`, PIDs 89 through 127. Every run used RW
`0x20006c000` and direct-JIT RX `0x9000d8000`, executed the known-return stub,
retired both mappings and descriptors, and proved both PID-specific and global
exact `eboot.bin` absence.

Production integration now uses that same hybrid mechanism. The Xbyak patch
keeps execution-visible addresses on RX while redirecting emission, rewrites,
growth copies, and explicit patches to RW; Dynarmic's constant pool separates
its writable storage pointer from its executable RIP target. Prospero's native
spin lock no longer creates a second standalone Xbyak cache. The allocator
keeps trusted mapping sizes, aliases, and descriptors in a fixed 32-slot
out-of-band registry keyed by the exact executable code pointer, so writable
cache bytes cannot authorize an arbitrary unmap or close. Duplicate and unknown
cleanup fails closed, equal descriptor ownership is deduplicated, and native
JIT map/unmap calls share the payload SDK VM-operation lock. A JIT-map output
becomes cleanup-authoritative only after a successful result plus non-null,
non-`MAP_FAILED`, page-aligned validation; failed-call output is never unmapped.
The Prospero
`dynarmic` target and full `yuzu-cmd` target both build successfully; hardware
Dynarmic execution and teardown remain pending after the reboot. The committed
production ELF is SHA-256
`c49362194ccd31b9c110d845a2618875d3981aa231438348f44991cd9bbb6bcc`;
the sequence-zero and 600-frame wrappers pin these exact bytes.
Artifact inspection of those exact bytes confirms unresolved imports for
`sceKernelJitCreateSharedMemory`, `sceKernelJitCreateAliasOfSharedMemory`,
`sceKernelJitMapSharedMemory`, and `sceKernelMunmap`, with no unresolved
`mprotect`. Its strings contain the dual-alias allocation/registry diagnostics
and none of the former `JIT-eligible W^X`, initial RW-demotion, cache-RX, or
cache-RW transition diagnostics. This proves the pinned ELF—not merely the
current source tree—contains the intended non-transition allocator.

The first production dual-alias sequence-zero run,
`20260803T104700Z-swapchain-run1.log`, created four distinct 32 MiB caches,
completed sequences 0 through 7 with successful submit, wait, and present
results, emitted `GAME PASS 8 frames`, terminated normally, and left no exact
`eboot.bin`. The qualification runner nevertheless rejected the scoped target
klog because it contained the first observed `FMEM allocation timeout` and
`LOW FMEM ... timed-out 1 pages` warning. Swapdev still reported all 43,008
pages free, and no fault, panic, JIT error, or GPU error occurred. The preceding
20 one-page probes each proved complete teardown and would total only 320 KiB
even if retained, while production allocates four concurrent 32 MiB shared
backings; the evidence therefore identifies transient startup allocator
pressure but does not yet prove its source. Do not expand the accepted warning
baseline. Treat this boot as allocator-dirty. The next hardware diagnostic is
one cleanup-first sequence-zero run immediately after a fresh reboot, with no
preceding probe or other `/dev/gc` workload and with scoped target-klog capture.

That fresh-boot discriminator passed in
`20260803T105357Z-swapchain-run1.log`, PID 89. The exact pinned production ELF
created all four 32 MiB dual-alias caches, completed the eight-frame
sequence-zero oracle, retired cleanly, and left both PID-specific and global
`eboot.bin` queries absent. Its scoped target klog contained only the accepted
raw-ELF `0x4000` resource-leak baseline: the FMEM timeout did not recur. The
operator saw magenta followed by the 2048 board with its known faint but
otherwise correct palette. This is the required visible sequence-zero evidence
and clears the same bytes for the two cleanup-first 600-frame immediate-relaunch
gate.

The first long-gate attempt, `20260803T105437Z-swapchain-run1.log`, was a
host-timeout diagnostic rather than an Eden failure. The exact ELF created four
dual-alias caches and presented successfully through frame 32, but the wrapper's
60-second `curl` deadline expired before either 600-frame oracle could be
reached; cleanup then proved PID 91 and global exact-process absence. The
observed rate is about 0.75 presents per second, making the old 120-second
ceiling structurally incapable of satisfying 600 real presents. The long
wrapper now defaults to 900 seconds, permits at most 1200, and extends continuous
target-klog capture 120 seconds past the selected request deadline. The
eight-frame preflight retains its shorter bounded policy. This is a host-only
gate correction; ELF, ROM, sidecar, and their hashes remain identical.

The prior sequence-zero canary's operator-visible result is also confirmed:
magenta appeared first, followed by the 2048 game with a faint but otherwise
correct palette. This is positive presentation evidence, not the outstanding
two-run 600-frame qualification and not yet a color-transfer calibration pass.

The identical pinned ELF then completed two immediate cleanup-first 600-frame
workloads. Run one,
`Vulkan-PS5/examples/qualification-logs/20260803T105733Z-swapchain-run1.log`,
emitted the exact native 600-present and `GAME PASS 600 frames` oracles and the
operator again saw magenta followed by the faint-but-correct 2048 board. Run
two, `20260803T110931Z-swapchain-run1.log`, reached the same two 600-frame
oracles and retired PID 96 with no remaining exact `eboot.bin`, JIT failure,
allocation failure, GPU failure, or crash. The wrapper rejected run two only
because its required `Total Pipeline Count` line was absent. This is two-run
renderer/relaunch evidence, but it is not yet the promised persistent
shader-cache evidence.

Audit found that the rejection exposed a real homebrew cache gap rather than
a renderer failure: `2048.nro` has program ID zero, and Vulkan
`LoadDiskResources` returned before assigning either cache filename. The old
bare anchored count regex also could not match Eden's decorated LOG_INFO line.
Eden commit `aac3627` now leaves the guest program ID unchanged but derives a
Prospero-only, path-independent cache namespace from the exact loader backing
file's full SHA-256. The ID reserves bit 63 and fails closed to uncached
execution on a hash or read error. For the pinned `2048.nro`, the exact cache
identity is `cd7e7f3438309201`; real nonzero title IDs bypass hashing. Hashing
completes before GPU workers start and uses a bounded heap buffer. The Vulkan
pipeline cache destructor now drains queued transferable pipeline
serialization before driver-cache serialization and member teardown. The
dedicated host suite passes 9 assertions across real-ID pass-through, known
SHA-256 vectors, path independence, content invalidation, short reads, stalled
reads, and null content; the strict full Prospero frontend also builds.

The rebuilt source-committed ELF is SHA-256
`ad5160147212771bb43b98aea8f4a835bcb735c315b4c84e362761d9cdf956cb`.
The 2048 wrapper pins those bytes and the exact derived cache identity. It
requires the native 600-present marker, `GAME PASS`, a real PSBC host-pipeline
marker, visible output, bounded teardown, and immediate relaunch. It does not
claim transferable guest-pipeline caching: 2048 uses an SDL software renderer
and does not submit a guest Maxwell graphics or compute pipeline.

The rebuilt sequence-zero hardware canary
`Vulkan-PS5/examples/qualification-logs/20260803T114052Z-swapchain-run1.log`
used PID 100 and passed the exact magenta intermediate/swapchain readbacks,
eight native presents, `GAME PASS 8 frames`, clean teardown, derived identity
`cd7e7f3438309201`, and a fresh `Total Pipeline Count: 0`. The operator saw
magenta followed by the faint-but-correct 2048 board. The wrapper initially
reported a false fifth-oracle failure because an ANSI reset follows decorated
LOG_INFO text before the physical line end; removing the invalid end anchor
matches the already-recorded exact identity without weakening its content.
Cleanup then found PID 100 and global exact `eboot.bin` absent. This clears the
new bytes for the two-run 600-frame lifecycle gate.

Fresh-cache long run
`Vulkan-PS5/examples/qualification-logs/20260803T114349Z-swapchain-run1.log`
then completed 600 native presents and `GAME PASS 600 frames`, with exact
identity `cd7e7f3438309201`, initial pipeline count zero, operator-visible
magenta and faint-but-correct 2048 output, and bounded teardown. No
`vulkan.bin` existed afterward; only a 44-byte `vulkan_pipelines.bin` existed.
The immediately relaunched log `20260803T115607Z-swapchain-run1.log` therefore
correctly reported zero loaded entries. Source and log audit found no
`PipelineCache::CreateGraphicsPipeline` or compute-pipeline event at all.
`SerializePipeline` creates its file header before testing environment
eligibility, so total file absence proves that no transferable serialization
task was queued. The 46 ACO/PSBC compilations are fixed Eden/Vulkan-PS5 host
blit, filter, and presentation pipelines, not guest shader-cache entries. The
44-byte driver file is only Eden's wrapper plus the standard 32-byte Vulkan
cache header; Vulkan-PS5 does not currently add compiled PSBC pipelines to it.
The nonzero pipeline-count requirement was therefore impossible for this
workload and has been removed rather than replaced with a fake cache claim.

The second long run was stopped once that startup result made the cache oracle
unrecoverable. A single ps5debug query transiently reported exact process
absence while the operator still saw the game. The pinned cleanup ELF was then
relaunched explicitly; websrv recovered, the exact-name check passed again,
and the operator confirmed the game closed. After any interrupted runner, one
absence query is no longer sufficient: relaunch cleanup, wait, repeat the
query, and reconcile with operator-visible state before another ELF.

After separating the lifecycle oracle from the inapplicable guest-pipeline
cache oracle, the same source-committed ELF completed the canonical FW 5.50
pair. Run one is
`Vulkan-PS5/examples/qualification-logs/20260803T120353Z-swapchain-run1.log`
(PID 109, log SHA-256
`c130dcbe13ad345bd29d56b76793301ac1ca3adf260631f6a3542b8bba012511`,
target-klog SHA-256
`b327cd312553563f57319ca102967f35d28291c7ae68815c6d8fe4f5829dc59b`).
Run two immediately relaunched as PID 111 in
`20260803T121613Z-swapchain-run1.log` (log SHA-256
`d914afe632efbac2f54b7216ce13361126f12da5147a31919eb149c7886a7dc8`,
target-klog SHA-256
`5ff35eb92c0f390f6631ab1436874ddfed548345a86fd74cb9c76ff6aff10027`).
Both runs reached exactly 600 native presents and `GAME PASS 600 frames`, used
identical ELF/sidecar/NRO identities, completed bounded teardown, and passed
PID-scoped plus repeated global exact-`eboot.bin` absence checks. The
operator-visible result for these exact bytes remains magenta followed by the
faint-but-correct 2048 board. No crash, allocation failure, JIT protection
failure, native command failure, or presentation failure appears in either
log. This completes the active FW 5.50 2048 lifecycle/relaunch gate; it does
not claim guest shader-cache persistence.

Vulkan-PS5 commit `93c7325` then retires the temporary first-eight
command-buffer-end, queue-submit, acquire, and present checkpoints. It retains
the committed-success 100-frame progress and exact 600-frame marker, all
existing submit failures, and adds fail-only native command-buffer-end and
queue-present diagnostics. The affected host WSI, lifecycle, and command
recording selection passes all 22 tests; the Prospero static library and
swapchain example build. The repository-wide host build still stops at the
already documented, unrelated stale four-argument meta-attachment calls in
`tests/pipeline.c`. The integrated post-retirement Eden ELF builds as SHA-256
`55083aa102b030c6ed205b72ef5f18e42dd4b8f77ef024f2fe9f46d33620b3bb`.
String audit finds no retired checkpoint text and does find the retained
100/600 progress plus new fail-only messages. These post-gate bytes have not
been launched and therefore do not replace the qualified
`ad5160147212771bb43b98aea8f4a835bcb735c315b4c84e362761d9cdf956cb`
evidence.

An additional candidate workload is `../Flappy_Bird_NX.nro`, SHA-256
`6e7cd9a1a22a0102a4f68ba6e434378c9b7381ce4f44a43ca376953f536aa54d`.
Its header contains the expected `NRO0` magic, it embeds `romfs:/` graphics,
WAV/MP3 audio, SDL2, SDL_ttf, and SDL2_mixer code, and its content-derived
cache identity will therefore be independent of its launch path. Keep it as a
separate candidate until a cleanup-first bounded canary proves continuous
presentation and fail-soft audio behavior. Its expected cache identity is
`ee7cd9a1a22a0102`, and its Mesa GLES2 path makes it the first homebrew
candidate for real guest-pipeline serialization. It supplements rather than
replaces the active 2048 lifecycle evidence.

Two retail base-title candidates are available locally but must not be copied
to the console until private key provisioning is explicitly authorized. Into
the Breach NSP SHA-256
`c00ede302d7ffbe1fd757679c88262bc63e849343efb4690b135d3ccac277b9f`
is a structurally valid 283,775,312-byte PFS0 with four content-ID-consistent
NCAs, ticket, certificate, and title ID `010057D00B612000`; its decrypted CNMT
version/roles remain unverified. A Short Hike NSP SHA-256
`4b5239ae15c55016f059920069cc652f75c57f4c583fe997d905bb901efba6b1`
is a structurally valid 319,465,742-byte PFS0 whose unencrypted CNMT XML
identifies application `01004890117B2000`, version 0, generation 11, and whose
four declared NCA sizes and hashes match exactly. Use A Short Hike as the
primary future commercial 3D/cache candidate, beginning with a loader-only
preflight and cleanup-first eight-frame canary. Neither audit read, hashed,
copied, or modified the private key files.

The preceding serial-only slice serialized every Prospero Dynarmic
executable-VM operation with one
process-lifetime guard: cache eligibility allocation and initial demotion,
full-entry RW/RX transitions, unmap, and the separate lazy Xbyak spin-lock code
generator's construction, protection changes, and teardown. Non-Prospero
Xbyak ownership remains unchanged. The GPU-free
`eden-ps5-dynarmic-jit-wx-probe.elf` uses OS-chosen anonymous addresses only and
exercises four exact `0x2004000` mappings through four bounded W^X cycles each,
executing a known-return stub only after a successful RX transition and
unmapping every established mapping. The source-committed rebuilt probe is
SHA-256
`7905e56f44fd419900258b247372c0885f305ec562b9111df47c070483bdcbc8`;
the source-committed rebuilt full Eden ELF is SHA-256
`b15cdff162630eb9d9dfd01195407b8a904020aee58068726e44bd909ecc164e`.
Both are build artifacts, not target evidence. The pinned probe wrapper must
complete all 20 fresh cleanup-first launches before the full ELF is eligible
for the InvadersNX gate.

The earlier pre-commit probe bytes
`4c08c78a084211a38749ca2d165e36f01c2d1fd71edf3d282b45b0a4391287b8`
passed 20 cleanup-first processes on FW 5.50 in logs
`20260803T024554Z-swapchain-run1.log` through
`20260803T024933Z-swapchain-run1.log`: 320 successful RW-to-RX transitions,
320 known-return executions, 320 RX-to-RW transitions, and 80 unmaps, all with
`errno=0`. PIDs were the even sequence 102 through 140; the wrapper and an
independent final exact-name query found no `eboot.bin`. Because committing and
rebuilding changed the embedded SCM bytes, this is strong diagnostic evidence
but is not substituted for byte-identical qualification of the pinned
`7905e56f...` probe.

The pinned, source-committed probe then passed the same 20-run gate on FW 5.50.
Logs `20260803T025321Z-swapchain-run1.log` through
`20260803T025701Z-swapchain-run1.log` cover PIDs 142 through 180. Every process
reports four successful eligibility mappings and initial RW demotions, 16
successful full-map RW-to-RX transitions, 16 known-return executions, 16
successful RX-to-RW transitions, four successful unmaps, and the exact PASS
oracle, all with `errno=0`. Every per-PID and global exact-process check passed,
and an independent final query also found no `eboot.bin`. Across the pinned
bytes this is 320 promotions, 320 executions, 320 demotions, and 80 unmaps with
clean bounded teardown. The GPU-free JIT W^X preflight is therefore qualified;
the remaining active proof is the repinned full Eden ELF's two-run 2048
renderer/WSI gate under normal multithreaded load.

The first full `b15cdff1...` replay,
`20260803T025758Z-swapchain-run1.log`, clears the former first-cache RW-to-RX
boundary under Eden's normal multithreaded initialization and executes the
InvadersNX guest. It contains no JIT/mprotect failure or crash. InvadersNX then
exits its guest process normally before producing a display buffer, so Eden
correctly rejects `expected=600 actual=0`; cleanup and exact-process checks
pass. This is positive JIT evidence but not an InvadersNX renderer pass. The
separate guest SDL/window exit is under investigation and must not be hidden by
weakening the presentation oracle.

To qualify the intermittent protection fix under a known rendering workload,
`tools/run_fw550_2048_jit_stress.sh` pins the repointed sequence-zero wrapper
and executes it in 20 fresh cleanup-first processes. Each run uses the same
full `b15cdff1...` ELF, requires exact magenta renderer/swapchain readback and
eight presented frames, rejects every JIT/allocation/presentation failure, and
requires bounded teardown plus exact process absence. This supplements, rather
than replaces, the already-passed GPU-free 20-process W^X probe.

The first production stress run,
`20260803T030325Z-swapchain-run1.log`, fails at the former first-cache promotion
after full Eden initialization reaches `CreateManagedDisplayLayer`. The SDK
hardening preserves the underlying error as `EFAULT` instead of the old
flattened `EPERM`:
`base=0x303200000 size=0x2004000 errno=14`. Cleanup and both exact-process
checks pass. Because the same bytes complete 320 serial probe promotions and
the owned range is exact, this identifies a live VM-entry lookup race during
concurrent process-map mutation, not missing JIT permission or invalid cache
geometry. The stress wrapper correctly stops at run one; the production EPERM
goal remains open. Do not retry the failed in-process transition. The
cooperative VM-operation serialization and exact fresh-lookup implementation
above is built and independently reviewed, but remains hardware-pending; no ELF
from this new slice has been launched yet.

## Active construction diagnostic (2026-08-02)

The unnormalized-sampler fix, OpenAGC runtime API 55 compute-scratch path, and
Vulkan-PS5 consecutive descriptor-binding traversal close the three successive
`RasterizerVulkan` construction blockers. The last failure was Eden's valid
`VkDescriptorUpdateTemplateEntry` for two storage buffers spanning bindings 0
and 1; Vulkan-PS5 incorrectly required the whole entry to fit binding 0. The
shared descriptor cursor now carries template updates, ordinary writes, and
copies across compatible consecutive bindings while handling sparse and
zero-count bindings, nonzero array offsets, and unaligned template payloads.
The public compute-scratch implementation is OpenAGC commit `babc3b8`, with
clean-checkout unnormalized-descriptor dependency follow-up `579c401`;
Vulkan-PS5 commits `8789fc1` and `8cefa75` close the unnormalized-sampler and
consecutive-descriptor blockers respectively. A clean checkout containing the
two OpenAGC commits passes `make test`; unrelated OpenAGC reference-game work
remains outside those commits.

The final source-integrated Prospero ELF
`4eae3b998f9a92664d41b86325a62bc8f9d2186a8c592e471ac180038923e490`
was replayed cleanup-first on FW `5.500.008`. Log
`Vulkan-PS5/examples/qualification-logs/20260802T074820Z-swapchain-run1.log`
reaches the buffer-cache runtime/cache, query-cache runtime/cache, pipeline
cache, DMA acceleration, fence manager, and final
`eden-ps5: INIT CHECKPOINT rasterizer` markers. The bounded runner therefore
matches the scoped construction oracle for complete `RasterizerVulkan` member
construction, and the former partial-construction mutex-lock failure is absent.
The overall guarded run still fails because of the later coredump described
below; it is not a clean runner PASS.

This proves constructor completion and pipeline acceptance only. It does not
yet prove descriptor GPU execution, scratch-ring readback, visible rendering,
presentation, or orderly teardown.

A follow-up source-integrated candidate,
`847db1f2b0e66eaa19a43bcc35afb3e2f39de215c1babf4c7313d157def1f72e`,
moves the Prospero user path before logging/configuration, selects Eden's null
audio sink, reuses the Opus initialization thread as its main worker, and caps
Prospero pipeline-cache workers at four. FW `5.500.008` log
`Vulkan-PS5/examples/qualification-logs/20260802T081021Z-swapchain-run1.log`
uses `/data/homebrew/eden_ps5/user`, contains no SDL `dsp` error, clears the
former Opus thread-creation boundary, and again reaches the final rasterizer
checkpoint.

The service-thread audit found that BSD also retains two companion IPC workers.
Removing those workers would change blocking socket behavior, so the active
policy leaves every service and BSD worker intact. Instead, Prospero uses one
pipeline compiler worker and disables the unused CemuHook UDP and custom
Joy-Con HID scanner workers while retaining SDL3 controller input, the null
audio sink, and VI's normal dedicated VSync thread. Other platforms retain
their existing worker counts and input backends. The focused host test
`eden.ps5_thread_budget` rebuilds from current source and passes all six cases
and 20 assertions, as do the host `core`/`video_core` builds and the
source-integrated Prospero `yuzu-cmd` build. The completion audit also confirms
that `src/core/hle/service` and `src/core/hle/kernel/kernel.cpp` have no diff
from Eden revision `612409c7ba`: service launch order, all six socket service
registrations, both BSD companion workers, all three VI services, and the
dedicated VSync thread therefore retain their original guest-visible
semantics.

FW `5.500.008` artifact
`98fc5b71589c07a09cdea3440b8790fa22c0e43b4a51398abf46b239b5c930b3`
was replayed cleanup-first in
`Vulkan-PS5/examples/qualification-logs/20260802T085539Z-swapchain-run1.log`.
It records `available=15 selected=1` and
`custom_hid=false udp=false sdl=true`, clears VI and the former uncaught
`thread constructor failed`, and progresses through network/user clock update
calls. The new boundary is a deterministic `CPUCore_0` SIGSEGV at execute
address `0x303404010`, recorded in the paired `.klog`; the kernel retires PID
137 but enters crash cleanup and reports the established raw-ELF `0x4000` VM
warning. Thread-budget advancement is proven, but clean teardown, leak-free
qualification, and runner PASS remain open.

OpenAGC commit `5ff3eea9f3c3413afb8bb39f4ddff135d179f232`
replaces the global 16-slot graphics user-SGPR check with the gfx1013 stage
contract: 32 slots for graphics and 16 for compute. It also validates the
compiler's encoded `PGM_RSRC2.USER_SGPR` allocation, protects fused-stage
continuation/layout slots, and replays sparse pointer-free inline push
constants by their exact used mask. The exact Eden `0xfffff0ff`/29-entry
reflection, dynamic `GS_31`, `GS_32` rejection, containing application push
ranges, and failure cases pass 19,809 assertions; all 19 generic CTest entries
and a fresh Prospero library build pass. This is host/build qualification only
until the first presentation pipeline is reached again on hardware.

The first source-integrated ELF containing that OpenAGC commit has SHA-256
`dc2e4cc259597a7d4faff9048675d681533177979583dc816a52cfd10595370c`.
Cleanup-first FW 5.50 run
`Vulkan-PS5/examples/qualification-logs/20260802T101532Z-swapchain-run1.log`
again records `available=15 selected=1`, completes rasterizer construction,
passes the former VI constructor boundary, and reaches
`CreateManagedDisplayLayer`. It stops earlier than presentation pipeline
creation when the 32 MiB Dynarmic cache cannot transition from RW to RX:
`mprotect` returns `EPERM`, followed by an instruction-read protection fault.
The paired `.klog` records `canCoredump=false`, complete PID retirement, and a
4 KiB VM-resource warning. An exact post-run process query found no
`eboot.bin`; nevertheless this is a failed crash lifecycle, not clean teardown,
and it does not hardware-qualify the 29-SGPR correction.

The diagnosed allocator incorrectly combined the original executable JIT-shm
handle with a same-address RW mapping and later `mprotect`. PS5 JIT shared
memory is an alias-based design, while Dynarmic embeds identical write/execute
addresses throughout generated code. The first anonymous correction used a
page-aligned RW mapping and serialized, checked RW/RX transitions at the same
address. Every Prospero allocation, protection, or unmap failure terminates
through the bounded, non-coredumping SystemService path without executing an
invalid mapping. Host Dynarmic and `eden.ps5_thread_budget` builds/tests pass,
and the source-integrated Prospero ELF rebuilds.

Eden commit `29cc79b55d47fb158d4564ddca219ae7d1cc187c` contains that
allocator and produces source-integrated ELF SHA-256
`9990d879aa606846e48abad527d42032147c62b12977de77199c1154de81824f`.
Cleanup-first FW 5.50 run
`Vulkan-PS5/examples/qualification-logs/20260802T103800Z-swapchain-run1.log`
maps all four 32 MiB anonymous caches RW, executes after their RX transitions,
and contains neither the former `mprotect` failure nor a native allocation
failure. It reaches six 29-user-SGPR NGG pipeline compilations without the
former OpenAGC `0x8089000b` rejection, hardware-clearing the graphics 32-slot
pipeline-creation gate. Draw and presentation proof remain open.

That single transition pass is not qualification. Cleanup-first run
`Vulkan-PS5/examples/qualification-logs/20260802T110600Z-swapchain-run1.log`
with source-integrated ELF SHA-256
`506d3d8340811176a8a705402e1d7a9378853e4779d7a5367316963e6f8956d1`
clears the former compatible-3D image-view failure, then fails closed on the
first 32 MiB cache RW-to-RX transition at `CreateManagedDisplayLayer` with
reported `errno=1`. The paired kernel log shows normal process retirement with
no coredump, and the independent exact-name postcheck finds no `eboot.bin`.
Together with the earlier `101532` failure and `103800` pass at the identical
address and size, this proves the delayed executable elevation is intermittent
on FW 5.50. Payload-SDK libc replaces every `kernel_mprotect` failure with
synthetic `EPERM`; its helper walks and edits the live VM map without taking the
map lock, so this does not prove a policy denial.

The active diagnostic now requests executable eligibility during anonymous
`mmap`, matching the SDK LLVM MCJIT allocation path, immediately demotes the
exact whole mapping (including Dynarmic's metadata page) to RW, and retains
serialized fail-closed whole-mapping RW/RX transitions. This candidate requires
repeated-transition and immediate-relaunch hardware evidence. Any recurrence
moves the production path to JIT shared-memory RW/RX handles remapped at the
same numeric virtual address, preserving Dynarmic's pointer assumptions without
the unlocked protection helper.

Cleanup-first FW 5.50 run
`Vulkan-PS5/examples/qualification-logs/20260802T111910Z-swapchain-run1.log`
with integrated ELF SHA-256
`da805c5ba6a29931c7bd20e41d1723dfa8ccf739c5f65a6fafee2eb64bc06fdf`
maps and immediately demotes all four JIT-eligible caches, clears the formerly
intermittent first RX transition, reaches `CreateManagedDisplayLayer`, and
again compiles six 29-user-SGPR NGG pipelines. It then reaches the same format
51 2D image-view rejection, remains live until the bounded 120-second request
expires, and is retired with no exact-name `eboot.bin` process left. This is
one positive transition sample, not repeated JIT qualification. Vulkan-PS5
commit `881c531` and Eden commit `55ea61f` add Prospero-only rejection context
for the image's type/flags/extent and the exact mip/layer interval; focused host
tests and both Prospero builds pass. The next cleanup-first run must capture
that diagnostic before any further image-view semantic change.

Cleanup-first diagnostic run
`Vulkan-PS5/examples/qualification-logs/20260802T112522Z-swapchain-run1.log`
with integrated ELF SHA-256
`8a09bc3ce245e1d296f4bdd60f1f62509bf2033065bf043f57ab20ef8f6c6fa8`
provides a second consecutive positive JIT-transition sample and again retires
with no matching PID. The failing view request is exactly color aspect, mip 0,
one mip, base layer 0, and one layer; therefore the original nonzero depth-slice
range theory does not explain this call. No base-mip or base-layer driver
rejection fires, narrowing the remaining choices to image/view format or type
compatibility and native OpenAGC view creation. Vulkan-PS5 commit `11cc8ba`
adds those final Prospero-only rejection-stage diagnostics; its focused image
and pipeline tests and full Prospero build pass. A cleanup-first capture with
that commit is the immediate next action.

Cleanup-first run
`Vulkan-PS5/examples/qualification-logs/20260802T113018Z-swapchain-run1.log`
with integrated ELF SHA-256
`74d513b689c770f559a1d820d59ab7f6007b79548dd2997de1d68a97487f5a28`
identifies the exact remaining rejection: an ordinary 2D mutable image with
Vulkan format 44 (`VK_FORMAT_B8G8R8A8_UNORM`) and flags `0x108` is viewed as
format 51 (`VK_FORMAT_A8B8G8R8_UNORM_PACK32`). Both formats are in Vulkan's
32-bit compatibility class, but Vulkan-PS5 and OpenAGC incorrectly separated
their BGRA and RGBA/A8 format families. OpenAGC commit `6f29f4b` and Vulkan-PS5
commit `14758ae` unify their supported RGBA8/BGRA8 UNORM/SRGB variants into the
same mutable-view class. Direct OpenAGC descriptor coverage and the exact
Vulkan command-recording view regression pass; both full host suites and both
Prospero builds pass.

Cleanup-first FW 5.500.008 run
`Vulkan-PS5/examples/qualification-logs/20260802T114341Z-swapchain-run1.log`
with integrated ELF SHA-256
`8a6e4d70c86b36d8cd9c4c73f9bd1f87ea3436156be1c32ae4c18ffc207176c1`
uses the pinned cleanup ELF SHA-256
`9fd6b41cf2ea87989c4217234c6f34c96a1ca5dc482355af1258539db77d4d76`;
the preflight independently proves no prior exact `eboot.bin`. It supplies a
fourth consecutive positive first JIT-transition sample, clears the format-44
to format-51 `vkCreateImageView` rejection, and reaches
the first render-pass attachment clear. The clear path incorrectly builds its
internal graphics pipeline for the underlying image format 44 while binding
the legal mutable view as format 51; OpenAGC correctly rejects the mismatched
pipeline and color target with `0x80890501`. `vkEndCommandBuffer` propagates
`VK_ERROR_INITIALIZATION_FAILED`, but the frontend still terminates on the
uncaught Vulkan exception. The kernel records PID 178 as an app crash with
`canCoredump=false`, retires it, and the runner independently reports no exact
`eboot.bin` process. This is bounded crash retirement, not orderly emulator
teardown or leak-free relaunch evidence. OpenAGC commit `c1ddcce` generalizes
supported mutable color views by uncompressed storage width and explicit BC
family while keeping depth/stencil and multiplane formats fail-closed.
Vulkan-PS5 commit `99e1249` retains and enforces non-empty image format lists,
implements its supported Vulkan compatibility classes, and keys both
attachment-clear pipeline lookups by the framebuffer view format. OpenAGC's
19 CTest entries and 19,926 direct assertions pass; Vulkan-PS5's 62 host tests
and both Prospero builds pass.

Cleanup-first FW 5.500.008 run
`Vulkan-PS5/examples/qualification-logs/20260802T115623Z-swapchain-run1.log`
with integrated ELF SHA-256
`a707e44eb900c399af594adaed45f7b28a20363dab53bade741b9f61edca64d8`
uses the same pinned cleanup hash and exact-process preflight. It supplies a
fifth consecutive positive first JIT-transition sample, clears both the
mutable view and attachment-clear failures, and records the first native draw.
The next boundary is a color image barrier from
`VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` to `VK_IMAGE_LAYOUT_GENERAL`:
OpenAGC rejects the requested sampled-read `1/1` to storage-write `4/1`
usage/owner transition with `0x80890001`. `vkEndCommandBuffer` propagates the
error, the GPU-thread exception is caught, and the frontend remains bounded
until the 120-second runner retires PID 181. The kernel log contains no app
crash, coredump, JIT protection error, or native allocation failure, and the
runner proves no exact `eboot.bin` remains. This is still forced retirement,
not orderly teardown or presentation evidence.

Vulkan-PS5 commit `02a3859` stops treating generic Vulkan memory scopes as
storage-image access. It preserves an exact read state until the next typed
consumer, prepares descriptor images over their exact view ranges, and rejects
generic barriers after native write states until OpenAGC exposes a same-state
dependency primitive. The exact positive and fail-closed command regressions,
the full 62-test host suite, the Vulkan-PS5 Prospero build, and Eden's strict
integrated Prospero build pass.

Cleanup-first FW 5.500.008 run
`Vulkan-PS5/examples/qualification-logs/20260802T121701Z-swapchain-run1.log`
with integrated ELF SHA-256
`5f281b3f6f13fd0e41aabdc0c4a9384625c275e4373d504b689c8c569cc60dec`
uses the same pinned cleanup and exact-process preflight. It supplies a sixth
consecutive positive first JIT-transition sample, clears the formerly failing
generic image barrier, and records two native draws. The next boundary is a
1280x720 render-pass begin rejected solely because the framebuffer stores a
different render-pass handle (`rp_match=0`), even though Vulkan permits a
render pass compatible with the one used at framebuffer creation. The uncaught
`vkEndCommandBuffer` exception triggers an app crash for PID 184;
`canCoredump=false`, the kernel retires the process, and the independent exact
postcheck finds no `eboot.bin`. This remains crash retirement, not teardown or
presentation proof.

Vulkan-PS5 commits `660ceb2` and `c9615a3` replaced render-pass handle
identity with conservative compatibility and added a bounded hardware dump.
Cleanup-first diagnostic run
`Vulkan-PS5/examples/qualification-logs/20260802T124647Z-swapchain-run1.log`
with ELF SHA-256
`80bf3bddbf760a3784bf12f134b06df978dd4a9e1d87294fa976fd6451b4e406`
proved that the framebuffer's retained creation-render-pass pointer had been
freed: its header contained allocator metadata while the untouched fixed-array
tail still described the expected first subpass. Vulkan permits the creation
render pass to be destroyed while the framebuffer survives. Vulkan-PS5 commit
`8dc2c2f` therefore stores an owned, pointer-free compatibility snapshot in
each framebuffer, copies completed synthetic dynamic-rendering state, bounds
the corruption diagnostic, and adds a regression which destroys render pass A
before beginning the framebuffer with compatible render pass B. The focused
test, full 62-test host suite, Vulkan-PS5 Prospero build, and strict integrated
Eden build pass.

Cleanup-first runs
`Vulkan-PS5/examples/qualification-logs/20260802T125540Z-swapchain-run1.log`
and `Vulkan-PS5/examples/qualification-logs/20260802T130046Z-swapchain-run1.log`
with integrated ELF hashes
`db931ce94a7816e98771248648ba1604151a140b0079539e476bcd51f4780571`
and `64864e74cb018649c8ca91a765529b962b821c99ec47a91cff3fccc429c00015`
clear render-pass begin, advance through all VI service acquisition calls, and
compile 40 native shader stages without a Vulkan, OpenAGC, GPU, JIT, or native
allocation failure. Both deliberately bounded launcher windows expire before
the 600-frame oracle, retire their exact process, and find no remaining
`eboot.bin`; they are progress evidence rather than teardown qualification.
Vulkan-PS5 commit `af0ab3b` removes the obsolete per-push-constant hot-path
diagnostic. The standard 600-frame runner now uses a bounded, configurable
`EDEN_PS5_WEBSRV_TIMEOUT`, currently defaulting to 900 seconds for the measured
2048 workload.

Vulkan-PS5 commit `0696b18` adds eight-sample Prospero checkpoints around
command-buffer end, native submission/fence wait, swapchain acquisition, and
presentation. Cleanup-first run
`Vulkan-PS5/examples/qualification-logs/20260802T130908Z-swapchain-run1.log`
with integrated ELF SHA-256
`73f2c78e669255f61af52e5c0907e00bc85f89d203a24cf2030ce27c1c357111`
proves every sampled command end, submission, fence wait, acquisition, and
native presentation succeeds; frames 0 through 7 rotate across all three
swapchain images. This disproves a failure inside those sampled host calls, but
does not prove that their scanout contains visible guest pixels. Later direct
observation found black output, so the earlier inference that successful native
presentation alone proved visible rendering is withdrawn.

The scoped service-host-thread qualification uses committed sidecar
`src/ps5/eden-2048-thread-budget.launch`, SHA-256
`216b923630466a0be9cafae8d54b36617fe62309e0bbbdf65a3f566bda8eca67`,
without changing guest-visible services or the existing 600-frame sidecar.
Cleanup-first FW 5.500.008 run
`Vulkan-PS5/examples/qualification-logs/20260802T131312Z-swapchain-run1.log`
and target kernel log
`Vulkan-PS5/examples/qualification-logs/20260802T131312Z-swapchain-run1-target.klog`
use the same integrated ELF and pinned cleanup SHA-256
`9fd6b41cf2ea87989c4217234c6f34c96a1ca5dc482355af1258539db77d4d76`.
They pass all VI calls, record real draws, submit and present eight consecutive
frames, call `ShutdownMainProcess`, emit `eden-ps5: GAME PASS 8 frames`, and
exit inside 30 seconds. The runner proves PID 202 absent, reports no app crash
or coredump, and accepts only the already-qualified raw-ELF VM baseline warning
of `0x4000`; there is no OpenAGC/native allocation failure or additional leak.
The Prospero frontend still forces `Settings::AudioEngine::Null` for this
renderer slice, so audio does not consume an extra native service or worker.
This completes the service host-thread budget gate; it does not replace the
two-run 600-frame, relaunch, FW 11.60, or CTS/deqp completion gates.

The completion audit rebuilt the current source-integrated `yuzu-cmd` target
at Eden `77f1636c69`, producing ELF SHA-256
`78326dd461d4c2e060eaa783791462549de81824068dd17345735a61bddaa35f`.
The first cleanup-first replay,
`Vulkan-PS5/examples/qualification-logs/20260802T132420Z-swapchain-run1.log`,
failed closed at the already-known intermittent first Dynarmic RW-to-RX
transition (`mprotect`, `errno=1`) before executing the invalid mapping; the
guarded runner found PID 205 absent. This is a negative W^X sample, not a VI or
service-budget regression. After another cleanup-ELF launch and exact-name
absence check, the identical ELF passed in
`Vulkan-PS5/examples/qualification-logs/20260802T132557Z-swapchain-run1.log`
with scoped kernel log
`Vulkan-PS5/examples/qualification-logs/20260802T132557Z-swapchain-run1-target.klog`.
It again clears every VI call, presents frames 0 through 7, emits
`GAME PASS 8 frames`, and completes the bounded kernel exit lifecycle with PID
208 absent. The scoped kernel log contains no crash or coredump marker and
exactly one `0x4000` VM-resource warning: the established raw-ELF teardown
baseline, not zero-VM-leak evidence. No native allocation or OpenAGC failure is
present in the passing run.

The active 600-frame audit commits Eden `e0fbf33d77` and Vulkan-PS5
`bd77da1`. The guarded runner now requires the exact FW `5.500.008` line, a
success-only 600-native-present marker, the bounded guest PASS, run-two
pipeline evidence, and final scoped plus global process absence. It rejects
allocation, mapping, JIT-protection, GPU-thread, and presentation failures,
and its failure trap reruns the pinned cleanup ELF. The qualification-only
sidecar enables a main-thread SDL input cycle without adding a worker; its
SHA-256 is
`e5c10f0d91bcb683f8e9f41a1bce44228d07317ff1f07236fcfabf702f4a4bac`.
The host launch-config tests pass 40 assertions, the full Vulkan-PS5 suite
passes 62 tests, and both Prospero integration builds pass.

Cleanup-first FW 5.500.008 runs
`Vulkan-PS5/examples/qualification-logs/20260802T145232Z-swapchain-run1.log`
and
`Vulkan-PS5/examples/qualification-logs/20260802T145623Z-swapchain-run1.log`
used integrated ELF SHA-256 values
`2d02767227317c9bbcbb02a440b7c2227b3ec6bf05843f58fa92abb91dfe042a`
and
`cf34ada745127f3f65eec4a780cbc0a9fc300833d3a82dcb65d9b047fb97bc57`
respectively. Both advance the qualification input counter beyond 500 presses,
record successful native presentation for frames 0 through 7, then miss the
first 100-success milestone and time out at the bounded 60-second gate. The
second ELF includes Eden `abdef11698`, which adds the NRO's documented B-button
restart input before directional moves; the result is unchanged. Inspection of
the NRO's public source confirms that it redraws on every `appletMainLoop()`
iteration, so a completed board does not explain the missing frames. Direct
hardware observation during the second run reported black output. Therefore
these runs prove neither visible presentation nor a 600-frame pass and the
two-run gate remains open. Neither log contains a Dynarmic `mprotect`, native
allocation, OpenAGC, GPU-thread, or present-call failure. Each failure path
reran the pinned cleanup ELF, and repeated independent global checks found no
exact `eboot.bin`; a final manual cleanup was also performed before the console
reboot. The next hardware attempt must start from the rebooted device, remain a
single diagnostic run until visible guest pixels are confirmed, and still run
the pinned cleanup plus exact-name absence check before launch.

Eden commit `e4268cdd0d` adds bounded, Prospero-only samples at the CPU display
source, intermediate presentation image, and destination swapchain image.
Cleanup-first eight-frame run
`Vulkan-PS5/examples/qualification-logs/20260802T151544Z-swapchain-run1.log`
with integrated ELF SHA-256
`8a8108c0bbc75ec7efe2ca09117fa61883d62deb0b2da8c02f842487cbe71585`
records the non-accelerated 1280x720 RGBA8888 source with all 256 sampled CPU
bytes nonzero (`fafafafa` first pixel), but every sampled byte from both the
intermediate image and swapchain image is zero. Native presents 0 through 7
still succeed and the frontend exits with `GAME PASS 8 frames`; direct display
observation remains black. This moves the first proven zero stage ahead of the
final transfer and native presentation path, into window adaptation.

Eden commit `f282a91102` adds the next bounded discriminator: WindowAdapt
sequences 0 through 7 explicitly clear magenta, while sequence 0 skips every
guest layer draw. The first cleanup-first attempt with post-commit ELF SHA-256
`1bcdd135d958cb3baee6470f6ee4c414b11ba77c0f0cf155069857bd7f0e1ad2`,
`Vulkan-PS5/examples/qualification-logs/20260802T152205Z-swapchain-run1.log`,
fails closed before its first guest frame on the known intermittent Dynarmic
RW-to-RX `mprotect(EPERM)` boundary. The guarded failure path retires PID 97 and
finds no exact `eboot.bin`; it provides no rendering evidence. After another
pinned cleanup launch and independent exact-name absence check, the identical
ELF completes eight frames in
`Vulkan-PS5/examples/qualification-logs/20260802T152320Z-swapchain-run1.log`.
Sequence 0 records the magenta control with guest layers skipped, its native
clear draw records and submits successfully, yet all 64 sampled bytes in both
the intermediate and swapchain images remain zero. Sequences 1 through 4 are
also zero, and direct display observation reports no magenta and only black.
The frontend nevertheless emits `GAME PASS 8 frames`, exits cleanly, and the
runner proves PID 101 and every exact `eboot.bin` absent. The current blocker
is therefore the Vulkan-PS5/OpenAGC offscreen color-attachment write path (or
its visibility transition), not guest framebuffer generation, fullscreen
sampling alone, the final image transfer, or native presentation. Before the
next Eden launch, add an exact B8G8R8A8 UNORM optimal-image `GENERAL`
render/clear/readback regression below the frontend and fix that failure at its
owning layer.

Vulkan-PS5 commits `79e89d6` and `0dcc4b2` add that lower-layer regression
and a transfer-clear discriminator. Cleanup-first FW 5.500.008 run
`Vulkan-PS5/examples/qualification-logs/20260802T153944Z-format-attachments-run1.log`
used post-commit ELF SHA-256
`0381b48b521c68ea81137f09cd3b439a724b0b2f359240a2bc4cdae2f2e6e330`.
The explicit full-rect magenta `vkCmdClearAttachments` draw records, submits,
and fences successfully but reads back zero at the first sampled pixel. The
control then clears the same optimal, device-local B8G8R8A8 UNORM image with
`vkCmdClearColorImage` and passes exact magenta checks at the first, center,
and last pixels through the same image-to-buffer copy, mapped allocation, and
invalidation path. PID 108 self-exits and the runner independently finds no
matching `eboot.bin`. This isolates the active defect to OpenAGC's graphics
color-target write path; allocation, transfer writes, image-to-buffer copy,
host visibility, and BGRA interpretation are now directly qualified. Fix and
regress the graphics target binding/export/cache state before rebuilding or
launching Eden again.

OpenAGC commit `f7110fb` strengthens every writer release from GCR `0x603` to
Mesa-aligned `0x703`, adding the previously omitted GL2 invalidate while
retaining forward sequencing and GLM/GL2 writeback. Its exact packet fixtures
and all 19,924 host assertions pass, and the integrated Prospero build passes.
Cleanup-first A/B run
`Vulkan-PS5/examples/qualification-logs/20260802T154908Z-format-attachments-run1.log`
used ELF SHA-256
`bbbbac660ed5bd56123b8f808d72b3923b83dc4258a2dbdceea8f500638aba83`.
The result is unchanged: the graphics attachment clear reads zero and the
same-image transfer clear passes three exact magenta samples; PID 111 then
self-exits with no matching `eboot.bin`. The missing GL2 invalidate was a real
barrier weakness but is not the sole cause of this black output. The next
bounded discriminator must issue a hardcoded fragment-color draw without push
constants to the same image, then reuse the proven copy/readback path. A pass
would isolate the meta-clear shader or push-constant path; another zero would
keep color-target addressing, rasterization, and graphics completion ordering
as the remaining lower-layer suspects.

Vulkan-PS5 commit `5e8b3fd` adds the fixed-magenta fragment discriminator with
no descriptors or push constants. Cleanup-first FW 5.500.008 run
`Vulkan-PS5/examples/qualification-logs/20260802T155537Z-format-attachments-run1.log`
used post-commit ELF SHA-256
`d53cc055ee968ed1ae09ba553f1208cbc0f814b5a8f3f2d9a22130b320e170a9`.
The ordinary fullscreen graphics draw passes all three exact magenta samples
on the same legacy render pass, optimal device-local image, and readback path;
the transfer clear also passes, while `vkCmdClearAttachments` alone remains
zero. PID 114 self-exits and the runner finds no matching `eboot.bin`. This
qualifies OpenAGC color-target addressing, device-local graphics writes,
graphics-to-transfer visibility, and the fixed-shader raster path. The active
defect is now confined to Vulkan-PS5's meta attachment-clear pipeline or its
direct push-constant/state replay. Compare a normal push-constant fullscreen
draw on this same image before changing OpenAGC synchronization or surface
registers.

Vulkan-PS5 commit `6bfb04d` adds that same-image ordinary push-constant
control. Cleanup-first run
`Vulkan-PS5/examples/qualification-logs/20260802T155922Z-format-attachments-run1.log`
used ELF SHA-256
`8dcee21b1873af59ad626e8ff9a6d51b7013455955b2ba5719687bc95c6169a8`.
The fixed shader, ordinary `vec4` fragment push-constant draw, and transfer
clear all pass three exact magenta samples; only `vkCmdClearAttachments`
remains zero. PID 117 self-exits with no matching `eboot.bin`. The clear and
control fragment SPIR-V are instruction-equivalent apart from debug names, and
both ordinary and direct paths ultimately call OpenAGC's
`agcCmdPushConstants`. Therefore neither shader generation nor general push
constants explain the failure. The next fix belongs in Vulkan-PS5's direct
meta-clear replay or cached dynamic-rendering pipeline state; preserve the
passing ordinary controls while replacing or correcting that bypass.

Vulkan-PS5 front-face A/B commits `aedfde1` and `253addc` temporarily align
the meta pipeline with the passing control and then restore the original state.
Cleanup-first run
`Vulkan-PS5/examples/qualification-logs/20260802T160402Z-format-attachments-run1.log`
used ELF SHA-256
`d50010a33f2d247cf2d9ba57425ccc5bf126ee0d46abafe36dd93daed5b77cfb`.
Its result is identical: meta clear zero, all three controls exact magenta, and
PID 120 absent after self-exit. Front-face encoding is therefore eliminated.
The next lower-level action is to run the direct meta bind/push/viewport/target/
draw sequence with the known passing ordinary pipeline, or capture and diff
the emitted PM4 streams, to separate cached meta-pipeline construction from
the bypass replay itself. Do not return to Eden qualification until this exact
regression passes.

Vulkan-PS5's unexported native color-target control now performs that exact
discriminator without exposing an OpenAGC handle through the public Vulkan
ABI. Cleanup-first FW 5.500.008 run
`Vulkan-PS5/examples/qualification-logs/20260802T161848Z-format-attachments-run1.log`
used ELF SHA-256
`6afc4d5a8bb262b67497b9dfd6c1e66c94dc839b302079ce6d58f24734245207`.
The ordinary Vulkan draw and the direct native replay of its known-good legacy
pipeline both write exact magenta at all three samples. The ordinary
push-constant draw and transfer clear also pass, while only the cached meta
attachment pipeline still produces zero. PID 123 self-exits, and independent
checks find neither `eboot.bin` nor `eboot.elf`. This eliminates the direct
bind/viewport/scissor/color-target/draw bypass and confines the defect to meta
pipeline construction. Replace the invalid dynamic-rendering meta pipeline
with an active-render-pass/subpass-compatible legacy pipeline, bind the full
subpass attachment set, and retain an MRT slot regression before returning to
Eden.

The earlier terminal boundary was `vkCreateImageView`: Vulkan format 51
(`VK_FORMAT_A8B8G8R8_UNORM_PACK32`) with 2D view type returns
`VK_ERROR_FEATURE_NOT_PRESENT`, and the GPU thread catches the exception at
emulated time 5.83 seconds. The process then remains live until the guarded
web request expires at 120 seconds. The failure branch captures its `.klog`
before retiring PID 162, so it proves no kernel crash, coredump, or allocation
failure before the forced retirement but cannot prove clean teardown or a
leak-free exit. The runner's kill reported PID 162 absent, and an independent
exact-name postcheck also found no `eboot.bin` process.

The failure was a legal 2D depth-slice view of a 3D image created with
`VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT`, not a format-support failure. The
Mesa RADV gfx10 path confirms the required encoding model: retain the original
3D allocation base, encode a 2D-array resource descriptor, and select the
requested depth slices with its base/last-array fields. OpenAGC commit
`30e830c` adds that ABI-safe image flag and descriptor path with minified-depth
range validation and single-mip enforcement. Its generic suite passes all 19
CTest entries and 19,827 assertions, and its Prospero build passes. Vulkan-PS5
commit `2f23dd2` maps and validates the Vulkan contract, accepts both compatible
2D and 2D-array views, and adds the format/image/view regressions. Its focused
`vulkan_ps5.image_cube_array` test and full 62-test host suite pass, as does the
full Prospero build. Review follow-ups `2509110` (OpenAGC) and `0964bd8`
(Vulkan-PS5) accept legal depth-one 3D-compatible images, validate framebuffer
extent against the view's minified mip dimensions, and propagate the view mip
through color/depth render-pass and clear bindings. Command-level regression
now records a mip-1/slice-1 render pass, clear, and draw. OpenAGC passes all 19
CTest entries and 19,830 assertions; Vulkan-PS5 passes all 62 host tests; both
Prospero builds pass. The cleanup-first draw and presentation runs recorded
above later hardware-qualified this correction; it is no longer the active
gate.

## Scope

This plan targets PS5 homebrew built with `ps5-payload-sdk`. It does not port
the PS4/OpenOrbis frontend or inherit PS4 platform assumptions. PS4 work may be
used only as a behavioral reference after the equivalent PS5 contract has been
identified and tested independently.

The upstream source baseline for this plan is Eden revision `612409c7ba`; the
PS5 planning branch begins at `b5cdae421b`. The initial graphics baseline was
Vulkan-PS5 revision `e78b64eaf8`, OpenAGC revision `6a9b7bcac3`, and
openagc-psbc revision `ef8a98cb5e`, plus SDL2 2.30.12 and the pinned Mesa/Zink
integration recorded in the adjacent projects. The current construction-proof
revisions are pinned in the active diagnostic above.

## Active completion goal

Finish general-purpose Vulkan-PS5 support required by Eden at revision
`612409c7ba` and by other Vulkan applications. Completion requires all of the
following evidence; a startup-only probe or a single visible frame is not the
finish line:

- Refresh `Vulkan-PS5/analysis/eden-compatibility.md` against this exact Eden
  revision and the current OpenAGC, openagc-psbc, and Vulkan-PS5 revisions.
- Complete and qualify the uncompressed and BC format matrix actually required
  by Eden. Keep D24, ASTC, ETC/EAC, and unsupported storage-image combinations
  fail-closed until a native implementation or an explicit, tested conversion
  path exists.
- Implement the clear, color/depth/stencil attachment clear, blit, resolve,
  transfer, synchronization, and other command forms observed in real Eden
  workloads through public OpenAGC APIs. Vulkan-PS5 must not regain a private
  PM4, raw-address, firmware-selection, allocation, or submission path.
- Complete the full Prospero application build, PS5 surface/native-window WSI,
  and standard Vulkan entrypoint integration without Eden-specific driver
  workarounds.
- Prove Eden's production VMA allocator, shader cache, pipeline creation,
  command recording, rendering, presentation, failure cleanup, orderly
  teardown, and immediate relaunch with real workloads.
- Add host regression coverage and hardware oracles for every capability slice,
  update the relevant documentation, and create a scoped commit after each
  verified meaningful change.
- Qualify the final pinned identical ELF and library bytes first on FW 5.50 and
  then on FW 11.60 with bounded waits, clean logs, zero leaked processes or
  native allocations, and a targeted Vulkan CTS/deqp set. Until that endpoint
  evidence exists, Vulkan-PS5 remains explicitly non-conformant.

## Frontend decision

Use RmlUi for the production PS5 interface. Keep Dear ImGui optional and
limited to developer diagnostics.

The frontend is a hybrid of existing Eden architecture:

- Use `src/yuzu_cmd` as the native executable, boot-flow, input-subsystem, and
  emulation-lifecycle starting point.
- Use Android's navigation and screen workflows as the console-oriented UX
  reference. Do not port Kotlin, Android fragments, Android XML widgets, JNI,
  or `ANativeWindow` dependencies.
- Extract the useful lifecycle behavior from Android's `EmulationSession` into
  a PS5-native C++ controller.
- Use `frontend_common` and core APIs for configuration, content, firmware,
  mods, play time, profiles, and applet contracts.
- Do not port the Qt frontend. Its game models, workers, applets, and main
  window are coupled to Qt types, signals, models, and widgets, and pacbrew has
  no required Qt stack.

The pacbrew `ps5-payload-rmlui` recipe currently packages RmlUi 6.2 with SDL2,
FreeType, SDL2_image, and optional LuaJIT bindings. The package exists but must
still be built, installed into the active PS5 sysroot, and qualified on
hardware. Lua must remain optional; the first frontend must not require it.

## Native PS5 input and audio decision

The production Prospero frontend will use native PS5 services for controller
and emulation audio. SDL3 remains only a temporary command-frontend diagnostic
bridge while the native platform loop is implemented; SDL2 is not introduced
as a second incompatible runtime. The target architecture is:

- obtain the active user through `libSceUserService` and read controllers with
  `libScePad`, including buttons, sticks, motion, touchpad, light bar, and
  vibration where Eden exposes those capabilities;
- translate native pad state into `InputCommon::InputSubsystem` and RmlUi
  navigation, and replace SDL quit events with a bounded Eden-native
  lifecycle/stop signal;
- implement an `audio_core` sink backed by `libSceAudioOut`, retaining Eden's
  null sink as the fail-closed fallback until AudioOut qualification passes;
- remove SDL from the Prospero runtime target after native input, lifecycle,
  and event handling have hardware evidence.

The mGBA PS4 port is the behavioral reference for the AudioOut transport: a
48 kHz signed-16-bit stereo stream, four bounded 1024-frame packets, one
blocking output worker, producer/consumer synchronization, and explicit
close/join teardown. SharpEmu independently confirms the Gen5 AudioOut ABI for
`sceAudioOutInit`, `sceAudioOutOpen`, `sceAudioOutOutput`, and
`sceAudioOutClose`; the installed PS5 payload SDK exports those symbols. A
small standalone PS5 probe must still qualify the exact declarations, format,
buffer length, blocking behavior, error handling, and teardown before Eden's
production sink uses them.

`SDL2_mixer` is not an Eden emulation-audio backend: Eden already produces and
mixes PCM, and SDL2_mixer does not provide the missing PS5 device driver. It may
be packaged only for optional launcher/UI sounds. pacbrew may continue to
package SDL2/SDL3 for interim tools or third-party package dependencies, but
core PS5 controller and audio correctness must not depend on either SDL branch.

## UI architecture

Add a `src/ps5` frontend rather than adding PS5 conditionals throughout Qt or
Android code. Its UI boundary should contain:

- `Ps5Application`: initialization, main loop, shutdown, and process verdict.
- `Ps5EmulationSession`: initialize, boot, pause, resume, stop, and teardown.
- `Ps5EmuWindow`: Eden `Core::Frontend::EmuWindow` implementation and
  framebuffer layout.
- `RmlSystemInterfacePs5`: time, logging, cursor/clipboard stubs, and safe
  unsupported behavior.
- `RmlFileInterfacePs5`: read-only packaged assets plus writable Eden user
  paths.
- `RmlRenderInterfaceVulkanPs5`: RmlUi geometry, textures, scissor, transforms,
  and frame submission through ordinary Vulkan APIs.
- `RmlInputPs5`: `libSceUserService`/`libScePad` controller state translated
  into Eden input and RmlUi focus/navigation events.
- A small presentation model that exposes Eden data to RML documents without
  making core or frontend-common code depend on RmlUi.

The RmlUi renderer must use Eden's Vulkan instance/device/queue and the
Vulkan-PS5 swapchain. It must not create a second VideoOut implementation,
inspect firmware, use OpenAGC handles, or submit an independent native queue
stream. Launcher rendering and the in-game overlay must share the same Vulkan
ownership and bounded synchronization model.

## Reusable Eden components

| Need | Reuse | PS5 work |
| --- | --- | --- |
| Emulator boot and shutdown | `src/yuzu_cmd/yuzu.cpp`, Android `EmulationSession` behavior | Remove CLI/Android assumptions and expose a PS5 session controller |
| Window contract | `Core::Frontend::EmuWindow`, SDL Vulkan window patterns | Add a PS5 window/surface path and Vulkan-PS5 WSI hookup |
| Configuration | `frontend_common::Config`, `Common::Settings` | Add PS5 config paths and RmlUi bindings |
| Content and firmware | `frontend_common` managers and Android native calls | Add controller-driven file selection and progress/error models |
| Game metadata | Core loader/VFS metadata paths | Add a non-Qt/non-Kotlin game-library model and icon cache |
| Input | `InputCommon::InputSubsystem` | Map native `libScePad` state into Eden controller capabilities and RmlUi navigation |
| Audio | `audio_core` sink interface | Implement and qualify a bounded `libSceAudioOut` sink; retain the null sink as fail-closed fallback |
| Applets | `core/frontend/applets` contracts | Implement RmlUi error, profile, controller, and software-keyboard applets first |
| Renderer | Eden Vulkan renderer and VMA | Link Vulkan-PS5 and add PS5 surface/entrypoint discovery |
| UI workflows | Android navigation graphs and settings taxonomy | Re-express them as RML/CSS documents and C++ presentation models |

## Required UI screens

Implement screens in this order:

1. First-run setup for user data, production keys, firmware, and game folders.
2. Game-library grid/list with icons, favorites, refresh, and launch.
3. Game information and per-game configuration.
4. Global system, CPU, renderer, audio, and controller settings.
5. Profile selection and profile management.
6. Shader-loading and boot progress with actionable errors.
7. In-game quick menu: resume, pause, settings, restart, and stop.
8. Software keyboard, controller configuration, and error applets.
9. Add-ons, updates/DLC, save management, Amiibo/cabinet, and other secondary
   workflows after the basic game loop is stable.

Web applets, multiplayer UI, updater, driver downloads, Discord integration,
and desktop-only utilities are initially disabled. Each returns a deliberate
unsupported result where Eden requires a frontend contract; none may silently
succeed.

## Platform work

### Build and dependencies

- Add an explicit Prospero/PS5 CMake platform. Do not masquerade as Android,
  Linux, FreeBSD, or OpenOrbis.
- Cross-build C++20 with ps5-payload-sdk libc++ and static/package-resolved
  dependencies.
- Disable Qt, cubeb, RenderDoc, update checking, Discord, web services, Wi-Fi
  scanning, libusb, and host-only crash dump paths for the first target. Keep
  SDL3 only in the temporary command-frontend diagnostic build until the
  native PS5 event/input loop replaces it.
- Consume pacbrew packages for OpenAGC, openagc-psbc, Vulkan-PS5,
  Vulkan-Headers, RmlUi, FreeType, FFmpeg, OpenSSL, Boost, fmt, zstd, lz4,
  Opus, and other dependencies only as Eden reaches them. SDL2, SDL2_image,
  SDL2_mixer, or SDL3 packages are allowed for interim tooling, optional UI
  features, or unavoidable third-party package edges, not as the production
  Prospero controller or emulation-audio layer.
- Add a pacbrew Eden package only after a reproducible standalone Prospero
  build exists.

### Runtime and OS abstraction

- Qualify x86-64 Dynarmic/Xbyak executable-memory allocation, protection
  changes, instruction-cache behavior, and teardown on PS5.
- Implement PS5 paths for host memory, fibers, TLS, threads, timing, signals or
  exceptions, filesystem roots, logging, sockets, and power/lifecycle events.
- Keep every wait bounded and ensure shutdown works after partial
  initialization, failed game load, and renderer failure.
- Avoid Android JNI, Linux procfs/udev, desktop environment, and PS4
  OpenOrbis-specific branches.

### Graphics

- Link Eden to Vulkan-PS5 through standard Vulkan entrypoints. No Eden code may
  include private OpenAGC headers or low-level GPU interfaces.
- Add the PS5 surface/native-window bridge and static/shared entrypoint lookup.
- Prove VMA allocation policies, shader compilation, pipeline cache, command
  recording, swapchain recreation, and clean object retirement with an actual
  Eden workload.
- Add Vulkan/OpenAGC capabilities only when required by a captured Eden call
  trace and supported with generic plus hardware tests.

## Vulkan-PS5 dependency status

The graphics foundation is usable but Vulkan-PS5 is not finished for Eden.

Completed evidence at the current baseline:

- The native migration audit has zero direct low-level calls.
- The strict pinned Mesa/Zink capability report has zero gaps and advertises
  Vulkan 1.2.
- The Eden startup profile reports zero extension, feature, limit, and queue
  gaps.
- SDL/EGL/Zink context creation, exact pixel readback, visible presentation,
  teardown, and immediate relaunch passed identical binaries on FW 5.500.008
  and FW 11.600.005.
- `VK_EXT_depth_clip_enable` is qualified on both endpoints without the former
  Zink warning.
- Eden's 109 unique guest image formats have been audited: 68 are direct
  Vulkan-PS5 image formats, ASTC and ETC/EAC remain explicit transcode cases,
  D24 remains fail-closed, and RGB32 forms remain buffer-only.
- The scalar/vector attachment matrix has exact-pixel FW 5.50 evidence for 36
  formats, including signed and unsigned integer fragment exports.
- 2D disjoint-subresource self-blits and mixed 2D/3D color blits are implemented
  and FW 5.50-qualified; feedback-loop self-blits remain deliberately
  fail-closed.
- Eden now has a `Ps5` window-system type, maps it to Vulkan-PS5's
  VideoOut-backed `VK_EXT_headless_surface`, and resolves the statically linked
  `vkGetInstanceProcAddr` on Prospero. The complete host `video_core` target and
  a Prospero compiler syntax pass cover this integration.
- Eden now has an explicit Prospero bootstrap target. Its Release O3 ELF uses
  only standard Vulkan entrypoints plus the PS5 system-exit service and passed
  two identical-hash 600-frame instance/surface/device/swapchain/clear/present
  lifecycles on FW 5.500.008, including clean teardown and immediate relaunch.
  The tested SHA-256 is
  `3e07642449b6dddd371cb233bddb88a62a70a50a15efb20f43f028493591fa9e`.
- The same PS5 frontend now compiles Eden's pinned production VMA 3.3.0
  implementation and exercises mapped upload/readback plus device-local
  allocations through an upload→device→readback copy oracle. Exact Release O3
  bytes `5df47079ba9dfb0f00c052f3e721670e04fd1ee75b9beb8f93a0c1590d74f778`
  passed twice on FW 5.500.008, verified all 4,096 bytes, returned to
  `allocations=0 bytes=0`, presented 600 frames, tore down, and immediately
  relaunched.

Remaining work that can block real Eden games:

- Expand the bootstrap-only Prospero platform into Eden's full dependency and
  application build. The minimal Vulkan lifecycle has hardware credit, but it
  does not yet construct the guest shader cache, renderer command streams,
  RmlUi, input, audio, or game boot. Eden's production `MemoryAllocator`
  wrapper is now part of the Prospero bootstrap, but that exact wrapper build
  still needs FW 5.50 execution. The underlying production VMA implementation
  and its Vulkan allocation contract are hardware-proven.
- Add ASTC and ETC/EAC conversion in Eden or a general-purpose Vulkan path when
  actual game traces require them. Keep D24 and unsupported storage-image
  combinations honest until native support or conversion is implemented.
- Continue implementing any remaining clear, blit, depth/stencil, attachment,
  and resolve forms only from captured application requirements; unsupported
  forms must continue to fail closed.
- Run the broader native-only Vulkan feature sequence on FW 5.50 and the full
  advertised-feature endpoint sequence on FW 11.60; the Zink endpoint pass is
  not a substitute for complete driver qualification.
- Add targeted Vulkan CTS/deqp coverage and retain a non-conformant claim until
  that evidence supports otherwise.
- Exercise the real Eden renderer, shader cache, VMA use, presentation, game
  transitions, teardown, and immediate relaunch. A zero-gap startup probe does
  not prove game compatibility.

The older `Vulkan-PS5/analysis/eden-compatibility.md` revision matrix contains
stale pre-migration statements, including old direct-call and presentation
status. Refresh it against this Eden revision before treating its detailed
format/command inventory as a release gate.

### Progress evidence

On 2026-08-02 the first Eden-side Vulkan integration slices completed:

- A clean macOS configuration built all 614 objects in the `video_core` target.
- The PS5 compiler accepted `vulkan_library.cpp`, `vulkan_instance.cpp`, and
  `vulkan_surface.cpp` with `__PROSPERO__` enabled and warnings-as-errors source
  conventions preserved.
- The initial PS5 compile probe exposed and then closed an Xlib/Wayland
  fallthrough in both the instance-extension and surface-creation switches.
- The explicit Prospero CMake path builds
  `eden-ps5-vulkan-bootstrap.elf`. The final Release O3 bytes passed twice on
  FW 5.500.008 for 600 presented frames, bounded synchronization, full
  teardown, exact process absence, and immediate relaunch. The investigation
  also fixed a general Vulkan reusable-clear state bug and an optimized
  OpenAGC VideoOut attribute-stack corruption; it did not add an Eden-specific
  graphics workaround.
- The next sub-gate compiled the exact VMA implementation used by Eden's
  frontends into that target. Two identical-hash FW 5.50 runs proved host
  upload, device-local allocation, GPU copies, invalidated readback, zero live
  VMA allocations at teardown, 600 presentations, and immediate relaunch. The
  next renderer step is the real `MemoryAllocator`/scheduler/shader-cache path,
  not another allocation harness.
- The in-progress compute sub-gate now builds Eden's production
  `vulkan_quad_indexed.comp` with the host shader tool, binds its actual
  descriptor and push-constant interface, checks GPU-generated quad indices
  through VMA readback, and recreates its Vulkan pipeline from serialized
  cache bytes. FW 5.50 exposed and hardware-confirmed a Vulkan partial-range
  descriptor-state bug; Vulkan-PS5 `01a49cb` fixes it. The subsequent
  1,024-thread no-store result isolated OpenAGC's all-ones
  `COMPUTE_RESOURCE_LIMITS`; OpenAGC `2be2b1c` derives the gfx10 wave-count
  policy and is generic/Prospero-build clean. The corrected exact ELF remains
  pending FW 5.50 execution because the console is reachable but its guarded
  websrv/FTP launcher services are not currently running.
- The bootstrap now compiles Eden's production `vulkan_wrapper.cpp` and
  `vulkan_memory_allocator.cpp` rather than calling `vmaCreateBuffer`
  directly. Its upload, device-local, and readback allocations use
  `MemoryAllocator::CreateBuffer`; mapped access, cache maintenance, and
  destruction use Eden's `vk::Buffer` RAII path. A context constructor keeps
  the normal renderer-owned `Device` constructor unchanged while allowing the
  staged bootstrap to adopt its existing Vulkan/VMA device. The complete host
  `video_core` target and two clean Release Prospero builds pass, and the two
  independently built ELFs are byte-identical at
  `2abe2f150839748ae3a06532ae18a33d61780c6d73341bfd219c54d8e63b58e4`.
  Hardware qualification is pending because FW 5.50 websrv/FTP remain closed.
- The first full (non-bootstrap) Prospero configuration exposed a host zstd
  config file that reported itself found without exporting any target Eden can
  link. `Findzstd.cmake` now rejects that unusable result instead of aliasing a
  nonexistent target, allowing CPM/pacbrew fallback. Host configuration and
  `video_core` still build, while the Prospero configuration now advances to
  SDL3's unsupported Unix desktop-window check. That SDL platform selection is
  the next full-build dependency gate.
- The PS5 dependency policy now explicitly selects SDL3's Unix-console mode:
  Eden retains controller/event support while its native PS5 frontend remains
  responsible for Vulkan surfaces, so no X11 or Wayland target enters the
  cross-build. `AddJsonPackage` now also honors its documented per-call option
  override, allowing the web-disabled PS5 build to compile cpp-httplib without
  importing host OpenSSL; web-enabled builds still require a proper pacbrew
  OpenSSL target. Host configuration and `video_core` remain clean, and full
  Prospero configuration now reaches the missing FFmpeg target, the next
  dependency/package gate.
- The full Prospero configuration now resolves the pacbrew-compatible OpenSSL
  3.6.2 sysroot install and Eden's source-built FFmpeg 8.0 dependency. FFmpeg
  package recipes, patches, and PS5 build references are available in the
  pinned local pacbrew checkout at
  `/Users/bizkut/Downloads/PS5/homebrew/pacbrew-repo`; use that tree as the
  Prospero FFmpeg source/reference rather than inventing a second packaging
  path. FFmpeg
  must link its configure probes through `prospero-clang`, because raw
  `prospero-lld` omits crt/libc/SceLibcInternal and incorrectly reports PS5
  math functions unavailable. The PS5-only linker-driver selection preserves
  other platforms. Boost.Process's FreeBSD shortcut also assumes
  `close_range`, which the Prospero libc does not expose; the PS5 build now
  selects Boost's portable `/dev/fd` implementation through a narrow patched
  capability macro. FFmpeg's four required static libraries, Boost.Process,
  and Eden's complete Release `video_core` target now cross-build for
  Prospero. A clean native macOS `video_core` build also passes, confirming the
  platform-specific selections do not regress the host path.
- The full SDL3 command frontend now cross-builds and links as a production
  Prospero PIE, `eden-ps5.elf`. SDL3 remains the event/controller frontend,
  while `EmuWindow_SDL3_VK` selects Eden's PS5 window-system type so
  Vulkan-PS5 owns the native VideoOut surface without an X11, Wayland, or SDL
  native-window dependency. The complete link includes Eden core, scheduler,
  shader recompiler, shader-cache dependencies, VMA renderer, FFmpeg,
  OpenSSL, Vulkan-PS5, OpenAGC, and VideoOut. The Prospero SDK's narrower
  FreeBSD libc required two capability-based network fixes: omit DCCP when
  `IPPROTO_DCCP` is absent, and read an interface name directly from
  `sockaddr_dl` because the SDK declares but does not export `link_ntoa` or
  `link_ntoa_r`. The resulting unstripped 63 MiB ELF has discovery hash
  `b5033930f2637cdb8e4bb90e1201d61b2afba301c5f28da90857308911594105` before
  the launch-contract slice; this is historical build evidence, not a hardware
  qualification pin. A native macOS `core` rebuild passes after the shared
  network change.
- Websrv launches do not provide a dependable argv contract, so the production
  ELF now reads `/data/homebrew/eden_ps5/eden.launch` before creating
  `Core::System` or any Vulkan object. A missing file and exact `init\n` select
  the no-game preflight. A game launch is exactly
  `game\n<absolute-path>\n`, optionally followed by one exact
  `frames=<1..108000>\n` line. The complete file is capped at 1,050 bytes and
  the path at 1,024 bytes; relative paths, controls, unknown/extra lines,
  malformed numeric limits, I/O, and oversized inputs fail closed. The exit callback now injects an SDL quit
  event on PS5 so Eden reaches `Pause` and `ShutdownMainProcess` instead of
  calling `exit` from the emulation callback. After C++ teardown and logger
  shutdown, the frontend requests app termination through
  `sceSystemServiceKillApp`, with a two-second grace bound and `_Exit` fallback
  rather than an unbounded process loop. The isolated host CTest covers exact
  modes, malformed forms, missing-file defaulting, and file-size bounds. The
  full Release Prospero frontend rebuild passes at discovery hash
  `0ee124ca63263d6c3d101405261703a80784b0f90cadffcc0ee6d0da00959262`;
  hardware qualification and pinning remain pending.
- `tools/run_fw550_init.sh` composes the shared Vulkan-PS5 guarded runner for
  two consecutive full-frontend init launches. It requires explicit ELF and
  cleanup SHA-256 pins, uploads the committed `init\n` sidecar under the exact
  production remote path, verifies all three artifacts after upload, runs
  cleanup first, scopes klog to the launched PID, requires the stable
  `eden-ps5: INIT PASS` oracle, checks the kernel `KillApp` lifecycle and exact
  process absence, then repeats immediately. The production game path emits a
  separate `eden-ps5: GAME PASS` only after `ShutdownMainProcess`. The shared
  runner's sidecar upload and fail-closed hash/missing-file behavior is covered
  by Vulkan-PS5 CTest. The current rebuilt discovery ELF is
  `f20a4ca8a55720c55f3c5311a8a0fff4d600320c832d4191a150950883476a9f`;
  it remains unqualified while FW 5.50 ports 8080, 2121, and 3232 are closed.
- `EmuWindow_SDL3::OnFrameDisplayed` now provides the qualification frame
  oracle. When the optional sidecar limit is nonzero, an atomic counter queues
  the same SDL quit event after the exact requested composite/present count;
  there is no wall-clock stop thread and teardown remains on the main frontend
  path. The counter saturates at its limit, and the post-shutdown verdict
  compares the observed count before printing its exact frame oracle. The guarded
  The original `tools/run_fw550_2048.sh` gate pins and re-verifies the local
  `2048.nro` (`cd7e7f343830920196590d99c82a9f1ab8a375eeaeb943fa6c671aa68250a20d`),
  uploads a committed 600-frame game sidecar, requires `GAME PASS 600 frames` only after
  `ShutdownMainProcess`, and repeats immediately. The second launch also
  requires Eden's `Total Pipeline Count` diagnostic to be nonzero, proving
  that the production shader/pipeline cache was populated and observed across
  the repeated workload instead of accepting presentation alone. The shared
  runner fails closed if that secondary oracle is missing. Parser CTest, the
  shared runner's matching/missing-oracle coverage, and the complete Release
  Prospero frontend build pass; the current discovery ELF hash is
  `5bdedb4c7f342fa82adc0b073bc12a34beca3e6eeaf2f16872a019be04fad019`.
  This describes the original 2048 long-run gate; the current InvadersNX
  substitution and its required evidence are recorded below.
- Eden now has an explicit `__PROSPERO__` guest-memory backend instead of the
  unsuitable FreeBSD anonymous-SHM path. It disables 4 KiB fastmem on the
  16 KiB PS5 host, reserves one contiguous backing range, maps it from tracked
  64 MiB direct-memory chunks, and releases every chunk after unmapping. A
  dedicated 4 GiB probe touched both ends of all 64 chunks and passed two
  consecutive cleanup-first, SHA-pinned FW 5.50 launches with clean process
  exit and kernel logs. The qualified discovery probe hash is
  `1ce0b9403b21305e09774a1085d352991a62c349ea3a3dd6feefed2090b25535`;
  the full Eden/2048 gate remains pending. Sparse commitment remains a later
  optimization because Eden currently requires a stable contiguous raw
  backing pointer.
- The next startup failure was a separate 4 GiB anonymous `VirtualBuffer`,
  identified as the 39-bit process page table rather than guest RAM. Prospero
  now preserves the full logical table while committing zero-initialized
  64 KiB chunks on demand, and it disables Dynarmic's contiguous page-table
  optimization so CPU accesses use Eden's existing memory callbacks. Smaller
  ordinary `VirtualBuffer` objects use tracked, 16 KiB-aligned CPU flexible
  mappings with exact release. Probe bytes
  `ba68ab06b05540868cdd828c94c41d47ec8c022861fe3ae1eae50617ca4290a5`
  passed twice on FW 5.500.008, exercising far-apart sparse entries, a 64 MiB
  flexible allocation, move/resize, teardown, and immediate relaunch. The
  exact runs are `20260802T045445Z-swapchain-run1` and
  `20260802T045456Z-swapchain-run1`; both target klogs show successful process
  retirement with no panic, fault, assertion, or stale process.
- The first full `2048.nro` launch after the page-table gate reached Dynarmic
  construction and failed with `Xbyak::Error: can't alloc` because the generic
  allocator requested a 512 MiB executable `mmap` on Prospero. The interim
  flexible-memory cache subsequently proved unsuitable: FW 5.50 accepted the
  RWX request but faulted on the first instruction fetch at `0x303404010`, the
  exact Dynarmic dispatcher address. Prospero now enables Dynarmic's existing
  no-execute support, creates each cache with `sceKernelJitCreateSharedMemory`,
  maps one RW address, and performs checked RW-to-RX/RX-to-RW transitions
  around emission and patching. Create, map, descriptor-close, protection, and
  unmap failures are diagnosed and fail closed. The per-core A32/A64 cache is
  32 MiB because JIT shared memory remains physically charged to the native
  app; four 64 MiB caches left no headroom even for a later 64 KiB sparse page
  table chunk (`0x8002000c`). Ordinary flexible-buffer failures now report the
  exact requested/aligned size and kernel result.

  Cleanup-first FW 5.50 run `20260802T090402Z-swapchain-run1` proved the W^X
  transition removed the execute fault and reached guest execution. Run
  `20260802T090810Z-swapchain-run1` pinned the 64 KiB out-of-memory boundary.
  Artifact `6b49e812f4589d66af38d3119521c5edc51d68e1c3625d873758d5ab391e1373`
  with the initial 16 MiB caches then crossed that boundary, created the managed VI
  display layer, initialized HID and NVDRV, and stopped at the next independent
  `VK_ERROR_FORMAT_NOT_SUPPORTED` boundary in
  `20260802T090914Z-swapchain-run1`. The full Prospero frontend build, native
  host `core`/`video_core` builds, and `eden.ps5_thread_budget` CTest pass. This
  is discovery evidence, not a clean-run qualification: the format exception
  still triggers the coredump/forced-retirement path.
- The 16 MiB cache was exactly Dynarmic's prelude reservation and failed on the
  first later protection change. Raising A32/A64 caches to 32 MiB clears that
  boundary without returning to the 64 MiB-per-cache exhaustion case. Eden now
  qualifies presentation images by their real operations: raw CPU uploads use
  only `TRANSFER_DST | SAMPLED` and metadata-free linear tiling, while capture
  images use only `TRANSFER_SRC | COLOR_ATTACHMENT`. Allocation failures print
  the complete image contract and Vulkan wrapper failures identify their call.
  Vulkan-PS5 separately accepts the Vulkan-legal `GENERAL` color-attachment
  reference layout while retaining fail-closed rejection of other layouts.
  Its host command-recording test and Prospero static build pass.

  Cleanup-first FW 5.50 artifact
  `46461eb7697c5942c382df27cd3a606998576433cf5e6b0cb8b38eec3633b876`
  in `20260802T093051Z-swapchain-run1` clears the JIT cache-size, image-format,
  image-copy-metadata, and render-pass-layout boundaries. It reaches first
  presentation pipeline compilation and identifies the next independent error:
  OpenAGC rejects primitive-shader user-SGPR reflection with native result
  `0x8089000b`, surfaced as `VK_ERROR_INITIALIZATION_FAILED`. Eden's dedicated
  GPU-thread exception channel records the first failure, wakes blocked fence
  waiters, requests frontend exit, suppresses `GAME PASS`, and prevents an
  uncaught-exception coredump. The runner still had to retire the process after
  its 120-second bound and reported a 16 KiB VM-resource leak, so clean teardown
  and immediate relaunch remain unqualified.
- Eden's Vulkan instance creation now forwards the API version negotiated from
  `vkEnumerateInstanceVersion` instead of hard-coding Vulkan 1.3. This keeps
  the application honest against Vulkan-PS5's current Vulkan 1.2 contract.
  The FW 5.50 run `20260802T051019Z-swapchain-run1` identified the previous
  `VK_ERROR_INCOMPATIBLE_DRIVER`; the rebuilt ELF then reached physical-device
  suitability, OpenAGC initialization, and PSBC SPIR-V-to-ACO compilation in
  `20260802T051216Z-swapchain-run1`. That artifact is
  `d7aacb715a7ec61e83d0fcecfd2d5965529d03d5cd062ab739c5bd74fe7945e3`.
  Subsequent guarded slices closed unnormalized-sampler, API 55 compute-scratch,
  and consecutive descriptor-binding gaps. The historical construction
  artifact `4eae3b998f9a92664d41b86325a62bc8f9d2186a8c592e471ac180038923e490`
  reaches the final rasterizer checkpoint in
  `20260802T074820Z-swapchain-run1`. Follow-up candidate
  `847db1f2b0e66eaa19a43bcc35afb3e2f39de215c1babf4c7313d157def1f72e`
  removes the `dsp`/Opus startup boundary and reaches the same checkpoint in
  `20260802T081021Z-swapchain-run1`. The one-worker/SDL-only host-input policy
  then clears the VI thread boundary in `20260802T085539Z-swapchain-run1`; the
  32 MiB per-core cache policy clears the 16 MiB prelude-capacity and 64 MiB
  flexible-memory exhaustion boundaries. The narrowed presentation resource
  policy and Vulkan-PS5 `GENERAL` render-pass support then clear the guest
  image-format boundary. OpenAGC `5ff3eea` closes the primitive-shader
  reflection rejection in host/build qualification; its first integrated
  replay stopped earlier at the unsupported JIT-shm/mprotect hybrid. Later
  cleanup-first runs hardware-qualified the same-address W^X success path and
  the 29-SGPR presentation pipeline through eight presented frames. The
  current production gate is the two-run 600-frame FW 5.50 workload and
  immediate relaunch recorded in the immediate next slice.

## Milestones and gates

### P0: reproducible skeleton

- Cross-configure Eden for PS5 using installed pacbrew dependencies.
- Build a minimal ELF with Qt and Android excluded, then remove the temporary
  SDL3 command-frontend dependency once native PS5 lifecycle/input is active.
- Initialize logging, paths, settings, `libScePad` controller input, RmlUi,
  Vulkan, and clean teardown without booting a game.
- Add host-testable navigation and failure-cleanup tests.

### P1: launcher

- Render the setup flow and game library through RmlUi.
- Scan a configured directory, display metadata/icons, persist settings, and
  select a profile entirely with a controller.
- Pass repeated launch/exit and missing/corrupt asset failure gates.

### P2: renderer bring-up

- Boot a homebrew Switch application through Eden's Vulkan renderer.
- Capture the exact Vulkan calls and close only genuine general-purpose ICD
  gaps.
- Prove visible output, bounded waits, clean teardown, and immediate relaunch
  first on FW 5.50, then replay identical bytes on FW 11.60.

### P3: playable loop

- Add loading UI, quick menu, pause/resume/stop, controller configuration,
  audio, software keyboard, and error/profile applets.
- Run `2048.nro`, then a small representative game set covering textures,
  depth, compute, caches, save data, and restart.

### P4: packaging and longevity

- Package Eden and all pinned dependencies through pacbrew.
- Record ELF/library SHA-256 values and dependency revisions.
- Run a 30-minute game/launcher cycle, repeated game switching, teardown, and
  immediate relaunch on FW 5.50 and FW 11.60.
- Require no leaked processes, unbounded waits, validation errors, native
  lifetime failures, or increasing memory plateau.

## Immediate next slice

The 2026-08-02/03 cleanup-first FW 5.500.008 investigation now proves that
legacy-compatible color meta pipelines create, record, submit, and self-exit
without an API error, but the first color-target write to the new image still
reads zero. OpenAGC commit `ed7b9df` adds the Mesa GFX10.3 color-writer release
sequence (`FLUSH_AND_INV_CB_META` followed by
`FLUSH_AND_INV_CB_DATA_TS` and forward-sequenced GCR `0x703`); all 19,953
exact host assertions pass. Cleanup-first ELF SHA-256
`65b3b427f7c3750eff5b962cdbd7e00097deadfc5ecd47885ff3012897c2952c`
in log `20260802T170935Z-format-attachments-run1.log` is unchanged: the first
clear is zero and PID 150 is absent afterward. The independent reset run
`20260802T171114Z-format-attachments-run1.log` proves that a fixed shader drawn
in the same command after a compute zero reset is also zero, while the next
submission's direct native fixed-pipeline helper writes exact magenta from
that confirmed-zero image. Log `20260802T171642Z-format-attachments-run1.log`
then proves that the direct legacy-compatible meta pipeline also writes exact
magenta when run without another reset. Shader export, NGG, push constants,
color-target binding, and color-to-transfer readback are therefore qualified;
the remaining defect is a first-use or same-submit initialization/ordering
dependency. Acquire-only and full CB-flush experiments for
`UNDEFINED -> RenderTarget` in logs `20260802T172046Z` and
`20260802T172202Z` did not change the zero result and were reverted.
Vulkan-PS5 commit `3870834` then split the oracle into two submissions. Its
cleanup-first ELF SHA-256
`f4a35ad357337b92c866c25a2fb83beb2c00709954eb1db323683f7816f0feca`
and log `20260802T172755Z-format-attachments-run1.log` prove all 3,686,400
baseline bytes are zero after submission A; submission B contains only the
production attachment clear and readback but remains zero; the next ordinary
fixed-shader draw writes exact magenta. PID 171 is absent afterward. This
eliminates first-use allocation and cross-submission initialization: the
active defect is specifically the production meta-clear replay/state path.
Every run used the pinned cleanup ELF and ended with exact `eboot.bin`
absence. Because the user is away from the console, automated readback/process
evidence may continue, but visible presentation must remain unverified until
direct observation is available.

The first direct-production rewrite then replaced the recursive color-clear
commands with the same native bind/push/viewport/complete-MRT-target/draw
sequence used by the passing helper, including layered rectangles and a shared
fail-closed restoration epilogue. The focused host command-recording test
passes, but cleanup-first ELF SHA-256
`f9baef2629de7c5b5a4a0c5d639a71361f1b1eeffb575e48505dde275247a9f5`
in `20260802T173413Z-format-attachments-run1.log` still reports exact zero for
the production clear; the later fixed and direct-native controls remain exact
magenta and PID 174 is absent. That production rewrite was therefore reverted
instead of committed. The strongest remaining distinction is temporal: the
failed clear is the process's first graphics pipeline/default-state emission,
whereas the passing graphics controls execute after it. The next oracle must
put the fixed shader first, then repeat it after a separate priming submission,
before changing meta-clear code again.

That temporal diagnosis is now superseded by an exact alternating-color
oracle. Vulkan-PS5 commit `ae9da26` makes a fixed-magenta draw the first
graphics operation, verifies a separate zero reset, runs the production clear,
then repeats the fixed draw. The initial cleanup-first ELF
`c28109962ae8db810a77638324a5b695d0e8bc50685641749d6addada0332a7e`
in `20260802T174750Z-format-attachments-run1.log` reads zero immediately after
the first magenta draw, then reads magenta immediately after the following
zero clear. The extended ELF
`7426cf8c4dac8d136af8f76498c566baeb5b1d79f559c48bc2fb0a623a55c213`
in `20260802T174913Z-format-attachments-run1.log` continues the sequence:
production magenta reads zero, while the next magenta draw reads magenta.
This proves that all three writes execute but each in-command consumer can
overtake the preceding cache release; the submission-tail fence exposes the
write only after that consumer has already copied stale data. It also
invalidates the proposed graphics `CONTEXT_CONTROL` change, which was not
applied.

OpenAGC commit `7b60617` fixes the owner. Every command buffer now owns a
dedicated four-byte GPU completion cell. An ordinary same-queue transition
from a writing usage emits its existing Mesa-aligned ReleaseMem to that cell,
then immediately waits for the monotonically increasing value before allowing
the next consumer. Explicit v2 ownership release/acquire labels are unchanged,
capacity preflight includes the exact seven-dword wait, begin/reset clears and
flushes the cell, and packet tests verify the shared nonzero address and values.
The focused suite passes 19,966 assertions; the full 19-test matrix passes; the
Prospero build and Vulkan command-recording test pass. Cleanup-first FW
5.500.008 ELF
`9afb34077db63206712be1a6596ba34f41444e312a7fbbf036d13052ed96d9e0`
in `20260802T175715Z-format-attachments-run1.log` passes the first fixed draw,
all 3,686,400 bytes of the independent zero reset, the production
`vkCmdClearAttachments`, and the second fixed draw. PID 183 self-exits, the
runner independently finds no exact `eboot.bin`, and no extra compute partial
flush is required for this path. Because the user is away from the console,
these automated readback and lifecycle results do not qualify visible output.

The integrated Eden rebuild against those commits produced ELF SHA-256
`1b9684d472f3732107f281944ddb6add91bf6155467b3b606f35fbbca4f21289`.
Its first 600-frame attempt,
`Vulkan-PS5/examples/qualification-logs/20260802T180320Z-swapchain-run1.log`,
reaches eight successful native presents but does not reach 100 presents or the
600-frame oracle within the 60-second guard. The cleanup trap retires PID 186
and independently proves exact process absence. This is bounded progress, not
600-frame, relaunch, teardown, or visible-presentation qualification.

Two cleanup-first standalone oracles then eliminate the fresh intermediate
image and cross-submit semaphore paths. The first uses Eden's 1280x720 mutable,
extended-usage `COLOR_ATTACHMENT|TRANSFER_SRC` image, `UNDEFINED` first use,
`GENERAL` render-pass attachment, full magenta production clear, and transfer
readback. ELF SHA-256
`f8b94a50401f86d73f0a24a28cc4a6955bf2f034b8c45129a82dc377566da9ef`
passes in `20260802T181422Z-format-attachments-run1.log`, with PID 190 absent.
The split-submit variant signals a render-ready semaphore from the graphics
submit and waits at `TRANSFER` in a second command buffer before readback. ELF
SHA-256
`1bd964ca67371ab8c8dd5555138b6cfdb124ee7ab2462c62cb5548ff5cbeb4be`
also passes in `20260802T181717Z-format-attachments-run1.log`, with PID 193
absent. The temporary standalone code was removed after collecting evidence so
the committed attachment regression remains reachable.

The earlier WindowAdapt control accidentally counted the applet-capture pass,
whose separate `applet_frame` has no presentation readback buffer. Consequently
its sequence-zero magenta clear and layer suppression never targeted the frame
copied to the swapchain. The corrected Prospero-only qualifier counts only
frames that own `qualification_readback`. Cleanup-first ELF SHA-256
`78de222ef564d6416f79f168e625b53f7195eb2f1f6d3d19108b3c25ff440396`
passes the committed eight-frame sidecar in
`20260802T183213Z-swapchain-run1.log`: presented sequence zero reads exact
magenta from the intermediate image (`ff00ffff`, 48 of 64 sampled bytes
nonzero), while the swapchain image remains exactly zero. PID 224 and the
global exact-name check are absent after bounded teardown. This moves the
active black-output owner to Vulkan-PS5's scaling `vkCmdBlitImage` path. A
forced `vkCmdCopyImage` A/B is invalid because the 1280x720 intermediate and
surface extents differ; it fails command recording and is reverted. No visual
claim is made while the user is away from the console.

A second cleanup-first A/B forced nearest filtering without changing the
scaling blit geometry. ELF SHA-256
`3c58eef97de85b4d76e1bfef87f3a37424cf4b44f0bdd1c9d7ef1e59b5496721`
passes the bounded eight-frame lifecycle in
`20260802T183758Z-swapchain-run1.log`, but its presented-frame intermediate is
still exact magenta while the swapchain sample remains zero. The filter change
was reverted, eliminating linear filtering as the zero-write owner.

The scanout-aware `vkCmdBlitImage` format fix is necessary hardening but is not
sufficient to repair Eden's black output. Cleanup-first FW 5.500.008
qualification of Eden ELF SHA-256
`7dc8c40268852d87d51e9a191c433aed31513e78cc92be93d36959bc184e9885`
in `20260802T222130Z-swapchain-run1.log` reads exact magenta from the 1280x720
intermediate image (`ff00ffff`) while the 1920x1080 scanout image remains
exactly zero. The bounded runner retired PID 239 and independently found no
exact `eboot.bin` process.

Three cleanup-first controls rule out the immediate alternatives. A direct GPU
`vkCmdClearColorImage` to the scanout image also leaves scanout zero (ELF
`62829490634e8d5300c25e1ae5f52ede3bf0481a1a3f50d1c11a8f9010866acb`,
`20260802T222604Z-swapchain-run1.log`, PID 242 absent). A CPU-invalidated sample
of the mapped scanout allocation after native present is likewise zero (ELF
`94490709391777f2ff9ec8f73b8b32228a3a13466a0e396dc501b7e245aa0671`,
`20260802T222854Z-swapchain-run1.log`, PID 245 absent), so this is not solely a
stale image-to-buffer readback. Conversely, a temporary CPU write of magenta
to that same mapped allocation, followed by flush and native present, reads
exact magenta (ELF
`4c8b8c7a6278bc1a871c47514c62f3da25f691955147b5f642a41f232c8b7eb7`,
`20260802T223204Z-swapchain-run1.log`, PID 247 absent). That control proves the
mapped scanout allocation and present chain can carry nonzero pixels; it is
not a rendering fix and makes no visible-output claim. Bypassing Eden's
sequence-zero intermediate readback also leaves scanout zero (ELF
`7e37c8dc9131793033665eefde9dcca890b86ece8252a930ab3b5016e91cf717`,
`20260802T224359Z-swapchain-run1.log`, PID 258 absent), eliminating that
readback as the cause. All temporary Eden and WSI controls have been removed.

A dedicated cleanup-first `vulkan_ps5_scanout_matrix_probe` now qualifies eight
generic paths with exact full-image 1920x1080 readback: (A) BGRA Garlic source
to an ordinary scaled destination, (B) RGBA Garlic source to mutable scanout
through a scaling blit, (C) a fixed-fragment draw directly to scanout, (D) a
BGRA Garlic source reused by a later submission and blitted to scanout, and
(E) a first-use Eden-style BGRA UNORM Garlic intermediate produced through a
`MAY_ALIAS`, `GENERAL` render pass and then blitted to scanout in a later
submission, and (F) the same producer/blit contract backed by the exact type-0
Onion memory and `TRANSFER_SRC|COLOR_ATTACHMENT` usage selected by Eden's VMA
allocation, (G) three Eden-style Onion images placed at VMA-equivalent offsets
in one allocation with separate semaphore-ordered producer and consumer
submissions, and (H) the decisive ordering case where the consumer command
buffer is recorded before its producer is submitted. The restored A-D baseline ELF
`e28c6da8d9e05def5c6ff1ddcba960f07526a7c3ade2944ce7f50a872fc0978b`
passes in `20260802T225010Z-swapchain-run1.log` with PID 264 absent. The
extended A-E ELF
`f9a8dc161add27f6e7ee97ffcf3cae85deb6aee0b3293bfd2e9c3140df69eae6`
passes in `20260802T225413Z-swapchain-run1.log` with PID 266 and the global
exact-name check absent. The A-F ELF
`08e71ef77656b2b5b258383eef91f472e1a20a66fe9e6e5e664410dcbe419e8b`
passes every exact readback in `20260802T230614Z-swapchain-run1.log`, with PID
273 and the global exact-name check absent. The user also visually confirmed
magenta presentation from the diagnostic matrix; this is visible standalone
scanout evidence, not yet visible Eden game output. A preliminary
nonzero-memory-offset variation is
invalid evidence: it failed source-image creation before graphics work (ELF
`46a29b...`, `20260802T224607Z-swapchain-run1.log`, PID 261 absent). Source
audit confirms that placed-image view descriptors include `memory_offset`; the
failed variation is not a placement diagnosis.

Bounded native-blit logging then compares the passing matrix and failing Eden
production call. Eden ELF
`5bf7a1c3066aa571abdb8a8355dbf3cdaa5ba3061f041513d506e0c0169ac6e1`
completes eight frames in `20260802T230201Z-swapchain-run1.log` with exact
sequence-zero intermediate magenta and zero scanout; PID 270 is absent. Its
source differs from matrix E by type-0 Onion placement and usage `0x11`, but
matrix F reproduces those values and passes. The extended native-state matrix
ELF `924e80d9aa58d9399089c817007f062c800635d95fc44a2b31b02f33dab3166e`
also passes in `20260802T230901Z-swapchain-run1.log`, with source state
`usage=1 owner=1`, destination state `usage=2 owner=1`, PID 275 absent, and all
logged image descriptors otherwise matching Eden's production blit. Forcing
Garlic is therefore not a demonstrated fix. At that checkpoint the remaining
candidate set was an
Eden-production command-sequence, cross-command state, VMA multi-placement, or
cache/descriptor-lifetime difference, not generic scanout allocation, Onion
sampling alone, native presentation, GPU scaling blits, direct scanout draws,
cross-submit reuse, the format correction, qualification readback, or the
isolated Eden-style intermediate render pass.

Matrix G passes all three shared-allocation images and 6,220,800 exact pixels
in `20260802T231736Z-swapchain-run1.log` (ELF
`56a7fe0b5251660a7027861b1f139a221203a177c23b6908d07b0753c6d7aa04`,
PID 283 and the global exact process name absent). Pre-fix H then reproduces
the real cross-command failure after A-G pass: OpenAGC reports a transition
state mismatch followed by `agcQueueSubmit` `0x80890003` in
`20260802T232313Z-swapchain-run1.log`; cleanup retires PID 288. The root cause
is that Vulkan-PS5 discarded the barrier's concrete Vulkan prior state and
encoded the consumer from its stale record-time OpenAGC state, while render
pass `finalLayout=GENERAL` also lowered the concrete color-target endpoint to
ShaderRead prematurely.

OpenAGC commit `cf9843d` adds a public, fail-closed v2 committed-state
transition: record-time mismatch is allowed only for that explicit flag and
submit-time validation must match the globally committed prior state. Its host
suite passes 19,993 assertions with zero failures. Vulkan-PS5 commit `364771b`
uses the declared `oldLayout`/source access when concrete, selects the public
committed-state path only when a separately recorded command needs it, and
preserves ColorTarget through a render pass ending in `GENERAL` until the
application's next explicit barrier. Fixed matrix ELF
`4d742eb1efa5f0785e22ac8118b9f3af091799d813ea330aec041608abcdd2ef`
passes H first and then the complete A-H exact-readback matrix in
`20260802T233527Z-swapchain-run1.log`; PID 297 and the global exact process
name are absent. The temporary native-blit logger was removed before the
production rebuild.

Eden ELF `3152f85a293574f7f3a94b9226a5ebe541a03185100bd7ac70301f75ff9c3412`
then completes the cleanup-first eight-frame `2048.nro` checkpoint in
`20260802T233817Z-swapchain-run1.log` and exits with PID 299 and the global
exact process name absent. Dynarmic's RW-to-RX transition succeeds on this
attempt, but sequence zero still has exact intermediate magenta and zero
swapchain samples; later diagnostic samples are also zero. Therefore the
committed-state defect is proven and fixed, but it is not the final Eden black
screen owner and no visible Eden output is claimed.

Case I extends H with Eden's exact diagnostic sequence: sixteen sparse 1x1
source copies at buffer offsets 0-60, the scaling blit, then sixteen sparse
destination copies at offsets 64-124 in the same 128-byte buffer. ELF
`fb17860994c7378c1adc7bf579d15a2048ceac8d25118ba987141fb13f320add`
passes H and both I halves with exact magenta in
`20260802T234715Z-swapchain-run1.log`; PID 302 and the global exact process
name are absent after cleanup. The longer aggregate exited later, so this is
focused H/I evidence rather than a complete A-I gate. Vulkan-PS5 commit
`4044adb` retains the regression. The sparse source read before blit is
therefore not the black-screen owner.

A bounded transition journal on Eden ELF
`a8d51af581a9f431495f07f32adba40b1ef55498a42f4e7da159169ba663de28`
completes eight frames in `20260802T234847Z-swapchain-run1.log` with PID 305
and the global exact process name absent. It shows the production source is
already committed as ColorTarget when the consumer records, the barrier
encodes ColorTarget-to-CopySource without the committed-state fallback, and
the blit records successfully with the same CopySource/CopyDestination state
pair as H/I. The material remaining difference is virtual placement: Eden's
first source/destination are around `0x31e000000`/`0x310e00000`, after the
4 GiB guest reservation, while standalone matrix resources are around the
already-qualified `0x2...` band. Earlier direct GPU scanout clears also stayed
zero, so high virtual placement of the Garlic/display target is the leading
candidate; this is not yet proof of an address-encoding defect.

An attempted fixed 64 GiB guest reservation is rejected evidence: ELF
`b3122a9a6137a99985651f84a424577139ad1676322650fa1c972957d2a8d2a1`
made the console unreachable in `20260802T235346Z-swapchain-run1.log`; the
user subsequently confirmed that the PS5 kernel panicked, so cleanup and exact
process absence could not be verified. The source change was immediately
reverted, but audit found that this unsafe binary still occupied Eden's default
launch path. Vulkan-PS5 commit `40a777a` now rejects its exact SHA-256 before
network access, and the refusal regression passes. Rebuilding `yuzu-cmd` from
the reverted source replaced the default artifact with SHA-256
`c740359b62fea2abc21b71928d2aa048f395e2bbd2b97e563dca90b3c07a3682`;
this new hash is build evidence only and has not been launched. The rejected
ELF must never be relaunched. Future address probes must use OS-chosen VA-only
reservations without `MAP_FIXED`, report inconclusive placement explicitly,
and retain the cleanup-first exact-process gate.

The Mesa/OpenAGC address audit now isolates the high-placement defect more
precisely. Sampled-image descriptors and color-target registers encode all
three observed `0x2...`/`0x3...` image addresses correctly; their upper base
extensions remain zero below 1 TiB. The failing contract is instead RADV's
address32 shader-resource ABI. `openagc-psbc` compiles descriptor, indirect-set,
push-constant, and vertex-table pointers with a fixed upper dword of `2`, while
the OpenAGC command runtime previously truncated its unconstrained Eden
resource-arena addresses around `0x3...` to one low SGPR without validation.
The shader consequently reconstructs a different `0x2...` address. This
explains why Eden's source clear and transfer readback are exact magenta while
the sampled-image blit writes zero, and why the standalone matrix passes when
its resource arena naturally occupies the `0x2...` band. OpenAGC commit
`6ac4442` adds the immediate fail-closed guard before indirect table writes or
user-data emission; its host suite passes 19,995 assertions with zero failures.
That guard prevents silent misaddressing but intentionally makes current Eden
high-band recording fail until the compiler/runtime contract becomes
device-selected. `openagc-psbc` commit `00520bb` adds the required per-request
`address32_hi` compiler input (including a successful high-3 host compile), and
Vulkan-PS5 commit `40a777a` first added the device field and compiler plumbing.
At that checkpoint Vulkan still initialized the field to the qualified high-2
constant pending an OpenAGC device query.

That first production part is now implemented: OpenAGC commit `89d2dd1`
eagerly creates an isolated 16 MiB flexible command-resource arena, captures a
stable device-selected address32 high dword, exposes it before pipeline
compilation, and confines descriptor, indirect-set, push-constant, and vertex
tables to the same 4 GiB window. The full host CTest suite passes 19/19, the
runtime suite passes 20,003 assertions, and the Prospero static library builds.
Vulkan-PS5 commit `de33853` queries that value at device creation and feeds it
to every PSBC compile; host lifecycle and Prospero static builds pass. Shader
reflection/cache identity validation and cleanup-first hardware evidence remain
required before this defect is closed.

The artifact-identity half is implemented in OpenAGC commit `f7ec59e` and
openagc-psbc commit `89bce63`. Reflection v3/compiler API 20 and PSBC API 22
record the compile request's address32 high dword, bind it into the shared
linkage hash with an explicit little-endian encoding, and reject a v3 shader
whose recorded window differs from the device before native shader allocation.
Legacy v1/v2 shaders remain accepted only when they carry no address32 resource
pointer. Re-tagging a stale sidecar without recomputing its linkage and a
correctly hashed artifact from another window both fail closed. The OpenAGC
runtime suite passes 20,077 assertions, the unaffected host CTest set passes
10/10, both OpenAGC and PSBC Prospero builds pass, and Vulkan-PS5's host and
Prospero static libraries still build. Vulkan's current pipeline cache stores
only its standard header and does not reuse PSBC native binaries, so executable
cache identity is presently enforced at this reflection boundary. The rebuilt,
never-launched Eden ELF is SHA-256
`18295a780e72d724c4f2eeb4bcf4a868c4ba2fe3c122b7de0dab43b922251f22`.
It is not qualification evidence until a direct-backend-only FW 5.50 boot has
passed the pinned cleanup/exact-process preflight and sequence-zero scanout.
The dedicated `tools/run_fw550_2048_sequence0.sh` gate now enforces that order
with the eight-frame sidecar and requires exact magenta intermediate and
swapchain readback at sequence zero before the long gate. It and the two-run
600-frame wrapper pin the current Eden/cleanup/runner/helper identities and the
canonical PyPS4debug source revision and lockfile, keep
allocation/JIT/presentation failure rejection mandatory, and constrain the
application request to 1-120 seconds (60 by default). Visual confirmation
remains required before advancing from the sequence-zero gate.

The first cleanup-first address32 replay now passes the automated sequence-zero
gate with the identical Eden ELF above. Log
`Vulkan-PS5/examples/qualification-logs/20260803T011355Z-swapchain-run1.log`
identifies exact FW `5.500.008`, loads the pinned `2048.nro`, completes every
Dynarmic cache demotion without an RW/RX failure, and reports exact BGRA
magenta from both the intermediate and swapchain at sequence zero
(`nonzero_bytes=48`, `hash=6fc6b825c3dda003`, `first=ff00ffff`). Subsequent
readbacks are also nonzero and the application emits `GAME PASS 8 frames`.
The scoped kernel log records PID 89, same-app `KillApp`, `All processes
exited`, no crash/XoM violation, and only the accepted raw-ELF `0x4000` warning;
independent PID-scoped and global exact-name checks both find no `eboot.bin`.
This closes the automated black-scanout regression. A second cleanup-first
replay, `20260803T020317Z-swapchain-run1.log`, also passed with PID 91 and no
remaining exact `eboot.bin`; the user saw magenta followed by the 2048 board.
The board colors were correct but faint, leaving brightness/gamma quality as a
later issue rather than a missing-channel or black-scanout blocker.

`2048.nro` is retained as that short visible sequence-zero smoke workload, but
it is no longer the active continuous 600-present workload. Attempts
`20260803T020440Z-swapchain-run1.log` and
`20260803T020648Z-swapchain-run1.log` reached only eight native presentation
successes before their 60- and 90-second bounds. Both runs were cleaned up and
left no exact `eboot.bin`; the longer run continued advancing the host input
injector without another native present, so it did not satisfy either the
`GAME PASS 600 frames` or independent native 600-success oracle. Increasing
the timeout would not turn that result into qualification evidence.

The active FW 5.50 long gate instead uses the user-selected pinned
`InvadersNX.nro` (SHA-256
`4ad1a05d7e7edba203d086151bf83d2be02bf2ead8695ce4d21f21b4bdf27433`).
Its reference source loops while `appletMainLoop()` remains active and calls
`SDL_RenderPresent()` unconditionally after update and render on every
iteration. Input only changes game state, with Minus as the normal exit, so
the 600-frame sidecar omits `input_cycle=1`. The NRO uses an SDL2 software
renderer and embedded `romfs:/` resources; SDL2_mixer initialization failure
disables audio without aborting the application. The two-run wrapper must
still require Vulkan-PS5's independent native 600-success marker in addition
to Eden's post-shutdown game verdict, exact FW 5.500.008, a nonzero second-run
pipeline count, bounded teardown, and exact process absence. This workload
substitution does not change the pinned Eden ELF or relax any completion gate.

The successful PS4 port uses Eden's native `renderer_gnm` over `opengnm`, not
Vulkan. That is relevant evidence for a future direct `renderer_agc`: its
rasterizer/cache/presentation structure can be forward-ported while replacing
gfx8 GNM objects and the GCN compiler contract with public OpenAGC objects and
PSBC gfx1013 shaders. It is not evidence for loading installed
`libSceAgcDriver`. On both FW 5.50 and FW 11.60, isolated installed-driver
submission returned success without executing the marker, and its module start
mutates persistent GPU state. Direct `/dev/gc` and installed-driver use are
therefore mutually exclusive for the whole boot, with no fallback between
them. The active Vulkan gate gets one cleanup-first sequence-zero retest after
the address32 fix; if that exact oracle still fails, start the direct OpenAGC
renderer as a separate backend rather than replacing `/dev/gc` with the
installed driver.

The Prospero Dynarmic code cache remains fail-closed: every allocation,
demotion, RW-to-RX, RX-to-RW, and unmap failure enters the noreturn PS5
termination path before invalid code can execute. Hardware logs have shown an
intermittent RW-to-RX `mprotect` `EPERM`, so the two-run wrapper now rejects the
common `eden-ps5 dynarmic ... failed:` prefix in addition to its narrower
memory and invalid-mapping diagnostics. A shell regex regression covers the
initial-demotion, both protection directions, unmap, and overflow messages and
does not match a healthy cache-ready message. This strengthens the 600-frame
gate but does not claim that the intermittent firmware failure is resolved.
The diagnostic Eden ELF
`30a2743c6e16288c9f4e9cc3422e4bf794aaf11feb2e1b7204ea00de5a641576`
then hit this exact fail-closed `EPERM` twice consecutively before graphics in
`20260802T230942Z-swapchain-run1.log` and
`20260802T231044Z-swapchain-run1.log`; PIDs 277 and 280 and the global exact
process name were absent after cleanup. This persistent relaunch failure is
now an active owner of the eventual two-run gate, not merely a historical
observation.

The cleanup-first FW 5.50 Flappy canary now uses a 300-frame sidecar and a
45-second host bound. The longer bound is intentional: renderer startup takes
about 23 seconds on this build, so the former 30-second bound captured only a
small part of the live game while 150 seconds remains unnecessarily long.
OpenAGC runtime API v58 exposes the first uniform command-local buffer state
span (`a812187`), and Vulkan-PS5 walks those exact spans for vertex tails
(`e1b9be8`) and transfer copies (`1a62d45`) without relaxing mixed descriptor
validation. OpenAGC also accounts transition reference journals by resource
type (`9dc517f`) and gives persistent buffer state its own 32,769-interval,
512-KiB metadata bound with a cross-command 2,048-interval regression
(`d7ed7f2`). The OpenAGC host suite passes 36,396 assertions and the focused
Vulkan command-recording test passes.

Hardware logs show monotonic progress. Run
`20260803T224251Z-swapchain-run1.log` passed the former vertex-tail blocker and
isolated a fragmented `vkCmdCopyBuffer`; run
`20260803T224529Z-swapchain-run1.log` then reached native present sequence 31
without a Vulkan error. Run `20260803T224657Z-swapchain-run1.log` created two
real guest graphics pipelines and wrote two transferable cache records before
exposing the old persistent-state bound. With the independent persistent
bound, `20260803T225229Z-swapchain-run1.log` reached native present sequence
63, retained changing nonzero intermediate and swapchain hashes, and ended
only at the 45-second host deadline. The user visually confirmed magenta
followed by the Flappy Bird intro. PID 113 and the global exact `eboot.bin`
name were absent in both post-run checks. This is visible guest presentation
evidence, but not yet the automated 300-present or immediate cache-reload
completion gate because the static intro stops submitting new frames.

The requested format-122/130 OpenAGC audit found no missing native format
implementation to add. Vulkan `VK_FORMAT_B10G11R11_UFLOAT_PACK32` maps to
OpenAGC `AGC_FORMAT_R11G11B10_FLOAT`; its four-byte layouts, sampled image
views, color-target tuple, pipeline export validation, transfers, and FW 5.50
hardware qualification are already present. Vulkan
`VK_FORMAT_D32_SFLOAT_S8_UINT` maps to
`AGC_FORMAT_D32_FLOAT_S8_UINT`; OpenAGC already implements split D32/S8
planes, layouts, views, aspect-specific state, target binding, transfers,
clears, and combined HTILE. Vulkan-PS5 deliberately advertises format 130 for
depth/stencil attachment and transfer use, but not combined-format sampling;
that narrower advertisement remains fail-closed until combined sampled-aspect
behavior is qualified. OpenAGC's current host suite passes 36,396 assertions,
and Vulkan-PS5's lifecycle, clear-color/depth-stencil, and command-recording
format regressions pass. The unrelated full Vulkan-PS5 build currently stops
in stale `tests/pipeline.c` calls to the expanded meta-attachment helper; this
does not invalidate the focused format results and must be repaired as its own
test-maintenance slice.

The current Eden cache telemetry removes the last ambiguity in the warm Flappy
run. `disk-load-complete` separately reports graphics/compute records
discovered, pipelines successfully built, and rejected records. The wrapper
now requires a nonzero graphics load under cache identity
`ee7cd9a1a22a0102`; historical run `20260803T224657Z` remains the run-one
creation/write evidence. The updated Prospero build completes and produces
ELF SHA-256
`01a0aa1da1469078243127777a725d5cf84b09d85403ff3d1ca16263b2490a83`.

The cleanup-first replay is preserved at
`Vulkan-PS5/examples/qualification-logs/flappy-bird/20260803T230453Z-swapchain-run1.log`.
Under the identical NRO and derived identity it parsed five graphics records,
successfully rebuilt all five, rejected none, and created or rewrote none at
runtime: `graphics_discovered=5`, `graphics_loaded=5`,
`records_rejected=0`. This closes the immediate transferable-cache reload
oracle; compute remained zero because this workload did not submit a compute
pipeline. The run again reached native present sequence 63 without a Vulkan,
OpenAGC, JIT, or presentation error, then hit the intentional 45-second host
bound before `GAME PASS 300 frames`. The wrapper therefore failed the
independent long-frame gate as designed, launched pinned cleanup, and proved
PID 116 plus the global exact `eboot.bin` name absent twice. Do not extend the
deadline merely to wait on a static intro; progress the workload or improve
guest throughput before retrying the 300-present gate.

The next diagnostic does not lengthen that deadline. Flappy's source renders
every applet-loop iteration, but its scene loader's intended one-second delay
costs roughly ten host seconds in the current run. Perpetual A injection can
therefore re-enter a loading scene after death/restart and consume most of the
bounded window. The sidecar grammar now accepts `input_cycle=N`: legacy value
one remains unbounded, while values 2-10000 cap synthesized key presses without
restoring blocking SDL event waits. Parser coverage passes 47 assertions, the
Prospero build completes, and the new ELF SHA-256 is
`4fbccbe733095447f6d851110986c58c7eb0ce363be581043e808b3b5628d67d`.

The first capped-input replay,
`20260803T231149Z-swapchain-run1.log`, proved the mechanism: it emitted
`stopped presses=64 limit=64`, retained 5/5 cache reload with zero rejection,
and cleaned PID 119 plus the global exact name. It still reached only present
sequence 63 because the 64th press had already entered one final loading
delay. The next sidecar stops earlier at 48 presses, keeps the identical ELF,
and has SHA-256
`55f2678db0d2c92dcaf887ba3ba7555ba39d00016156555267110f8e602df81c`.

The 48-press replay is preserved at
`20260803T231355Z-swapchain-run1.log`. It stopped input at 25.43 seconds,
retained the 5/5 graphics-cache reload with zero rejection, completed another
loading interval, and still produced no frame beyond native present sequence
63 before the 45-second bound. Cleanup proved PID 122 and the global exact
name absent twice. This rules out perpetual restart input as the owner of the
64-frame ceiling. Flappy has now supplied its intended visible guest graphics,
record-write, and immediate record-reload evidence; do not spend more hardware
cycles adjusting its timeout or input cap for the independent long-frame gate.
The final calibration keeps the full Flappy goal rather than substituting a
different workload. It limits injection to two presses: the first advances the
splash, the second is consumed by the following loading scene, and no later
press can trigger another transition. The identical ELF is retained; the new
sidecar SHA-256 is
`44f0abd17639f09a237074de03db839c755d7e0767a9d32e1de99f35977ff5f2`.

The two-press replay `20260803T231831Z-swapchain-run1.log` validates that
choice: it stopped injection at 2.39 seconds and advanced through native
present sequences 127 and 255 with the same 5/5 cache reload and zero
rejections. The 45-second curl envelope expired when the process clock was
37.86 seconds because launcher overhead consumes about seven seconds before
the application clock starts. This is no longer a 64-frame stall. The wrapper
now uses a bounded 55-second host deadline, leaving roughly ten seconds of
margin for the final 44 frames and teardown while remaining far below the
rejected 150-second diagnostic window.

The cleanup-first final automated run passes at
`Vulkan-PS5/examples/qualification-logs/flappy-bird/20260803T232023Z-swapchain-run1.log`
with target klog `20260803T232023Z-swapchain-run1-target.klog`. It identifies
exact FW `5.500.008`, uses only the direct `/dev/gc` backend, reports fail-soft
null audio, stops input at exactly two presses, loads all five discovered guest
graphics pipelines with zero rejected records, and emits
`eden-ps5: GAME PASS 300 frames`. Orderly teardown reaches the pipeline-cache
destructor with the same 5/5/0 counters. The guarded runner accepts only the
known raw-ELF `0x4000` warning, reports the canary PASS, and independently
proves PID 128 and the global exact `eboot.bin` name absent twice after exit.
Pinned identities are Eden ELF
`4fbccbe733095447f6d851110986c58c7eb0ce363be581043e808b3b5628d67d`,
sidecar `44f0abd17639f09a237074de03db839c755d7e0767a9d32e1de99f35977ff5f2`,
NRO `6e7cd9a1a22a0102a4f68ba6e434378c9b7381ce4f44a43ca376953f536aa54d`,
and wrapper `f8d51a5d7a7533985975faf47ac888ef40ae80971f73681c9cb29326609225ed`.
The exact current ELF still requires operator confirmation that Flappy output
was visible on the display before this active goal can be marked complete.

The follow-up Flappy format-error slice covers every enum reported by the
passing 300-frame canary. Vulkan-PS5 now exposes the already-present OpenAGC
storage-image path for color formats 44, 51, 64, 76, 83, 97, 100, 103, 109,
and 122 in commit `236687c3818a`. Its lifecycle regression requires the exact
sampled/storage/color-attachment/transfer feature combination Eden requests,
and its image-format query must accept that combined usage. The complete
62-test host suite passes. Format 130 (`VK_FORMAT_D32_SFLOAT_S8_UINT`) remains
truthfully limited to its qualified attachment and transfer subset: Eden no
longer asks for sampled support during Prospero format inventory and reports
the format as non-sampleable, because the earlier FW 5.50 sampled probe left
the native queue pending. The Prospero `video_core` rebuild passes. A guarded
full link from Eden `b77d7b5c4994` and Vulkan-PS5 `236687c3818a` produced
`build-prospero-full-audit2/bin/eden-ps5.elf`, SHA-256
`ff3c252883753a41ba3a27cb7aea3bd9b392cb208f6672c2e3b816d21e74fc82`.
The updated guarded wrapper is SHA-256
`5f997989294561995cdf9834b102d6d0b49edc98a3f629ca847a6a636c16f846`
and rejects any recurrence of those eleven exact format-selection errors. A
guarded cleanup-first Flappy replay must still prove that these eleven startup
errors are absent and that the expanded color storage paths do not regress
guest rendering, teardown, or immediate relaunch. The following guarded replay
supplies that hardware evidence.

The guarded FW 5.50 replay now passes at
`Vulkan-PS5/examples/qualification-logs/flappy-bird/20260804T003035Z-swapchain-run1.log`
with target klog `20260804T003035Z-swapchain-run1-target.klog`. It ran the
pinned cleanup ELF first, observed global exact `eboot.bin` absence twice,
launched only the direct `/dev/gc` backend on exact FW `5.500.008`, loaded all
five discovered graphics-cache records with zero rejection, presented exactly
300 frames, and reached the same 5/5/0 telemetry at destruction. PID 130 and
the global exact process name were absent in every repeated post-run check.
The saved log contains zero `Render.Vulkan <Error>` records, zero occurrences
of the eleven targeted `GetSupportedFormat` failures, and zero `VK_ERROR`,
device-loss, GPU-thread, submit, acquire, or present failures. This closes the
automated Flappy format-error and lifecycle gate for the pinned ELF. Operator
confirmation was obtained from an identical watched replay at
`Vulkan-PS5/examples/qualification-logs/flappy-bird/20260804T003308Z-swapchain-run1.log`:
the display showed the magenta clear followed by the Flappy Bird intro. That
run again passed 300 frames, direct `/dev/gc`, 5/5/0 cache reload/destruction,
bounded teardown, and all repeated PID/global absence checks, with zero Vulkan
or targeted format errors. Its only application error-level records are two
non-fatal guest BSD calls before network initialization. The remaining warning
records are known stub/compatibility diagnostics plus one NVMap pin-count
imbalance during teardown; none produced a critical, GPU, Vulkan, process, or
presentation failure. Target klog's `Debug suspend sync failed(35)` is emitted
by ps5debug during post-exit process inspection, after the application has
already stopped, and is not an Eden or GPU execution failure. This completes
the pinned FW 5.50 Flappy canary including operator-visible presentation.

The dependent pacbrew packaging gate is now hardened before FFmpeg or other
SDL consumers are rebuilt. The OpenAGC and Vulkan-PS5 recipes validate their
installed CMake metadata and reject any `SceAgcDriver` dependency. The
libsamplerate recipe now uses the Prospero CMake toolchain, installs a
relocatable `SampleRate` package, and verifies its header, static archive,
pkg-config file, and imported target. SDL2 explicitly supplies both
`OpenAGC_DIR` and `SampleRate_DIR`, enables static libsamplerate, and applies a
SHA-256-pinned direct-backend packaging patch. Its installed CMake target,
pkg-config file, and `prospero-sdl2-config` must expose OpenAGC and
libsamplerate while remaining free of `SceAgcDriver`. FFmpeg rejects the old
mixed-driver metadata and refuses to configure unless SDL's static metadata
contains libsamplerate.

Recipe-equivalent clean Prospero builds passed for libsamplerate and for SDL2
with samplerate both enabled and disabled. Both SDL metadata modes passed the
new checker. A fresh external CMake consumer of the enabled install linked
SDL2, OpenAGC, `kernel`, `SceVideoOut`, and libsamplerate without
`SceAgcDriver`. This is host packaging evidence only; `makepkg`/pacman is not
available on this Mac, and no new PS5 runtime qualification is claimed from
this slice. Build FFmpeg and the remaining dependent packages only from these
validated package contracts.

1. Complete the device-selected address32 contract: give OpenAGC a dedicated
   same-4-GiB resource arena, expose its selected high dword, pass that value
   through Vulkan-PS5 into `openagc-psbc`, record it in versioned shader
   reflection/cache identity, and reject every cross-window allocation or
   shader/device mismatch. Cover high-2, high-3, and boundary-crossing cases
   on host, then run cleanup-first matrix case J in Eden's post-guest placement
   order. Do not advance the long gate until Eden sequence zero has exact
   magenta swapchain readback and user-confirmed visible presentation.
2. **Complete on FW 5.50.** After sequence-zero scanout is proven, repeat the
   cleanup-first `2048.nro` 600-frame workload twice on FW 5.50 through the
   renderer, WSI, and present
   path. Require visible frames, bounded teardown, and immediate relaunch on
   both runs; do not claim guest shader-cache coverage from its SDL software
   renderer.
3. Run a cleanup-first Flappy Bird canary, then A Short Hike once private key
   provisioning is explicitly authorized. Advance the persistent shader-cache
   gate only after telemetry proves a guest graphics/compute pipeline was
   created, run one writes a transferable record, and immediate run two loads
   a nonzero count under the identical real or derived cache identity.
4. **Complete.** The bounded end/submit/acquire/present checkpoints measured
   the 600-frame runtime and were removed after the stable pair. The 100/600
   committed-success markers and fail-only diagnostics remain.
5. Qualify a small `libSceUserService`/`libScePad` probe, implement the native
   controller/event/lifecycle bridge, and remove SDL3 from the production
   Prospero target.
6. Qualify the `libSceAudioOut` ABI with a standalone bounded-buffer probe,
   then implement Eden's native AudioOut sink using the mGBA transport pattern.
   Keep null audio as an explicit fail-closed fallback; do not use SDL2_mixer
   for emulated audio.
7. Remove the temporary construction checkpoints after stable renderer start,
   update the exact evidence and hashes, and commit that verified slice without
   staging unrelated diagnostic work.
8. Refresh the Eden compatibility audit at revision `612409c7ba`, run the FW
   5.50 regression matrix and targeted CTS/deqp subset, and close any remaining
   format or command gaps demonstrated by those results.
9. Build and install the pacbrew RmlUi package, make its SDL2 dependency
   optional or isolate it from the production runtime, and layer the
   controller-driven launcher over the proven emulator lifecycle. Keep Dear
   ImGui diagnostic-only.
10. Freeze the final ELF/library hashes and replay the identical bytes and full
   advertised-feature gate on FW 11.60.
