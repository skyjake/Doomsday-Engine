#ifndef __JHERETIC_VERSION_H__
#define __JHERETIC_VERSION_H__

//#define VERSION 130 // Original Heretic version.

#define SAVE_VERSION	130	// Don't change this.

#ifndef JHERETIC_VER_ID
#  ifdef _DEBUG
#    define JHERETIC_VER_ID "+D Doomsday"
#  else
#    define JHERETIC_VER_ID "Doomsday"
#  endif
#endif

#define VERSION 210
#define VERSION_TEXT "1.4."DOOMSDAY_RELEASE_NAME
#define VERSIONTEXT "Version "VERSION_TEXT" "__DATE__" ("JHERETIC_VER_ID")"

#endif
