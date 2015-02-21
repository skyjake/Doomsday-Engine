# Qmake is used to find out the Qt install location.
if (NOT QMAKE)
    find_program (QMAKE qmake-qt5 qt5-qmake qmake
        HINTS ENV PATH
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
if (NOT DEFINED QT_PREFIX_DIR)
    message (STATUS "QMake path: ${QMAKE}")

    qmake_query (QT_VERSION "QT_VERSION")
    message (STATUS "  Qt version: ${QT_VERSION}")
    if (NOT DENG_QT5_OPTIONAL)
        if (QT_VERSION VERSION_LESS "5.0")
            message (FATAL_ERROR "Qt 5.0 or later required!")
        endif ()
    endif ()

    # Path for CMake config modules.
    qmake_query (QT_PREFIX "QT_INSTALL_PREFIX")
    message (STATUS "  Qt install prefix: ${QT_PREFIX}")
    set (QT_PREFIX_DIR "${QT_PREFIX}" CACHE PATH "Qt install prefix")
    
    if (APPLE)
        qmake_query (QT_BINS "QT_INSTALL_BINS")
        set (MACDEPLOYQT_COMMAND "${QT_BINS}/macdeployqt" CACHE PATH "Qt's macdeployqt executable path")
    endif ()
endif ()
