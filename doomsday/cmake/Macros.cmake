# Appends a string to an existing variable.
macro (append var str)
    if ("${${var}}" STREQUAL "")
        set (${var} "${str}")
    else ()
        set (${var} "${${var}} ${str}")
    endif ()
endmacro (append)

# Checks all the files in the arguments and removes the ones that
# are not applicable to the current platform.
function (deng_filter_platform_sources outName)
    list (REMOVE_AT ARGV 0) # outName
    foreach (fn ${ARGV})
        set (filtered NO)
        if ("${fn}" MATCHES ".*_windows\\..*") # Windows-specific
            if (NOT WIN32)
                set (filtered YES)
            endif ()
        elseif ("${fn}" MATCHES ".*_macx\\..*") # OS X specific
            if (NOT APPLE)
                set (filtered YES)
            endif ()
        elseif ("${fn}" MATCHES ".*_unix\\..*") # Unix specific files (Linux / OS X / etc.)
            if (NOT UNIX)
                set (filtered YES)
            endif ()
        elseif ("${fn}" MATCHES ".*_x11\\..*") # X11 specific files
            if (APPLE OR NOT UNIX)
                set (filtered YES)
            endif ()
        endif ()
        if (NOT filtered)
            list (APPEND result ${fn})
        endif ()
    endforeach (fn)
    set (${outName} ${result} PARENT_SCOPE)
endfunction (deng_filter_platform_sources)

macro (enable_cxx11 target)
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        set_property (TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
        set_property (TARGET ${target} PROPERTY CXX_STANDARD 11)
    else ()
        # This should be unnecessary with CMake 3.2+; the above should be enough.
        append (CMAKE_C_FLAGS   "-std=c11")
        append (CMAKE_CXX_FLAGS "-std=c++11")
    endif ()
endmacro (enable_cxx11)

macro (strict_warnings target)
    if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set_property (TARGET ${target} 
            APPEND PROPERTY COMPILE_OPTIONS -Wall -Wextra -pedantic
        )
    endif ()
endmacro (strict_warnings)

# The arguments are a list of resource files/directories with the 
# destination directory separated by a comma:
#   res/macx/shell.icns,Resources
#
# DENG_RESOURCES is set to a list of the individual source files
# to be added to add_executable().
#
# deng_bundle_resources() must be called after the target is added
# to specify the locations of the individual files.
function (deng_find_resources)
    string (LENGTH ${CMAKE_CURRENT_SOURCE_DIR} srcDirLen)
    math (EXPR srcDirLen "${srcDirLen} + 1")
    foreach (pair ${ARGV})
        string (REGEX REPLACE "(.*),.*" "\\1" fn ${pair})
        string (REGEX REPLACE ".*,(.*)" "\\1" dest ${pair})
        set (origFn ${fn})
        list (APPEND src ${fn})
        if (NOT IS_ABSOLUTE ${fn})
            set (fn ${CMAKE_CURRENT_SOURCE_DIR}/${fn})
        endif ()
        if (NOT IS_DIRECTORY ${fn})
            # Just add as a single file.
            list (APPEND spec "${origFn},${dest}")
        else ()
            # Do a glob to find all the files.
            file (GLOB_RECURSE _all ${fn}/*)
            # Determine length of the base path since it will be omitted 
            # from destination.
            get_filename_component (baseDir ${fn} DIRECTORY)
            string (LENGTH ${baseDir} baseLen)
            math (EXPR baseLen "${baseLen} + 1")
            foreach (path ${_all})
                get_filename_component (subDir ${path} DIRECTORY)
                string (SUBSTRING ${subDir} ${baseLen} -1 subDest)
                string (SUBSTRING ${path} ${srcDirLen} -1 subPath)
                list (APPEND spec "${subPath},${dest}/${subDest}")
                list (APPEND src ${subPath})
            endforeach (path)
        endif ()
    endforeach (pair)
    set (_deng_resource_spec ${spec} PARENT_SCOPE)
    set (DENG_RESOURCES ${src} PARENT_SCOPE)
endfunction (deng_find_resources)

# Called after deng_setup_resources() and add_executable() to cause
# the files to be actually placed into the bundle.
function (deng_bundle_resources)
    foreach (pair ${_deng_resource_spec})
        string (REGEX REPLACE "(.*),.*" "\\1" fn ${pair})
        string (REGEX REPLACE ".*,(.*)" "\\1" dest ${pair})
        set_source_files_properties (${fn} PROPERTIES MACOSX_PACKAGE_LOCATION ${dest})        
    endforeach (pair)
endfunction (deng_bundle_resources)
