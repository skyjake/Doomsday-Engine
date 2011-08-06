/**\file dd_zip.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Zip (aka, Pk3) Package files.
 *
 * Uses zlib for decompression of "Deflated" files.
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>
#include <zlib.h>

#include "de_base.h"
#include "de_platform.h"
#include "de_defs.h"
#include "de_console.h"
#include "de_misc.h"

#include "sys_direc.h"
#include "sys_reslocator.h"

// MACROS ------------------------------------------------------------------

#define SIG_LOCAL_FILE_HEADER   0x04034b50
#define SIG_CENTRAL_FILE_HEADER 0x02014b50
#define SIG_END_OF_CENTRAL_DIR  0x06054b50

// Maximum tolerated size of the comment.
#define MAXIMUM_COMMENT_SIZE    2048

// This is the length of the central directory end record (without the
// comment, but with the signature).
#define CENTRAL_END_SIZE        22

// File header flags.
#define ZFH_ENCRYPTED           0x1
#define ZFH_COMPRESSION_OPTS    0x6
#define ZFH_DESCRIPTOR          0x8
#define ZFH_COMPRESS_PATCHED    0x20    // Not supported.

// Compression methods.
enum {
    ZFC_NO_COMPRESSION = 0,     // Supported format.
    ZFC_SHRUNK,
    ZFC_REDUCED_1,
    ZFC_REDUCED_2,
    ZFC_REDUCED_3,
    ZFC_REDUCED_4,
    ZFC_IMPLODED,
    ZFC_DEFLATED = 8,           // The only supported compression (via zlib).
    ZFC_DEFLATED_64,
    ZFC_PKWARE_DCL_IMPLODED
};

// TYPES -------------------------------------------------------------------

/**
 * @defgroup packageFlags Package flags.
 */
/*@{*/
#define PF_RUNTIME              0x1 // Loaded at runtime (for reset).
/*@}*/

typedef struct package_s {
    int flags; /// @see packageFlags

    /// File stream handle if open else @c NULL.
    DFILE* file;

    /// Load order depth index.
    uint order;

    /// Next package in the llist.
    struct package_s* next;

    /// Absolute path.
    ddstring_t name;
} package_t;

typedef struct zipentry_s {
    /// Absolute path.
    ddstring_t name;

    /// Owner package for this entry.
    package_t* package;

    /// Offset from the beginning of the package.
    uint offset;

    /// Size of the entry in zip (post compression).
    size_t size;

    /// Size of the original entry if compressed, else @c 0.
    size_t deflatedSize;

    /// Unix timestamp.
    uint lastModified;
} zipentry_t;

#pragma pack(1)
typedef struct localfileheader_s {
    uint32_t      signature;
    uint16_t      requiredVersion;
    uint16_t      flags;
    uint16_t      compression;
    uint16_t      lastModTime;
    uint16_t      lastModDate;
    uint32_t      crc32;
    uint32_t      compressedSize;
    uint32_t      size;
    uint16_t      fileNameSize;
    uint16_t      extraFieldSize;
} localfileheader_t;

typedef struct descriptor_s {
    uint32_t      crc32;
    uint32_t      compressedSize;
    uint32_t      size;
} descriptor_t;

typedef struct centralfileheader_s {
    uint32_t      signature;
    uint16_t      version;
    uint16_t      requiredVersion;
    uint16_t      flags;
    uint16_t      compression;
    uint16_t      lastModTime;
    uint16_t      lastModDate;
    uint32_t      crc32;
    uint32_t      compressedSize;
    uint32_t      size;
    uint16_t      fileNameSize;
    uint16_t      extraFieldSize;
    uint16_t      commentSize;
    uint16_t      diskStart;
    uint16_t      internalAttrib;
    uint32_t      externalAttrib;
    uint32_t      relOffset;

    /*
     * file name (variable size)
     * extra field (variable size)
     * file comment (variable size)
     */
} centralfileheader_t;

typedef struct centralend_s {
    uint16_t      disk;
    uint16_t      centralStartDisk;
    uint16_t      diskEntryCount;
    uint16_t      totalEntryCount;
    uint32_t      size;
    uint32_t      offset;
    uint16_t      commentSize;
} centralend_t;
#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static package_t* zipRoot;
static zipentry_t* zipFiles;
static uint numZipFiles, maxZipFiles;

static boolean loadingForStartup = true;

// CODE --------------------------------------------------------------------

static __inline boolean isValidIndex(zipindex_t index)
{
    return (index != 0 && index - 1 < numZipFiles);
}

static zipentry_t* findZipEntryForIndex(zipindex_t index)
{
    if(!isValidIndex(index))
        return 0; // Doesn't exist.
    return &zipFiles[index - 1];
}

/**
 * Allocate a zipentry array element and return a pointer to it.
 * Duplicate entries are removed later.
 */
static zipentry_t* newZipEntry(const ddstring_t* name)
{
    uint oldCount = numZipFiles;
    boolean changed = false;

    numZipFiles++;
    while(numZipFiles > maxZipFiles)
    {
        // Double the size of the array.
        maxZipFiles *= 2;
        if(!maxZipFiles)
            maxZipFiles = 1;
        zipFiles = realloc(zipFiles, sizeof(zipentry_t) * maxZipFiles);
        changed = true;
    }

    // Clear the new memory.
    if(changed)
    {
        memset(&zipFiles[oldCount], 0, sizeof(zipentry_t) * (maxZipFiles - oldCount));
    }

    // Take a copy of the name. This is freed in Zip_Shutdown().
    Str_Copy(&zipFiles[oldCount].name, name);

    // Return a pointer to the first new zipentry.
    return &zipFiles[oldCount];
}

/**
 * Sorts all the zip entries alphabetically. All the paths are absolute.
 */
int C_DECL Zip_EntrySorter(const void* a, const void* b)
{
    return Str_CompareIgnoreCase(&((const zipentry_t*)a)->name, Str_Text(&((const zipentry_t*) b)->name));
}

/**
 * Sorts all the zip entries alphabetically.
 */
static void sortZipEntries(void)
{
    // Sort all the zipentries by name. (Note: When lots of files loaded,
    // most of list is already sorted. Quicksort becomes slow...)
    qsort(zipFiles, numZipFiles, sizeof(zipentry_t), Zip_EntrySorter);
}


static package_t* findPackageByName(const char* fileName)
{
    package_t* pack = NULL;
    if(NULL != fileName && fileName[0] && numZipFiles > 0)
    {
        ddstring_t buf;

        // Transform the given path into one we can process.
        Str_Init(&buf); Str_Set(&buf, fileName);
        F_FixSlashes(&buf, &buf);

        // Perform the search.
        for(pack = zipRoot; pack; pack = pack->next)
        {
            if(!Str_CompareIgnoreCase(&pack->name, Str_Text(&buf)))
                break;
        }
        Str_Free(&buf);
    }
    return pack;
}

static void linkPackage(package_t* pack)
{
    assert(NULL != pack);
    pack->next = zipRoot;
}

static void unlinkPackage(package_t* pack)
{
    assert(NULL != pack);
    if(NULL == zipRoot) return;

    if(pack == zipRoot)
    {
        zipRoot = pack->next;
        return;
    }

    if(NULL != zipRoot->next)
    {
        package_t* other = zipRoot;
        do
        {
            if(other->next == pack)
            {
                other->next = pack->next;
                break;
            }
        } while(NULL != (other = other->next));
    }
}

static int removeZipFilesByPackage(package_t* pack)
{
    int i, oldNumZipFiles = numZipFiles;

    for(i = 0; i < (int)numZipFiles; )
    {
        zipentry_t* file = &zipFiles[i];
        if(file->package != pack)
        {
            ++i;
            continue;
        }

        Str_Free(&file->name);
        if(i != numZipFiles - 1)
            memmove(zipFiles + i, zipFiles + i + 1, sizeof(zipentry_t) * (maxZipFiles - i - 1));
        --numZipFiles;
    }

    if(numZipFiles == 0)
    {
        free(zipFiles), zipFiles = NULL;
        maxZipFiles = 0;
    }
    else
    {
        memset(zipFiles + numZipFiles, 0, sizeof(zipentry_t) * (maxZipFiles - numZipFiles));
    }

    return oldNumZipFiles - numZipFiles;
}

/**
 * Adds a new package to the list of packages.
 */
static package_t* newPackage(void)
{
    // When duplicates are removed, newer packages are favored.
    static uint packageCounter = 0;
    package_t* pack = calloc(1, sizeof(*pack));

    Str_Init(&pack->name);
    pack->order = packageCounter++;
    if(!loadingForStartup)
        pack->flags = PF_RUNTIME;
    linkPackage(pack);

    return zipRoot = pack;
}

static void destroyPackage(package_t* pack)
{
    assert(pack);
    if(pack->file)
        F_Close(pack->file);
    F_ReleaseFileId(Str_Text(&pack->name));
    Str_Free(&pack->name);
    free(pack);
}

static boolean closePackage(package_t* pack)
{
    if(NULL != pack)
    {
        unlinkPackage(pack);
        removeZipFilesByPackage(pack);
        destroyPackage(pack);
        return true;
    }
    return false; // No such file loaded.
}

/**
 * If two or more packages have the same name, the file from the last
 * loaded package is the one to use. Others will be removed from the
 * directory. The entries must be sorted before this can be done.
 */
static void removeDuplicateZipEntries(void)
{
    boolean modified = false;

    if(numZipFiles == 0)
        return; // Obviously no duplicates.

    // One scan through the directory is enough.
    { uint i;
    for(i = 0; i < numZipFiles - 1; ++i)
    {
        zipentry_t* entry = &zipFiles[i];
        zipentry_t* loser;

        // Is this entry the same as the next one?
        if(Str_CompareIgnoreCase(&entry[0].name, Str_Text(&entry[1].name)))
            continue; // No.

        // One of these should be removed; the newer one survives.
        if(entry[0].package->order > entry[1].package->order)
            loser = entry + 1;
        else
            loser = entry;

        // Overwrite the loser with the last entry in the entire entry directory.
        Str_Free(&loser->name);
        memcpy(loser, &zipFiles[numZipFiles - 1], sizeof(zipentry_t));
        memset(&zipFiles[numZipFiles - 1], 0, sizeof(zipentry_t));
        // One less entry.
        numZipFiles--;

        modified = true;
    }}

    if(modified)
    {   // Sort the entries again.
        sortZipEntries();
    }
}

/**
 * Use zlib to inflate a compressed entry.
 * @return              @c true, if successful.
 */
static boolean inflateZipEntry(void* in, size_t inSize, void* out, size_t outSize)
{
    z_stream stream;
    int result;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = in;
    stream.avail_in = (uInt) inSize;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.next_out = out;
    stream.avail_out = (uInt) outSize;

    if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return false;

    // Do the inflation in one call.
    result = inflate(&stream, Z_FINISH);

    if(stream.total_out != outSize)
    {
        Con_Message("inflateZipEntry: Failure due to %s.\n", (result == Z_DATA_ERROR ? "corrupt data" : "zlib error"));
        return false;
    }

    // We're done.
    inflateEnd(&stream);
    return true;
}

static size_t readZipEntry(zipentry_t* entry, void* buffer)
{
    assert(entry && buffer);
    F_Seek(entry->package->file, entry->offset, SEEK_SET);
    if(entry->deflatedSize != 0)
    {
        // Read the compressed data into a buffer.
        void* compressedData = malloc(entry->deflatedSize);
        boolean result;
        F_Read(compressedData, entry->deflatedSize, entry->package->file);

        // Run zlib's inflate to decompress.
        result = inflateZipEntry(compressedData, entry->deflatedSize, buffer, entry->size);

        free(compressedData);

        if(!result)
            return 0; // Inflate failed.
    }
    else
    {
        // Read the uncompressed data directly to the buffer provided by the caller.
        F_Read(buffer, entry->size, entry->package->file);
    }
    return entry->size;
}

/**
 * The path inside the pack might be mapped to another virtual location.
 *
 * Data files (pk3, zip, lmp, wad, deh) in the root are mapped to Data/Game/Auto.
 * Definition files (ded) in the root are mapped to Defs/Game/Auto.
 * Paths that begin with a '@' are mapped to Defs/Game/Auto.
 * Paths that begin with a '#' are mapped to Data/Game/Auto.
 * Key-named directories at the root are mapped to another location.
 */
static void applyMappings(ddstring_t* dest, const ddstring_t* src)
{
    // Manually mapped to Defs?
    if(Str_At(src, 0) == '@')
    {
        ddstring_t* out = (dest == src? Str_New() : dest);

        Str_Appendf(out, "%sauto"DIR_SEP_STR, Str_Text(GameInfo_DefsPath(DD_GameInfo())));
        Str_PartAppend(out, Str_Text(src), 1, Str_Length(src)-1);

        if(dest == src)
        {
            Str_Copy(dest, out);
            Str_Delete(out);
        }
        return;
    }

    // Manually mapped to Data?
    if(Str_At(src, 0) == '#')
    {
        ddstring_t* out = (dest == src? Str_New() : dest);

        Str_Appendf(out, "%sauto"DIR_SEP_STR, Str_Text(GameInfo_DataPath(DD_GameInfo())));
        Str_PartAppend(out, Str_Text(src), 1, Str_Length(src)-1);

        if(dest == src)
        {
            Str_Copy(dest, out);
            Str_Delete(out);
        }
        return;
    }

    if(strchr(Str_Text(src), DIR_SEP_CHAR) == NULL)
    {   // No directory separators; i.e., a root file.
        resourcetype_t type = F_GuessResourceTypeByName(Str_Text(src));
        resourceclass_t rclass;
        ddstring_t mapped;

        /// \kludge Treat DeHackEd patches as packages so they are mapped to Data.
        rclass = (type == RT_DEH? RC_PACKAGE : F_DefaultResourceClassForType(type));
        /// < kludge end

        Str_Init(&mapped);
        switch(rclass)
        {
        case RC_UNKNOWN: // Not mapped.
            break;
        case RC_DEFINITION: // Mapped to the Defs directory.
            Str_Appendf(&mapped, "%sauto"DIR_SEP_STR, Str_Text(GameInfo_DefsPath(DD_GameInfo())));
            break;
        default: // Some other type of known resource. Mapped to the Data directory.
            Str_Appendf(&mapped, "%sauto"DIR_SEP_STR, Str_Text(GameInfo_DataPath(DD_GameInfo())));
            break;
        }
        Str_Append(&mapped, Str_Text(src));
        Str_Copy(dest, &mapped);

        Str_Free(&mapped);
        return;
    }

    // There is at least one level of directory structure.

    if(dest != src)
        Str_Copy(dest, src);

    // Key-named directories in the root might be mapped to another location.
    F_ApplyPathMapping(dest);
}

/**
 * Finds the central directory end record in the end of the file.
 * Note: This gets awfully slow if the comment is long.
 *
 * @return              @c true, if successful.
 */
static boolean locateCentralDirectory(DFILE* file)
{
    int pos = CENTRAL_END_SIZE; // Offset from the end.
    uint signature;

    // Start from the earliest location where the signature might be.
    while(pos < MAXIMUM_COMMENT_SIZE)
    {
        F_Seek(file, -pos, SEEK_END);

        // Is this the signature?
        F_Read(&signature, 4, file);
        if(ULONG(signature) == SIG_END_OF_CENTRAL_DIR)
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

void Zip_Init(void)
{
    VERBOSE( Con_Message("Initializing Package subsystem...\n") );

    // This'll force the loader NOT the flag new records Runtime. (?)
    loadingForStartup = true;

    zipRoot = NULL;
    zipFiles = 0;
    numZipFiles = 0;
    maxZipFiles = 0;
}

void Zip_Shutdown(void)
{
    // Close package files and free the nodes.
    { package_t* pack = zipRoot;
    while(pack)
    {
        package_t* next = pack->next;
        destroyPackage(pack);
        pack = next;
    }}

    // Free the file directory.
    { uint i;
    for(i = 0; i < numZipFiles; ++i)
        Str_Free(&zipFiles[i].name);
    }
    free(zipFiles);

    zipRoot = NULL;
    zipFiles = NULL;
    numZipFiles = 0;
    maxZipFiles = 0;
}

void Zip_EndStartup(void)
{
    loadingForStartup = false;
}

int Zip_Reset(void)
{
    int unloadedResources = 0;
    package_t* pack = zipRoot;
    while(pack)
    {
        package_t* next = pack->next;
        if(pack->flags & PF_RUNTIME)
        {
            closePackage(pack);
            ++unloadedResources;
        }
        pack = next;
    }
    return unloadedResources;
}

boolean Zip_Open(const char* fileName, DFILE* prevOpened)
{
    assert(fileName);
    {
    centralend_t summary;
    zipentry_t* entry;
    package_t* pack;
    void* directory;
    char* pos;
    uint index;
    DFILE* file;

    if(prevOpened == NULL)
    {   // Try to open the file.
        if((file = F_Open(fileName, "rb")) == NULL)
        {
            Con_Message("Warning:Zip_Open: \"%s\" not found!\n", fileName);
            return false;
        }
    }
    else
    {   // Use the previously opened file.
        file = prevOpened;
    }

    VERBOSE( Con_Message("Zip_Open: \"%s\"\n", F_PrettyPath(fileName)) );

    // Scan the end of the file for the central directory end record.
    if(!locateCentralDirectory(file))
    {
        Con_Error("Zip_Open: Central directory in \"%s\" not found!\n", fileName);
    }

    // Read the central directory end record.
    F_Read(&summary, sizeof(summary), file);

    // Does the summary say something we don't like?
    if(USHORT(summary.diskEntryCount) != USHORT(summary.totalEntryCount))
    {
        Con_Error("Zip_Open: Multipart Zip file \"%s\" not supported.\n", fileName);
    }

    // Read the entire central directory into memory.
    directory = malloc(ULONG(summary.size));
    F_Seek(file, ULONG(summary.offset), SEEK_SET);
    F_Read(directory, ULONG(summary.size), file);

    pack = newPackage();
    Str_Set(&pack->name, fileName);
    pack->file = file;

    // Read all the entries.
    pos = directory;
    for(index = 0; index < USHORT(summary.totalEntryCount);
        ++index, pos += sizeof(centralfileheader_t))
    {
        centralfileheader_t* header = (void*) pos;
        char* nameStart = pos + sizeof(centralfileheader_t);
        ddstring_t entryName;

        // Advance the cursor past the variable sized fields.
        pos += USHORT(header->fileNameSize) + USHORT(header->extraFieldSize) + USHORT(header->commentSize);

        // Copy characters up to fileNameSize.
        Str_Init(&entryName);
        Str_PartAppend(&entryName, nameStart, 0, USHORT(header->fileNameSize));

        // Directories are skipped.
        if(ULONG(header->size) == 0 && Str_RAt(&entryName, 0) == '/')
        {
            Str_Free(&entryName);
            continue;
        }

        // Do we support the format of this file?
        if(USHORT(header->compression) != ZFC_NO_COMPRESSION &&
           USHORT(header->compression) != ZFC_DEFLATED)
        {
            Con_Error("Zip_Open: %s:'%s' uses an unsupported compression algorithm.\n",
                      fileName, Str_Text(&entryName));
        }

        if(USHORT(header->flags) & ZFH_ENCRYPTED)
        {
            Con_Error("Zip_Open: %s:'%s' is encrypted.\n  Encryption is not supported.\n",
                      fileName, Str_Text(&entryName));
        }

        // Convert all slashes to the host OS's directory separator,
        // for compatibility with the sys_filein routines.
        F_FixSlashes(&entryName, &entryName);

        // In some cases the path inside the pack is mapped to another virtual location.
        applyMappings(&entryName, &entryName);

        // Make it absolute.
        F_PrependBasePath(&entryName, &entryName);

        // We can add this file to the zipentry list.
        entry = newZipEntry(&entryName);
        entry->package = pack;
        entry->size = ULONG(header->size);
        if(USHORT(header->compression) == ZFC_DEFLATED)
        {   // Compressed using the deflate algorithm.
            entry->deflatedSize = ULONG(header->compressedSize);
        }
        else
        {   // No compression.
            entry->deflatedSize = 0;
        }

        // The modification date is inherited from the real file (note
        // recursion).
        entry->lastModified = file->lastModified;

        // Read the local file header, which contains the extra field size (Info-ZIP!).
        { localfileheader_t localHeader;
        F_Seek(file, ULONG(header->relOffset), SEEK_SET);
        F_Read(&localHeader, sizeof(localHeader), file);

        entry->offset = ULONG(header->relOffset) + sizeof(localfileheader_t) + USHORT(header->fileNameSize) + USHORT(localHeader.extraFieldSize);
        }

        Str_Free(&entryName);
    }

    // The central directory is no longer needed.
    free(directory);

    sortZipEntries();
    removeDuplicateZipEntries();

    // File successfully opened!
    return true;
    }
}

boolean Zip_Close(const char* fileName)
{
    package_t* pack = findPackageByName(fileName);
    if(NULL != pack)
        return closePackage(pack);
    return false; // Not loaded.
}

int Zip_Iterate2(int (*callback) (const ddstring_t*, void*), void* paramaters)
{
    assert(callback);
    {
    uint i;
    int result = 0;
    for(i = 0; i < numZipFiles; ++i)
    {
        zipentry_t* entry = &zipFiles[i];
        if((result = callback(&entry->name, paramaters)) != 0)
            break;
    }
    return result;
    }
}

int Zip_Iterate(int (*callback) (const ddstring_t*, void*))
{
    return Zip_Iterate2(callback, 0);
}

zipindex_t Zip_Find(const char* searchPath)
{
    assert(searchPath && searchPath[0]);
    {
    zipindex_t begin, end, mid;
    ddstring_t fullPath;
    int relation;

    if(numZipFiles == 0)
        return 0; // None registered yet.

    // Convert to an absolute path.
    Str_Init(&fullPath); Str_Set(&fullPath, searchPath);
    F_PrependBasePath(&fullPath, &fullPath);

    // Init the search.
    begin = 0;
    end = numZipFiles - 1;

    while(begin <= end)
    {
        mid = (begin + end) / 2;

        // How does this compare?
        relation = Str_CompareIgnoreCase(&fullPath, Str_Text(&zipFiles[mid].name));
        if(!relation)
        {   // Got it! We return a 1-based index.
            Str_Free(&fullPath);
            return mid + 1;
        }
        if(relation < 0)
        {   // What we are searching must be in the first half.
            if(mid > 0)
                end = mid - 1;
            else
            {
                Str_Free(&fullPath);
                return 0; // Not found.
            }
        }
        else
        {   // Then it must be in the second half.
            begin = mid + 1;
        }
    }

    // It wasn't found.
    Str_Free(&fullPath);
    return 0;
    }
}

size_t Zip_GetSize(zipindex_t index)
{
    zipentry_t* entry;
    if((entry = findZipEntryForIndex(index)))
        return entry->size;
    return 0; // Doesn't exist.
}

uint Zip_GetLastModified(zipindex_t index)
{
    zipentry_t* entry;
    if((entry = findZipEntryForIndex(index)))
        return entry->lastModified;
    return 0; // Doesn't exist.
}

const char* Zip_SourceFile(zipindex_t index)
{
    zipentry_t* entry = findZipEntryForIndex(index);
    if(entry)
        return Str_Text(&entry->package->name);
    return ""; // Doesn't exist.
}

size_t Zip_Read(zipindex_t index, void* buffer)
{
    zipentry_t* entry;
    if((entry = findZipEntryForIndex(index)))
    {
        VERBOSE2(
        Con_Message("Zip_Read: \"%s:%s\" (%lu bytes%s)\n", F_PrettyPath(Str_Text(&entry->package->name)),
                    F_PrettyPath(Str_Text(&entry->name)), (unsigned long) entry->size,
                    (entry->deflatedSize? ", deflated" : "")) );
        return readZipEntry(entry, buffer);
    }
    return 0; // Doesn't exist.
}
