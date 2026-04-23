# Doomsday Engine - Post-build operations

cmake_minimum_required (VERSION 3.10)
include (../cmake/Config.cmake)

# Deployment of system libraries. These scripts are needed on platforms where the
# regular "install" target does not produce fully standalone binaries. On macOS,
# deploy_apple.py is run separately on each application bundle after it has been
# fully assembled (via `deng_deploy_target`).
if (DE_ENABLE_DEPLOYMENT)
    if (CYGWIN)
        set (scriptName "deploy_cygwin.py")
    elseif (MSYS)
        set (scriptName "deploy_msys.py")
    elseif (MINGW)
        set (scriptName "deploy_mingw.py")
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
    # Code-sign and notarize the application bundles (deploy_apple.py has already
    # been run at this point).
    if (TARGET client)
        deng_codesign (client)
    endif ()
    if (TARGET shell)
        deng_codesign (shell)
    endif ()
endif ()
