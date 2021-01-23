# Doomsday Engine - Post-build operations

cmake_minimum_required (VERSION 3.1)
include (../cmake/Config.cmake)

# Deployment of system libraries
if (DE_ENABLE_DEPLOYMENT)
    if (CYGWIN)
        set (scriptName "deploy_cygwin.py")
    elseif (MSYS)
        set (scriptName "deploy_msys.py")
    else ()
        set (scriptName)
    endif ()
    if (scriptName)
        install (CODE "
            message (STATUS \"Copying dependencies...\")
            execute_process (
                COMMAND ${PYTHON_EXECUTABLE} ${DE_SOURCE_DIR}/build/scripts/${scriptName}
                    \${CMAKE_INSTALL_PREFIX}/bin
                OUTPUT_QUIET
            )"
            COMPONENT post)
    endif ()
endif ()

if (APPLE)
    # Code-sign and notarize the application bundles.
    if (TARGET client)
        deng_codesign (client)
    endif ()
    if (TARGET shell)
        deng_codesign (shell)
    endif ()
endif ()
