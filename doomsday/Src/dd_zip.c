
//**************************************************************************
//**
//** DD_ZIP.C
//**
//** Loading .pk3/.zip files (no compression!). 
//** Finding files inside packages. 
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

#include "sys_direc.h"

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
	DFILE *file;
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
		if(pack->file) F_Close(pack->file);
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
 * Sorts all the zip entries alphabetically. All the paths are absolute.
 */
int C_DECL Zip_EntrySorter(const void *a, const void *b)
{
	// Compare the names.
	return stricmp( ((const zipentry_t*)a)->name, 
		((const zipentry_t*)b)->name );
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
 * access. If prevOpened is not NULL, all data will be read from there.
 */
boolean Zip_Open(const char *fileName, DFILE *prevOpened)
{
	DFILE *file;
	package_t *pack;
	uint signature, i;
	localfileheader_t header;
	zipentry_t *entry;
	char *entryName;
	char fullPath[256];
	
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
	
	VERBOSE( Con_Message("Zip_Open: %s\n", fileName) );

	pack = Zip_NewPackage();
	strcpy(pack->name, fileName);

	// Read the directory and add the files into the zipentry array.
	// We'll keep reading until there are no more files.
	while(!deof(file))
	{
		// First comes the signature.
		F_Read(&signature, 4, file);
		
		if(signature != SIG_LOCAL_FILE_HEADER
			&& signature != SIG_CENTRAL_FILE_HEADER)
		{
			Con_Message("Zip_Open: %s is not a Zip file?\n", fileName);
			F_Close(file);
			return false;
		}

		if(signature == SIG_CENTRAL_FILE_HEADER)
		{
			// There are no more files.
			break;
		}

		// This must be a local file data segment. Read the header.
		F_Read(&header, sizeof(header), file);

		// Read the file name. This memory is freed below.
		entryName = calloc(header.fileNameSize + 1, 1);
		F_Read(entryName, header.fileNameSize, file);
		
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
		F_Seek(file, header.extraFieldSize, SEEK_CUR);

		if(header.size)
		{
			// Convert all slashes to backslashes, for compatibility with 
			// the sys_filein routines.
			for(i = 0; entryName[i]; i++)
				if(entryName[i] == '/') entryName[i] = '\\';

			// Make it absolute.
			M_PrependBasePath(entryName, fullPath);

			// We can add this file to the zipentry list.
			entry = Zip_NewFiles(1);
			
			// This memory is freed in Zip_Shutdown().
			entry->name = malloc(strlen(fullPath) + 1);
			strcpy(entry->name, fullPath);

			entry->package = pack;
			entry->size = header.size;

			// This is where the data begins.
			entry->offset = F_Tell(file);

			// Skip the data.
			F_Seek(file, header.size, SEEK_CUR);
		}

		free(entryName);
	}

	pack->file = file;

	// Sort all the zipentries by name. (Note: When lots of files loaded,
	// most of list is already sorted. Quicksort becomes slow...)
	qsort(gFiles, gNumFiles, sizeof(zipentry_t), Zip_EntrySorter);

	// File successfully opened!
	return true;
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
	
	for(i = 0; i < gNumFiles; i++)
		if(iterator(gFiles[i].name, parm))
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
	end = gNumFiles - 1;

	while(begin <= end)
	{
		mid = (begin + end) / 2;
		
		// How does this compare?
		relation = strcmp(fullPath, gFiles[mid].name);
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

	F_Seek(pack->file, entry->offset, SEEK_SET);
	F_Read(buffer, entry->size, pack->file);
	return entry->size;
}