if (NOT AMETHYST_COMMAND)
    find_program (AMETHYST_COMMAND amethyst 
        PATHS
            /usr/local/bin
            /usr/bin
            ${AMETHYST_DIR}
            ENV PATH
            ENV HOME
            ENV HOMEPATH
        PATH_SUFFIXES
            bin
            .local/bin
            amethyst/bin
            Amethyst/bin
        DOC "Amethyst document processor executable"
    )
    mark_as_advanced (AMETHYST_COMMAND)
    if (AMETHYST_COMMAND)
        message (STATUS "Found Amethyst: ${AMETHYST_COMMAND}")
    endif ()
else ()
    set (AMETHYST_FOUND YES)
endif ()

if (AMETHYST_STDLIB_DIR)
    set (AMETHYST_FLAGS "-i${AMETHYST_STDLIB_DIR}")
endif ()
