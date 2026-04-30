find_package (Qt6 COMPONENTS Core Gui Widgets)

if (NOT Qt6_FOUND)
    # Auto-detect Qt 6 under C:/Qt/ on Windows (standard Qt installer layout)
    if (WIN32 AND NOT Qt6_DIR)
        file (GLOB _qt6_candidates
            "C:/Qt/6.*/msvc*_64"
            "C:/Qt/6.*/*_64"
        )
        if (_qt6_candidates)
            list (SORT _qt6_candidates ORDER DESCENDING)
            list (GET _qt6_candidates 0 _qt6_prefix)
            list (INSERT CMAKE_PREFIX_PATH 0 "${_qt6_prefix}")
        endif ()
        unset (_qt6_candidates)
        unset (_qt6_prefix)
    endif ()

    # Auto-detect Homebrew qt@6 on macOS (keg-only, not in PATH by default)
    if (APPLE AND NOT Qt6_DIR)
        find_program (_brew brew)
        if (_brew)
            execute_process (COMMAND ${_brew} --prefix qt@6
                OUTPUT_VARIABLE _qt6_prefix
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if (_qt6_prefix)
                list (INSERT CMAKE_PREFIX_PATH 0 "${_qt6_prefix}")
            endif ()
        endif ()
    endif ()

    find_package (Qt6 COMPONENTS Core Gui Widgets)
endif ()

if (NOT Qt6_FOUND)
    message (FATAL_ERROR "Qt6 not found (try setting Qt6_DIR)")
endif ()

add_definitions (-DHAVE_QT6)

# Convenience wrapper: deng_target_link_qt (target PRIVATE Widgets Core ...)
macro (deng_target_link_qt target visibility)
    foreach (_qtcomp ${ARGN})
        target_link_libraries (${target} ${visibility} Qt6::${_qtcomp})
    endforeach ()
endmacro ()

# macOS/Windows: run the Qt deployment tool during install
macro (deng_install_deployqt target)
    cmake_policy (SET CMP0087 NEW)
    if (APPLE)
        get_target_property (_qtbin Qt6::qmake IMPORTED_LOCATION)
        get_filename_component (_qtbin "${_qtbin}" DIRECTORY)
        # Derive macdeployqt from Qt6::qmake's bin directory to avoid using a
        # stale cached find_program result pointing to a different Qt version.
        set (_macdeployqt "${_qtbin}/macdeployqt")
        install (CODE "
            execute_process (COMMAND \"${_macdeployqt}\"
                \"\${CMAKE_INSTALL_PREFIX}/$<TARGET_BUNDLE_DIR_NAME:${target}>\")
        ")
    elseif (WIN32)
        get_target_property (_qtbin Qt6::qmake IMPORTED_LOCATION)
        get_filename_component (_qtbin "${_qtbin}" DIRECTORY)
        find_program (WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qtbin}")
        install (CODE "
            execute_process (COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                \"\${CMAKE_INSTALL_PREFIX}/bin/$<TARGET_FILE_NAME:${target}>\")
        ")
    endif ()
endmacro ()
