# Qmake is used to find out the Qt install location.
set (QMAKE qmake CACHE STRING "Path of the qmake binary of the Qt version to use")

# Runs qmake to query one of its configuration variables.
function (qmake_query qvar outvar)
    execute_process (COMMAND "${QMAKE}" -query ${qvar} OUTPUT_VARIABLE result)
    string (STRIP "${result}" result)    
    set (${outvar} "${result}" PARENT_SCOPE)
endfunction (qmake_query)

# Check Qt version.
if (NOT DEFINED QT_PREFIX_DIR)
    message (STATUS "QMake path: ${QMAKE}")

    qmake_query ("QT_VERSION" QT_VERSION)
    message (STATUS "  Qt version: ${QT_VERSION}")
    if (NOT DENG_QT5_OPTIONAL)
        if (QT_VERSION VERSION_LESS "5.0")
            message (FATAL_ERROR "Qt 5.0 or later required!")
        endif ()
    endif ()

    # Path for CMake config modules.
    qmake_query ("QT_INSTALL_PREFIX" QT_PREFIX)
    message (STATUS "  Qt install prefix: ${QT_PREFIX}")
    set (QT_PREFIX_DIR "${QT_PREFIX}" CACHE PATH "Qt install prefix")
endif ()
