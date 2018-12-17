find_package (glbinding QUIET)

if (TARGET glbinding::glbinding)
    if (NOT TARGET glbinding)
        add_library (glbinding ALIAS glbinding::glbinding)
    endif ()
endif ()

if (NOT TARGET glbinding)
    set (GLBINDING_RELEASE 3.0.2)
    message (STATUS "cginternals/glbinding ${GLBINDING_RELEASE} will be downloaded and built")
    include (ExternalProject)
    set (glbindingOpts
        -Wno-dev
    )
    if (MSVC)
        # Don't bother with multiconfig, always use the release build.
        list (APPEND glbindingOpts -DCMAKE_BUILD_TYPE=Release)
    endif ()
    #set (glbindingLibName glbinding.dll)
    ExternalProject_Add (github-glbinding
        GIT_REPOSITORY   https://github.com/cginternals/glbinding.git
        GIT_TAG          v${GLBINDING_RELEASE}
        CMAKE_ARGS       ${glbindingOpts}
        PREFIX           glbinding
        BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/glbinding/src/github-glbinding-build/code/${glbindingLibName}
        INSTALL_COMMAND  ""
    )
    add_library (glbinding INTERFACE)
    target_include_directories (glbinding INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/glbinding/src/github-glbinding/include
        ${CMAKE_CURRENT_BINARY_DIR}/glbinding/src/github-glbinding-build/include)
    target_link_libraries (glbinding INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/glbinding/src/github-glbinding-build/code/${glbindingLibName})
    add_dependencies (glbinding github-glbinding)
endif ()
