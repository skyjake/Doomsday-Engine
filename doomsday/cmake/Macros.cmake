# Appends a string to an existing variable.
macro (append var str)
    if ("${${var}}" STREQUAL "")
        set (${var} "${str}")
    else ()
        set (${var} "${${var}} ${str}")
    endif ()
endmacro (append)

# Returns a subset of a list. If length is -1, the entire remainder
# of the list is returned.
function (sublist outputVariable startIndex length)
    set (_sublist ${ARGV})
    list (REMOVE_AT _sublist 0)
    list (REMOVE_AT _sublist 0)
    # Remove items prior to the start.
    foreach (i RANGE ${startIndex})
        list (REMOVE_AT _sublist 0)
    endforeach (i)
    # Adjust the length.
    if (length GREATER -1)
        list (LENGTH _sublist s)
        while (s GREATER length)
            math (EXPR lastIdx "${s} - 1")
            list (REMOVE_AT _sublist ${lastIdx})
            set (s ${lastIdx})
        endwhile (s GREATER length)
    endif ()
    set (${outputVariable} ${_sublist} PARENT_SCOPE)
endfunction (sublist)

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

macro (deng_target_defaults target)
    set_target_properties (${target} PROPERTIES 
        VERSION       ${DENG_VERSION}
        SOVERSION     ${DENG_COMPAT_VERSION}
    )
    if (APPLE)
        set_property (TARGET ${target} PROPERTY INSTALL_RPATH "@loader_path/../Frameworks")
    elseif (UNIX)
        set_property (TARGET ${target} 
            PROPERTY INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_LIB_DIR}
        )
    endif ()        
    enable_cxx11 (${target})
    strict_warnings (${target})    
    #cotire (${target})
endmacro (deng_target_defaults)

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

# Set up resource bundling on OS X.
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
        if (NOT pair MATCHES ".*,.*")
            # No comma means the destination defaults to Resources.
            set (dest Resources)
        endif ()
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

# Build a Doomsday 2 package using the buildpackage.py script.
function (deng_add_package packName)
    set (outName "${packName}.pack")
    get_filename_component (fullPath "${outName}" ABSOLUTE)
    if (NOT EXISTS ${fullPath})
        message (FATAL_ERROR "deng_package: \"${outName}\" not found")
    endif ()
    set (outDir ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_command (
        OUTPUT ${outName}
        COMMAND ${PYTHON_EXECUTABLE} 
            "${DENG_SOURCE_DIR}/build/scripts/buildpackage.py"
            ${fullPath}
            ${outDir}
    )
    add_custom_target (${packName} ALL DEPENDS ${outName})
    set_property (TARGET ${packName} PROPERTY OUTPUT_NAME "${outDir}/${outName}")
    install (FILES ${outDir}/${outName} DESTINATION ${DENG_INSTALL_DATA_DIR})
    set (DENG_REQUIRED_PACKAGES ${DENG_REQUIRED_PACKAGES} ${packName} PARENT_SCOPE)
endfunction (deng_add_package)

function (deng_find_packages fullPaths)
    set (PKG_DIR "${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_DATA_DIR}")
    sublist (names 1 -1 ${ARGV})
    list (REMOVE_DUPLICATES names)
    foreach (name ${names})
        if (TARGET ${name})
            get_property (loc TARGET ${name} PROPERTY OUTPUT_NAME)            
            list (APPEND result ${loc})
        else ()
            # Check the installed packages.
            set (fn "${PKG_DIR}/${name}.pack")
            if (EXISTS ${fn})
                list (APPEND result ${fn})
            else ()
                message (WARNING "Package \"${name}\" not found")
            endif ()
        endif ()
    endforeach (name)
    set (${fullPaths} ${result} PARENT_SCOPE)
endfunction (deng_find_packages)

macro (deng_add_library target)
    # Form the list of source files.
    sublist (_src 1 -1 ${ARGV})
    deng_filter_platform_sources (_src ${_src})
    # Define the target and namespace alias.
    add_library (${target} SHARED ${_src})
    set (_src)
    add_library (Deng::${target} ALIAS ${target})
    # Libraries use the "deng_" prefix.
    string (REGEX REPLACE "lib(.*)" "deng_\\1" _outName ${target})
    set_target_properties (${target} PROPERTIES 
        OUTPUT_NAME ${_outName}
        FOLDER Libraries
    )
    set (_outName)
    # Compiler settings.
    deng_target_defaults (${target})
    target_include_directories (${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
        $<INSTALL_INTERFACE:include/>
    )    
    #cotire (${target})
endmacro (deng_add_library)

macro (deng_deploy_library target name)
    install (TARGETS ${target}
        EXPORT ${name} 
        LIBRARY DESTINATION ${DENG_INSTALL_LIB_DIR}
        INCLUDES DESTINATION include)
    install (EXPORT ${name} DESTINATION lib/cmake/${name} NAMESPACE Deng::)
    install (FILES ${name}Config.cmake DESTINATION lib/cmake/${name})
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include/de)
        install (DIRECTORY include/de DESTINATION include)
    endif ()
endmacro (deng_deploy_library)

# Defines a new GUI application target that includes all the required Doomsday
# 2 packages.)
macro (deng_add_application target)
    sublist (src 1 -1 ${ARGV})
    # Check for additional bundle resources.
    list (FIND src EXTRA_RESOURCES idx)
    if (idx GREATER -1)
        math (EXPR pos "${idx} + 1")
        sublist (extraRes ${pos} -1 ${src})
        sublist (src 0 ${idx} ${src})
    endif ()
    deng_find_packages (pkgs ${DENG_REQUIRED_PACKAGES})
    if (APPLE)
        deng_find_resources (${pkgs} ${extraRes})
        add_executable (${target} MACOSX_BUNDLE ${src} ${DENG_RESOURCES})
        deng_bundle_resources ()
        install (TARGETS ${target} DESTINATION .)
        set (MACOSX_BUNDLE_BUNDLE_NAME "net.dengine.${target}")
        set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE ${target})
    else ()
        add_executable (${target} ${src})
        install (TARGETS ${target} DESTINATION bin)
        install (FILES ${pkgs} DESTINATION ${DENG_INSTALL_DATA_DIR})
    endif ()
    deng_target_defaults (${target})
    set (src)
    set (pkgs)
    set (idx)
    set (pos)
    set (extraRes)
endmacro (deng_add_application)
