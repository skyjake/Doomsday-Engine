if (NOT DEFINED AMETHYST_EXECUTABLE)
    find_program (AMETHYST_EXECUTABLE amethyst 
        DOC "Amethyst document processor executable"
    )
    mark_as_advanced (AMETHYST_EXECUTABLE)
    message (STATUS "Found Amethyst: ${AMETHYST_EXECUTABLE}")
endif ()

if (NOT AMETHYST_EXECUTABLE STREQUAL AMETHYST_EXECUTABLE-NOTFOUND)
    set (AMETHYST_FOUND YES)
endif ()
