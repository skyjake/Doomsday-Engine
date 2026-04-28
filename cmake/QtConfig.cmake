# Auto-detect Homebrew qt@5 on macOS (keg-only, not in PATH by default)
if (APPLE AND NOT Qt5_DIR)
    find_program (_brew brew)
    if (_brew)
        execute_process (COMMAND ${_brew} --prefix qt@5
            OUTPUT_VARIABLE _qt5_prefix
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if (_qt5_prefix)
            list (INSERT CMAKE_PREFIX_PATH 0 "${_qt5_prefix}")
        endif ()
    endif ()
endif ()

# Convenience wrapper: deng_target_link_qt (target PRIVATE Widgets Core ...)
macro (deng_target_link_qt target visibility)
    foreach (_qtcomp ${ARGN})
        target_link_libraries (${target} ${visibility} Qt5::${_qtcomp})
    endforeach ()
endmacro ()

# macOS/Windows: run the Qt deployment tool during install
macro (deng_install_deployqt target)
    cmake_policy (SET CMP0087 NEW)
    if (APPLE)
        get_target_property (_qtbin Qt5::qmake IMPORTED_LOCATION)
        get_filename_component (_qtbin "${_qtbin}" DIRECTORY)
        find_program (MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qtbin}")
        install (CODE "
            execute_process (COMMAND \"${MACDEPLOYQT_EXECUTABLE}\"
                \"\${CMAKE_INSTALL_PREFIX}/$<TARGET_BUNDLE_DIR_NAME:${target}>\")
        ")
    elseif (WIN32)
        get_target_property (_qtbin Qt5::qmake IMPORTED_LOCATION)
        get_filename_component (_qtbin "${_qtbin}" DIRECTORY)
        find_program (WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qtbin}")
        install (CODE "
            execute_process (COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                \"\${CMAKE_INSTALL_PREFIX}/bin/$<TARGET_FILE_NAME:${target}>\")
        ")
    endif ()
endmacro ()
