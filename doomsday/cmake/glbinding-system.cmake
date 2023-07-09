# Find the glbinding headers and library in standard system paths

find_path(
  glbinding_INCLUDE_DIR
  NAMES glbinding/glbinding-version.h
  PATHS /usr/local/include
  PATHS /usr/include/
)

find_library(
  glbinding_LIBRARIES
  NAMES glbinding
  PATHS /usr/local/lib64/
  PATHS /usr/local/lib/
  PATHS /usr/lib64/
  PATHS /usr/lib/
)

if (glbinding_INCLUDE_DIR AND glbinding_LIBRARIES)
  SET(glbinding_FOUND TRUE)
  SET(glbinding::glbinding_FOUND TRUE)

  FILE(READ ${glbinding_INCLUDE_DIR}/glbinding/glbinding-version.h GLBINDING_VERSION_H)
  STRING(REGEX MATCH "define[ ]+GLBINDING_VERSION_MAJOR[ ]+\"[0-9]+" GLBINDING_MAJOR_VERSION_LINE "${GLBINDING_VERSION_H}")
 STRING(REGEX REPLACE "define[ ]+GLBINDING_VERSION_MAJOR[ ]+\"([0-9]+)" "\\1" glbinding_VERSION_MAJOR "${GLBINDING_MAJOR_VERSION_LINE}")

  STRING(REGEX MATCH "define[ ]+GLBINDING_VERSION_MINOR[ ]+\"[0-9]+" GLBINDING_MINOR_VERSION_LINE "${GLBINDING_VERSION_H}")
 STRING(REGEX REPLACE "define[ ]+GLBINDING_VERSION_MINOR[ ]+\"([0-9]+)" "\\1" glbinding_VERSION_MINOR "${GLBINDING_MINOR_VERSION_LINE}")

  message (STATUS "Using system glbinding library, version ${glbinding_VERSION_MAJOR}.${glbinding_VERSION_MINOR}")

ENDIF (glbinding_INCLUDE_DIRS AND glbinding_LIBRARIES)

