
//**************************************************************************
//**
//** DD_ZIP.C
//**
//** Loading .pk3/.zip files (no compression!). Finding files inside 
//** packages. A stream interface (like fopen/fread) for files inside 
//** packages, used by the F_ routines.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>

#include "de_base.h"
#include "de_console.h"

// MACROS ------------------------------------------------------------------

#define SIG_LOCAL_FILE_HEADER	0x04034b50
#define SIG_CENTRAL_FILE_HEADER	0x02014b50

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
	FILE *file;
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
#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static package_t *gZipRoot;
static zipentry_t *gFiles;
static int gNumFiles, gAllocatedFiles;

// CODE --------------------------------------------------------------------

/*
 * Initializes the zip file database.
 */
void Zip_Init(void)
{
	VERBOSE( Con_Message("Zip_Init: Initializing package system...\n") );

	gZipRoot = NULL;
	gFiles = 0;
	gNumFiles = 0;
	gAllocatedFiles = 0;
}

/*
 * Shuts down the zip file database and frees all resources.
 */
void Zip_Shutdown(void)
{
	package_t *pack, *next;
	int i;

	// Close package files and free the nodes.
	for(pack = gZipRoot; pack; pack = next)
	{
		next = pack->next;
		if(pack->file) fclose(pack->file);
		free(pack);
	}

	// Free the file directory.
	for(i = 0; i < gNumFiles; i++) free(gFiles[i].name);
	free(gFiles);

	gZipRoot = NULL;
	gFiles = NULL;
	gNumFiles = 0;
	gAllocatedFiles = 0;
}

/*
 * Allocate new zipentry array elements. Returns a pointer to the first
 * new zipentry_t.
 */
zipentry_t* Zip_NewFiles(unsigned int count)
{
	int oldCount = gNumFiles;
	boolean changed = false;
	
	gNumFiles += count;
	while(gNumFiles > gAllocatedFiles)
	{
		// Double the size of the array.
		gAllocatedFiles *= 2;
		if(gAllocatedFiles <= 0) gAllocatedFiles = 1;
		
		// Realloc the zipentry array.
		gFiles = realloc(gFiles, sizeof(zipentry_t) * gAllocatedFiles);
		changed = true;
	}

	// Clear the new memory.
	if(changed)
	{
		memset(gFiles + oldCount, 0, sizeof(zipentry_t) 
			* (gAllocatedFiles - oldCount));
	}
	
	// Return a pointer to the first new zipentry.
	return gFiles + oldCount;
}

/*
 * Adds a new package to the list of packages.
 */
package_t* Zip_NewPackage(void)
{
	package_t *newPack = calloc(sizeof(package_t), 1);
	newPack->next = gZipRoot;
	return gZipRoot = newPack;
}

/*
 * Opens the file zip, reads the directory and stores the info for later
 * access.
 */
boolean Zip_Open(const char *fileName)
{
	FILE *file;
	package_t *pack;
	uint signature, i;
	localfileheader_t header;
	zipentry_t *entry;
	char *entryName;
	
	// Try to open the file.
	if((file = fopen(fileName, "rb")) == NULL)
	{
		Con_Message("Zip_Open: %s not found.\n", fileName);
		return false;
	}
	
	VERBOSE( Con_Message("Zip_Open: %s\n", fileName) );

	pack = Zip_NewPackage();
	strcpy(pack->name, fileName);

	// Read the directory and add the files into the zipentry array.
	// We'll keep reading until there are no more files.
	while(!feof(file))
	{
		// First comes the signature.
		fread(&signature, 4, 1, file);
		
		if(signature != SIG_LOCAL_FILE_HEADER
			&& signature != SIG_CENTRAL_FILE_HEADER)
		{
			Con_Message("Zip_Open: %s is not a Zip file?\n", fileName);
			fclose(file);
			return false;
		}

		if(signature == SIG_CENTRAL_FILE_HEADER)
		{
			// There are no more files.
			break;
		}

		// This must be a local file data segment. Read the header.
		fread(&header, sizeof(header), 1, file);

		// Read the file name. This memory is freed in Zip_Shutdown().
		entryName = calloc(header.fileNameSize + 1, 1);
		fread(entryName, header.fileNameSize, 1, file);
		
		// Do we support the format of this file?
		if(header.compression != ZFC_NO_COMPRESSION
			|| header.compressedSize != header.size)
		{
			Con_Error("Zip_Open: %s: '%s' is compressed.\n  Compression is "
				"not supported.\n", fileName, entryName);
		}
		if(header.flags & ZFH_ENCRYPTED)
		{
			Con_Error("Zip_Open: %s: '%s' is encrypted.\n  Encryption is "
				"not supported.\n", fileName, entryName);
		}

		// Skip the extra data.
		fseek(file, header.extraFieldSize, SEEK_CUR);

		if(header.size)
		{
			// We can add this file to the zipentry list.
			entry = Zip_NewFiles(1);
			entry->name = entryName;
			entry->package = pack;
			entry->size = header.size;

			// This is where the data begins.
			entry->offset = ftell(file);

			// Skip the data.
			fseek(file, header.size, SEEK_CUR);
			
			// Convert all slashes to backslashes, for compatibility with 
			// the sys_filein routines.
			for(i = 0; entry->name[i]; i++)
				if(entry->name[i] == '/')
					entry->name[i] = '\\';
		}
	}

	pack->file = file;

	// File successfully opened!
	return true;
}

/*
 * Iterates through the zipentry list. If the finder func returns true,
 * the iteration stops and the the 1-based index of the zipentry is 
 * returned. Parm is passed to the finder func. Returns zero if nothing
 * is found.
 */
zipindex_t Zip_Find(int (*finder)(const char*, void*), void *parm)
{
	int i;
	
	for(i = 0; i < gNumFiles; i++)
		if(finder(gFiles[i].name, parm))
			return i + 1;

	// Nothing was accepted.
	return 0;
}

/*
 * Returns the size of a zipentry.
 */
uint Zip_GetSize(zipindex_t index)
{
	index--;
	if(index < 0 || index >= gNumFiles) return 0; // Doesn't exist.
	return gFiles[index].size;
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
	if(index < 0 || index >= gNumFiles) return 0; // Doesn't exist.

	entry = gFiles + index;
	pack = entry->package;

	VERBOSE2( Con_Printf("Zip_Read: %s: '%s' (%i bytes)\n",
		pack->name, entry->name, entry->size) );

	fseek(pack->file, entry->offset, SEEK_SET);
	fread(buffer, entry->size, 1, pack->file);
	return entry->size;
}