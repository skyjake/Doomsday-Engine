find_package (glbinding QUIET)

#if (TARGET glbinding::glbinding)
#    if (NOT TARGET glbinding)
#        add_library (glbinding ALIAS glbinding::glbinding)
#    endif ()
#endif ()

if (NOT TARGET glbinding::glbinding AND NOT TARGET glbinding)
    set (GLBINDING_RELEASE 3.0.2)
    message (STATUS "cginternals/glbinding ${GLBINDING_RELEASE} will be downloaded and built")
    include (ExternalProject)
    set (glbindingOpts
        -Wno-dev
        -DOPTION_BUILD_EXAMPLES=NO
        -DOPTION_BUILD_TOOLS=NO
        -DOPTION_BUILD_TESTS=NO
    )
    if (MSVC)
        # Don't bother with multiconfig, always use the release build.
        list (APPEND glbindingOpts -DCMAKE_BUILD_TYPE=Release)
    endif ()
    if (APPLE)
        set (glbindingLibName libglbinding.${GLBINDING_RELEASE}.dylib)
    else ()
        message (FATAL_ERROR "defined shared library suffix")
    endif ()
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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/glbinding/src/github-glbinding/source/glbinding/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/glbinding/src/github-glbinding-build/source/glbinding/include>)
    target_link_libraries (glbinding INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/glbinding/src/github-glbinding-build/${glbindingLibName})
    install (TARGETS glbinding EXPORT glbinding)
    install (EXPORT glbinding DESTINATION ${DE_INSTALL_LIB_DIR})
    add_dependencies (glbinding github-glbinding)
endif ()

if (TARGET glbinding::glbinding)
    set (GLBINDING_TARGET glbinding::glbinding)
else ()
    set (GLBINDING_TARGET glbinding)
endif ()
