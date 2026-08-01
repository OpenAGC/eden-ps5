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
the adjacent `VulkanMemoryAllocator` checkout. A host
`glslangValidator` is required at build time to regenerate the embedded
SPIR-V from Eden's production `vulkan_quad_indexed.comp`; it is not a PS5
runtime dependency.

The current gate performs:

1. Vulkan instance, PS5 surface, device, and three-image FIFO swapchain
   creation.
2. Eden's production VMA implementation with mapped upload/readback and a
   device-local buffer.
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

For an Eden run, set the runner's `VULKAN_PS5_QUALIFICATION_ELF`,
`VULKAN_PS5_QUALIFICATION_REMOTE_NAME`, `VULKAN_PS5_QUALIFICATION_LABEL`,
`VULKAN_PS5_QUALIFICATION_PASS_PATTERN`, and
`VULKAN_PS5_QUALIFICATION_PASS_DESCRIPTION` controls as documented in
`../Vulkan-PS5/README.md`, and pin both application and cleanup ELF hashes.
