//===========================================================================
// M_FILEHASH.H
//===========================================================================
#ifndef __DOOMSDAY_MISC_FILE_HASH_H__
#define __DOOMSDAY_MISC_FILE_HASH_H__

void	FH_Clear(void);
void	FH_Init(const char *pathList);
boolean	FH_Find(const char *name, char *foundPath);

#endif 