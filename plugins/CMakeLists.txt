include_directories(${AvsCore_SOURCE_DIR}/include)

# These plugins are for Windows only
if (WIN32)
  # VDubFilter is x86/x64-only, so default to OFF unless target is x86/x64.
  set(VDUBFILTER_DEFAULT "OFF")
  if (CMAKE_VS_PLATFORM_NAME STREQUAL "Win32" OR CMAKE_VS_PLATFORM_NAME STREQUAL "x64")
       set(VDUBFILTER_DEFAULT "ON")
  endif()

  option(BUILD_VDUBFILTER "Build VDubFilter plugin" ${VDUBFILTER_DEFAULT})
  option(BUILD_VDUBFILTER "Build VDubFilter plugin" ON)
  #option(BUILD_VFAPIFILTER "Build VFAPIFilter plugin" ON)
endif()

# DevIL detection: skip auto detection if paths are pre-supplied via -DIL_LIBRARIES=...
if(IL_LIBRARIES AND IL_INCLUDE_DIR)
  message(STATUS "DevIL paths pre-supplied, skipping auto-detection.")
  set(DevIL_FOUND TRUE)
  set(DEVIL_PLUGIN_STATE ON)
else()
  include(FindDevIL) # official CMake module
  if(DevIL_FOUND)
    set(DEVIL_PLUGIN_STATE ON)
  else()
    MESSAGE(WARNING "DevIL library not found. ImageSeq plugin will not be built.")
    set(DEVIL_PLUGIN_STATE OFF)
  endif()
endif()

# SoundTouch detection: skip auto-detection if paths are pre-supplied via -DSOUNDTOUCH_LINK_LIBRARIES=...
if(SOUNDTOUCH_LINK_LIBRARIES AND SOUNDTOUCH_INCLUDE_DIRS)
  message(STATUS "SoundTouch paths pre-supplied, skipping auto-detection.")
  set(SOUNDTOUCH_PLUGIN_STATE ON)
else()
  find_package(PkgConfig)
  if(PkgConfig_FOUND)
    pkg_check_modules(SOUNDTOUCH soundtouch)
  endif()

  if(SOUNDTOUCH_FOUND)
    set(SOUNDTOUCH_PLUGIN_STATE ON)
  else()
    # check to see if SoundTouch's CMake files were installed and whether the library itself exists
    MESSAGE(WARNING "SoundTouch not found by pkg-config, falling back to CMake detection")
    find_package(SoundTouch)
    find_library(SOUNDTOUCH SoundTouch PATH ${SoundTouch_LIBRARY_DIRS})
    if(SoundTouch_FOUND)
      # override the SOUNDTOUCH_LINK_LIBRARIES and SOUNDTOUCH_INCLUDE_DIRS variables
      # that pkg-config would have set, and allow these to be overridden at configure time as well
      set(SOUNDTOUCH_LINK_LIBRARIES ${SOUNDTOUCH} CACHE PATH "Path to SoundTouch library")
      set(SOUNDTOUCH_INCLUDE_DIRS ${CMAKE_PREFIX_PATH}/include/soundtouch CACHE PATH "Path to SoundTouch headers")
      set(SOUNDTOUCH_PLUGIN_STATE ON)
    else()
      MESSAGE(WARNING "SoundTouch library not found. TimeStretch plugin will not be built.")
      set(SOUNDTOUCH_PLUGIN_STATE OFF)
    endif()
  endif()
endif()

option(BUILD_IMAGESEQ "Build ImageSeq plugin" ${DEVIL_PLUGIN_STATE})
option(BUILD_TIMESTRETCH "Build TimeStretch plugin" ${SOUNDTOUCH_PLUGIN_STATE})
option(BUILD_SHIBATCH "Build Shibatch plugin" ON)
option(BUILD_CONVERTSTACKED "Build ConvertStacked plugin" ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  if(NOT MSVC)
    add_definitions("-DAVSC_WIN32_GCC32")
  endif()
endif()

if (WIN32)
  if(BUILD_DIRECTSHOWSOURCE)
    add_subdirectory("DirectShowSource")
  endif()

# obviously depends on VirtualDub being available natively,
# which is only true for X86 and AMD64
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCHID)
if( ("${ARCHID}" STREQUAL "x86") OR
    ("${ARCHID}" STREQUAL "x64") OR
    ("${ARCHID}" STREQUAL "i686") OR
    ("${ARCHID}" STREQUAL "amd64") OR
    ("${ARCHID}" STREQUAL "x86_64") )
  if (BUILD_VDUBFILTER)
    add_subdirectory("VDubFilter")
  endif()
endif()

  #if (BUILD_VFAPIFILTER)
    #add_subdirectory("VFAPIFilter")
  #endif()
endif()

if (BUILD_IMAGESEQ)
add_subdirectory("ImageSeq")
endif()

if (BUILD_TIMESTRETCH)
add_subdirectory("TimeStretch")
endif()

if (BUILD_SHIBATCH)
add_subdirectory("Shibatch")
endif()

if (BUILD_CONVERTSTACKED)
add_subdirectory("ConvertStacked")
endif()
