#ifndef __JHEXEN_VERSION_H__
#define __JHEXEN_VERSION_H__

// Past distributions
//#define VERSIONTEXT "ID V1.2"
//#define VERSIONTEXT "RETAIL STORE BETA"		// 9/26/95
//#define VERSIONTEXT "DVL BETA 10 05 95" // Used for GT for testing
//#define VERSIONTEXT "DVL BETA 10 07 95" // Just an update for Romero
//#define VERSIONTEXT "FINAL 1.0 (10 13 95)" // Just an update for Romero

#ifndef VER_ID
#  define VER_ID "Doomsday"
#endif

// Version numbering changes: 200 means JHexen v1.0.
#define VERSION 200
#define VERSION_TEXT "2.0."DOOMSDAY_RELEASE_NAME

#ifdef RANGECHECK
#  define VERSIONTEXT "Version "VERSION_TEXT" +R "__DATE__" ("VER_ID")"
#else
#  define VERSIONTEXT "Version "VERSION_TEXT" "__DATE__" ("VER_ID")"
#endif

#endif
