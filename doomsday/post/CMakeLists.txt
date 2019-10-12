include (../cmake/Config.cmake)

if (APPLE)
    # Code-sign and notarize the application bundles.
    if (TARGET client)
        deng_codesign (client)
    endif ()
    if (TARGET shell)
        deng_codesign (shell)
    endif ()
endif ()
