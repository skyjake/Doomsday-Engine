# - Try to find EAX
# Once done this will define
#
#  EAX_FOUND - system has EAX
#  EAX_INCLUDE_DIRS - the EAX include directory
#  EAX_LIBRARIES - Link these to use EAX
#  EAX_DEFINITIONS - Compiler switches required for using EAX
#
#  Copyright (c) 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
#
#  Redistribution and use is allowed according to the terms of the
#  GNU GPL v3
#


if (EAX_LIBRARIES AND EAX_INCLUDE_DIRS)
  # in cache already
  set(EAX_FOUND TRUE)
else (EAX_LIBRARIES AND EAX_INCLUDE_DIRS)
  find_path(EAX_INCLUDE_DIR
    NAMES
      eax.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
      c:/sdk/eax/include
  )

  find_library(EAX_LIBRARY
    NAMES
      eax
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
      c:/sdk/eax/lib
  )

  set(EAX_INCLUDE_DIRS
    ${EAX_INCLUDE_DIR}
  )
  set(EAX_LIBRARIES
    ${EAX_LIBRARY}
)

  if (EAX_INCLUDE_DIRS AND EAX_LIBRARIES)
     set(EAX_FOUND TRUE)
  endif (EAX_INCLUDE_DIRS AND EAX_LIBRARIES)

  if (EAX_FOUND)
    if (NOT EAX_FIND_QUIETLY)
      message(STATUS "Found EAX: ${EAX_LIBRARIES}")
    endif (NOT EAX_FIND_QUIETLY)
  else (EAX_FOUND)
    if (EAX_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find EAX")
    endif (EAX_FIND_REQUIRED)
  endif (EAX_FOUND)

  # show the EAX_INCLUDE_DIRS and EAX_LIBRARIES variables only in the advanced view
  mark_as_advanced(EAX_INCLUDE_DIRS EAX_LIBRARIES)

endif (EAX_LIBRARIES AND EAX_INCLUDE_DIRS)

