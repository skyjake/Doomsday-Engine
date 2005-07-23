#ifndef __JDOOM_VERSION_H__
#define __JDOOM_VERSION_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// DOOM version
//enum { VERSION = 110 }; // obsolete

#ifndef JDOOM_VER_ID
#  ifdef _DEBUG
#    define JDOOM_VER_ID "+D Doomsday"
#  else
#    define JDOOM_VER_ID "Doomsday"
#  endif
#endif

// My my, the names of these #defines are really well chosen...
#define VERSION_TEXT "1.15."DOOMSDAY_RELEASE_NAME
#define VERSIONTEXT "Version "VERSION_TEXT" "__DATE__" ("JDOOM_VER_ID")"

// All the versions of Doom have different savegame IDs, but
// 500 will be the savegame base from now on.
#define SAVE_VERSION_BASE	500
#define SAVE_VERSION		(SAVE_VERSION_BASE + gamemode)

#endif							// __JDOOM_VERSION_H__
