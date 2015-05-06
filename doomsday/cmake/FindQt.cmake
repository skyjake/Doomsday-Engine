# Qmake is used to find out the Qt install location.
if (NOT QMAKE)
    find_program (QMAKE NAMES qmake-qt5 qt5-qmake qmake qmake-qt4 qt4-qmake
        PATHS ENV PATH
        HINTS ENV DENG_DEPEND_PATH
        DOC "Path of the qmake executable to use"
    ) 
endif ()

if (NOT QMAKE)
    message (FATAL_ERROR "qmake not found -- set the QMAKE variable manually")
endif ()

# Runs qmake to query one of its configuration variables.
function (qmake_query result qtvar)
    execute_process (COMMAND "${QMAKE}" -query ${qtvar} 
        OUTPUT_VARIABLE output 
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set (${result} "${output}" PARENT_SCOPE)
endfunction (qmake_query)

# Check Qt version.
if (NOT DEFINED QT_MODULE OR 
    NOT DEFINED QT_PREFIX_DIR OR 
    (WIN32 AND NOT WINDEPLOYQT_COMMAND) OR
    (APPLE AND NOT MACDEPLOYQT_COMMAND))
    message (STATUS "QMake path: ${QMAKE}")

    qmake_query (QT_VERSION "QT_VERSION")
    message (STATUS "  Qt version: ${QT_VERSION}")

    if (QT_VERSION VERSION_LESS "4.8")
        message (FATAL_ERROR "Qt 4.8 or later required! (Qt 5 recommended)")
    endif ()

    if (NOT QT_VERSION VERSION_LESS "5.0")
	set (QT_MODULE "Qt5" CACHE STRING "Qt CMake module to use")
    else ()
	set (QT_MODULE "Qt4" CACHE STRING "Qt CMake module to use")
    endif ()
    mark_as_advanced (QT_MODULE)

    # Path for CMake config modules.
    qmake_query (QT_PREFIX "QT_INSTALL_PREFIX")
    message (STATUS "  Qt install prefix: ${QT_PREFIX}")
    set (QT_PREFIX_DIR "${QT_PREFIX}" CACHE PATH "Qt install prefix")

    qmake_query (_qtLibs "QT_INSTALL_LIBS")
    set (QT_LIBS ${_qtLibs} CACHE PATH "Qt library directory")
    mark_as_advanced (QT_LIBS)
    
    qmake_query (QT_BINS "QT_INSTALL_BINS")
    if (APPLE)
        set (MACDEPLOYQT_COMMAND "${QT_BINS}/macdeployqt" CACHE PATH 
	     "Qt's macdeployqt executable path")
        mark_as_advanced (MACDEPLOYQT_COMMAND)
    elseif (WIN32)
        set (WINDEPLOYQT_COMMAND "${QT_BINS}/windeployqt" CACHE PATH 
	     "Qt's windeployqt executable path")
        mark_as_advanced (WINDEPLOYQT_COMMAND)
    endif ()
endif ()
