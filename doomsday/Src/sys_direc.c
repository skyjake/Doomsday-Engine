
//**************************************************************************
//**
//** DD_DIREC.C
//**
//** Directory and file system related stuff.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Dir_GetDir(directory_t *dir)
{
	memset(dir, 0, sizeof(*dir));

	dir->drive = _getdrive();
	_getcwd(dir->path, 255);
	if(dir->path[strlen(dir->path)-1] != '\\')
		strcat(dir->path, "\\");

	/* VERBOSE2( printf("Dir_GetDir: %s\n", dir->path) ); */
}

int Dir_ChDir(directory_t *dir)
{
	int success;

	_chdrive(dir->drive);
	success = !_chdir(dir->path); // Successful if == 0.

	VERBOSE2( Con_Printf("Dir_ChDir: %s: %s\n", 
		success? "Succeeded" : "Failed", M_Pretty(dir->path)) );

	return success;
}

void Dir_MakeDir(const char *path, directory_t *dir)
{
	char temp[256];

	Dir_FileDir(path, dir);
	Dir_FileName(path, temp);
	strcat(dir->path, temp);
	Dir_ValidDir(dir->path); // Make it a well formed path.
}

//===========================================================================
// Dir_FileDir
//	Translates the given filename (>,} => basedir).
//===========================================================================
void Dir_FileDir(const char *str, directory_t *dir)
{
	char temp[256], pth[256];

	M_TranslatePath(str, pth);
	_fullpath(temp, pth, 255);
	_splitpath(temp, dir->path, pth, 0, 0);
	strcat(dir->path, pth);
	dir->drive = toupper(dir->path[0]) - 'A' + 1;
}

void Dir_FileName(const char *str, char *name)
{
	char ext[100];

	_splitpath(str, 0, 0, name, ext);
	strcat(name, ext);
}

int Dir_FileID(const char *str)
{
	int i, id = 0x5c33f10e;
	char temp[256], *ptr;

	memset(temp, 0, sizeof(temp));
	// Do a little scrambling with the full path name.
	_fullpath(temp, str, 255);
	strupr(temp);	
	for(i = 0, ptr = temp; *ptr; ptr++, i++)
		((unsigned char*) &id)[i % 4] += *ptr;
	return id;
}

//===========================================================================
// Dir_IsEqual
//	Returns true if the directories are equal.
//===========================================================================
boolean Dir_IsEqual(directory_t *a, directory_t *b)
{
	if(a->drive != b->drive) return false;
	return !stricmp(a->path, b->path);
}

//===========================================================================
// Dir_IsAbsolute
//	Returns true iff the given path is absolute (starts with \ or / or
//	the second character is a ':' (drive).
//===========================================================================
int Dir_IsAbsolute(const char *str)
{
	if(!str) return 0;
	if(str[0] == '\\' || str[0] == '/' || str[1] == ':') return 1;
	return 0;
}

//===========================================================================
// Dir_ValidDir
//	Appends a backslash, if necessary. Also converts forward slashes into
//	backward ones. Does not check if the directory actually exists, just
//	that it's a well-formed path name.
//===========================================================================
void Dir_ValidDir(char *str)
{
	int i, len = strlen(str);

	// DOS-ify slashes.
	for(i = 0; i < len; i++) if(str[i] == '/') str[i] = '\\';

	// Remove whitespace from the end.
	for(i = len - 1; isspace(str[i]) && i >= 0; i--) str[i] = 0; 

	// Make sure it's valid.
	if(str[len - 1] != '\\') strcat(str, "\\");
}

//===========================================================================
// Dir_MakeAbsolute
//	Converts a possibly relative path to a full path.
//===========================================================================
void Dir_MakeAbsolute(char *path)
{
	char buf[300];

	_fullpath(buf, path, 255);
	strcpy(path, buf);
}