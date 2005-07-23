#ifndef __P_V19_SAVEG__
#define __P_V19_SAVEG__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

void            SV_v19_LoadGame(char *filename);

// Persistent storage/archiving.
// These are the load / save game routines.
void            P_v19_UnArchivePlayers(void);
void            P_v19_UnArchiveWorld(void);
void            P_v19_UnArchiveThinkers(void);
void            P_v19_UnArchiveSpecials(void);

#endif
