cmake_minimum_required(VERSION 3.22)
project(d2r_activity_native_kgsl_host LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
  message(FATAL_ERROR "This host executes original ARM64 modules on Android hardware only")
endif()
# Refer to original sources; do not configure the old renderer target or copy
# its loader, CPU gates, HLE services, JNI, or rendering implementation.
get_filename_component(NVN_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(D2R_ROOT "$ENV{D2R_ROOT}" CACHE PATH "External D2R Mobile validation checkout")
if(NOT D2R_ROOT OR NOT EXISTS "${D2R_ROOT}/dm/host/hle.h")
  message(FATAL_ERROR "Optional D2R integration requires -DD2R_ROOT=<external checkout>")
endif()
get_filename_component(REPO "${D2R_ROOT}" ABSOLUTE)
set(HOST "${REPO}/dm/host")
set(PLATFORM "${HOST}/android")
set(SHA "${REPO}/dm/third_party/runtime-codecs")
set(DEFLATE "${REPO}/dm/third_party/libdeflate/lib")
set(NATIVE_HOST_SOURCES
  "${PLATFORM}/main.c" "${PLATFORM}/platform_elf.c" "${PLATFORM}/headless.c"
  "${PLATFORM}/display.c" "${PLATFORM}/platform_fs.c"
  "${HOST}/loader.c" "${HOST}/hle.c" "${HOST}/entry.S" "${HOST}/thread.c"
  "${HOST}/hle_os.c" "${HOST}/hle_svc.c" "${HOST}/hle_thread.c"
  "${HOST}/hle_sdkinit.c" "${HOST}/hle_table.c" "${HOST}/hle_count.c"
  "${HOST}/hle_oe.c" "${HOST}/hle_time.c" "${HOST}/hle_net.c"
  "${HOST}/hle_account.c" "${HOST}/hle_friends.c" "${HOST}/hle_misc.c"
  "${HOST}/hle_hid.c" "${HOST}/hle_touch.c" "${HOST}/hle_fs.c"
  "${HOST}/dm_i18n.c" "${HOST}/hle_tact.c"
  "${REPO}/dm/gfx/hle_vi.c" "${REPO}/dm/gfx/hle_nv.c"
  "${SHA}/sha256.c" "${SHA}/sha1.c" "${SHA}/md5.c"
  "${DEFLATE}/deflate_decompress.c" "${DEFLATE}/zlib_decompress.c"
  "${DEFLATE}/adler32.c" "${DEFLATE}/utils.c" "${DEFLATE}/arm/cpu_features.c")
set(NVN_D2R_HOST_BRIDGE ON CACHE BOOL "Original HLE_FORCE bridge" FORCE)
foreach(header NVN_CS_HEADER NVN_GRAPHICS_HEADER NVN_GRAPHICS_API_HEADER
               NVN_MAXWELL_HEADER NVN_TEXTURE_HEADER NVN_TEXTURE_API_HEADER NVN_AHB_GRAPHICS_HEADER NVN_COPY_PM4_HEADER NVN_TEXTURE_CONVERSION_HEADER NVN_FULLSIZE_LINEAR_HEADER NVN_TEXTURE_BRIDGE_HEADER NVN_WINDOW_SOURCE_BRIDGE_HEADER NVN_PIPELINE_QUALIFICATION_HEADER)
  set(${header} "" CACHE FILEPATH "No probe specialization in the game host" FORCE)
endforeach()
foreach(slots NVN_EXTERNAL_IMAGE_SLOTS NVN_WINDOW_SLOTS NVN_FULLSIZE_DISPLAY_SLOTS NVN_WINDOW_SOURCE_SLOTS)
  set(${slots} "" CACHE STRING "No qualification slots in the game host" FORCE)
endforeach()
foreach(mode NVN_TEXTURE_BRIDGE_PROBE NVN_TEXTURE_BRIDGE_DIAGNOSTICS NVN_NATIVE_PIPELINE_PROBE NVN_API_PROBE NVN_DEVICE_INSPECT NVN_IOCOHERENT_INSPECT NVN_AHB_PROBE NVN_AHB_PRESENT NVN_COPY_PROBE NVN_COUNTER_PROBE NVN_SAMPLER_PROBE NVN_TEXTURE_CONVERSION_PROBE)
  set(${mode} OFF CACHE BOOL "No probe mode in the game host" FORCE)
endforeach()
set(NVN_TEXTURE_DESCRIPTOR_FLAGS "0" CACHE STRING "No descriptor qualification observer in the game host" FORCE)
add_subdirectory("${NVN_REPO}/nvn-native" driver)
# Host-only bounded input observation reuses original platform_elf.c.
target_compile_definitions(nvn_d2r_host_bridge PRIVATE NN_D2R_PROGRAM_INPUT_OBSERVE=1)
target_compile_options(nvn_kgsl PRIVATE -fno-emulated-tls)
add_library(dmhost_activity_kgsl SHARED ${NATIVE_HOST_SOURCES} activity_host.c)
set_source_files_properties("${PLATFORM}/main.c" PROPERTIES COMPILE_DEFINITIONS main=dm_android_main)
target_sources(dmhost_activity_kgsl PRIVATE $<TARGET_OBJECTS:nvn_d2r_host_bridge>)
target_include_directories(dmhost_activity_kgsl PRIVATE "${HOST}" "${SHA}")
target_compile_definitions(dmhost_activity_kgsl PRIVATE
  _GNU_SOURCE DM_STRICT_HLE=1 DM_HEADLESS=1 DM_ANDROID_APP=1 DM_TLS_DESC=1 DM_NATIVE_SHARED_ENTRY=1 MBEDTLS_CONFIG_FILE="dm_sha_config.h")
# ABI2: original dynamic TLSDESC host. Canonical elfdesc modules are mandatory.
# No preload/initial-exec shim and no renderer from the original Android target.
target_compile_options(dmhost_activity_kgsl PRIVATE
  -O1 -g -fno-omit-frame-pointer -fno-emulated-tls -fvisibility=hidden -Wall -Wno-format)
target_link_options(dmhost_activity_kgsl PRIVATE
  -Wl,-z,max-page-size=16384 -Wl,--no-undefined -Wl,--gc-sections
  "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/dmhost_activity_kgsl.map")
target_link_libraries(dmhost_activity_kgsl PRIVATE nvn_kgsl m dl)
target_link_options(dmhost_activity_kgsl PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map")
target_include_directories(dmhost_activity_kgsl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${NVN_REPO}/nvn-native/adapters/d2r")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/activity_host.c" PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror")
string(REPLACE ";" "\n" HOST_SOURCE_TEXT "${NATIVE_HOST_SOURCES}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/host-sources.txt" CONTENT "${HOST_SOURCE_TEXT}\n")
