#ifndef __P_V13_SAVEG__
#define __P_V13_SAVEG__

void SV_v13_LoadGame(char *filename);

// Persistent storage/archiving.
// These are the load / save game routines.
void P_v13_UnArchivePlayers (void);
void P_v13_UnArchiveWorld (void);
void P_v13_UnArchiveThinkers (void);
void P_v13_UnArchiveSpecials (void);

#endif

