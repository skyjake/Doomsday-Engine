#ifndef __P_SAVEG__
#define __P_SAVEG__

void SV_Init(void);
void SV_SaveGameFile(int slot, char *str);
int SV_SaveGame(char *filename, char *description);
int SV_GetSaveDescription(char *filename, char *str);
int SV_LoadGame(char *filename);
// Write a client savegame file.
void SV_SaveClient(unsigned int gameid);
void SV_ClientSaveGameFile(unsigned int game_id, char *str);
void SV_LoadClient(unsigned int gameid);

unsigned short SV_ThingArchiveNum(mobj_t *mo);
mobj_t *SV_GetArchiveThing(int num);

void SV_Write(void *data, int len);
void SV_WriteByte(byte val);
void SV_WriteShort(short val);
void SV_WriteLong(long val);
void SV_WriteFloat(float val);
void SV_Read(void *data, int len);
byte SV_ReadByte();
short SV_ReadShort();
long SV_ReadLong();
float SV_ReadFloat();

// Persistent storage/archiving.
// These are the load / save game routines.
void P_ArchivePlayers (void);
void P_UnArchivePlayers(boolean *infile, boolean *loaded);
void P_ArchiveWorld (void);
void P_UnArchiveWorld (void);
void P_ArchiveThinkers (void);
void P_UnArchiveThinkers (void);
void P_ArchiveSpecials (void);
void P_UnArchiveSpecials (void);

#endif

