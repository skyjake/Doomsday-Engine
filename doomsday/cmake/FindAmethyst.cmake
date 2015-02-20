if (NOT DEFINED AMETHYST_COMMAND)
    find_program (AMETHYST_COMMAND amethyst 
        DOC "Amethyst document processor executable"
    )
    mark_as_advanced (AMETHYST_COMMAND)
    message (STATUS "Found Amethyst: ${AMETHYST_COMMAND}")
endif ()

if (NOT AMETHYST_COMMAND STREQUAL AMETHYST_COMMAND-NOTFOUND)
    set (AMETHYST_FOUND YES)
endif ()
