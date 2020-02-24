# Appends a string to an existing variable.
macro (append var str)
    if ("${${var}}" STREQUAL "")
        set (${var} "${str}")
    else ()
        set (${var} "${${var}} ${str}")
    endif ()
endmacro (append)

macro (append_unique var str)
    string (REPLACE "+" "\\+" _match ${str})
    if (NOT ${var} MATCHES ".*${_match}.*")
        append (${var} ${str})
    endif ()
    set (_match)
endmacro (append_unique)

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

# Removes matching items from the list.
function (list_remove_matches listName expr)
    foreach (item ${${listName}})
        if (NOT item MATCHES ${expr})
            list (APPEND result ${item})
        endif ()
    endforeach (item)
    set (${listName} ${result} PARENT_SCOPE)
endfunction (list_remove_matches)

macro (clean_paths outputVariable text)
    string (REGEX REPLACE "${CMAKE_BINARY_DIR}/([A-Za-z0-9]+)"
        "\\1" ${outputVariable} "${text}"
    )
endmacro ()

macro (enable_cxx11 target)
    if (NOT MSVC)
        if (NOT CMAKE_VERSION VERSION_LESS 3.2 OR # Clang/GCC & CMake 3.2+
            (NOT CMAKE_VERSION VERSION_LESS 3.1 AND
                NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
            set_property (TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
            set_property (TARGET ${target} PROPERTY CXX_STANDARD 11)
            set_property (TARGET ${target} PROPERTY C_STANDARD_REQUIRED ON)
            set_property (TARGET ${target} PROPERTY C_STANDARD 11)
        endif ()
    endif ()
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        append (CMAKE_C_FLAGS "-fms-extensions")
    endif ()
endmacro (enable_cxx11)

macro (strict_warnings target)
    if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set_property (TARGET ${target}
            APPEND PROPERTY COMPILE_OPTIONS -Wall -Wextra -pedantic
        )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_property (TARGET ${target}
            APPEND PROPERTY COMPILE_OPTIONS -Wall -Wextra -Wno-deprecated-declarations
        )
    endif ()
endmacro (strict_warnings)

macro (relaxed_warnings target)
    if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set_property (TARGET ${target}
            APPEND PROPERTY COMPILE_OPTIONS
                -Wno-missing-field-initializers
                -Wno-missing-braces
                -Wno-nested-anon-types
                -Wno-gnu-anonymous-struct
                -Wno-deprecated-declarations
        )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_property (TARGET ${target}
            APPEND PROPERTY COMPILE_OPTIONS
                -Wno-missing-field-initializers
        )
    endif ()
endmacro (relaxed_warnings)

# Apply cotire to improve build efficiency.
macro (deng_cotire target precompiledHeader)
    if (DE_ENABLE_COTIRE)
        set_target_properties (${target} PROPERTIES
            COTIRE_ADD_UNITY_BUILD        FALSE
            COTIRE_CXX_PREFIX_HEADER_INIT ${precompiledHeader}
        )
        cotire (${target})
    endif ()
endmacro (deng_cotire)

macro (deng_target_rpath target)
    if (APPLE)
        set (_extraRPath)
        set_target_properties (${target} PROPERTIES
            INSTALL_RPATH "@loader_path/../Frameworks;@executable_path/../${DE_INSTALL_LIB_DIR};${_extraRPath}"
        )
        if (${target} MATCHES "test_.*")
            # These won't be deployed, so we can use the full path.
            set_property (TARGET ${target} APPEND PROPERTY
                INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${DE_INSTALL_LIB_DIR};${CMAKE_INSTALL_PREFIX}/${DE_INSTALL_PLUGIN_DIR}"
            )
        endif ()
        set (_extraRPath)
    elseif (UNIX)
        set_property (TARGET ${target}
            PROPERTY INSTALL_RPATH
                "${CMAKE_INSTALL_PREFIX}/${DE_INSTALL_PLUGIN_DIR};${CMAKE_INSTALL_PREFIX}/${DE_INSTALL_LIB_DIR};$ORIGIN/../${DE_INSTALL_PLUGIN_DIR};$ORIGIN/../${DE_INSTALL_LIB_DIR}"
        )
    endif ()
endmacro (deng_target_rpath)

macro (deng_target_defaults target)
    if (APPLE)
        deng_xcode_attribs (${target})
        # macOS version numbers come from the Info.plist, we don't need version symlinks.
    elseif (MSYS OR CYGWIN)
        #set_target_properties (${target} PROPERTIES
        #    VERSION ${DE_VERSION}
        #)
    else ()
        set_target_properties (${target} PROPERTIES
            VERSION   ${DE_VERSION}
            SOVERSION ${DE_COMPAT_VERSION}
        )
    endif ()
    if (MSVC)
        set_target_properties (${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DE_VS_STAGING_DIR}/Debug/bin"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DE_VS_STAGING_DIR}/Release/bin"
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${DE_VS_STAGING_DIR}/MinSizeRel/bin"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${DE_VS_STAGING_DIR}/RelWithDebInfo/bin"

            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${DE_VS_STAGING_DIR}/Debug/bin"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${DE_VS_STAGING_DIR}/Release/bin"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${DE_VS_STAGING_DIR}/MinSizeRel/bin"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${DE_VS_STAGING_DIR}/RelWithDebInfo/bin"
        )
    endif ()
    deng_target_rpath (${target})
    enable_cxx11 (${target})
    strict_warnings (${target})
endmacro (deng_target_defaults)

# Checks all the files in the arguments and removes the ones that
# are not applicable to the current platform.
function (deng_filter_platform_sources outName)
    list (REMOVE_AT ARGV 0) # outName
    foreach (fn ${ARGV})
        set (filtered NO)
        if ("${fn}" MATCHES ".*\\.DS_Store")
            set (filtered YES)
        elseif ("${fn}" MATCHES ".*_windows\\..*" OR
            "${fn}" MATCHES ".*/windows/.*") # Windows-specific
            if (NOT (WIN32 OR CYGWIN OR MSYS))
                set (filtered YES)
            endif ()
        elseif ("${fn}" MATCHES ".*_macx\\..*") # macOS specific
            if (NOT APPLE)
                set (filtered YES)
            endif ()
        elseif ("${fn}" MATCHES ".*_unix\\..*" OR
                "${fn}" MATCHES ".*/unix/.*") # Unix specific files (Linux / macOS / etc.)
            if (NOT UNIX OR MINGW)
                set (filtered YES)
            endif ()
        elseif ("${fn}" MATCHES ".*_x11\\..*") # X11 specific files
            if (APPLE OR MSYS OR CYGWIN OR NOT UNIX)
                set (filtered YES)
            endif ()
        endif ()
        if (NOT filtered)
            list (APPEND result ${fn})
        endif ()
    endforeach (fn)
    set (${outName} ${result} PARENT_SCOPE)
endfunction (deng_filter_platform_sources)

macro (deng_glob_sources varName globbing)
    file (GLOB _src ${globbing})
    list (APPEND ${varName} ${_src})
    set (_src)
endmacro ()

# Combines multiple source files into a single large file.
macro (deng_merge_sources srcName globbing)
    if (DE_ENABLE_TURBO)
        file (GLOB _files ${globbing})
        deng_filter_platform_sources (_mergingSources ${_files})
        set (_turbo ${CMAKE_CURRENT_BINARY_DIR}/src_${srcName}_turbo.cpp)
        add_custom_command (
            OUTPUT  ${_turbo}
            COMMAND ${PYTHON_EXECUTABLE} "${DE_CMAKE_DIR}/merge_sources.py"
                    ${_turbo} ${_mergingSources}
            DEPENDS ${_mergingSources}
            COMMENT "Merging sources ${globbing}"
        )
        # The original source files should not be compiled any more.
        # They remain part of the project so they are available in the IDE.
        set_property (SOURCE ${_mergingSources} PROPERTY HEADER_FILE_ONLY YES)
        set_property (SOURCE ${_turbo} PROPERTY SKIP_AUTOMOC YES)
        list (APPEND SOURCES ${_turbo};${_mergingSources})
        set (_files)
        set (_mergingSources)
        set (_turbo)
    else ()
        # Simply use the source files as is.
        deng_glob_sources (SOURCES ${globbing})
    endif ()
endmacro()

# Set up resource bundling on macOS.
# The arguments are a list of resource files/directories with the
# destination directory separated by a comma:
#
#   res/macx/shell.icns,Resources
#
# If the destination is omitted, it defaults to "Resources".
#
# If the file path is the name of an existing target, its location
# is used as the path.
#
# DE_RESOURCES is set to a list of the individual source files
# to be added to add_executable().
#
# deng_bundle_resources() must be called after the target is added
# to specify the locations of the individual files.
function (deng_find_resources)
    string (LENGTH ${CMAKE_CURRENT_SOURCE_DIR} srcDirLen)
    math (EXPR srcDirLen "${srcDirLen} + 1")
    set (src)
    foreach (pair ${ARGV})
        string (REGEX REPLACE "(.*),.*" "\\1" fn ${pair})
        string (REGEX REPLACE ".*,(.*)" "\\1" dest ${pair})
        if (NOT pair MATCHES ".*,.*")
            # No comma means the destination defaults to Resources.
            set (dest Resources)
        endif ()
        if (TARGET ${fn})
            get_property (ptype TARGET ${fn} PROPERTY TYPE)
            if (ptype STREQUAL INTERFACE_LIBRARY)
                get_property (fns TARGET ${fn} PROPERTY INTERFACE_LINK_LIBRARIES)
            else ()
                # Use the location of the target.
                get_property (fns TARGET ${fn} PROPERTY DE_LOCATION)
            endif ()
        else ()
            set (fns ${fn})
        endif ()
        foreach (fn ${fns})
            set (origFn ${fn})
            if (NOT IS_ABSOLUTE ${fn})
                set (fn ${CMAKE_CURRENT_SOURCE_DIR}/${fn})
            endif ()
            if (NOT EXISTS ${fn})
                # Just ignore it.
                message (STATUS "deng_find_resources: Ignoring ${fn} -- not found")
            elseif (NOT IS_DIRECTORY ${fn})
                # Add as a single file.
                list (APPEND src ${origFn})
                list (APPEND spec "${origFn},${dest}")
            else ()
                #list (APPEND src ${origFn})
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
                    # Omit the current source directory.
                    if (path MATCHES "${CMAKE_CURRENT_SOURCE_DIR}.*")
                        string (SUBSTRING ${path} ${srcDirLen} -1 subPath)
                    else ()
                        set (subPath ${path})
                    endif ()
                    list (APPEND spec "${subPath},${dest}/${subDest}")
                    list (APPEND src ${subPath})
                endforeach (path)
            endif ()
        endforeach (fn)
    endforeach (pair)
    set (_deng_resource_spec ${spec} PARENT_SCOPE)
    set (DE_RESOURCES ${src} PARENT_SCOPE)
endfunction (deng_find_resources)

# Called after deng_setup_resources() and add_executable() to cause
# the files to be actually placed into the bundle.
function (deng_bundle_resources)
    foreach (pair ${_deng_resource_spec})
        string (REGEX REPLACE "(.*),.*" "\\1" fn ${pair})
        string (REGEX REPLACE ".*,(.*)" "\\1" dest ${pair})
        #message (STATUS "Bundling: ${fn} -> ${dest}")
        set_source_files_properties (${fn} PROPERTIES MACOSX_PACKAGE_LOCATION ${dest})
    endforeach (pair)
endfunction (deng_bundle_resources)

# Build a Doomsday 2 package using the buildpackage.py script.
function (deng_add_package packName)
    set (outName "${packName}.pack")
    get_filename_component (fullPath "${outName}" ABSOLUTE)
    if (NOT EXISTS ${fullPath})
        message (FATAL_ERROR "deng_add_package: \"${outName}\" not found")
    endif ()
    set (outDir ${CMAKE_CURRENT_BINARY_DIR})
    # Build the package immediately during the CMake run.
    if (NOT PYTHON_EXECUTABLE)
        message (FATAL_ERROR "Python interpreter not found (required for packaging resources)")
    endif ()
    execute_process (COMMAND
        ${PYTHON_EXECUTABLE}
        "${DE_SOURCE_DIR}/build/scripts/buildpackage.py" ${fullPath} ${outDir}
        OUTPUT_VARIABLE msg
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    clean_paths (msg "${msg}")
    message (STATUS "${msg}")
    # Find all the source files for the package.
    file (GLOB_RECURSE packSrc "${fullPath}/*")
    list_remove_matches (packSrc ".*\\.DS_Store")
    # Ensure the package gets rebuilt if the source files are edited.
    add_custom_command (OUTPUT ${outDir}/${outName}
        COMMAND ${PYTHON_EXECUTABLE} "${DE_SOURCE_DIR}/build/scripts/buildpackage.py"
                ${fullPath} ${outDir}
        DEPENDS ${packSrc}
        COMMENT "Packaging ${packName}..."
    )
    # The package target is used for dependency tracking and deployment.
    add_custom_target (${packName} SOURCES ${packSrc} ${outDir}/${outName})
    set_target_properties (${packName} PROPERTIES
        DE_LOCATION "${outDir}/${outName}"
        FOLDER Packages
    )
    if (NOT APPLE)
        set (packComponent packs)
    else ()
        # On the Mac, packages are bundled with the apps, however if the SDK is installed,
        # the packages need to be made available separately.
        set (packComponent sdk)
    endif ()
    if (NOT IOS)
        install (FILES ${outDir}/${outName}
            DESTINATION ${DE_INSTALL_DATA_DIR}
            COMPONENT ${packComponent}
        )
    endif ()
    if (MSVC)
        # In addition to installing, copy the packages to the build products
        # directories so that executables can be run in them.
        foreach (cfg ${DE_CONFIGURATION_TYPES})
            file (MAKE_DIRECTORY ${DE_VS_STAGING_DIR}/${cfg}/data)
            file (COPY ${outDir}/${outName} DESTINATION ${DE_VS_STAGING_DIR}/${cfg}/data)
        endforeach (cfg)
    endif ()
    set (DE_REQUIRED_PACKAGES ${DE_REQUIRED_PACKAGES} ${packName} PARENT_SCOPE)
endfunction (deng_add_package)

function (deng_find_packages fullPaths)
    set (PKG_DIR "${CMAKE_INSTALL_PREFIX}/${DE_INSTALL_DATA_DIR}")
    sublist (names 1 -1 ${ARGV})
    if (NOT names)
        return ()
    endif ()
    list (REMOVE_DUPLICATES names)
    foreach (name ${names})
        if (TARGET ${name})
            get_property (loc TARGET ${name} PROPERTY DE_LOCATION)
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

# Adds an SDK library target.
macro (deng_add_library target)
    # Form the list of source files.
    sublist (_src 1 -1 ${ARGV})
    deng_filter_platform_sources (_src ${_src})
    # Define the target and namespace alias.
    if (NOT IOS)
        add_library (${target} SHARED ${_src})
    else ()
        add_library (${target} STATIC ${_src})
    endif ()
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
    if (APPLE AND NOT IOS)
        set_property (TARGET ${target} PROPERTY BUILD_WITH_INSTALL_RPATH ON)
    endif ()
    target_include_directories (${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
        $<INSTALL_INTERFACE:include/>
    )
    #cotire (${target})
endmacro (deng_add_library)

macro (deng_deploy_library target name)
    if (DE_ENABLE_SDK)
        install (TARGETS ${target} EXPORT ${name}
            RUNTIME DESTINATION bin COMPONENT libs
            LIBRARY DESTINATION ${DE_INSTALL_LIB_DIR} COMPONENT libs
            ARCHIVE DESTINATION ${DE_INSTALL_LIB_DIR} COMPONENT sdk
        )
        install (EXPORT ${name} DESTINATION ${DE_INSTALL_CMAKE_DIR}/${name}
            FILE ${name}-config.cmake
            NAMESPACE Deng::
            COMPONENT sdk
        )
        if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include/de)
            install (DIRECTORY include/de DESTINATION include COMPONENT sdk)
        endif ()
    else ()
        if (NOT APPLE)
            # When the SDK is disabled, only the runtime binary is installed.
            install (TARGETS ${target}
                RUNTIME DESTINATION bin COMPONENT libs
                LIBRARY DESTINATION ${DE_INSTALL_LIB_DIR} COMPONENT libs
            )
        endif ()
    endif ()
    if (WIN32 AND DE_SIGNTOOL_CERT)
        get_property (_outName TARGET ${target} PROPERTY OUTPUT_NAME)
        deng_signtool (bin/${_outName}.dll libs)
    endif ()
endmacro (deng_deploy_library)

function (deng_deploy_target target)
    if (NOT DE_ENABLE_DEPLOYMENT)
        return ()
    endif ()
    if (APPLE)
        # Application bundles need to contain all the required dependencies.
        get_property (outName TARGET ${target} PROPERTY OUTPUT_NAME)
        install (CODE "
            execute_process (COMMAND ${PYTHON_EXECUTABLE}
                ${DE_SOURCE_DIR}/build/scripts/apple_deps.py
                \"${DE_DISTRIB_DIR}/${outName}.app\"
            )")
    endif ()
endfunction ()

macro (deng_codesign target)
    if (APPLE AND DE_CODESIGN_APP_CERT)
        get_property (_outName TARGET ${target} PROPERTY OUTPUT_NAME)
        install (CODE "
            file (GLOB fw
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app/Contents/PlugIns/Doomsday/*.bundle/Contents/MacOS/*\"
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app/Contents/PlugIns/Doomsday/*.bundle/Contents/Frameworks/*.dylib\"
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app/Contents/PlugIns/Doomsday/*.bundle\"
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app/Contents/PlugIns/*/*.dylib\"
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app/Contents/Frameworks/*.dylib\"
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app/Contents/Frameworks/*.framework\"
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app/Contents/MacOS/*\"
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app/PlugIns/*.bundle/*\"
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app/PlugIns/*.bundle\"
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app/*.dylib\"
            )
            foreach (fn IN LISTS fw)
                set (_skip NO)
                if (fn MATCHES \".*app/PlugIns.*\" AND NOT fn MATCHES \".*\\\\.bundle$\")
                    get_filename_component (fn2 \${fn} NAME)
                    if (NOT fn MATCHES \".*\${fn2}.bundle/\${fn2}$\")
                        set (_skip YES)
                        message (STATUS \"Skipping \${fn} -- not an executable\")
                    endif ()
                endif ()
                if (NOT _skip)
                message (STATUS \"Signing \${fn}...\")
                execute_process (COMMAND ${CODESIGN_COMMAND} --verbose
                        --options runtime
                        --timestamp
                        --force -s \"${DE_CODESIGN_APP_CERT}\"
                        ${DE_FW_CODESIGN_EXTRA_FLAGS}
                        \"\${fn}\"
                )
                endif ()
            endforeach (fn)
            message (STATUS \"Signing \${CMAKE_INSTALL_PREFIX}/${_outName}.app using '${DE_CODESIGN_APP_CERT}'...\")
            execute_process (COMMAND ${CODESIGN_COMMAND} --verbose
                --options runtime
                --timestamp
                --force -s \"${DE_CODESIGN_APP_CERT}\"
                ${DE_CODESIGN_EXTRA_FLAGS}
            )
            message (STATUS \"Notarizing \${CMAKE_INSTALL_PREFIX}/${_outName}.app...\")
            execute_process (COMMAND ${DENG_SOURCE_DIR}/build/scripts/notarize.py 
                \"\${CMAKE_INSTALL_PREFIX}/${_outName}.app\"
                ${DENG_NOTARIZATION_APPLE_ID}
            )")
    endif ()
    if (WIN32 AND DE_SIGNTOOL_CERT)
        get_property (_outName TARGET ${target} PROPERTY OUTPUT_NAME)
        deng_signtool (bin/${_outName}.exe "")
    endif ()
endmacro ()

macro (deng_xcode_attribs target)
    set_target_properties (${target} PROPERTIES
        XCODE_ATTRIBUTE_USE_HEADERMAP NO
        XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN NO
        XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN NO
    )
endmacro (deng_xcode_attribs)

macro (macx_set_bundle_name name)
    # Underscores are not allowed in bundle identifiers.
    string (REPLACE "_" "." MACOSX_BUNDLE_BUNDLE_NAME ${name})
endmacro (macx_set_bundle_name)

# Defines a new GUI application target that includes all the required Doomsday
# 2 packages.)
function (deng_add_application target)
    sublist (src 1 -1 ${ARGV})
    # Check for additional bundle resources.
    list (FIND src EXTRA_RESOURCES idx)
    if (idx GREATER -1)
        math (EXPR pos "${idx} + 1")
        sublist (extraRes ${pos} -1 ${src})
        sublist (src 0 ${idx} ${src})
    endif ()
    message (STATUS "Packages required for application: ${DE_REQUIRED_PACKAGES}")
    deng_find_packages (pkgs ${DE_REQUIRED_PACKAGES})
    if (APPLE)
        # Required packages are included in the application bundle.
        deng_find_resources (${pkgs} ${extraRes})
        add_executable (${target} MACOSX_BUNDLE ${src} ${DE_RESOURCES})
        deng_bundle_resources ()
        install (TARGETS ${target} DESTINATION .)
        macx_set_bundle_name ("net.dengine.${target}")
        set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE ${target})
    else ()
        add_executable (${target} ${src})
        install (TARGETS ${target} DESTINATION bin)
        # Packages are installed separately.
    endif ()
    if (WIN32)
        set_property (TARGET ${target} PROPERTY WIN32_EXECUTABLE ON)
    endif ()
    deng_target_defaults (${target})
    set_property (TARGET ${target} PROPERTY FOLDER Apps)
    if (target MATCHES "test_.*")
        if (APPLE)
            # Tests should be runnable from products/.
            set_property (TARGET ${target} APPEND PROPERTY
                INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${DE_INSTALL_LIB_DIR};${CMAKE_INSTALL_PREFIX}/${DE_INSTALL_PLUGIN_DIR}"
            )
        endif ()
    endif ()
endfunction (deng_add_application)

function (add_pkgconfig_interface_library target)
    sublist (pkgNames 1 -1 ${ARGV})
    list (GET pkgNames 0 first)
    if (NOT first STREQUAL "OPTIONAL")
        set (checkMode REQUIRED)
    else ()
        list (REMOVE_AT pkgNames 0)
    endif ()
    foreach (pkg ${pkgNames})
        set (prefix "PKG_${pkg}")
        pkg_check_modules (${prefix} ${checkMode} ${pkg})
        if (NOT checkMode STREQUAL "REQUIRED" AND NOT ${prefix}_FOUND)
            return ()
        endif ()
        # Locate full paths of the required shared libraries.
        foreach (lib ${${prefix}_LIBRARIES})
            find_library (path ${lib} HINTS ${${prefix}_LIBRARY_DIRS})
            if (NOT path STREQUAL path-NOTFOUND)
                get_filename_component (path ${path} REALPATH)
                list (APPEND libs ${path})
                unset (path CACHE)
                set (path)
            endif ()
        endforeach (lib)
        list (APPEND cflags ${${prefix}_CFLAGS})
    endforeach (pkg)
    list (REMOVE_DUPLICATES cflags)
    list (REMOVE_DUPLICATES libs)
    add_library (${target} INTERFACE)
    target_compile_options (${target} INTERFACE ${cflags})
    target_include_directories (${target} INTERFACE ${${prefix}_INCLUDE_DIRS})
    target_link_libraries (${target} INTERFACE ${libs})
endfunction (add_pkgconfig_interface_library)

function (fix_bundled_install_names binaryFile)
    if (IOS)
        return ()
    endif ()
    if (NOT EXISTS ${binaryFile})
        message (STATUS "ERROR: fix_bundled_install_names: ${binaryFile} not found")
        return ()
    endif ()
    if (binaryFile MATCHES ".*\\.bundle")
        set (ref "@loader_path/../Frameworks")
    else ()
        set (ref "@rpath")
    endif ()
    sublist (libs 1 -1 ${ARGV})
    # Check for arguments.
    list (GET libs 0 first)
    if (first STREQUAL "LD_PATH")
        list (GET libs 1 ref)
        list (REMOVE_AT libs 1 0)
    endif ()
    list (GET libs -1 last)
    if (last STREQUAL "VERBATIM")
        set (verbatim ON)
        list (REMOVE_AT libs -1)
    endif ()
    find_program (OTOOL_EXECUTABLE otool)
    execute_process (COMMAND ${OTOOL_EXECUTABLE}
        -L ${binaryFile}
        OUTPUT_VARIABLE deps
    )
    foreach (fn ${libs})
        if (NOT verbatim)
            get_filename_component (base "${fn}" NAME)
        else ()
            set (base "${fn}")
        endif ()
        string (REGEX MATCH "([^\n]+/${base}) \\(compatibility version" matched ${deps})
        if (CMAKE_MATCH_1)
            string (STRIP ${CMAKE_MATCH_1} depPath)
            if (NOT depPath MATCHES "@rpath/")
                message (STATUS "Changing install name: ${depPath}")
                message (STATUS ${CMAKE_INSTALL_NAME_TOOL}
                    -change "${depPath}" "${ref}/${base}"
                    "${binaryFile}")
                execute_process (COMMAND ${CMAKE_INSTALL_NAME_TOOL}
                    -change "${depPath}" "${ref}/${base}"
                    "${binaryFile}"
                )
            endif ()
        endif ()
    endforeach (fn)
endfunction (fix_bundled_install_names)

# Fixes the install names of the listed libraries that have been bundled into
# the target.
function (deng_bundle_install_names target)
    sublist (libs 1 -1 ${ARGV})
    # Check for arguments.
    list (GET libs 0 first)
    if (first STREQUAL "SCRIPT_NAME")
        list (GET libs 1 _s)
        set (_suffix "-${_s}")
        list (REMOVE_AT libs 0)
        list (REMOVE_AT libs 0)
    endif ()
    set (scriptName "${CMAKE_CURRENT_BINARY_DIR}/postbuild${_suffix}-${target}.cmake")
    # Correct the install names of the dependent libraries.
    file (GENERATE OUTPUT "${scriptName}" CONTENT "
set (CMAKE_MODULE_PATH ${DE_SOURCE_DIR}/cmake)
set (CMAKE_INSTALL_NAME_TOOL ${CMAKE_INSTALL_NAME_TOOL})
set (IOS ${IOS})
if (NOT IOS)
    set (bundleSubDir Contents/MacOS)
endif ()
include (Macros)
fix_bundled_install_names (\"${CMAKE_CURRENT_BINARY_DIR}/\${INT_DIR}/${target}.bundle/\${bundleSubDir}/${target}\"
    \"${libs}\")\n")
    add_custom_command (TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DINT_DIR=${CMAKE_CFG_INTDIR} -P "${scriptName}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction (deng_bundle_install_names)

# macOS: Install the libraries of a dependency target into the application bundle.
function (deng_install_bundle_deps target)
    if (APPLE AND NOT IOS)
        sublist (_deps 1 -1 ${ARGV})
        get_property (_outName TARGET ${target} PROPERTY OUTPUT_NAME)
        set (_fwDir "${_outName}.app/Contents/Frameworks")
        foreach (_dep ${_deps})
            if (TARGET ${_dep})
                if (_dep MATCHES "(.*)::(.*)")
                    install (FILES $<TARGET_FILE:${_dep}> DESTINATION ${_fwDir})
                else ()
                    #message (STATUS "CHECKING ${_dep}")
                    get_property (libs TARGET ${_dep} PROPERTY INTERFACE_LINK_LIBRARIES)
                    set (_fixInst YES)
                    if (NOT libs)
                        set (libs $<TARGET_LINKER_FILE:${_dep}>)
                        set (_fixInst NO)
                    endif ()
                    foreach (_tlib ${libs})
                        if (IS_DIRECTORY ${_tlib})
                            install (DIRECTORY ${_tlib} DESTINATION ${_fwDir})
                        else ()
                            if (EXISTS ${_tlib} OR _tlib MATCHES "^\\\$<")
                                #message (STATUS "INSTALLING: ${_tlib}")
                                install (FILES ${_tlib} DESTINATION ${_fwDir})
                                get_filename_component (_tlibname ${_tlib} NAME)
                                if (_fixInst)
                                    install (CODE "
                                        set (CMAKE_MODULE_PATH ${DE_SOURCE_DIR}/cmake)
                                        set (CMAKE_INSTALL_NAME_TOOL ${CMAKE_INSTALL_NAME_TOOL})
                                        include (Macros)
                                        fix_bundled_install_names (\${CMAKE_INSTALL_PREFIX}/${_fwDir}/${_tlibname} ${_tlibname})")
                                endif ()
                            else ()
                                #message (STATUS "NOT installing: ${_tlib}")
                            endif ()
                        endif ()
                    endforeach (_tlib)
                endif ()
            endif ()
        endforeach (_dep)
    endif ()
endfunction (deng_install_bundle_deps)

# Installs a tool executable into the approprite place(s).
function (deng_install_tool target)
    # macOS: Also install to the client application bundle.
    if (APPLE)
        set (dest "Doomsday.app/Contents/MacOS")
        get_property (name TARGET ${target} PROPERTY OUTPUT_NAME)
        if (NOT name)
            set (name ${target})
        endif()
        install (TARGETS ${target} DESTINATION ${dest})
        if (DE_CODESIGN_APP_CERT)
            install (CODE "
                execute_process (COMMAND ${CODESIGN_COMMAND}
                    --verbose -s \"${DE_CODESIGN_APP_CERT}\"
                    \"\${CMAKE_INSTALL_PREFIX}/${dest}/${name}\"
                )
            ")
        endif ()
    else ()
        sublist (comp 1 -1 ${ARGV})
        if ("${comp}" STREQUAL "")
            set (comp "tools")
        endif ()
        install (TARGETS ${target} DESTINATION bin COMPONENT ${comp})
        if (WIN32 AND DE_SIGNTOOL_CERT)
            get_property (_outName TARGET ${target} PROPERTY OUTPUT_NAME)
            if (NOT _outName)
                set (_outName ${target})
            endif ()
            deng_signtool (bin/${_outName}.exe ${comp})
        endif ()
    endif ()
endfunction (deng_install_tool)

# Install an external library that exists at configuration time.
# Used to deploy third-party libraries of dependencies. `library` is the
# full path to the shared library to install.
# Not applicable to macOS because libraries are not installed but instead
# bundled with the applicatino.
macro (deng_install_library library)
    if (UNIX_LINUX)
        string (REGEX REPLACE "(.*)\\.so" "\\1-*.so" versioned ${library})
        file (GLOB _links ${library}.* ${versioned})
        install (FILES ${library} ${_links}
            DESTINATION ${DE_INSTALL_PLUGIN_DIR}
            PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
        )
    elseif (MSYS OR CYGWIN OR MINGW)
        message (STATUS "Library will be installed: ${library}")
        install (PROGRAMS ${library} DESTINATION bin)
    elseif (MSVC)
        message (STATUS "Library will be installed: ${library}")
        install (PROGRAMS ${library} DESTINATION ${DE_INSTALL_LIB_DIR})

        # In addition to installing, copy the libraries straight to the
        # build products directories so we can run executables in them.
        foreach (cfg ${DE_CONFIGURATION_TYPES})
            file (MAKE_DIRECTORY ${DE_VS_STAGING_DIR}/${cfg}/bin)
            file (COPY ${library} DESTINATION ${DE_VS_STAGING_DIR}/${cfg}/bin)
        endforeach (cfg)
    endif ()
endmacro (deng_install_library)

# Defines a command for generating a source file from an Amethyst document project.
# `ameSourceDir` is expected to contain a number of .ame files, with `mainSrc`
# being the name of the main one. The contents of `ameSourceDir` are used (only)
# as dependencies to know when the output file needs regenerating. `type` is the
# amestd output generator def (e.g., TXT or RTF).
function (deng_add_amedoc type file ameSourceDir mainSrc)
    if (AMETHYST_FOUND)
        set (pfm ${DE_AMETHYST_PLATFORM})
        set (opts)
        if (type STREQUAL MAN)
            set (descText "manual page")
            set (pfm UNIX) # man pages are always for Unix
        elseif (type STREQUAL RTF)
            set (descText "rich text document")
        elseif (type STREQUAL HTML)
            set (descText "HTML document")
        else ()
            set (descText "text document")
            if (WIN32)
                set (opts "-dCR_NL")
            endif ()
        endif ()
        file (GLOB_RECURSE _ameSrc ${ameSourceDir}/*.ame)
        get_filename_component (_name ${file} NAME)
        add_custom_command (
            OUTPUT ${file}
            COMMAND "${AMETHYST_COMMAND}"
                ${AMETHYST_FLAGS}
                -d${type}
                -d${pfm}
                ${opts}
                -o${_name}
                ${ameSourceDir}/${mainSrc}
            DEPENDS ${_ameSrc}
            COMMENT "Compiling ${descText}..."
        )
        if (${type} STREQUAL MAN)
            install (FILES ${file} DESTINATION ${DE_INSTALL_MAN_DIR})
        elseif (UNIX)
            install (FILES ${file} DESTINATION ${DE_INSTALL_DOC_DIR}/doomsday)
        else ()
            install (FILES ${file} DESTINATION ${DE_INSTALL_DOC_DIR})
        endif ()
    endif ()
endfunction (deng_add_amedoc)

# Links one or more of the Doomsday libraries to a target.
# Prefers to use the libraries that are part of the current build.
# Otherwise, will fall back to system-provided versions.
#
# Example:  deng_link_libraries (client PUBLIC DengAppfw)
#
function (deng_link_libraries target visibility)
    sublist (names 2 -1 ${ARGV})
    foreach (name ${names})
        set (libTarget)
        if (name STREQUAL DengCore)
            set (libTarget Deng::libcore)
        endif ()
        if (name STREQUAL DengGui)
            set (libTarget Deng::libgui)
        endif ()
        if (name STREQUAL DengDoomsday)
            set (libTarget Deng::libdoomsday)
        endif ()
        if (name STREQUAL DengGameKit)
            set (libTarget Deng::libgamekit)
        endif ()
        if (name STREQUAL DengGloom)
            set (libTarget Deng::libgloom)
        endif ()
        if (name STREQUAL DengLegacy)
            message (FATAL_ERROR "DengLegacy has been removed")
        endif ()
        if (name STREQUAL DengShell)
            message (FATAL_ERROR "DengShell has been renamed")
        endif ()
        if (name STREQUAL DengComms)
            message (FATAL_ERROR "DengComms has been removed")
        endif ()
        if (name STREQUAL DengAppfw)
            message (FATAL_ERROR "DengAppfw has been removed")
        endif ()
        if (libTarget)
            list (APPEND libTargets ${libTarget})
            if (NOT TARGET ${libTarget})
                find_package (${name} REQUIRED)
            endif ()
        else ()
            list (APPEND libTargets ${name})
        endif ()
    endforeach (name)
    target_link_libraries (${target} ${visibility} ${libTargets})
endfunction (deng_link_libraries)

macro (deng_clean_path outvar)
    if (${ARGC} GREATER 1)
        set (${outvar} ${ARGV1})
        if (CYGWIN)
            execute_process (COMMAND cygpath -u ${ARGV1}
                OUTPUT_VARIABLE ${outvar}
                OUTPUT_STRIP_TRAILING_WHITESPACE)
        endif ()
    else ()
        set (${outVar} "")
    endif ()
endmacro ()
