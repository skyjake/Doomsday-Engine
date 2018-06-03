# Doomsday Engine -- libcore

cmake_minimum_required (VERSION 3.1)
project (DE_LIBCORE)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (CPlus)
include (ZLIB)
find_package (Git QUIET)

# Definitions.
add_definitions (
    -D__LIBCORE__=1
    -DLIBCORE_MAJOR_VERSION=${DE_VERSION_MAJOR}
    -DLIBCORE_MINOR_VERSION=${DE_VERSION_MINOR}
    -DLIBCORE_PATCHLEVEL=${DE_VERSION_PATCH}
    -DLIBCORE_RELEASE_LABEL="${DE_RELEASE_TYPE}"
    -DLIBCORE_VERSION="${DE_VERSION}"
)
if (DEFINED DE_BUILD)
    add_definitions (-DLIBCORE_BUILD_TEXT="${DE_BUILD}")
endif ()
if (GIT_FOUND)
    execute_process (
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --long
        OUTPUT_VARIABLE DE_GIT_DESCRIPTION
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (DE_GIT_DESCRIPTION)
        message (STATUS "Revision: ${DE_GIT_DESCRIPTION}")
        add_definitions (-DLIBCORE_GIT_DESCRIPTION="${DE_GIT_DESCRIPTION}")
    endif ()
endif ()

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/[a-zA-Z]*)
file (GLOB SOURCES src/*.cpp src/*.h src/core/*.h)

deng_merge_sources (concurrency    src/concurrency/*.cpp)
deng_merge_sources (core           src/core/*.cpp)
deng_merge_sources (data           src/data/*.cpp)
deng_merge_sources (filesys        src/filesys/*.cpp)
deng_merge_sources (filesys_remote src/filesys/remote/*.cpp)
deng_merge_sources (net            src/net/*.cpp)
deng_merge_sources (scriptsys      src/scriptsys/*.cpp)
deng_merge_sources (widgets        src/widgets/*.cpp)

if (WIN32)
    # Update the resource file.
    set (DE_LIBCORE_VERSION_WINDOWS ${DE_VERSION_WINDOWS})
    configure_file (res/win32/core.rc.in core.rc)
    list (APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/core.rc)
    include_directories (res/win32)
endif ()

deng_add_package (net.dengine.stdlib)

deng_add_library (libcore ${SOURCES} ${HEADERS})
target_include_directories (libcore PRIVATE ${ZLIB_INCLUDE_DIR})
#deng_target_link_qt (libcore PUBLIC Core Network)
target_link_libraries (libcore PUBLIC ${ZLIB_LIBRARIES})
target_link_libraries (libcore PRIVATE cplus)
deng_deploy_library (libcore DengCore)

deng_cotire (libcore src/precompiled.h)
