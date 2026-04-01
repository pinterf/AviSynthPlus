CMAKE_MINIMUM_REQUIRED( VERSION 3.6.2...3.11 )

set(PluginName "DirectShowSource")
set(ProjectName "Plugin${PluginName}")

# Sensible defaults that should just work if WINSDK is installed and baseclasses built
set(DEFAULT_BASECLASSES_PATH "C:/Program Files/Microsoft SDKs/Windows/v7.1/Samples/multimedia/directshow/baseclasses")
if(CMAKE_SIZEOF_VOID_P EQUAL 4) # 32-bit
  set(DEFAULT_BASECLASSES_LIB "${DEFAULT_BASECLASSES_PATH}/Release_MBCS/strmbase.lib")
else() # 64-bit
  set(DEFAULT_BASECLASSES_LIB "${DEFAULT_BASECLASSES_PATH}/x64/Release_MBCS/strmbase.lib")
endif()

# We need these variables set by the user to compile successfully
set(DSHOWSRC_BASECLASSES_PATH "${DEFAULT_BASECLASSES_PATH}" CACHE STRING   "Folder path to the DirectShow example baseclasses.")
set(DSHOWSRC_BASECLASSES_LIB  "${DEFAULT_BASECLASSES_LIB}"  CACHE FILEPATH "File path to the DirectShow example baseclasses precompiled static library ('strmbase.lib').")

# Create library
project(${ProjectName} VERSION ${PROJECT_VERSION} LANGUAGES CXX)
list (APPEND SourceFiles
    "directshow_source.cpp"
    "directshow_source.h"
)
add_library(${ProjectName} SHARED ${SourceFiles})
set_property(GLOBAL APPEND PROPERTY BUILT_TARGETS ${ProjectName})
set_target_properties(${ProjectName} PROPERTIES "OUTPUT_NAME" ${PluginName})
if (MINGW)
  set_target_properties(${ProjectName} PROPERTIES PREFIX "")
  set_target_properties(${ProjectName} PROPERTIES IMPORT_PREFIX "")
endif()

# Library dependencies
target_link_libraries(${ProjectName} "Winmm.lib" "Quartz.lib" "Ole32.lib" "User32.lib" "Oleaut32.lib" "Advapi32.lib" ${DSHOWSRC_BASECLASSES_LIB})

# Include directories
target_include_directories(${ProjectName} PRIVATE ${AvsCore_SOURCE_DIR} ${DSHOWSRC_BASECLASSES_PATH})

# Mark this target for skipping /permissive- 
# Old v7 SDK code won't compile otherwise, DirectShow Base Classes use non-conformant C++.
# High-level CMakeLists.txt checks this property and won't inject the flag.
set_target_properties(${ProjectName} PROPERTIES SKIP_PERMISSIVE_FLAG ON)

if (MSVC_IDE)
  # Copy output to a common folder for easy deployment
  add_custom_command(
    TARGET ${ProjectName}
    POST_BUILD
    COMMAND xcopy /Y \"$(TargetPath)\" \"${CMAKE_BINARY_DIR}/Output/plugins\"
  )
endif()

INSTALL(TARGETS "${ProjectName}"
        DESTINATION "${CORE_PLUGIN_INSTALL_PATH}/avisynth")
