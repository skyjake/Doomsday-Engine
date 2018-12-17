if (MSYS2_LIBS_DIR)
    add_library (the_Foundation INTERFACE)
    set (_tfDir ${MSYS2_LIBS_DIR}/${DE_ARCH}/the_Foundation)
    target_link_libraries (the_Foundation INTERFACE
        debug ${_tfDir}/lib/libFoundationd.lib
        optimized ${_tfDir}/lib/libFoundation.lib
    )
    target_include_directories (the_Foundation INTERFACE
        ${_tfDir}/include
    )
    file (GLOB _bins ${_tfDir}/lib/*.dll)
    foreach (_bin ${_bins})
        deng_install_library (${_bin})
    endforeach (_bin)
endif ()

if (NOT TARGET the_Foundation)
    find_package (the_Foundation REQUIRED)
    add_library (the_Foundation ALIAS the_Foundation::the_Foundation)
endif ()
