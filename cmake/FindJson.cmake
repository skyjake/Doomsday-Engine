set (DE_JSON_DIR "${DE_EXTERNAL_SOURCE_DIR}/json")

if (NOT TARGET json)
    add_library (json INTERFACE)
    target_include_directories (json INTERFACE "${DE_JSON_DIR}/include")
endif ()
