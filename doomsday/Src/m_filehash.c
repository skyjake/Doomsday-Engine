/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * m_filehash.c: File Name Hash Table
 *
 * Finding files *fast*.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// Number of entries in the hash table.
#define HASH_SIZE 512

// TYPES -------------------------------------------------------------------

typedef struct direcnode_s {
	struct direcnode_s *next;
	struct direcnode_s *parent;
	char *path;
	uint count;
	boolean processed;
	boolean isOnPath;
} direcnode_t;

typedef struct hashnode_s {
	struct hashnode_s *next;
	direcnode_t *directory;
	char *fileName;
} hashnode_t;

typedef struct hashentry_s {
	hashnode_t *first;
	hashnode_t *last;
} hashentry_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void FH_AddDirectory(const char *path);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hashentry_t gHashTable[HASH_SIZE];
static direcnode_t *gDirecFirst, *gDirecLast;

// CODE --------------------------------------------------------------------

/*
 * Empty the contents of the file hash.
 */
void FH_Clear(void)
{
	direcnode_t *next;
	hashentry_t *entry;
	hashnode_t *nextNode;
	int i;

	// Free the directory nodes.
	while(gDirecFirst)
	{
		next = gDirecFirst->next;
		free(gDirecFirst->path);
		free(gDirecFirst);
		gDirecFirst = next;
	}
	gDirecLast = NULL;

	// Free the hash table.
	for(i = 0, entry = gHashTable; i < HASH_SIZE; i++, entry++)
	{
		while(entry->first)
		{
			nextNode = entry->first->next;
			free(entry->first->fileName);
			free(entry->first);
			entry->first = nextNode;
		}
	}

	// Clear the entire table.
	memset(gHashTable, 0, sizeof(gHashTable));
}

/*
 * Somewhat similar to strtok().
 */
char* M_StrTok(char **cursor, char *delimiters)
{
	char *begin = *cursor;

	while(**cursor && !strchr(delimiters, **cursor))
		(*cursor)++;

	if(**cursor)
	{
		// Stop here.
		**cursor = 0;

		// Advance one more so we'll start from the right character on 
		// the next call.
		(*cursor)++;
	}

	return begin;
}

/*
 * Returns [ a new | the ] directory node that matches the name and has
 * the specified parent node.
 */
direcnode_t *FH_DirecNode(const char *name, direcnode_t *parent)
{
	direcnode_t *node;

	// Just iterate through all directory nodes.
	for(node = gDirecFirst; node; node = node->next)
		if(!strcmp(node->path, name) && node->parent == parent)
			return node;

	// Add a new node.
	node = malloc(sizeof(*node));
	node->next = NULL;
	node->parent = parent;
	if(gDirecLast) gDirecLast->next = node;
	gDirecLast = node;
	if(!gDirecFirst) gDirecFirst = node;
	
	// Make a copy of the path. Freed in FH_Clear().
	node->path = malloc(strlen(name) + 1);
	strcpy(node->path, name);

	// No files yet.
	node->count = 0;
	node->isOnPath = false;
	node->processed = false;

	return node;
}

/*
 * The path is split into as many nodes as necessary. 
 * Parent links are set. Returns the node that identifies the 
 * given path.
 */
direcnode_t *FH_BuildDirecNodes(const char *path)
{
	char *tokPath, *cursor;
	char *part;
	direcnode_t *node, *parent;
	char relPath[256];

	// Let's try to make it a relative path.
	M_RemoveBasePath(path, relPath);
	strlwr(relPath);

	tokPath = cursor = malloc(strlen(relPath) + 1);
	strcpy(tokPath, relPath);
	parent = NULL;

	// Continue splitting as long as there are parts.
	while(*(part = M_StrTok(&cursor, "\\")))
	{
		node = FH_DirecNode(part, parent);
		parent = node;
	}

	free(tokPath);
	return node;
}

/*
 * This is hash function. It uses the base part of the file name to
 * generate a somewhat-random number between 0 and HASH_SIZE.
 */
uint FH_HashFunction(const char *name)
{
	unsigned short key = 0;
	int i;
	
	// We stop when the name ends or the extension begins.
	for(i = 0; *name && *name != '.'; i++, name++)
	{
		if(i == 0) 
			key ^= *name;
		else if(i == 1)
			key *= *name;
		else if(i == 2)
		{
			key -= *name;
			i = -1;
		}
	}
	
	return key % HASH_SIZE;
}

/*
 * Creates a file node into a directory. 
 */
void FH_AddFile(const char *filePath, direcnode_t *dir)
{
	char name[256];
	hashnode_t *node;
	hashentry_t *slot;

	// Extract the file name.
	Dir_FileName(filePath, name);
	strlwr(name);

	// Create a new node and link it to the hash table.
	node = malloc(sizeof(hashnode_t));
	node->directory = dir;
	node->fileName = malloc(strlen(name) + 1);
	strcpy(node->fileName, name);
	node->next = NULL;

	// Calculate the key.
	slot = &gHashTable[ FH_HashFunction(name) ];
	if(slot->last) slot->last->next = node;
	slot->last = node;
	if(!slot->first) slot->first = node;

	// There's now one more file in the directory.
	dir->count++;
}

/*
 * Adds a file/directory to a directory. 
 * fn is an absolute path.
 */
int FH_ProcessFile(const char *fn, filetype_t type, void *parm)
{
	char path[256], *pos;

	if(type != FT_NORMAL) return true;

	// Extract the path from the full file name.
	strcpy(path, fn);
	pos = strrchr(path, '\\');
	if(pos) *pos = 0;

	// Add a node for this file.
	FH_AddFile(fn, FH_BuildDirecNodes(path));

	return true;
}

/*
 * Process a directory and add its contents to the file hash. 
 * If the path is relative, it is relative to the base path.
 */
void FH_AddDirectory(const char *path)
{
	direcnode_t *direc = FH_BuildDirecNodes(path);
	char searchPattern[256];

	// This directory is now on the search path.
	direc->isOnPath = true;

	if(direc->processed)
	{
		// This directory has already been processed. It means the 
		// given path was a duplicate. We won't process it again.
		return;
	}

	// Compose the search pattern.
	M_PrependBasePath(path, searchPattern);
	strcat(searchPattern, "\\*"); // We're interested in *everything*.
	F_ForAll(searchPattern, NULL, FH_ProcessFile);

	// Mark all existing directories processed.
	// Everything they contain is already in the table.
	for(direc = gDirecFirst; direc; direc = direc->next)
		direc->processed = true;
}

/*
 * Initialize the file hash using the given list of paths. 
 * The paths must be separated with semicolons.
 */
void FH_Init(const char *pathList)
{
	char *tokenPaths = malloc(strlen(pathList) + 1);
	char *path, *c;

	strcpy(tokenPaths, pathList);
	path = strtok(tokenPaths, ";");
	while(path)
	{
		// We'll be using strcmp.
		strlwr(path);

		// Convert all slashes to backslashes, so things are compatible 
		// with the sys_file routines.
		for(c = path; *c; c++) if(*c == '/') *c = '\\';

		// Add this path to the hash.
		FH_AddDirectory(path);

		// Get the next path.
		path = strtok(NULL, ";");
	}
	free(tokenPaths);
}

/*
 * Returns true if the path specified in the name begins from a directory
 * in the search path. The given name is a relative path.
 */
boolean FH_MatchDirectory(hashnode_t *node, const char *name)
{
	char *pos;
	char dir[256];
	direcnode_t *direc = node->directory;

	// We'll do this in reverse order.
	strcpy(dir, name);
	while((pos = strrchr(dir, '\\')) != NULL)
	{
		// The string now ends here.
		*pos = 0;

		// Where does the directory name begin?
		pos = strrchr(dir, '\\');
		if(!pos) 
			pos = dir;
		else
			pos++;

		// Is there no more parent directories?
		if(!direc) return false;

		// Does this match the node's directory?
		if(strcmp(direc->path, pos))
		{
			// Mismatch! This is not it.
			return false;
		}

		// So far so good. Move one directory level upwards.
		direc = direc->parent;
	}	

	// We must have now arrived at a directory on the search path.
	return direc && direc->isOnPath;
}

/*
 * Composes an absolute path name for the node.
 */
void FH_ComposePath(hashnode_t *node, char *foundPath)
{
	direcnode_t *direc = node->directory;
	char buf[256];

	strcpy(foundPath, node->fileName);
	while(direc)
	{
		sprintf(buf, "%s\\%s", direc->path, foundPath);
		strcpy(foundPath, buf);
		direc = direc->parent;
	}

	// Add the base path.
	M_PrependBasePath(foundPath, foundPath);
}

/*
 * Finds a file from the hash. The file name can be a relative or an
 * absolute path. The full path is returned. Returns true if successful.
 */
boolean FH_Find(const char *name, char *foundPath)
{
	char validName[256], *c;
	char baseName[256];
	hashentry_t *slot;
	hashnode_t *node;

	// Absolute paths are not in the hash (no need to put them there).
	if(Dir_IsAbsolute(name)) 
	{
		if(F_Access(name))
		{
			strcpy(foundPath, name);
			return true;
		}
		return false;		
	}

	// Convert the given file name into a file name we can process.
	strcpy(validName, name);
	strlwr(validName);
	for(c = validName; *c; c++) if(*c == '/') *c = '\\';

	// Extract the base name.
	Dir_FileName(validName, baseName);
	
	// Which slot in the hash table?
	slot = &gHashTable[ FH_HashFunction(baseName) ];

	// Paths in the hash are relative to their directory node.
	// There is one direcnode per search path directory.

	// Go through the candidates.
	for(node = slot->first; node; node = node->next)
	{
		// The file name in the node has no path.
		if(strcmp(node->fileName, baseName)) continue; 

		// If the directory compare passes, this is the match.
		// The directory must be on the search path for the test to pass.
		if(FH_MatchDirectory(node, validName))
		{
			FH_ComposePath(node, foundPath);
			return true;
		}
	}

	// Nothing suitable was found.
	return false;
}

