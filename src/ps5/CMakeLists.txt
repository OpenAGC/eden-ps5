# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Threads REQUIRED)
find_package(VulkanPS5 CONFIG QUIET)

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

add_executable(eden-ps5-vulkan-bootstrap vulkan_bootstrap.cpp)
target_compile_features(eden-ps5-vulkan-bootstrap PRIVATE cxx_std_20)
target_compile_options(eden-ps5-vulkan-bootstrap PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers)
target_link_libraries(eden-ps5-vulkan-bootstrap PRIVATE
    VulkanPS5::ICD SceSystemService)
set_target_properties(eden-ps5-vulkan-bootstrap PROPERTIES SUFFIX ".elf")
