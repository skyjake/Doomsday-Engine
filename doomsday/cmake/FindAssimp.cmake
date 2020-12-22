include (assimp-config) # from deps

add_library (assimp INTERFACE)
set (_cxxFlags ${ASSIMP_CXX_FLAGS})
separate_arguments (_cxxFlags)
target_compile_options (assimp INTERFACE ${_cxxFlags})
target_include_directories (assimp INTERFACE ${ASSIMP_INCLUDE_DIRS})
target_link_libraries (assimp INTERFACE -L${ASSIMP_LIBRARY_DIRS} -l${ASSIMP_LIBRARIES})
