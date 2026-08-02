# Eden PS5 Port Plan

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
`eden.ps5_thread_budget` passes, as do the host `core`/`video_core` builds and
the source-integrated Prospero `yuzu-cmd` build.

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

The next terminal boundary was `vkCreateImageView`: Vulkan format 51
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
Prospero builds pass. Hardware qualification of this correction, followed by
orderly failure propagation or sustained draw/presentation, is the active
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
  `tools/run_fw550_2048.sh` pins and re-verifies the local
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
  The 2048 result is not claimed until the FW 5.50 guarded run executes.
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
  and consecutive descriptor-binding gaps. The current pinned artifact
  `4eae3b998f9a92664d41b86325a62bc8f9d2186a8c592e471ac180038923e490`
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
  reflection rejection in host/build qualification, but the first integrated
  replay stopped earlier at the unsupported JIT-shm/mprotect hybrid. The
  current production gate is cleanup-first FW 5.50 proof of the ordinary
  anonymous same-address W^X allocator, followed by the still-unproven
  29-SGPR presentation pipeline.

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

1. Trace the guest NVDRV image request that currently ends in
   `VK_ERROR_FORMAT_NOT_SUPPORTED` after HID/NVDRV initialization. Record the
   exact guest format, usage, tiling, and public Vulkan-PS5/OpenAGC call that
   rejects it; add only a general-purpose format path justified by that trace.
2. Retain the construction checkpoints and JIT/flexible-memory diagnostics
   until renderer startup is stable. Convert the current uncaught format
   exception into a bounded failure while fixing the root format path, with
   neither a coredump nor a live process/native allocation.
3. Repeat the cleanup-first `2048.nro` workload twice on FW 5.50 through the
   real scheduler, shader cache, renderer, WSI, and present path. Require
   visible frames, bounded teardown, and immediate relaunch on both runs.
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
9. Freeze the final ELF/library hashes and replay the identical bytes and full
   advertised-feature gate on FW 11.60.
