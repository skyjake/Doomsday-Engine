# - Find curl
# Find the native CURL headers and libraries.
#
#  CURL_INCLUDE_DIRS - where to find curl/curl.h, etc.
#  CURL_LIBRARIES    - List of libraries when using curl.
#  CURL_FOUND        - True if curl found.

# Look for the header file.
FIND_PATH(CURL_INCLUDE_DIR NAMES curl/curl.h
  PATHS
  $ENV{LIBCURLDIR}/include
  /usr/local/include
  /usr/include)
MARK_AS_ADVANCED(CURL_INCLUDE_DIR)

# Look for the library.
FIND_LIBRARY(CURL_LIBRARY NAMES curl libcurl libcurl-4
  PATHS
  $ENV{LIBCURLDIR}/lib
  /usr/lib 
  /usr/local/lib
)
MARK_AS_ADVANCED(CURL_LIBRARY)

# Copy the results to the output variables.
IF(CURL_INCLUDE_DIR AND CURL_LIBRARY)
  SET(CURL_FOUND 1)
  SET(CURL_LIBRARIES ${CURL_LIBRARY})
  SET(CURL_INCLUDE_DIRS ${CURL_INCLUDE_DIR})
ELSE(CURL_INCLUDE_DIR AND CURL_LIBRARY)
  SET(CURL_FOUND 0)
  SET(CURL_LIBRARIES)
  SET(CURL_INCLUDE_DIRS)
ENDIF(CURL_INCLUDE_DIR AND CURL_LIBRARY)

# Report the results.
IF(NOT CURL_FOUND)
  SET(CURL_DIR_MESSAGE
    "CURL was not found. Make sure CURL_LIBRARY and CURL_INCLUDE_DIR are set.")
  IF(NOT CURL_FIND_QUIETLY)
    MESSAGE(STATUS "${CURL_DIR_MESSAGE}")
  ELSE(NOT CURL_FIND_QUIETLY)
    IF(CURL_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "${CURL_DIR_MESSAGE}")
    ENDIF(CURL_FIND_REQUIRED)
  ENDIF(NOT CURL_FIND_QUIETLY)
ENDIF(NOT CURL_FOUND)
