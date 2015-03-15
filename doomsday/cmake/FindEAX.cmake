if (WIN32)
    set (_oldPath ${EAX2_LIBRARY})
    find_library (EAX2_LIBRARY eax 
        PATHS ${EAX2_DIR} ENV DENG_DEPEND_PATH
        PATH_SUFFIXES Libs "EAX 2.0 SDK/Libs"        
    )
    mark_as_advanced (EAX2_LIBRARY)
    if (NOT _oldPath STREQUAL EAX2_LIBRARY)
        if (EAX2_LIBRARY)
            message (STATUS "Looking for EAX 2 - found")
        else ()
            message (STATUS "Looking for EAX 2 - not found (set the EAX2_DIR variable)")
        endif ()
    endif ()
    
    if (EAX2_LIBRARY AND NOT TARGET EAX2)
        add_library (EAX2 INTERFACE)
        get_filename_component (_libDir ${EAX2_LIBRARY} DIRECTORY)
        target_include_directories (EAX2 INTERFACE ${_libDir}/../Include)
        target_link_libraries (EAX2 INTERFACE ${EAX2_LIBRARY} ${_libDir}/eaxguid.lib)
        get_filename_component (_dll ${_libDir}/../dll/eax.dll REALPATH)
        deng_install_library (${_dll})
    endif ()
endif ()
