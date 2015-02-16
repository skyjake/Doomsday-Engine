if (NOT DEFINED AMETHYST)
    find_program (AMETHYST amethyst 
        DOC "Amethyst document processor executable"
    )
    mark_as_advanced (AMETHYST)
    message (STATUS "Found Amethyst: ${AMETHYST}")
endif ()

if (NOT AMETHYST STREQUAL AMETHYST-NOTFOUND)
    set (AMETHYST_FOUND 1)
endif ()
