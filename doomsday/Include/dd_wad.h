#ifndef __JHEXEN_WAD_H__
#define __JHEXEN_WAD_H__

#include "sys_file.h"

#define RECORD_FILENAMELEN	256

// File record flags.
#define	FRF_RUNTIME		0x1		// Loaded at runtime (for reset).

enum // Lump Grouping Tags
{
	LGT_NONE = 0,
	LGT_FLATS,
	LGT_SPRITES,
	NUM_LGTAGS
};

typedef struct
{
	char	filename[RECORD_FILENAMELEN];	// Full filename (every '\' -> '/').
	int		numlumps;		// Number of lumps.
	int		flags;			
	DFILE	*handle;		// File handle.
	char	iwad;
} filerecord_t;

typedef struct
{
	char		name[9];	// End in \0.
	DFILE		*handle;
	int			position, size;
	int			sent;
	char		group;		// Lump grouping tag (LGT_*).
} lumpinfo_t;

extern lumpinfo_t *lumpinfo;
extern int numlumps;

void W_InitMultipleFiles(char **filenames);
void W_EndStartup(void);
int W_CheckNumForName(char *name);
int W_GetNumForName(char *name);
int W_LumpLength(int lump);
const char *W_LumpName(int lump);
void W_ReadLump(int lump, void *dest);
void W_ReadLumpSection(int lump, void *dest, int startoffset, int length);
void *W_CacheLumpNum(int lump, int tag);
void *W_CacheLumpName(char *name, int tag);
boolean W_AddFile(const char *filename, boolean allowDuplicate);
boolean W_RemoveFile(char *filename);
void W_Reset();
void W_ChangeCacheTag(int lump, int tag);
void W_CheckIWAD(void);
boolean W_IsFromIWAD(int lump);
const char *W_LumpSourceFile(int lump);
unsigned int W_CRCNumberForRecord(int idx);
unsigned int W_CRCNumber(void);
void W_GetIWADFileName(char *buf, int bufSize);
void W_GetPWADFileNames(char *buf, int bufSize, char separator);

#endif