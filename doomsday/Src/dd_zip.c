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
 * dd_zip.h: Zip/Pk3 Files
 *
 * Loading .pk3/.zip files (no compression!). 
 * Finding files inside packages. 
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

#include "sys_direc.h"

// MACROS ------------------------------------------------------------------

#define SIG_LOCAL_FILE_HEADER	0x04034b50
#define SIG_CENTRAL_FILE_HEADER	0x02014b50
#define SIG_END_OF_CENTRAL_DIR	0x06054b50

// Maximum tolerated size of the comment.
#define	MAXIMUM_COMMENT_SIZE	2048

// This is the length of the central directory end record (without the
// comment, but with the signature).
#define CENTRAL_END_SIZE		22

// File header flags.
#define ZFH_ENCRYPTED			0x1
#define ZFH_COMPRESSION_OPTS	0x6
#define ZFH_DESCRIPTOR			0x8
#define ZFH_COMPRESS_PATCHED	0x20	// Not supported.

// Compression methods.
enum 
{
	ZFC_NO_COMPRESSION = 0,		// The only supported method.
	ZFC_SHRUNK,
	ZFC_REDUCED_1,
	ZFC_REDUCED_2,
	ZFC_REDUCED_3,
	ZFC_REDUCED_4,
	ZFC_IMPLODED,
	ZFC_DEFLATED = 8,
	ZFC_DEFLATED_64,
	ZFC_PKWARE_DCL_IMPLODED
};

// TYPES -------------------------------------------------------------------

typedef struct package_s {
	struct package_s *next;
	char name[256];
	DFILE *file;
	int order;
} package_t;

typedef struct zipentry_s {
	char *name;				// Relative path (from the base path).
	package_t *package;
	unsigned int offset;	// Offset from the beginning of the package.
	unsigned int size;
} zipentry_t;

#pragma pack(1)
typedef struct localfileheader_s {
	ushort	requiredVersion;
	ushort	flags;
	ushort	compression;
	ushort	lastModTime;
	ushort	lastModDate;
	uint	crc32;
	uint	compressedSize;
	uint	size;
	ushort	fileNameSize;
	ushort	extraFieldSize;
} localfileheader_t;

typedef struct descriptor_s {
	uint	crc32;
	uint	compressedSize;
	uint	size;
} descriptor_t;

typedef struct centralfileheader_s {
	uint	signature;
	ushort	version;
	ushort	requiredVersion;
	ushort	flags;
	ushort	compression;
	ushort	lastModTime;
	ushort	lastModDate;
	uint	crc32;
	uint	compressedSize;
	uint	size;
	ushort	fileNameSize;
	ushort	extraFieldSize;
	ushort	commentSize;
	ushort	diskStart;
	ushort	internalAttrib;
	uint	externalAttrib;
	uint	relOffset;	
/*  
	file name (variable size)
	extra field (variable size)
	file comment (variable size)
*/	
} centralfileheader_t;

typedef struct centralend_s {
	ushort	disk;
	ushort	centralStartDisk;
	ushort	diskEntryCount;
	ushort	totalEntryCount;
	uint	size;
	uint	offset;
	ushort	commentSize;
} centralend_t;
#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void Zip_RemoveDuplicateFiles(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static package_t *zipRoot;
static zipentry_t *zipFiles;
static int zipNumFiles, zipAllocatedFiles;

// CODE --------------------------------------------------------------------

/*
 * Initializes the zip file database.
 */
void Zip_Init(void)
{
	VERBOSE( Con_Message("Zip_Init: Initializing package system...\n") );

	zipRoot = NULL;
	zipFiles = 0;
	zipNumFiles = 0;
	zipAllocatedFiles = 0;
}

/*
 * Shuts down the zip file database and frees all resources.
 */
void Zip_Shutdown(void)
{
	package_t *pack, *next;
	int i;

	// Close package files and free the nodes.
	for(pack = zipRoot; pack; pack = next)
	{
		next = pack->next;
		if(pack->file) F_Close(pack->file);
		free(pack);
	}

	// Free the file directory.
	for(i = 0; i < zipNumFiles; i++) free(zipFiles[i].name);
	free(zipFiles);

	zipRoot = NULL;
	zipFiles = NULL;
	zipNumFiles = 0;
	zipAllocatedFiles = 0;
}

/*
 * Allocate a zipentry array element and return a pointer to it.
 * Duplicate entries are removed later.
 */
zipentry_t* Zip_NewFile(const char *name)
{
	int oldCount = zipNumFiles;
	boolean changed = false;
	
	zipNumFiles++;
	while(zipNumFiles > zipAllocatedFiles)
	{
		// Double the size of the array.
		zipAllocatedFiles *= 2;
		if(zipAllocatedFiles <= 0) zipAllocatedFiles = 1;
		
		// Realloc the zipentry array.
		zipFiles = realloc(zipFiles, sizeof(zipentry_t) * zipAllocatedFiles);
		changed = true;
	}

	// Clear the new memory.
	if(changed)
	{
		memset(zipFiles + oldCount, 0, sizeof(zipentry_t) 
			* (zipAllocatedFiles - oldCount));
	}

	// Take a copy of the name. This is freed in Zip_Shutdown().
	zipFiles[oldCount].name = malloc(strlen(name) + 1);
	strcpy(zipFiles[oldCount].name, name);
	
	// Return a pointer to the first new zipentry.
	return zipFiles + oldCount;
}

/*
 * Sorts all the zip entries alphabetically. All the paths are absolute.
 */
int C_DECL Zip_EntrySorter(const void *a, const void *b)
{
	// Compare the names.
	return stricmp( ((const zipentry_t*)a)->name, 
		((const zipentry_t*)b)->name );
}

/*
 * Sorts all the zip entries alphabetically.
 */
void Zip_SortFiles(void)
{
	// Sort all the zipentries by name. (Note: When lots of files loaded,
	// most of list is already sorted. Quicksort becomes slow...)
	qsort(zipFiles, zipNumFiles, sizeof(zipentry_t), Zip_EntrySorter);
}

/*
 * Adds a new package to the list of packages.
 */
package_t* Zip_NewPackage(void)
{
	// When duplicates are removed, newer packages are favored.
	static int packageCounter = 0;

	package_t *newPack = calloc(sizeof(package_t), 1);
	newPack->next = zipRoot;
	newPack->order = packageCounter++;
	return zipRoot = newPack;
}

/*
 * Finds the central directory end record in the end of the file.
 * Returns true if it successfully located. This gets awfully slow if
 * the comment is long.
 */
boolean Zip_LocateCentralDirectory(DFILE *file)
{
/*	int length = F_Length(file); */
	int pos = CENTRAL_END_SIZE; // Offset from the end.
	uint signature;
	
	// Start from the earliest location where the signature might be.
	while(pos < MAXIMUM_COMMENT_SIZE)
	{
		F_Seek(file, -pos, SEEK_END);

		// Is this is signature?
		F_Read(&signature, 4, file);
		if(signature == SIG_END_OF_CENTRAL_DIR)
		{
			// This is it!
			return true;
		}

		// Move backwards.
		pos++;
	}

	// Scan was not successful.
	return false;
}

/*
 * Copies num characters (up to destSize) and adds a terminating NULL.
 */
void Zip_CopyStr(char *dest, const char *src, int num, int destSize)
{
	// Only copy as much as we can.
	if(num >= destSize) num = destSize - 1;

	memcpy(dest, src, num);
	dest[num] = 0;
}

/*
 * Opens the file zip, reads the directory and stores the info for later
 * access. If prevOpened is not NULL, all data will be read from there.
 */
boolean Zip_Open(const char *fileName, DFILE *prevOpened)
{
	DFILE *file;
	package_t *pack;
	centralend_t summary;
	void *directory;
	char *pos;
	char buf[512];
	zipentry_t *entry;
	int index;
	uint i;
	
	if(prevOpened == NULL)
	{
		// Try to open the file.
		if((file = F_Open(fileName, "rb")) == NULL)
		{
			Con_Message("Zip_Open: %s not found.\n", fileName);
			return false;
		}
	}
	else
	{
		// Use the previously opened file.
		file = prevOpened;
	}
	
	VERBOSE( Con_Message("Zip_Open: %s\n", M_Pretty(fileName)) );

	// Scan the end of the file for the central directory end record.
	if(!Zip_LocateCentralDirectory(file))
	{
		Con_Error("Zip_Open: %s: Central directory not found.\n", 
			M_Pretty(fileName));
	}

	// Read the central directory end record.
	F_Read(&summary, sizeof(summary), file);
	
	// Does the summary say something we don't like?
	if(summary.diskEntryCount != summary.totalEntryCount)
	{
		Con_Error("Zip_Open: %s: Multipart Zip files are not supported.\n", 
			M_Pretty(fileName));
	}

	// Read the entire central directory into memory.
	directory = malloc(summary.size);
	F_Seek(file, summary.offset, SEEK_SET);
	F_Read(directory, summary.size, file);

	pack = Zip_NewPackage();
	strcpy(pack->name, fileName);
	pack->file = file;

	// Read all the entries.
	pos = directory;
	for(index = 0; index < summary.totalEntryCount; index++, 
		pos += sizeof(centralfileheader_t))
	{
		centralfileheader_t *header = (void*) pos;
		char *nameStart = pos + sizeof(centralfileheader_t);

		// Advance the cursor past the variable sized fields.
		pos += header->fileNameSize + header->extraFieldSize 
			+ header->commentSize;

		Zip_CopyStr(buf, nameStart, header->fileNameSize, sizeof(buf));

		// Directories are skipped.
		if(buf[header->fileNameSize - 1] == '/' && !header->size)
			continue;

		// Do we support the format of this file?
		if(header->compression != ZFC_NO_COMPRESSION
			|| header->compressedSize != header->size)
		{
			Con_Error("Zip_Open: %s: '%s' is compressed.\n  Compression is "
				"not supported.\n", M_Pretty(fileName), buf);
		}
		if(header->flags & ZFH_ENCRYPTED)
		{
			Con_Error("Zip_Open: %s: '%s' is encrypted.\n  Encryption is "
				"not supported.\n", M_Pretty(fileName), buf);
		}

		// Convert all slashes to backslashes, for compatibility with 
		// the sys_filein routines.
		for(i = 0; buf[i]; i++) if(buf[i] == '/') buf[i] = '\\';

		// Make it absolute.
		M_PrependBasePath(buf, buf);

		// We can add this file to the zipentry list.
		entry = Zip_NewFile(buf);
		entry->package = pack;
		entry->size = header->size;
		entry->offset = header->relOffset + 4 + sizeof(localfileheader_t)
			+ header->fileNameSize + header->extraFieldSize;
	}

	// The central directory is no longer needed.
	free(directory);

	Zip_SortFiles();
	Zip_RemoveDuplicateFiles();

	// File successfully opened!
	return true;
}

/*
 * If two or more packages have the same file, the file from the last
 * loaded package is the one to use. Others will be removed from the
 * directory. The entries must be sorted before this can be done.
 */
void Zip_RemoveDuplicateFiles(void)
{
	int i;
	boolean modified = false;
	zipentry_t *entry, *loser;

	// One scan through the directory is enough.
	for(i = 0, entry = zipFiles; i < zipNumFiles - 1; i++, entry++)
	{
		// Is this entry the same as the next one?
		if(!stricmp(entry[0].name, entry[1].name))
		{
			// One of these should be removed. The newer one survives.
			if(entry[0].package->order > entry[1].package->order)
				loser = entry + 1;
			else
				loser = entry;

			// Overwrite the loser with the last entry.
			free(loser->name);
			memcpy(loser, &zipFiles[zipNumFiles - 1], sizeof(zipentry_t));
			memset(&zipFiles[zipNumFiles - 1], 0, sizeof(zipentry_t));

			// One less entry.
			zipNumFiles--;
			modified = true;
		}
	}

	if(modified)
	{
		// Sort the entries again.
		Zip_SortFiles();
	}
}

/*
 * Iterates through the zipentry list. If the finder func returns true,
 * the iteration stops and the the 1-based index of the zipentry is 
 * returned. Parm is passed to the finder func. Returns zero if nothing
 * is found.
 */
zipindex_t Zip_Iterate(int (*iterator)(const char*, void*), void *parm)
{
	int i;
	
	for(i = 0; i < zipNumFiles; i++)
		if(iterator(zipFiles[i].name, parm))
			return i + 1;

	// Nothing was accepted.
	return 0;
}

/*
 * Find a specific path in the zipentry list. Relative paths are converted
 * to absolute ones. A binary search is used (the entries have been sorted).
 * Good performance: O(log n). Returns zero if nothing is found.
 */
zipindex_t Zip_Find(const char *fileName)
{
	zipindex_t begin, end, mid;
	int relation;
	char fullPath[256];

	// Convert to an absolute path.
	strcpy(fullPath, fileName);
	Dir_MakeAbsolute(fullPath);

	// Init the search.
	begin = 0;
	end = zipNumFiles - 1;

	while(begin <= end)
	{
		mid = (begin + end) / 2;
		
		// How does this compare?
		relation = stricmp(fullPath, zipFiles[mid].name);
		if(!relation)
		{
			// Got it! We return a 1-based index.
			return mid + 1;
		}
		if(relation < 0)
		{
			// What we are searching must be in the first half.
			end = mid - 1;
		}
		else
		{
			// Then it must be in the second half.
			begin = mid + 1;
		}
	}

	// It wasn't found.
	return 0;
}

/*
 * Returns the size of a zipentry.
 */
uint Zip_GetSize(zipindex_t index)
{
	index--;
	if(index < 0 || index >= zipNumFiles) return 0; // Doesn't exist.
	return zipFiles[index].size;
}

/*
 * Reads a zipentry into the buffer. The buffer must be large enough.
 * Zip_GetSize() returns the size. Returns the number of bytes read.
 */
uint Zip_Read(zipindex_t index, void *buffer)
{	
	package_t *pack;
	zipentry_t *entry;

	index--;
	if(index < 0 || index >= zipNumFiles) return 0; // Doesn't exist.

	entry = zipFiles + index;
	pack = entry->package;

	VERBOSE2( Con_Printf("Zip_Read: %s: '%s' (%i bytes)\n",
		M_Pretty(pack->name), M_Pretty(entry->name), entry->size) );

	F_Seek(pack->file, entry->offset, SEEK_SET);
	F_Read(buffer, entry->size, pack->file);
	return entry->size;
}
