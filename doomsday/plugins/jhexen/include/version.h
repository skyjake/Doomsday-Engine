#ifndef __JHEXEN_VERSION_H__
#define __JHEXEN_VERSION_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#ifndef VER_ID
#  define VER_ID "Doomsday"
#endif

#define GAMENAMETEXT "jHexen"

// Version numbering changes: 200 means JHexen v1.0.
#define VERSION 200
#define VERSION_TEXT "1.3."DOOMSDAY_RELEASE_NAME

#ifdef RANGECHECK
#  define VERSIONTEXT "Version "VERSION_TEXT" +R "__DATE__" ("VER_ID")"
#else
#  define VERSIONTEXT "Version "VERSION_TEXT" "__DATE__" ("VER_ID")"
#endif

#endif
