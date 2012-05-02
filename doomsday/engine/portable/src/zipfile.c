/**\file zipfile.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <zlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "lumpdirectory.h"
#include "pathdirectory.h"
#include "zipfile.h"
#include "m_misc.h"

typedef struct {
    size_t baseOffset;
    LumpInfo info;
} zipfile_lumprecord_t;

struct zipfile_s {
    /// Base file instance.
    abstractfile_t base;

    /// Directory containing structure and info records for all lumps.
    PathDirectory* lumpDirectory;

    /// LUT which maps logical lump indices to PathDirectoryNodes.
    PathDirectoryNode** lumpDirectoryMap;

    /// Vector of lump records.
    zipfile_lumprecord_t* lumpRecords;

    /// Lump cache data pointers.
    void** lumpCache;
};

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

/**
 * The path inside the zip might be mapped to another virtual location.
 *
 * \todo This is clearly implemented in the wrong place. Path mapping
 *       should be done at LumpDirectory level.
 *
 * Data files (pk3, zip, lmp, wad, deh) in the root are mapped to Data/Game/Auto.
 * Definition files (ded) in the root are mapped to Defs/Game/Auto.
 * Paths that begin with a '@' are mapped to Defs/Game/Auto.
 * Paths that begin with a '#' are mapped to Data/Game/Auto.
 * Key-named directories at the root are mapped to another location.
 */
static void ZipFile_ApplyPathMappings(ddstring_t* dest, const ddstring_t* src)
{
    // Manually mapped to Defs?
    if(Str_At(src, 0) == '@')
    {
        ddstring_t* out = (dest == src? Str_New() : dest);
        int dist;

        Str_Appendf(out, "%sauto/", Str_Text(Game_DefsPath(theGame)));
        dist = (Str_At(src, 1) == '/'? 2 : 1);
        Str_PartAppend(out, Str_Text(src), dist, Str_Length(src)-dist);

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
        int dist = 0;
        char* slash;

        Str_Appendf(out, "%sauto/", Str_Text(Game_DataPath(theGame)));
        slash = strrchr(Str_Text(src), '/');
        dist = slash - Str_Text(src);
        // Copy the path up to and including the last directory separator if present.
        if(slash - Str_Text(src) > 1)
            Str_PartAppend(out, Str_Text(src), 1, dist);

        if(slash)
        {
            // Is there a prefix to be omitted in the name?
            // The slash must not be too early in the string.
            if(slash >= Str_Text(src) + 2)
            {
                // Good old negative indices.
                if(slash[-2] == '.' && slash[-1] >= '1' && slash[-1] <= '9')
                    dist += slash[-1] - '1' + 1;
            }
        }

        Str_PartAppend(out, Str_Text(src), dist+1, Str_Length(src)-(dist+1));

        if(dest == src)
        {
            Str_Copy(dest, out);
            Str_Delete(out);
        }
        return;
    }

    if(strchr(Str_Text(src), '/') == NULL)
    {
        // No directory separators; i.e., a root file.
        resourcetype_t type = F_GuessResourceTypeByName(Str_Text(src));
        resourceclass_t rclass;
        ddstring_t mapped;

        /// \kludge
        // Some files require special handling...
        switch(type)
        {
        case RT_DEH: // Treat DeHackEd patches as packages so they are mapped to Data.
            rclass = RC_PACKAGE;
            break;
        case RT_NONE: { // *.lmp files must be mapped to Data.
            const char* ext = F_FindFileExtension(Str_Text(src));
            if(ext && !stricmp("lmp", ext))
            {
                rclass = RC_PACKAGE;
            }
            else
            {
                rclass = RC_UNKNOWN;
            }
            break;
          }
        default:
            rclass = F_DefaultResourceClassForType(type);
            break;
        }
        /// < kludge end

        Str_Init(&mapped);
        switch(rclass)
        {
        case RC_PACKAGE: // Mapped to the Data directory.
            Str_Appendf(&mapped, "%sauto/", Str_Text(Game_DataPath(theGame)));
            break;
        case RC_DEFINITION: // Mapped to the Defs directory.
            Str_Appendf(&mapped, "%sauto/", Str_Text(Game_DefsPath(theGame)));
            break;
        default: /* Not mapped */ break;
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
 * @return  @c true, if successful.
 */
static boolean ZipFile_LocateCentralDirectory(ZipFile* zip)
{
    int pos = CENTRAL_END_SIZE; // Offset from the end.
    uint32_t signature;
    assert(zip);

    // Start from the earliest location where the signature might be.
    while(pos < MAXIMUM_COMMENT_SIZE)
    {
        DFile_Seek(zip->base._file, -pos, SEEK_END);

        // Is this the signature?
        DFile_Read(zip->base._file, (uint8_t*)&signature, 4);
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

static void ZipFile_ReadLumpDirectory(ZipFile* zip)
{
    zipfile_lumprecord_t* record;
    int entryCount, index, pass;
    PathDirectoryNode* node;
    void* centralDirectory;
    ddstring_t entryPath;
    centralend_t summary;
    uint lumpIdx;
    char* pos;
    assert(zip);

    VERBOSE( Con_Message("ZipFile::readLumpDirectory: \"%s\"\n",
        F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)zip)))) );

    // Scan the end of the file for the central centralDirectory end record.
    if(!ZipFile_LocateCentralDirectory(zip))
    {
        Con_Error("ZipFile::readLumpDirectory: Central centralDirectory in %s not found!",
            Str_Text(AbstractFile_Path((abstractfile_t*)zip)));
    }

    // Read the central centralDirectory end record.
    DFile_Read(zip->base._file, (uint8_t*)&summary, sizeof(summary));

    // Does the summary say something we don't like?
    if(USHORT(summary.diskEntryCount) != USHORT(summary.totalEntryCount))
    {
        Con_Error("ZipFile::readLumpDirectory: Multipart Zip file \"%s\" not supported.",
            Str_Text(AbstractFile_Path((abstractfile_t*)zip)));
    }

    // Read the entire central centralDirectory into memory.
    centralDirectory = malloc(ULONG(summary.size));
    if(!centralDirectory)
        Con_Error("ZipFile::readLumpDirectory: Failed on allocation of %lu bytes for "
            "temporary copy of the central centralDirectory.", (unsigned long) ULONG(summary.size));
    DFile_Seek(zip->base._file, ULONG(summary.offset), SEEK_SET);
    DFile_Read(zip->base._file, (uint8_t*)centralDirectory, ULONG(summary.size));

    /**
     * Pass 1: Validate support and count the number of lump records we need.
     * Pass 2: Read all zip entries and populate the lump directory.
     */
    entryCount = 0;
    Str_Init(&entryPath);
    for(pass = 0; pass < 2; ++pass)
    {
        if(pass == 1)
        {
            if(entryCount == 0) break;

            // We can now allocate the records.
            zip->lumpRecords = (zipfile_lumprecord_t*)malloc(entryCount * sizeof(*zip->lumpRecords));
            if(!zip->lumpRecords)
                Con_Error("ZipFile::readLumpDirectory: Failed on allocation of %lu bytes for the lump record vector.", (unsigned long) (entryCount * sizeof(*zip->lumpRecords)));

            // Get the first record.
            record = zip->lumpRecords;
        }

        // Position the read cursor at the start of the buffered central centralDirectory.
        pos = centralDirectory;

        // Read all the entries.
        lumpIdx = 0;
        for(index = 0; index < USHORT(summary.totalEntryCount);
            ++index, pos += sizeof(centralfileheader_t))
        {
            const centralfileheader_t* header = (void*) pos;
            const char* nameStart = pos + sizeof(centralfileheader_t);
            localfileheader_t localHeader;

            // Advance the cursor past the variable sized fields.
            pos += USHORT(header->fileNameSize) + USHORT(header->extraFieldSize) + USHORT(header->commentSize);

            // Copy characters up to fileNameSize.
            Str_Clear(&entryPath);
            Str_PartAppend(&entryPath, nameStart, 0, USHORT(header->fileNameSize));

            // Directories are skipped.
            if(ULONG(header->size) == 0 && Str_RAt(&entryPath, 0) == '/')
            {
                continue;
            }

            // Do we support the format of this lump?
            if(USHORT(header->compression) != ZFC_NO_COMPRESSION &&
               USHORT(header->compression) != ZFC_DEFLATED)
            {
                if(pass != 0) continue;
                Con_Message("Warning: Zip %s:'%s' uses an unsupported compression algorithm, ignoring.\n",
                            Str_Text(AbstractFile_Path((abstractfile_t*)zip)), Str_Text(&entryPath));
            }

            if(USHORT(header->flags) & ZFH_ENCRYPTED)
            {
                if(pass != 0) continue;
                Con_Message("Warning: Zip %s:'%s' is encrypted.\n  Encryption is not supported, ignoring.\n",
                            Str_Text(AbstractFile_Path((abstractfile_t*)zip)), Str_Text(&entryPath));
            }

            if(pass == 0)
            {
                // Another record will be needed.
                ++entryCount;
                continue;
            }

            // Convert all slashes to our internal separator.
            F_FixSlashes(&entryPath, &entryPath);

            // In some cases the path inside the file is mapped to another virtual location.
            ZipFile_ApplyPathMappings(&entryPath, &entryPath);

            // Make it absolute.
            F_PrependBasePath(&entryPath, &entryPath);

            // Have we yet to intialize the directory?
            if(!zip->lumpDirectory)
            {
                zip->lumpDirectory = PathDirectory_NewWithFlags(PDF_ALLOW_DUPLICATE_LEAF);
            }

            F_InitLumpInfo(&record->info);
            node = PathDirectory_Insert(zip->lumpDirectory, Str_Text(&entryPath), '/');
            PathDirectoryNode_AttachUserData(node, record);

            record->info.lumpIdx = lumpIdx++;
            record->info.size = ULONG(header->size);
            if(USHORT(header->compression) == ZFC_DEFLATED)
            {
                // Compressed using the deflate algorithm.
                record->info.compressedSize = ULONG(header->compressedSize);
            }
            else // No compression.
            {
                record->info.compressedSize = record->info.size;
            }

            // The modification date is inherited from the real file (note recursion).
            record->info.lastModified = AbstractFile_LastModified((abstractfile_t*)zip);
            record->info.container = (abstractfile_t*)zip;

            // Read the local file header, which contains the extra field size (Info-ZIP!).
            DFile_Seek(zip->base._file, ULONG(header->relOffset), SEEK_SET);
            DFile_Read(zip->base._file, (uint8_t*)&localHeader, sizeof(localHeader));

            record->baseOffset = ULONG(header->relOffset) + sizeof(localfileheader_t) + USHORT(header->fileNameSize) + USHORT(localHeader.extraFieldSize);

            // Next record please!
            record++;
        }
    }

    // The file centralDirectory is no longer needed.
    free(centralDirectory);
    Str_Free(&entryPath);
}

static int insertNodeInLumpDirectoryMap(PathDirectoryNode* node, void* paramaters)
{
    ZipFile* zip = (ZipFile*)paramaters;
    zipfile_lumprecord_t* lumpRecord = (zipfile_lumprecord_t*)PathDirectoryNode_UserData(node);
    assert(lumpRecord && lumpRecord->info.lumpIdx >= 0 && lumpRecord->info.lumpIdx < ZipFile_LumpCount(zip));
    zip->lumpDirectoryMap[lumpRecord->info.lumpIdx] = node;
    return 0; // Continue iteration.
}

static void buildLumpDirectoryMap(ZipFile* zip)
{
    assert(zip);
    // Time to build the lump directory map?
    if(!zip->lumpDirectoryMap)
    {
        int lumpCount = ZipFile_LumpCount(zip);

        zip->lumpDirectoryMap = malloc(sizeof(*zip->lumpDirectoryMap) * lumpCount);
        if(!zip->lumpDirectoryMap)
            Con_Error("ZipFile::buildLumpDirectoryMap: Failed on allocation of %lu bytes for the lumpdirectory map.", (unsigned long) (sizeof(*zip->lumpDirectoryMap) * lumpCount));

        PathDirectory_Iterate2(zip->lumpDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH,
                               insertNodeInLumpDirectoryMap, (void*)zip);
    }
}

static zipfile_lumprecord_t* ZipFile_LumpRecord(ZipFile* zip, int lumpIdx)
{
    assert(zip);
    if(lumpIdx < 0 || lumpIdx >= ZipFile_LumpCount(zip)) return NULL;
    buildLumpDirectoryMap(zip);
    return (zipfile_lumprecord_t*)PathDirectoryNode_UserData(zip->lumpDirectoryMap[lumpIdx]);
}

ZipFile* ZipFile_New(DFile* file, const char* path, const LumpInfo* info)
{
    ZipFile* zip;

    if(!info) Con_Error("ZipFile::New: Received invalid LumpInfo.");

    zip = (ZipFile*)malloc(sizeof(*zip));
    if(!zip) Con_Error("ZipFile::New: Failed on allocation of %lu bytes for new ZipFile.",
                (unsigned long) sizeof *zip);

    AbstractFile_Init((abstractfile_t*)zip, FT_ZIPFILE, path, file, info);
    zip->lumpDirectory = NULL;
    zip->lumpDirectoryMap = NULL;
    zip->lumpRecords = NULL;
    zip->lumpCache = NULL;
    return zip;
}

int ZipFile_PublishLumpsToDirectory(ZipFile* zip, LumpDirectory* directory)
{
    int numPublished = 0;
    assert(zip);

    if(directory)
    {
        ZipFile_ReadLumpDirectory(zip);
        if(ZipFile_LumpCount(zip) > 0)
        {
            // Insert the lumps into their rightful places in the directory.
            LumpDirectory_CatalogLumps(directory, (abstractfile_t*)zip, 0, ZipFile_LumpCount(zip));
            numPublished += ZipFile_LumpCount(zip);
        }
    }
    return numPublished;
}

PathDirectoryNode* ZipFile_LumpDirectoryNode(ZipFile* zip, int lumpIdx)
{
    if(lumpIdx < 0 || lumpIdx >= ZipFile_LumpCount(zip)) return NULL;
    buildLumpDirectoryMap(zip);
    return zip->lumpDirectoryMap[lumpIdx];
}

ddstring_t* ZipFile_ComposeLumpPath(ZipFile* zip, int lumpIdx, char delimiter)
{
    PathDirectoryNode* node = ZipFile_LumpDirectoryNode(zip, lumpIdx);
    if(node)
    {
        return PathDirectory_ComposePath(PathDirectoryNode_Directory(node), node, Str_New(), NULL, delimiter);
    }
    return Str_New();
}

const LumpInfo* ZipFile_LumpInfo(ZipFile* zip, int lumpIdx)
{
    zipfile_lumprecord_t* lumpRecord = ZipFile_LumpRecord(zip, lumpIdx);
    if(!lumpRecord)
    {
        Con_Error("ZipFile::LumpInfo: Invalid lump index %i (valid range: [0..%i]).", lumpIdx, ZipFile_LumpCount(zip));
        exit(1); // Unreachable.
    }
    return &lumpRecord->info;
}

static int destroyRecord(PathDirectoryNode* node, void* paramaters)
{
    zipfile_lumprecord_t* rec = PathDirectoryNode_DetachUserData(node);
    if(rec)
    {
        F_DestroyLumpInfo(&rec->info);
        // The record itself is free'd later.
    }
    return 0; // Continue iteration.
}

void ZipFile_Delete(ZipFile* zip)
{
    assert(zip);

    F_ReleaseFile((abstractfile_t*)zip);
    ZipFile_ClearLumpCache(zip);

    if(zip->lumpDirectory)
    {
        if(PathDirectory_Size(zip->lumpDirectory) > 1 && zip->lumpCache)
        {
            free(zip->lumpCache);
        }

        PathDirectory_Iterate(zip->lumpDirectory, PCF_NO_BRANCH, NULL, PATHDIRECTORY_NOHASH, destroyRecord);
        PathDirectory_Delete(zip->lumpDirectory);
    }

    if(zip->lumpDirectoryMap) free(zip->lumpDirectoryMap);
    if(zip->lumpRecords) free(zip->lumpRecords);

    AbstractFile_Destroy((abstractfile_t*)zip);
    free(zip);
}

void ZipFile_ClearLumpCache(ZipFile* zip)
{
    int i, lumpCount = ZipFile_LumpCount(zip);
    assert(zip);

    if(lumpCount == 1)
    {
        if(zip->lumpCache)
        {
            // If the block has a user, it must be explicitly freed.
            if(Z_GetTag(zip->lumpCache) < PU_MAP)
                Z_ChangeTag(zip->lumpCache, PU_MAP);
            // Mark the memory pointer in use, but unowned.
            Z_ChangeUser(zip->lumpCache, (void*) 0x2);
        }
        return;
    }

    if(!zip->lumpCache) return;

    for(i = 0; i < lumpCount; ++i)
    {
        if(!zip->lumpCache[i]) continue;

        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(zip->lumpCache[i]) < PU_MAP)
            Z_ChangeTag(zip->lumpCache[i], PU_MAP);
        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(zip->lumpCache[i], (void*) 0x2);
    }
}

uint8_t* ZipFile_Compress(uint8_t* in, size_t inSize, size_t* outSize)
{
    return ZipFile_CompressAtLevel(in, inSize, outSize, Z_DEFAULT_COMPRESSION);
}

uint8_t* ZipFile_CompressAtLevel(uint8_t* in, size_t inSize, size_t* outSize, int level)
{
#define CHUNK_SIZE 32768
    z_stream stream;
    uint8_t chunk[CHUNK_SIZE];
    size_t allocSize = CHUNK_SIZE;
    uint8_t* output = M_Malloc(allocSize); // some initial space
    int result;
    int have;

    assert(outSize);
    *outSize = 0;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef*) in;
    stream.avail_in = (uInt) inSize;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    if(level < Z_NO_COMPRESSION)
    {
        level = Z_NO_COMPRESSION;
    }
    if(level > Z_BEST_COMPRESSION)
    {
        level = Z_BEST_COMPRESSION;
    }
    result = deflateInit(&stream, level);
    if(result != Z_OK)
    {
        free(output);
        return 0;
    }

    // Compress until all the data has been exhausted.
    do {
        stream.next_out = chunk;
        stream.avail_out = CHUNK_SIZE;
        result = deflate(&stream, Z_FINISH);
        if(result == Z_STREAM_ERROR)
        {
            free(output);
            *outSize = 0;
            return 0;
        }
        have = CHUNK_SIZE - stream.avail_out;
        if(have)
        {
            // Need more memory?
            if(*outSize + have > allocSize)
            {
                // Need more memory.
                allocSize *= 2;
                output = M_Realloc(output, allocSize);
            }
            // Append.
            memcpy(output + *outSize, chunk, have);
            *outSize += have;
        }
    } while(!stream.avail_out); // output chunk full, more data may follow

    assert(result == Z_STREAM_END);
    assert(stream.total_out == *outSize);

    deflateEnd(&stream);
    return output;
#undef CHUNK_SIZE
}

uint8_t* ZipFile_Uncompress(uint8_t* in, size_t inSize, size_t* outSize)
{
#define INF_CHUNK_SIZE 4096 // Uncompress in 4KB chunks.
    z_stream stream;
    uint8_t chunk[INF_CHUNK_SIZE];
    size_t allocSize = INF_CHUNK_SIZE;
    uint8_t* output = M_Malloc(allocSize); // some initial space
    int result;
    int have;

    assert(outSize);
    *outSize = 0;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef*) in;
    stream.avail_in = (uInt) inSize;

    result = inflateInit(&stream);
    if(result != Z_OK)
    {
        free(output);
        return 0;
    }

    // Uncompress until all the input data has been exhausted.
    do {
        stream.next_out = chunk;
        stream.avail_out = INF_CHUNK_SIZE;
        result = inflate(&stream, Z_FINISH);
        if(result == Z_STREAM_ERROR)
        {
            free(output);
            *outSize = 0;
            return 0;
        }
        have = INF_CHUNK_SIZE - stream.avail_out;
        if(have)
        {
            // Need more memory?
            if(*outSize + have > allocSize)
            {
                // Need more memory.
                allocSize *= 2;
                output = M_Realloc(output, allocSize);
            }
            // Append.
            memcpy(output + *outSize, chunk, have);
            *outSize += have;
        }
    } while(!stream.avail_out); // output chunk full, more data may follow

    // We should now be at the end.
    assert(result == Z_STREAM_END);

    inflateEnd(&stream);
    return output;
#undef INF_CHUNK_SIZE
}

boolean ZipFile_UncompressRaw(uint8_t* in, size_t inSize, uint8_t* out, size_t outSize)
{
    z_stream stream;
    int result;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef*) in;
    stream.avail_in = (uInt) inSize;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.next_out = (Bytef*) out;
    stream.avail_out = (uInt) outSize;

    if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return false;

    // Do the inflation in one call.
    result = inflate(&stream, Z_FINISH);

    if(stream.total_out != outSize)
    {
        inflateEnd(&stream);
        Con_Message("ZipFile::Uncompress: Failure due to %s (result code %i).\n",
                    (result == Z_DATA_ERROR ? "corrupt data" : "zlib error"), result);
        return false;
    }

    // We're done.
    inflateEnd(&stream);
    return true;
}

static size_t ZipFile_BufferLump(ZipFile* zip, const zipfile_lumprecord_t* lumpRecord,
    uint8_t* buffer)
{
    assert(zip && lumpRecord && buffer);

    DFile_Seek(zip->base._file, lumpRecord->baseOffset, SEEK_SET);

    if(lumpRecord->info.compressedSize != lumpRecord->info.size)
    {
        boolean result;
        uint8_t* compressedData = (uint8_t*)malloc(lumpRecord->info.compressedSize);
        if(!compressedData)
            Con_Error("ZipFile::BufferLump: Failed on allocation of %lu bytes for decompression buffer.", lumpRecord->info.compressedSize);

        // Read the compressed data into a temporary buffer for decompression.
        DFile_Read(zip->base._file, compressedData, lumpRecord->info.compressedSize);
        result = ZipFile_UncompressRaw(compressedData, lumpRecord->info.compressedSize,
                                       buffer, lumpRecord->info.size);
        free(compressedData);
        if(!result) return 0; // Inflate failed.
    }
    else
    {
        // Read the uncompressed data directly to the buffer provided by the caller.
        DFile_Read(zip->base._file, buffer, lumpRecord->info.size);
    }
    return lumpRecord->info.size;
}

size_t ZipFile_ReadLumpSection2(ZipFile* zip, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    const zipfile_lumprecord_t* lumpRecord = ZipFile_LumpRecord(zip, lumpIdx);
    if(!lumpRecord) return 0;

    VERBOSE2(
        ddstring_t* path = ZipFile_ComposeLumpPath(zip, lumpIdx, '/');
        Con_Printf("ZipFile::ReadLumpSection: \"%s:%s\" (%lu bytes%s) [%lu +%lu]",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)zip))),
                F_PrettyPath(Str_Text(path)), (unsigned long) lumpRecord->info.size,
                (lumpRecord->info.compressedSize != lumpRecord->info.size? ", compressed" : ""),
                (unsigned long) startOffset, (unsigned long)length);
        Str_Delete(path);
    )

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && zip->lumpCache)
    {
        size_t readBytes;
        boolean isCached;
        void** cachePtr;

        if(ZipFile_LumpCount(zip) > 1)
        {
            const uint cacheIdx = lumpIdx;
            cachePtr = &zip->lumpCache[cacheIdx];
            isCached = (NULL != zip->lumpCache[cacheIdx]);
        }
        else
        {
            cachePtr = (void**)&zip->lumpCache;
            isCached = (NULL != zip->lumpCache);
        }

        if(isCached)
        {
            VERBOSE2( Con_Printf(" from cache\n") )
            readBytes = MIN_OF(lumpRecord->info.size, length);
            memcpy(buffer, (uint8_t*)*cachePtr + startOffset, readBytes);
            return readBytes;
        }
    }

    VERBOSE2( Con_Printf("\n") )
    return ZipFile_BufferLump(zip, lumpRecord, buffer);
}

size_t ZipFile_ReadLumpSection(ZipFile* zip, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    return ZipFile_ReadLumpSection2(zip, lumpIdx, buffer, startOffset, length, true);
}

size_t ZipFile_ReadLump2(ZipFile* zip, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    const LumpInfo* info = ZipFile_LumpInfo(zip, lumpIdx);
    if(!info) return 0;
    return ZipFile_ReadLumpSection2(zip, lumpIdx, buffer, 0, info->size, tryCache);
}

size_t ZipFile_ReadLump(ZipFile* zip, int lumpIdx, uint8_t* buffer)
{
    return ZipFile_ReadLump2(zip, lumpIdx, buffer, true);
}

const uint8_t* ZipFile_CacheLump(ZipFile* zip, int lumpIdx, int tag)
{
    const LumpInfo* info = ZipFile_LumpInfo(zip, lumpIdx);
    const uint cacheIdx = lumpIdx;
    boolean isCached;
    void** cachePtr;

    VERBOSE2(
        ddstring_t* path = ZipFile_ComposeLumpPath(zip, lumpIdx, '/');
        Con_Printf("ZipFile::CacheLump: \"%s:%s\" (%lu bytes%s)",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)zip))),
                F_PrettyPath(Str_Text(path)), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : ""));
        Str_Delete(path);
    )

    if(ZipFile_LumpCount(zip) > 1)
    {
        // Time to allocate the cache ptr table?
        if(!zip->lumpCache)
            zip->lumpCache = (void**)calloc(ZipFile_LumpCount(zip), sizeof(*zip->lumpCache));
        cachePtr = &zip->lumpCache[cacheIdx];
        isCached = (NULL != zip->lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&zip->lumpCache;
        isCached = (NULL != zip->lumpCache);
    }

    VERBOSE2( Con_Printf(" %s\n", isCached? "hit":"miss") )

    if(!isCached)
    {
        uint8_t* ptr = (uint8_t*)Z_Malloc(info->size, tag, cachePtr);
        if(NULL == ptr)
            Con_Error("ZipFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpIdx);
        ZipFile_ReadLump2(zip, lumpIdx, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (uint8_t*)(*cachePtr);
}

void ZipFile_ChangeLumpCacheTag(ZipFile* zip, int lumpIdx, int tag)
{
    boolean isCached;
    void** cachePtr;
    assert(zip);

    if(ZipFile_LumpCount(zip) > 1)
    {
        const uint cacheIdx = lumpIdx;
        if(!zip->lumpCache) return; // Obviously not cached.

        cachePtr = &zip->lumpCache[cacheIdx];
        isCached = (NULL != zip->lumpCache[cacheIdx]);
    }
    else
    {
        cachePtr = (void**)&zip->lumpCache;
        isCached = (NULL != zip->lumpCache);
    }

    if(isCached)
    {
        VERBOSE2(
            ddstring_t* path = ZipFile_ComposeLumpPath(zip, lumpIdx, '/');
            Con_Printf("ZipFile::ChangeLumpCacheTag: \"%s:%s\" tag=%i\n",
                    F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)zip))),
                    F_PrettyPath(Str_Text(path)), tag);
            Str_Delete(path);
        )

        Z_ChangeTag2(*cachePtr, tag);
    }
}

int ZipFile_LumpCount(ZipFile* zip)
{
    assert(zip);
    return zip->lumpDirectory? PathDirectory_Size(zip->lumpDirectory) : 0;
}

boolean ZipFile_Recognise(DFile* file)
{
    boolean knownFormat = false;
    localfileheader_t hdr;
    size_t readBytes, initPos = DFile_Tell(file);
    DFile_Seek(file, 0, SEEK_SET);
    readBytes = DFile_Read(file, (uint8_t*)&hdr, sizeof(hdr));
    if(!(readBytes < sizeof(hdr)))
    {
        // Seek to the start of the signature.
        if(ULONG(hdr.signature) == SIG_LOCAL_FILE_HEADER)
        {
            knownFormat = true;
        }
    }
    // Reposition the stream in case another handler needs to process this file.
    DFile_Seek(file, initPos, SEEK_SET);
    return knownFormat;
}
