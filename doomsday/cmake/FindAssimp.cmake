include (assimp-config) # from deps

add_library (assimp INTERFACE)
target_compile_options (assimp INTERFACE ${ASSIMP_CXX_FLAGS})
target_include_directories (assimp INTERFACE ${ASSIMP_INCLUDE_DIRS})
target_link_libraries (assimp INTERFACE -L${ASSIMP_LIBRARY_DIRS} -l${ASSIMP_LIBRARIES})
