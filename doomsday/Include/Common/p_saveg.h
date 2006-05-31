#ifndef __P_SAVEG__
#define __P_SAVEG__

enum {
    sc_normal,
    sc_ploff,                   // plane offset
    sc_xg1
} sectorclass_e;

typedef enum lineclass_e {
    lc_normal,
    lc_xg1
} lineclass_t;

typedef enum {
    tc_null = -1,
    tc_end,
    tc_mobj,
    tc_xgmover,
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
#if __JDOOM__
    tc_glow,
    tc_flicker
#else
    tc_glow
#endif
} thinkerclass_t;

void            SV_Init(void);
void            SV_SaveGameFile(int slot, char *str);
int             SV_SaveGame(char *filename, char *description);
int             SV_GetSaveDescription(char *filename, char *str);
int             SV_LoadGame(char *filename);

// Write a client savegame file.
void            SV_SaveClient(unsigned int gameid);
void            SV_ClientSaveGameFile(unsigned int game_id, char *str);
void            SV_LoadClient(unsigned int gameid);

unsigned short  SV_ThingArchiveNum(mobj_t *mo);
mobj_t         *SV_GetArchiveThing(int num);

void            SV_Write(void *data, int len);
void            SV_WriteByte(byte val);
void            SV_WriteShort(short val);
void            SV_WriteLong(long val);
void            SV_WriteFloat(float val);
void            SV_Read(void *data, int len);
byte            SV_ReadByte();
short           SV_ReadShort();
long            SV_ReadLong();
float           SV_ReadFloat();

// Misc save/load routines.
void            SV_UpdateReadMobjFlags(mobj_t *mo, int ver);
#endif
