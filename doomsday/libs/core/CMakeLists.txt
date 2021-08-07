# Doomsday Engine -- libcore

cmake_minimum_required (VERSION 3.1)
project (DE_LIBCORE)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (the_Foundation REQUIRED)
include (ZLIB)
find_package (Git QUIET)

# Options.
option (DE_LIBLEGACY_WRITER_TYPE_CHECK
    "Type-check Writer1/Reader1 behavior (incompatible with no type checking)"
    OFF
)
if (DE_LIBLEGACY_WRITER_TYPE_CHECK)
    add_definitions (-DDE_WRITER_TYPECHECK=1)
endif ()

# Definitions.
add_definitions (
    -D__LIBCORE__=1
    -D__DE__=1
    -D__DOOMSDAY__=1
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
file (GLOB_RECURSE HEADERS include/de/*.h)
file (GLOB SOURCES src/*.cpp src/*.h)

deng_merge_sources (concurrency    src/concurrency/*.cpp)
deng_merge_sources (core           src/core/*.cpp)
deng_merge_sources (data           src/data/*.cpp)
deng_merge_sources (filesys        src/filesys/*.cpp)
deng_merge_sources (filesys_remote src/filesys/remote/*.cpp)
deng_merge_sources (net            src/net/*.cpp)
deng_merge_sources (scripting      src/scripting/*.cpp)
deng_merge_sources (term           src/term/*.cpp)
deng_merge_sources (widgets        src/widgets/*.cpp)
deng_glob_sources (SOURCES         src/legacy/*.c*) # no merging where C/C++ mixed

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
target_link_libraries (libcore PUBLIC ${ZLIB_LIBRARIES} the_Foundation::the_Foundation)
if (UNIX AND NOT APPLE)
    if (DE_BASE_DIR)
        set (baseDir "${DE_BASE_DIR}")
    else ()
        set (baseDir "${CMAKE_INSTALL_PREFIX}/${DE_INSTALL_DATA_DIR}")
    endif ()
    if (DE_LIBRARY_DIR)
        set (libDir "${DE_LIBRARY_DIR}")
    else ()
        set (libDir "${CMAKE_INSTALL_PREFIX}/${DE_INSTALL_PLUGIN_DIR}")
    endif ()
    target_compile_definitions (libcore PUBLIC
        -DDE_BASE_DIR="${baseDir}"
        -DDE_LIBRARY_DIR="${libDir}"
    )
    target_link_libraries (libcore PRIVATE atomic)
endif ()
if (WIN32)
    # timeBeginPeriod, timeEndPeriod
    target_link_libraries (libcore PRIVATE winmm)
    install (PROGRAMS ${the_Foundation_DIR}/../../lib_Foundation.dll
             DESTINATION bin)
endif ()
if (MSYS)
    install (PROGRAMS ${the_Foundation_DIR}/../../msys-Foundation.dll
             DESTINATION bin)
elseif (CYGWIN)
    install (PROGRAMS ${the_Foundation_DIR}/../../cyg_Foundation.dll
             DESTINATION bin)
endif ()
if (UNIX_LINUX)
    install (FILES ${the_Foundation_DIR}/../../lib_Foundation.so
             DESTINATION ${DE_INSTALL_LIB_DIR})
endif ()
deng_deploy_library (libcore DengCore)
deng_cotire (libcore src/precompiled.h)

if (DE_ENABLE_TESTS)
    set (coreTests
        test_archive test_bitfield test_commandline test_info test_log
        test_pointerset test_record test_script test_string test_stringpool
        test_timer test_vectors
    )
    foreach (test ${coreTests})
        add_subdirectory (../../tests/${test} ${CMAKE_CURRENT_BINARY_DIR}/${test})
    endforeach (test)
    if (APPLE)
        install (PROGRAMS ${the_Foundation_DIR}/../../lib_Foundation.dylib DESTINATION lib)
    endif ()
endif ()
