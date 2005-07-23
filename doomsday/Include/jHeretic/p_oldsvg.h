#ifndef __P_V13_SAVEG__
#define __P_V13_SAVEG__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

void            SV_v13_LoadGame(char *filename);

// Persistent storage/archiving.
// These are the load / save game routines.
void            P_v13_UnArchivePlayers(void);
void            P_v13_UnArchiveWorld(void);
void            P_v13_UnArchiveThinkers(void);
void            P_v13_UnArchiveSpecials(void);

#endif
