set (DE_STB_DIR "${DE_EXTERNAL_SOURCE_DIR}/stb")

if (NOT TARGET stb)
    add_library (stb INTERFACE)
    target_include_directories (stb INTERFACE "${DE_STB_DIR}/..")
endif ()
