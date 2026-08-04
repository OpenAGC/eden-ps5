# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Threads REQUIRED)
find_package(VulkanPS5 CONFIG QUIET)

if (TARGET common)
    add_executable(eden-ps5-host-memory-probe
        host_memory_probe.cpp
        runtime.cpp)
    target_compile_features(eden-ps5-host-memory-probe PRIVATE cxx_std_20)
    target_compile_options(eden-ps5-host-memory-probe PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(eden-ps5-host-memory-probe PRIVATE common SceSystemService)
    set_target_properties(eden-ps5-host-memory-probe PROPERTIES SUFFIX ".elf")

    add_executable(eden-ps5-virtual-buffer-probe
        virtual_buffer_probe.cpp
        runtime.cpp)
    target_compile_features(eden-ps5-virtual-buffer-probe PRIVATE cxx_std_20)
    target_compile_options(eden-ps5-virtual-buffer-probe PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(eden-ps5-virtual-buffer-probe PRIVATE common SceSystemService)
    set_target_properties(eden-ps5-virtual-buffer-probe PROPERTIES SUFFIX ".elf")

    add_executable(eden-ps5-dynarmic-jit-wx-probe
        dynarmic_jit_wx_probe.cpp
        runtime.cpp)
    target_compile_features(eden-ps5-dynarmic-jit-wx-probe PRIVATE cxx_std_20)
    target_compile_options(eden-ps5-dynarmic-jit-wx-probe PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(eden-ps5-dynarmic-jit-wx-probe PRIVATE common SceSystemService)
    set_target_properties(eden-ps5-dynarmic-jit-wx-probe PROPERTIES SUFFIX ".elf")

    add_executable(eden-ps5-dynarmic-jit-dual-alias-probe
        dynarmic_jit_dual_alias_probe.cpp
        runtime.cpp)
    target_compile_features(eden-ps5-dynarmic-jit-dual-alias-probe PRIVATE cxx_std_20)
    target_compile_options(eden-ps5-dynarmic-jit-dual-alias-probe PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(eden-ps5-dynarmic-jit-dual-alias-probe PRIVATE common SceSystemService)
    set_target_properties(eden-ps5-dynarmic-jit-dual-alias-probe PROPERTIES SUFFIX ".elf")
endif()

if (NOT TARGET VulkanPS5::ICD)
    if (NOT EDEN_PS5_VULKAN_PS5_SOURCE_DIR)
        set(_eden_adjacent_vulkan_ps5 "${PROJECT_SOURCE_DIR}/../Vulkan-PS5")
        if (EXISTS "${_eden_adjacent_vulkan_ps5}/CMakeLists.txt")
            set(EDEN_PS5_VULKAN_PS5_SOURCE_DIR "${_eden_adjacent_vulkan_ps5}")
        endif()
    endif()
    if (NOT EXISTS "${EDEN_PS5_VULKAN_PS5_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "VulkanPS5 package not found; install it or set "
            "EDEN_PS5_VULKAN_PS5_SOURCE_DIR")
    endif()

    set(VULKAN_PS5_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(VULKAN_PS5_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(VULKAN_PS5_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${EDEN_PS5_VULKAN_PS5_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/vulkan-ps5" EXCLUDE_FROM_ALL)
endif()

if (NOT EDEN_PS5_BOOTSTRAP_ONLY)
    if (NOT TARGET yuzu-cmd)
        message(FATAL_ERROR
            "The full PS5 frontend requires YUZU_CMD=ON")
    endif()
    target_link_libraries(yuzu-cmd PRIVATE VulkanPS5::ICD SceSystemService ScePad SceUserService)
    target_sources(yuzu-cmd PRIVATE
        launch_config.cpp
        runtime.cpp
        shader_cache_identity.cpp)
    target_link_libraries(yuzu-cmd PRIVATE OpenSSL::Crypto)
    set_target_properties(yuzu-cmd PROPERTIES
        OUTPUT_NAME "eden-ps5"
        SUFFIX ".elf")
    return()
endif()

set(EDEN_PS5_VMA_INCLUDE_DIR "" CACHE PATH
    "Directory containing Eden's pinned vk_mem_alloc.h")
if (NOT EDEN_PS5_VMA_INCLUDE_DIR)
    find_path(_eden_ps5_vma_include_dir vk_mem_alloc.h
        HINTS
            "${PROJECT_SOURCE_DIR}/.cache/cpm/vulkanmemoryallocator/v3.3.0/include"
            "${PROJECT_SOURCE_DIR}/../VulkanMemoryAllocator/include"
        NO_CMAKE_FIND_ROOT_PATH)
    if (_eden_ps5_vma_include_dir)
        set(EDEN_PS5_VMA_INCLUDE_DIR "${_eden_ps5_vma_include_dir}" CACHE PATH
            "Directory containing Eden's pinned vk_mem_alloc.h" FORCE)
    endif()
endif()

set(EDEN_PS5_VULKAN_UTILITY_INCLUDE_DIR "" CACHE PATH
    "Directory containing vulkan/vk_enum_string_helper.h")
if (NOT EDEN_PS5_VULKAN_UTILITY_INCLUDE_DIR)
    find_path(_eden_ps5_vulkan_utility_include_dir vulkan/vk_enum_string_helper.h
        HINTS
            "${PROJECT_SOURCE_DIR}/.cache/cpm/vulkanutilitylibraries/include"
            "${PROJECT_SOURCE_DIR}/../Vulkan-Utility-Libraries/include"
        NO_CMAKE_FIND_ROOT_PATH)
    if (_eden_ps5_vulkan_utility_include_dir)
        set(EDEN_PS5_VULKAN_UTILITY_INCLUDE_DIR
            "${_eden_ps5_vulkan_utility_include_dir}" CACHE PATH
            "Directory containing vulkan/vk_enum_string_helper.h" FORCE)
    endif()
endif()
if (NOT EDEN_PS5_VULKAN_UTILITY_INCLUDE_DIR)
    message(FATAL_ERROR
        "Vulkan Utility Libraries headers were not found; install them or set "
        "EDEN_PS5_VULKAN_UTILITY_INCLUDE_DIR")
endif()
if (NOT EDEN_PS5_VMA_INCLUDE_DIR)
    message(FATAL_ERROR
        "vk_mem_alloc.h was not found; install VulkanMemoryAllocator or set "
        "EDEN_PS5_VMA_INCLUDE_DIR")
endif()

find_program(EDEN_PS5_GLSLANG_VALIDATOR glslangValidator
    REQUIRED NO_CMAKE_FIND_ROOT_PATH)
set(_eden_ps5_quad_shader
    "${PROJECT_SOURCE_DIR}/src/video_core/host_shaders/vulkan_quad_indexed.comp")
set(_eden_ps5_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_eden_ps5_quad_shader_header
    "${_eden_ps5_generated_dir}/vulkan_quad_indexed_comp_spv.h")
add_custom_command(
    OUTPUT "${_eden_ps5_quad_shader_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_eden_ps5_generated_dir}"
    COMMAND "${EDEN_PS5_GLSLANG_VALIDATOR}"
        -V --quiet -DUseUniformBlock=1
        --variable-name VULKAN_QUAD_INDEXED_COMP_SPV
        -o "${_eden_ps5_quad_shader_header}"
        "${_eden_ps5_quad_shader}"
        --target-env spirv1.3
    DEPENDS "${_eden_ps5_quad_shader}"
    VERBATIM)
add_custom_target(eden-ps5-host-shaders
    DEPENDS "${_eden_ps5_quad_shader_header}")

add_executable(eden-ps5-vulkan-bootstrap
    vulkan_bootstrap.cpp
    "${PROJECT_SOURCE_DIR}/src/video_core/vulkan_common/vulkan_memory_allocator.cpp"
    "${PROJECT_SOURCE_DIR}/src/video_core/vulkan_common/vulkan_wrapper.cpp")
add_dependencies(eden-ps5-vulkan-bootstrap eden-ps5-host-shaders)
target_compile_features(eden-ps5-vulkan-bootstrap PRIVATE cxx_std_20)
target_compile_options(eden-ps5-vulkan-bootstrap PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers
    -Wno-nullability-extension -Wno-nullability-completeness)
target_compile_definitions(eden-ps5-vulkan-bootstrap PRIVATE
    EDEN_PS5_BOOTSTRAP_MINIMAL=1)
target_include_directories(eden-ps5-vulkan-bootstrap PRIVATE
    "${PROJECT_SOURCE_DIR}/src" "${EDEN_PS5_VMA_INCLUDE_DIR}"
    "${EDEN_PS5_VULKAN_UTILITY_INCLUDE_DIR}" "${_eden_ps5_generated_dir}")
target_link_libraries(eden-ps5-vulkan-bootstrap PRIVATE
    VulkanPS5::ICD SceSystemService)
set_target_properties(eden-ps5-vulkan-bootstrap PROPERTIES SUFFIX ".elf")
