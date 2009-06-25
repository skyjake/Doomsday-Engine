/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * dd_zip.h: Zip/Pk3 Files
 *
 * Loading .pk3/.zip files.
 * Finding files inside packages.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_defs.h"
#include "de_platform.h"
#include "de_console.h"
#include "de_misc.h"
#include "sys_direc.h"
#include "r_extres.h"

#include <zlib.h>

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

typedef struct package_s {
    struct package_s *next;
    char       *name;
    DFILE      *file;
    uint        order;
} package_t;

typedef struct zipentry_s {
    char       *name;           // Absolute path.
    package_t  *package;
    uint        offset;        // Offset from the beginning of the package.
    size_t      size;
    size_t      deflatedSize;  /* Size of the original deflated entry.
                                   If the entry is not compressed, this
                                   is set to zero. */
    uint        lastModified;  // Unix timestamp.
} zipentry_t;

#pragma pack(1)
typedef struct localfileheader_s {
    uint        signature;
    ushort      requiredVersion;
    ushort      flags;
    ushort      compression;
    ushort      lastModTime;
    ushort      lastModDate;
    uint        crc32;
    uint        compressedSize;
    uint        size;
    ushort      fileNameSize;
    ushort      extraFieldSize;
} localfileheader_t;

typedef struct descriptor_s {
    uint        crc32;
    uint        compressedSize;
    uint        size;
} descriptor_t;

typedef struct centralfileheader_s {
    uint        signature;
    ushort      version;
    ushort      requiredVersion;
    ushort      flags;
    ushort      compression;
    ushort      lastModTime;
    ushort      lastModDate;
    uint        crc32;
    uint        compressedSize;
    uint        size;
    ushort      fileNameSize;
    ushort      extraFieldSize;
    ushort      commentSize;
    ushort      diskStart;
    ushort      internalAttrib;
    uint        externalAttrib;
    uint        relOffset;

    /*
     * file name (variable size)
     * extra field (variable size)
     * file comment (variable size)
     */
} centralfileheader_t;

typedef struct centralend_s {
    ushort      disk;
    ushort      centralStartDisk;
    ushort      diskEntryCount;
    ushort      totalEntryCount;
    uint        size;
    uint        offset;
    ushort      commentSize;
} centralend_t;
#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void    removeDuplicateFiles(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static package_t *zipRoot;
static zipentry_t *zipFiles;
static uint zipNumFiles, zipAllocatedFiles;

// CODE --------------------------------------------------------------------

/**
 * Initializes the zip file database.
 */
void Zip_Init(void)
{
    VERBOSE(Con_Message("Zip_Init: Initializing package system...\n"));

    zipRoot = NULL;
    zipFiles = 0;
    zipNumFiles = 0;
    zipAllocatedFiles = 0;
}

/**
 * Shuts down the zip file database and frees all resources.
 */
void Zip_Shutdown(void)
{
    uint        i;
    package_t  *pack, *next;

    // Close package files and free the nodes.
    for(pack = zipRoot; pack; pack = next)
    {
        next = pack->next;
        if(pack->file)
            F_Close(pack->file);
        if(pack->name)
            M_Free(pack->name);
        M_Free(pack);
    }

    // Free the file directory.
    for(i = 0; i < zipNumFiles; ++i)
        M_Free(zipFiles[i].name);
    M_Free(zipFiles);

    zipRoot = NULL;
    zipFiles = NULL;
    zipNumFiles = 0;
    zipAllocatedFiles = 0;
}

/**
 * Allocate a zipentry array element and return a pointer to it.
 * Duplicate entries are removed later.
 */
static zipentry_t* newZipEntry(const char* name)
{
    uint                oldCount = zipNumFiles;
    boolean             changed = false;

    zipNumFiles++;
    while(zipNumFiles > zipAllocatedFiles)
    {
        // Double the size of the array.
        zipAllocatedFiles *= 2;
        if(!zipAllocatedFiles)
            zipAllocatedFiles = 1;

        // Realloc the zipentry array.
        zipFiles =
            M_Realloc(zipFiles, sizeof(zipentry_t) * zipAllocatedFiles);
        changed = true;
    }

    // Clear the new memory.
    if(changed)
    {
        memset(&zipFiles[oldCount], 0,
               sizeof(zipentry_t) * (zipAllocatedFiles - oldCount));
    }

    // Take a copy of the name. This is freed in Zip_Shutdown().
    zipFiles[oldCount].name = M_Malloc(sizeof(char) * (strlen(name) + 1));
    strcpy(zipFiles[oldCount].name, name);

    // Return a pointer to the first new zipentry.
    return &zipFiles[oldCount];
}

/**
 * Sorts all the zip entries alphabetically. All the paths are absolute.
 */
int C_DECL Zip_EntrySorter(const void *a, const void *b)
{
    // Compare the names.
    return stricmp(((const zipentry_t *) a)->name,
                   ((const zipentry_t *) b)->name);
}

/**
 * Sorts all the zip entries alphabetically.
 */
static void sortZipEntries(void)
{
    // Sort all the zipentries by name. (Note: When lots of files loaded,
    // most of list is already sorted. Quicksort becomes slow...)
    qsort(zipFiles, zipNumFiles, sizeof(zipentry_t), Zip_EntrySorter);
}

/**
 * Adds a new package to the list of packages.
 */
static package_t* newPackage(void)
{
    // When duplicates are removed, newer packages are favored.
    static uint         packageCounter = 0;
    package_t*          newPack = M_Calloc(sizeof(package_t));

    newPack->next = zipRoot;
    newPack->order = packageCounter++;
    return zipRoot = newPack;
}

/**
 * Finds the central directory end record in the end of the file.
 * Note: This gets awfully slow if the comment is long.
 *
 * @return              @c true, if successful.
 */
static boolean locateCentralDirectory(DFILE *file)
{
    int                 pos = CENTRAL_END_SIZE; // Offset from the end.
    uint                signature;

    // Start from the earliest location where the signature might be.
    while(pos < MAXIMUM_COMMENT_SIZE)
    {
        F_Seek(file, -pos, SEEK_END);

        // Is this is signature?
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

/**
 * Copies num characters (up to destSize) and adds a terminating NULL.
 */
static void copyStr(char* dest, const char* src, size_t num, size_t destSize)
{
    // Only copy as much as we can.
    if(num >= destSize)
        num = destSize - 1;

    memcpy(dest, src, num);
    dest[num] = 0;
}

/**
 * The path inside the pack is mapped to another virtual location.
 *
 * Data files (pk3, zip, lmp, wad, deh) in the root are mapped to Data/Game/Auto.
 * Definition files (ded) in the root are mapped to Defs/Game/Auto.
 * Paths that begin with a '@' are mapped to Defs/Game/Auto.
 * Paths that begin with a '#' are mapped to Data/Game/Auto.
 *
 * DJS 05/05/05
 * The folder in the root is mapped to another location
 *
 * Location is one of the following keynames.
 * Folder 'Models' is mapped to Data/Game/Models
 * Floder 'Flares' is mapped to Data/Game/Flares
 * Folder 'LightMaps' is mapped to Data/Game/LightMaps
 * Folder 'Music' is mapped to Data/Game/Music
 * Folder 'Textures' is mapped to Data/Game/Textures
 * Folder 'Flats' is mapped to Data/Game/Flats
 * Folder 'DetailTextures' is mapped to Data/Game/DetailTextures
 * Folder 'Patches' is mapped to Data/Game/Patches
 * Folder 'Sfx' is mapped to Data/Game/Sfx
 */
static void mapPath(char* path, size_t len)
{
    filename_t          mapped;

    if(strchr(path, DIR_SEP_CHAR) != NULL)
    {
        // There is at least one level of directory structure inside
        // the archive.

        // Check the beginning of the path.
        if(!strnicmp("Models" DIR_SEP_STR, path, 7) ||
           !strnicmp("Flares" DIR_SEP_STR, path, 7) ||
           !strnicmp("LightMaps" DIR_SEP_STR, path, 10) ||
           !strnicmp("Music" DIR_SEP_STR, path, 6) ||
           !strnicmp("Textures" DIR_SEP_STR, path, 9) ||
           !strnicmp("Flats" DIR_SEP_STR, path, 6) ||
           !strnicmp("DetailTextures" DIR_SEP_STR, path, 15) ||
           !strnicmp("Patches" DIR_SEP_STR, path, 8) ||
           !strnicmp("Sfx" DIR_SEP_STR, path, 4))
        {
            // Contents mapped to keyname folder.
            snprintf(mapped, FILENAME_T_MAXLEN, "%s%s", R_GetDataPath(),
                     path);
            strncpy(path, mapped, len);
            return;
        }
    }

    if(path[0] == '@') // Manually mapped to Defs.
    {
        Def_GetAutoPath(mapped, FILENAME_T_MAXLEN);
        strcat(mapped, path + 1);
        strcpy(path, mapped);
    }
    else if(path[0] == '#') // Manually mapped to Data.
    {
        sprintf(mapped, "%sauto" DIR_SEP_STR "%s", R_GetDataPath(),
                path + 1);
        strcpy(path, mapped);
    }
    else if(strchr(path, DIR_SEP_CHAR) == NULL)
    {   // The name contains no directory separators.
        // Check the extension.
        char       *ext = strrchr(path, '.');

        if(ext != NULL)
        {
            ++ext;

            if(!stricmp(ext, "pk3") ||
               !stricmp(ext, "zip") ||
               !stricmp(ext, "lmp") ||
               !stricmp(ext, "wad") ||
               !stricmp(ext, "deh"))
            {   // Data files are mapped to the Data directory.
                sprintf(mapped, "%sauto" DIR_SEP_STR, R_GetDataPath());
            }
            else if(!stricmp(ext, "ded"))
            {   // Definitions are mapped to the Defs directory.
                Def_GetAutoPath(mapped, FILENAME_T_MAXLEN);
            }
            else
            {
                strncpy(mapped, "", FILENAME_T_MAXLEN);
            }

            strncat(mapped, path, FILENAME_T_MAXLEN);
            strncpy(path, mapped, len);
        }
    }
}

/**
 * Opens the file zip, reads the directory and stores the info for later
 * access.
 *
 * @param prevOpened    If not @c NULL,, all data will be read
 *                      from there.
 */
boolean Zip_Open(const char* fileName, DFILE* prevOpened)
{
    centralend_t        summary;
    zipentry_t*         entry;
    package_t*          pack;
    void*               directory;
    char*               pos;
    filename_t          buf;
    uint                index;
    DFILE*              file;

    if(prevOpened == NULL)
    {   // Try to open the file.
        if((file = F_Open(fileName, "rb")) == NULL)
        {
            Con_Message("Zip_Open: %s not found.\n", fileName);
            return false;
        }
    }
    else
    {   // Use the previously opened file.
        file = prevOpened;
    }

    VERBOSE(Con_Message("Zip_Open: %s\n", M_PrettyPath(fileName)));

    // Scan the end of the file for the central directory end record.
    if(!locateCentralDirectory(file))
    {
        Con_Error("Zip_Open: %s: Central directory not found.\n",
                  M_PrettyPath(fileName));
    }

    // Read the central directory end record.
    F_Read(&summary, sizeof(summary), file);

    // Does the summary say something we don't like?
    if(USHORT(summary.diskEntryCount) != USHORT(summary.totalEntryCount))
    {
        Con_Error("Zip_Open: %s: Multipart Zip files are not supported.\n",
                  M_PrettyPath(fileName));
    }

    // Read the entire central directory into memory.
    directory = M_Malloc(ULONG(summary.size));
    F_Seek(file, ULONG(summary.offset), SEEK_SET);
    F_Read(directory, ULONG(summary.size), file);

    pack = newPackage();
    pack->name = M_Malloc(sizeof(char) * (strlen(fileName) + 1));
    strcpy(pack->name, fileName);
    pack->file = file;

    // Read all the entries.
    pos = directory;
    for(index = 0; index < USHORT(summary.totalEntryCount);
        ++index, pos += sizeof(centralfileheader_t))
    {
        localfileheader_t   localHeader;
        centralfileheader_t* header = (void *) pos;
        char*               nameStart = pos + sizeof(centralfileheader_t);

        // Advance the cursor past the variable sized fields.
        pos +=
            USHORT(header->fileNameSize) + USHORT(header->extraFieldSize) +
            USHORT(header->commentSize);

        copyStr(buf, nameStart, USHORT(header->fileNameSize),
                FILENAME_T_MAXLEN);

        // Directories are skipped.
        if(buf[USHORT(header->fileNameSize) - 1] == '/' &&
           ULONG(header->size) == 0)
            continue;

        // Do we support the format of this file?
        if(USHORT(header->compression) != ZFC_NO_COMPRESSION &&
           USHORT(header->compression) != ZFC_DEFLATED)
        {
            Con_Error("Zip_Open: %s: '%s' uses an unsupported compression "
                      "algorithm.\n", M_PrettyPath(fileName), buf);
        }
        if(USHORT(header->flags) & ZFH_ENCRYPTED)
        {
            Con_Error("Zip_Open: %s: '%s' is encrypted.\n  Encryption is "
                      "not supported.\n", M_PrettyPath(fileName), buf);
        }

        // Convert all slashes to the host OS's directory separator,
        // for compatibility with the sys_filein routines.
        Dir_FixSlashes(buf, FILENAME_T_MAXLEN);

        // In some cases the path inside the pack is mapped to another
        // virtual location.
        mapPath(buf, FILENAME_T_MAXLEN);

        // Make it absolute.
        M_PrependBasePath(buf, buf, FILENAME_T_MAXLEN);

        // We can add this file to the zipentry list.
        entry = newZipEntry(buf);
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

        // Read the local file header, which contains the correct
        // extra field size (Info-ZIP!).
        F_Seek(file, ULONG(header->relOffset), SEEK_SET);
        F_Read(&localHeader, sizeof(localHeader), file);

        entry->offset =
            ULONG(header->relOffset) + sizeof(localfileheader_t) +
            USHORT(header->fileNameSize) + USHORT(localHeader.extraFieldSize);
    }

    // The central directory is no longer needed.
    M_Free(directory);

    sortZipEntries();
    removeDuplicateFiles();

    // File successfully opened!
    return true;
}

/**
 * If two or more packages have the same file, the file from the last
 * loaded package is the one to use. Others will be removed from the
 * directory. The entries must be sorted before this can be done.
 */
static void removeDuplicateFiles(void)
{
    uint                i;
    boolean             modified = false;
    zipentry_t*         entry, *loser;

    if(!(zipNumFiles > 1))
        return; // Obviously no duplicates.

    // One scan through the directory is enough.
    for(i = 0, entry = zipFiles; i < zipNumFiles - 1; ++i, entry++)
    {
        // Is this entry the same as the next one?
        if(!stricmp(entry[0].name, entry[1].name))
        {   // One of these should be removed.
            // The newer one survives.
            if(entry[0].package->order > entry[1].package->order)
                loser = entry + 1;
            else
                loser = entry;

            // Overwrite the loser with the last entry in the entire
            // entry directory.
            M_Free(loser->name);
            memcpy(loser, &zipFiles[zipNumFiles - 1], sizeof(zipentry_t));
            memset(&zipFiles[zipNumFiles - 1], 0, sizeof(zipentry_t));

            // One less entry.
            zipNumFiles--;
            modified = true;
        }
    }

    if(modified)
    {   // Sort the entries again.
        sortZipEntries();
    }
}

/**
 * Iterates through the zipentry list. If the finder func returns
 * @c true,; iteration stops and the the 1-based index of the
 * zipentry is returned.
 *
 * @param parm          Ptr passed to the finder func.
 *
 * @return              Non-zero if something is found.
 */
zipindex_t Zip_Iterate(int (*iterator) (const char *, void *), void *parm)
{
    uint                i;

    for(i = 0; i < zipNumFiles; ++i)
    {
        if(iterator(zipFiles[i].name, parm))
            return i + 1;
    }

    // Nothing was accepted.
    return 0;
}

/**
 * Find a specific path in the zipentry list. Relative paths are converted
 * to absolute ones. A binary search is used (the entries have been sorted).
 * Good performance: O(log n).
 *
 * @return              Non-zero if something is found.
 */
zipindex_t Zip_Find(const char *fileName)
{
    zipindex_t          begin, end, mid;
    int                 relation;
    filename_t          fullPath;

    if(!zipNumFiles)
        return 0; // None registered yet.

    // Convert to an absolute path.
    strncpy(fullPath, fileName, FILENAME_T_MAXLEN);
    Dir_MakeAbsolute(fullPath, FILENAME_T_MAXLEN);

    // Init the search.
    begin = 0;
    end = zipNumFiles - 1;

    while(begin <= end)
    {
        mid = (begin + end) / 2;

        // How does this compare?
        relation = strnicmp(fullPath, zipFiles[mid].name,
                            FILENAME_T_MAXLEN);
        if(!relation)
        {   // Got it! We return a 1-based index.
            return mid + 1;
        }
        if(relation < 0)
        {   // What we are searching must be in the first half.
            if(mid > 0)
                end = mid - 1;
            else
                return 0; // Not found.
        }
        else
        {   // Then it must be in the second half.
            begin = mid + 1;
        }
    }

    // It wasn't found.
    return 0;
}

/**
 * Uses zlib to inflate a compressed entry.
 *
 * @return              @c true, if successful.
 */
static boolean inflateEntry(void *in, size_t inSize, void *out,
                            size_t outSize)
{
    z_stream    stream;
    int         result;

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
        Con_Message("inflateEntry: Failure due to %s.\n",
                    result == Z_DATA_ERROR ? "corrupt data" :
                    "zlib error");
        return false;
    }

    // We're done.
    inflateEnd(&stream);
    return true;
}

/**
 * @return              Size of a zipentry specified by index.
 */
size_t Zip_GetSize(zipindex_t index)
{
    if(index == 0 || index - 1 >= zipNumFiles)
        return 0; // Doesn't exist.

    return zipFiles[index - 1].size;
}

/**
 * Reads a zipentry into the buffer. The caller must make sure that
 * the buffer is large enough. Zip_GetSize() returns the size.
 *
 * @return              Number of bytes read.
 */
size_t Zip_Read(zipindex_t index, void *buffer)
{
    package_t  *pack;
    zipentry_t *entry;
    void       *compressedData;
    boolean     result;

    if(index == 0 || index - 1 >= zipNumFiles)
        return 0; // Doesn't exist.

    index--;
    entry = &zipFiles[index];
    pack = entry->package;

    VERBOSE2(Con_Message
             ("Zip_Read: %s: '%s' (%lu bytes%s)\n", M_PrettyPath(pack->name),
              M_PrettyPath(entry->name), (unsigned long) entry->size,
              (entry->deflatedSize? ", deflated" : "")));

    F_Seek(pack->file, entry->offset, SEEK_SET);

    if(entry->deflatedSize != 0)
    {
        // Read the compressed data into a buffer.
        compressedData = M_Malloc(entry->deflatedSize);
        F_Read(compressedData, entry->deflatedSize, pack->file);

        // Run zlib's inflate to decompress.
        result = inflateEntry(compressedData, entry->deflatedSize,
                              buffer, entry->size);

        M_Free(compressedData);

        if(!result)
            return 0; // Inflate failed.
    }
    else
    {
        // Read the uncompressed data directly to the buffer provided
        // by the caller.
        F_Read(buffer, entry->size, pack->file);
    }

    return entry->size;
}

/**
 * @return              "Last modified" timestamp of the zip entry.
 */
uint Zip_GetLastModified(zipindex_t index)
{
    if(index == 0 || index - 1 >= zipNumFiles)
        return 0; // Doesn't exist.

    return zipFiles[index - 1].lastModified;
}
