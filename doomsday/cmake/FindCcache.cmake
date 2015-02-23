find_program (CCACHE_PROGRAM ccache)

if (CCACHE_PROGRAM)
    set (CCACHE_FOUND YES)
    set (CCACHE_OPTION_DEFAULT ON)
else ()
    set (CCACHE_FOUND NO)
    set (CCACHE_OPTION_DEFAULT OFF)
endif ()

option (DENG_ENABLE_CCACHE "Use ccache when compiling" ${CCACHE_OPTION_DEFAULT})

if (DENG_ENABLE_CCACHE)
    set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property (GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    
    if (NOT DEFINED DENG_CCACHE_MSG)
        message (STATUS "Compiler cache enabled for all targets.")
        set (DENG_CCACHE_MSG ON CACHE BOOL "ccache usage notified")
        mark_as_advanced (DENG_CCACHE_MSG)
    endif ()
endif ()
