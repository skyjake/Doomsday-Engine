#ifndef __FILE_IO_H__
#define __FILE_IO_H__

#include <stdio.h>

#define deof(file) ((file)->flags.eof != 0)

typedef struct
{
	struct DFILE_flags_s 
	{
		unsigned char open : 1;
		unsigned char file : 1;
		unsigned char eof : 1;
	} flags;
	int size;
	void *data;
	char *pos;
} DFILE;

typedef int (*f_forall_func_t)(const char *fn, int parm);

void	F_InitDirec(void);
void	F_ShutdownDirec(void);
int		F_Access(const char *path);
DFILE *	F_Open(const char *path, const char *mode);
void	F_Close(DFILE *file);
int		F_Read(void *dest, int count, DFILE *file);
int		F_GetC(DFILE *file);
int		F_Tell(DFILE *file);
int		F_Seek(DFILE *file, int offset, int whence);
void	F_Rewind(DFILE *file);
int		F_ForAll(const char *filespec, int parm, f_forall_func_t func);

#if !__DOOMSDAY__
// Redefine to match the standard file stream routines.
#define	DFILE			FILE
#define F_Open			fopen
#define F_Tell			ftell
#define F_Rewind		rewind
#define F_Read(x,y,z)	fread(x,y,1,z)
#define F_Close			fclose
#endif

#endif