# Doomsday Engine -- Qt Configuration 

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_AUTOMOC OFF)
set (CMAKE_AUTORCC OFF)
set_property (GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER Generated)

find_package (Qt)

# Find Qt5 in all projects to ensure automoc is run.
list (APPEND CMAKE_PREFIX_PATH "${QT_PREFIX_DIR}")

# This will ensure automoc works in all projects.
if (QT_MODULE STREQUAL Qt5)
    find_package (Qt5 COMPONENTS Core Network)
else ()
    find_package (Qt4 REQUIRED)
endif ()

# Check for mobile platforms.
if (NOT QMAKE_XSPEC_CHECKED)
    qmake_query (xspec "QMAKE_XSPEC")
    set (QMAKE_XSPEC ${xspec} CACHE STRING "Value of QMAKE_XSPEC")
    set (QMAKE_XSPEC_CHECKED YES CACHE BOOL "QMAKE_XSPEC has been checked")
    mark_as_advanced (QMAKE_XSPEC)
    mark_as_advanced (QMAKE_XSPEC_CHECKED)
    if (QMAKE_XSPEC STREQUAL "macx-ios-clang")
        set (IOS YES CACHE BOOL "Building for iOS platform")
    endif ()
endif ()
