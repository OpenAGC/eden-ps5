# Eden PS5 Port Plan

## Current active slice (2026-08-03)

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

The GPU-free probe now adds a second thread with exactly 128 bounded anonymous
map/write/unmap cycles while the four Dynarmic-sized mappings execute their W^X
cycles. It uses no GPU API, fixed address, retry, or execution after failure.
Target proof requires 20 cleanup-first concurrent probe processes followed by
20 cleanup-first production 2048 startup/presentation processes on FW 5.50,
bounded teardown, and exact process absence. Only after that repeated gate may
the fix be considered hardware-qualified and the wider renderer goal resume.

Eden now serializes every Prospero Dynarmic executable-VM operation with one
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
the remaining active proof is the repinned full Eden ELF's two-run InvadersNX
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
`EDEN_PS5_WEBSRV_TIMEOUT`, defaulting to 300 seconds for cold shader caches.

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

1. Complete the device-selected address32 contract: give OpenAGC a dedicated
   same-4-GiB resource arena, expose its selected high dword, pass that value
   through Vulkan-PS5 into `openagc-psbc`, record it in versioned shader
   reflection/cache identity, and reject every cross-window allocation or
   shader/device mismatch. Cover high-2, high-3, and boundary-crossing cases
   on host, then run cleanup-first matrix case J in Eden's post-guest placement
   order. Do not advance the long gate until Eden sequence zero has exact
   magenta swapchain readback and user-confirmed visible presentation.
2. After sequence-zero scanout is proven, repeat the cleanup-first `2048.nro`
   600-frame workload twice on FW 5.50
   through the
   real scheduler, shader cache, renderer, WSI, and present path. Require
   visible frames, bounded teardown, and immediate relaunch on both runs.
3. Use the bounded end/submit/acquire/present checkpoints to measure the
   600-frame runtime, then remove them after the two-run gate is stable. Retain
   JIT/flexible-memory failure diagnostics until renderer relaunch is proven.
4. Qualify a small `libSceUserService`/`libScePad` probe, implement the native
   controller/event/lifecycle bridge, and remove SDL3 from the production
   Prospero target.
5. Qualify the `libSceAudioOut` ABI with a standalone bounded-buffer probe,
   then implement Eden's native AudioOut sink using the mGBA transport pattern.
   Keep null audio as an explicit fail-closed fallback; do not use SDL2_mixer
   for emulated audio.
6. Remove the temporary construction checkpoints after stable renderer start,
   update the exact evidence and hashes, and commit that verified slice without
   staging unrelated diagnostic work.
7. Refresh the Eden compatibility audit at revision `612409c7ba`, run the FW
   5.50 regression matrix and targeted CTS/deqp subset, and close any remaining
   format or command gaps demonstrated by those results.
8. Build and install the pacbrew RmlUi package, make its SDL2 dependency
   optional or isolate it from the production runtime, and layer the
   controller-driven launcher over the proven emulator lifecycle. Keep Dear
   ImGui diagnostic-only.
8. Freeze the final ELF/library hashes and replay the identical bytes and full
   advertised-feature gate on FW 11.60.
