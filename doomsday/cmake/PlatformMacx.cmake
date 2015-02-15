include (PlatformGenericUnix)

add_definitions (-DMACOSX)

set (DENG_FIXED_ASM_DEFAULT OFF)

macro (link_framework target fw)
    find_library (${fw}_LIBRARY ${fw})
    mark_as_advanced (${fw}_LIBRARY)
    target_link_libraries (deng_gui PRIVATE ${${fw}_LIBRARY})
endmacro (link_framework)
