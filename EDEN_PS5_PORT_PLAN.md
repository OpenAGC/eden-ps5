# Eden PS5 Port Plan

## Scope

This plan targets PS5 homebrew built with `ps5-payload-sdk`. It does not port
the PS4/OpenOrbis frontend or inherit PS4 platform assumptions. PS4 work may be
used only as a behavioral reference after the equivalent PS5 contract has been
identified and tested independently.

The upstream source baseline for this plan is Eden revision `612409c7ba`; the
PS5 planning branch begins at `b5cdae421b`. The graphics baseline is Vulkan-PS5
revision `e78b64eaf8`, OpenAGC revision `6a9b7bcac3`, and openagc-psbc revision
`ef8a98cb5e`, plus SDL2 2.30.12 and the pinned Mesa/Zink integration recorded in
the adjacent projects.

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
- `RmlInputPs5`: SDL2 controller, keyboard, and text input translated into
  RmlUi focus/navigation events.
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
| Input | `InputCommon::InputSubsystem`, SDL driver | Map PS5 SDL2 controller events and RmlUi navigation |
| Audio | `audio_core` sink interface | Implement or qualify a PS5 SDL2 audio sink |
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
- Disable Qt, SDL3, cubeb, RenderDoc, update checking, Discord, web services,
  Wi-Fi scanning, libusb, and host-only crash dump paths for the first target.
- Consume pacbrew packages for OpenAGC, openagc-psbc, Vulkan-PS5,
  Vulkan-Headers, SDL2, RmlUi, SDL2_image, FreeType, FFmpeg, OpenSSL, Boost,
  fmt, zstd, lz4, Opus, and other dependencies only as Eden reaches them.
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
  `game\n<absolute-path>\n`. The complete file is capped at 1,032 bytes and the
  path at 1,024 bytes; relative paths, controls, extra lines, malformed, I/O,
  and oversized inputs fail closed. The exit callback now injects an SDL quit
  event on PS5 so Eden reaches `Pause` and `ShutdownMainProcess` instead of
  calling `exit` from the emulation callback. After C++ teardown and logger
  shutdown, the frontend requests app termination through
  `sceSystemServiceKillApp`, with a two-second grace bound and `_Exit` fallback
  rather than an unbounded process loop. The isolated host CTest covers exact
  modes, malformed forms, missing-file defaulting, and file-size bounds. The
  full Release Prospero frontend rebuild passes at discovery hash
  `0ee124ca63263d6c3d101405261703a80784b0f90cadffcc0ee6d0da00959262`;
  hardware qualification and pinning remain pending.

## Milestones and gates

### P0: reproducible skeleton

- Cross-configure Eden for PS5 using installed pacbrew dependencies.
- Build a minimal ELF with Qt, Android, and SDL3 excluded.
- Initialize logging, paths, settings, SDL2 controller input, RmlUi, Vulkan,
  and clean teardown without booting a game.
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

1. Restore FW 5.50 websrv/FTP/klog services and run the already-pinned
   production-memory/compute bootstrap twice through the guarded runner,
   including immediate relaunch and exact process-absence checks.
2. Run the full `eden-ps5.elf` missing-sidecar/`init` preflight and verify
   bounded system-service termination, exact process absence, and immediate
   relaunch before giving it a game path.
3. Boot a small homebrew application through the real scheduler, shader cache,
   renderer, WSI, and present path, closing only general-purpose Vulkan/OpenAGC
   gaps exposed by that workload.
4. Build and install the pacbrew RmlUi package and layer the controller-driven
   launcher over the proven emulator lifecycle. Keep Dear ImGui diagnostic-only.
5. Run the FW 5.50 regression matrix and CTS/deqp subset, then freeze identical
   bytes for the deferred FW 11.60 endpoint replay.
