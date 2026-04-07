message (STATUS "Looking for glbinding...")

if (DE_USE_SYSTEM_GLBINDING)
    include (${DE_CMAKE_DIR}/glbinding-system.cmake)
else ()
    # Use the glbinding built by build_deps.py (deps/products is first in CMAKE_PREFIX_PATH).
    find_package (glbinding REQUIRED CONFIG)
endif ()

if (DE_USE_SYSTEM_GLBINDING)
    set (DE_GLBINDING_VERSION ${glbinding_VERSION_MAJOR})
else ()
    # glbinding's config package does not export a version variable; read from header.
    file (STRINGS "${DE_DEPENDS_DIR}/products/include/glbinding/glbinding-version.h"
          _glbinding_ver_line REGEX "^#define GLBINDING_VERSION_MAJOR")
    string (REGEX MATCH "[0-9]+" DE_GLBINDING_VERSION "${_glbinding_ver_line}")
endif ()

message (STATUS "Found glbinding version ${DE_GLBINDING_VERSION}")
