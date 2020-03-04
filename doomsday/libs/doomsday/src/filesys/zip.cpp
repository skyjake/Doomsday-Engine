/** @file zip.cpp  ZIP Archive (file).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "doomsday/filesys/zip.h"
#include "doomsday/filesys/lumpcache.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/game.h"
#include "doomsday/doomsdayapp.h"

#include <zlib.h>
#include <vector>

#include <de/App>
#include <de/ByteOrder>
#include <de/NativePath>
#include <de/LogBuffer>
#include <de/legacy/memory.h>
#include <de/legacy/memoryzone.h>
#include <cstring> // memcpy

#ifdef MSVC
#  undef min
#  undef max
#endif

namespace res {
namespace internal {

#define SIG_LOCAL_FILE_HEADER   0x04034b50
#define SIG_CENTRAL_FILE_HEADER 0x02014b50
#define SIG_END_OF_CENTRAL_DIR  0x06054b50

// Maximum tolerated size of the comment.
/// @todo Define this at Zip-class level.
#define MAXIMUM_COMMENT_SIZE    2048

// This is the length of the central directory end record (without the
// comment, but with the signature).
#define CENTRAL_END_SIZE        22

// File header flags.
#define ZFH_ENCRYPTED           0x1
#define ZFH_COMPRESSION_OPTS    0x6
#define ZFH_DESCRIPTOR          0x8
#define ZFH_COMPRESS_PATCHED    0x20 ///< Not supported.

// Compression methods.
/// @todo Define these at Zip level.
enum {
    ZFC_NO_COMPRESSION = 0,     ///< Supported format.
    ZFC_SHRUNK,
    ZFC_REDUCED_1,
    ZFC_REDUCED_2,
    ZFC_REDUCED_3,
    ZFC_REDUCED_4,
    ZFC_IMPLODED,
    ZFC_DEFLATED = 8,           ///< The only supported compression (via zlib).
    ZFC_DEFLATED_64,
    ZFC_PKWARE_DCL_IMPLODED
};

/// The following structures are used to read data directly from ZIP files.
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

/// @todo Do not position the stream here.
static bool readArchiveHeader(FileHandle &file, localfileheader_t &hdr)
{
    size_t readBytes, initPos = file.tell();
    // Seek to the start of the header.
    file.seek(0, SeekSet);
    readBytes = file.read((uint8_t *)&hdr, sizeof(localfileheader_t));
    // Return the stream to its original position.
    file.seek(initPos, SeekSet);
    if (!(readBytes < sizeof(localfileheader_t)))
    {
        hdr.signature       = littleEndianByteOrder.toHost(hdr.signature);
        hdr.requiredVersion = littleEndianByteOrder.toHost(hdr.requiredVersion);
        hdr.flags           = littleEndianByteOrder.toHost(hdr.flags);
        hdr.compression     = littleEndianByteOrder.toHost(hdr.compression);
        hdr.lastModTime     = littleEndianByteOrder.toHost(hdr.lastModTime);
        hdr.lastModDate     = littleEndianByteOrder.toHost(hdr.lastModDate);
        hdr.crc32           = littleEndianByteOrder.toHost(hdr.crc32);
        hdr.compressedSize  = littleEndianByteOrder.toHost(hdr.compressedSize);
        hdr.size            = littleEndianByteOrder.toHost(hdr.size);
        hdr.fileNameSize    = littleEndianByteOrder.toHost(hdr.fileNameSize);
        hdr.extraFieldSize  = littleEndianByteOrder.toHost(hdr.extraFieldSize);
        return true;
    }
    return false;
}

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

static bool readCentralEnd(FileHandle &file, centralend_t &end)
{
    size_t readBytes = file.read((uint8_t *)&end, sizeof(centralend_t));
    if (!(readBytes < sizeof(centralend_t)))
    {
        end.disk            = littleEndianByteOrder.toHost(end.disk);
        end.centralStartDisk= littleEndianByteOrder.toHost(end.centralStartDisk);
        end.diskEntryCount  = littleEndianByteOrder.toHost(end.diskEntryCount);
        end.totalEntryCount = littleEndianByteOrder.toHost(end.totalEntryCount);
        end.size            = littleEndianByteOrder.toHost(end.size);
        end.offset          = littleEndianByteOrder.toHost(end.offset);
        end.commentSize     = littleEndianByteOrder.toHost(end.commentSize);
        return true;
    }
    return false;
}

static String Zip_invalidIndexMessage(int invalidIdx, int lastValidIdx)
{
    String msg = Stringf("Invalid lump index %i", invalidIdx);
    if (lastValidIdx < 0) msg += " (file is empty)";
    else                  msg += Stringf(", valid range: [0..%i)", lastValidIdx);
    return msg;
}

/**
 * The path inside the zip might be mapped to another virtual location.
 *
 * @return  @c true= iff @a path was mapped to another location.
 *
 * @todo This is clearly implemented in the wrong place. Path mapping
 *       should be done at a higher level.
 */
static bool applyGamePathMappings(String &path)
{
    // Manually mapped to Defs?
    if (path.beginsWith('@'))
    {
        path.remove(BytePos(0), 1);
        if (path.first() == '/') path.remove(BytePos(0), 1);

        path = String("$(App.DefsPath)/$(GamePlugin.Name)/auto") / path;
        return true;
    }

    // Manually mapped to Data?
    if (path.beginsWith('#'))
    {
        path.remove(BytePos(0), 1);
        if (path.first() == '/') path.remove(BytePos(0), 1);

        // Is there a prefix to be omitted in the name?
        if (auto slash = path.lastIndexOf('/'))
        {
            // The slash must not be too early in the string.
            if (slash >= 2)
            {
                // Good old negative indices.
                if (path.at(slash - 2) == '.' && path.at(slash - 1) >= '1' && path.at(slash - 1) <= '9')
                    path.remove(slash - 2, 2);
            }
        }

        path = String("$(App.DataPath)/$(GamePlugin.Name)/auto") / path;
        return true;
    }

    // Implicitly mapped to another location?
    if (!path.contains('/'))
    {
        // No directory separators; i.e., a root file.
        const FileType &ftype = DD_GuessFileTypeFromFileName(path.fileName());

        switch (ftype.defaultClass())
        {
        case RC_PACKAGE:
            // Mapped to the Data directory.
            path = String("$(App.DataPath)/$(GamePlugin.Name)/auto") / path;
            return true;

        case RC_DEFINITION:
            // Mapped to the Defs directory?
            path = String("$(App.DefsPath)/$(GamePlugin.Name)/auto") / path;
            return true;

        default:
            return false;
        }
    }

    // Key-named directories in the root might be mapped to another location.
    const FS1::Schemes &schemes = App_FileSystem().allSchemes();
    DE_FOR_EACH_CONST(FS1::Schemes, i, schemes)
    {
        if (i->second->mapPath(path))
        {
            return true;
        }
    }
    return false;
}

} // namespace internal

using namespace internal;

Zip::LumpFile::LumpFile(Entry &entry, FileHandle *hndl, String path,
    const FileInfo &info, File1 *container)
    : File1(hndl, path, info, container)
    , entry(entry)
{}

const String &Zip::LumpFile::name() const
{
    return directoryNode().name();
}

Uri Zip::LumpFile::composeUri(Char delimiter) const
{
    return directoryNode().path(delimiter);
}

PathTree::Node &Zip::LumpFile::directoryNode() const
{
    return entry;
}

size_t Zip::LumpFile::read(uint8_t *buffer, bool tryCache)
{
    return zip().readLump(info_.lumpIdx, buffer, tryCache);
}

size_t Zip::LumpFile::read(uint8_t *buffer, size_t startOffset, size_t length, bool tryCache)
{
    return zip().readLump(info_.lumpIdx, buffer, startOffset, length, tryCache);
}

const uint8_t *Zip::LumpFile::cache()
{
    return zip().cacheLump(info_.lumpIdx);
}

Zip::LumpFile &Zip::LumpFile::unlock()
{
    zip().unlockLump(info_.lumpIdx);
    return *this;
}

Zip &Zip::LumpFile::zip() const
{
    return container().as<Zip>();
}

DE_PIMPL(Zip)
{
    LumpTree entries;                     ///< Directory structure and entry records for all lumps.
    std::unique_ptr<LumpCache> dataCache;  ///< Data payload cache.

    Impl(Public *i) : Base(i)
    {}

    /**
     * @param lump      Lump/file to be buffered.
     * @param buffer    Must be large enough to hold the entire uncompressed data lump.
     */
    size_t bufferLump(const LumpFile &lump, uint8_t *buffer)
    {
        DE_ASSERT(buffer);
        LOG_AS("Zip");

        const FileInfo &lumpInfo = lump.info();
        self().handle_->seek(lumpInfo.baseOffset, SeekSet);

        if (lumpInfo.isCompressed())
        {
            bool result;
            uint8_t *compressedData = (uint8_t *) M_Malloc(lumpInfo.compressedSize);
            if (!compressedData)
                throw Error("Zip::bufferLump",
                            stringf("Failed on allocation of %zu bytes for decompression buffer",
                                    lumpInfo.compressedSize));

            // Read the compressed data into a temporary buffer for decompression.
            self().handle_->read(compressedData, lumpInfo.compressedSize);

            // Uncompress into the buffer provided by the caller.
            result = uncompressRaw(compressedData, lumpInfo.compressedSize, buffer, lumpInfo.size);

            M_Free(compressedData);
            if (!result) return 0; // Inflate failed.
        }
        else
        {
            // Read the uncompressed data directly to the buffer provided by the caller.
            self().handle_->read(buffer, lumpInfo.size);
        }
        return lumpInfo.size;
    }
};

Zip::Zip(FileHandle &hndl, String path, const FileInfo &info, File1 *container)
    : File1(&hndl, path, info, container)
    , LumpIndex(true/*paths are unique*/)
    , d(new Impl(this))
{
    // Scan the end of the file for the central directory end record.
    /// @note: This gets awfully slow if the comment is long.
    bool foundCentralDirectory = false;
    // Start from the earliest location where the signature might be.
    {
        int pos = CENTRAL_END_SIZE; // Offset from the end.
        uint32_t signature;
        while (!foundCentralDirectory && pos < MAXIMUM_COMMENT_SIZE)
        {
            handle_->seek(-pos, SeekEnd);

            // Is this the signature?
            handle_->read((uint8_t *)&signature, sizeof(signature));
            if (littleEndianByteOrder.toHost(signature) == SIG_END_OF_CENTRAL_DIR)
            {
                foundCentralDirectory = true; // Yes, this is it.
            }
            else
            {
                // Move backwards.
                pos++;
            }
        }
    }

    if (!foundCentralDirectory)
        throw FormatError("Zip", "Central directory in \"" + NativePath(composePath()).pretty() + "\" not found");

    // Read the central directory end record.
    centralend_t summary;
    readCentralEnd(*handle_, summary);

    // Does the summary say something we don't like?
    if (summary.diskEntryCount != summary.totalEntryCount)
        throw FormatError("Zip", "Multipart zip file \"" + NativePath(composePath()).pretty() + "\" not supported");

    // We'll load the file directory using one continous read into a temporary
    // local buffer before we process it into our runtime representation.
    // Read the entire central directory into memory.
    void *centralDirectory = M_Malloc(summary.size);
    if (!centralDirectory)
        throw FormatError("Zip",
                          stringf("Failed on allocation of %u bytes for temporary copy of the "
                                  "central centralDirectory",
                                  summary.size));

    handle_->seek(summary.offset, SeekSet);
    handle_->read((uint8_t *)centralDirectory, summary.size);

    /**
     * Pass 1: Validate support and count the number of lump records we need.
     * Pass 2: Read all zip entries and populate the lump directory.
     */
    char *pos;
    int entryCount = 0;
    for (int pass = 0; pass < 2; ++pass)
    {
        if (pass == 1)
        {
            if (entryCount == 0) break;
        }

        // Position the read cursor at the start of the buffered central centralDirectory.
        pos = (char *)centralDirectory;

        // Read all the entries.
        uint lumpIdx = 0;
        for (int index = 0; index < summary.totalEntryCount; ++index, pos += sizeof(centralfileheader_t))
        {
            const centralfileheader_t *header = (centralfileheader_t *) pos;
            const char *nameStart = pos + sizeof(centralfileheader_t);
            localfileheader_t localHeader;

            // Advance the cursor past the variable sized fields.
            pos += DD_USHORT(header->fileNameSize) + DD_USHORT(header->extraFieldSize) + DD_USHORT(header->commentSize);

            String filePath = NativePath(nameStart, DD_USHORT(header->fileNameSize)).withSeparators('/');

            // Skip directories (we don't presently model these).
            if (DD_ULONG(header->size) == 0 && filePath.last() == '/') continue;

            // Do we support the format of this lump?
            if (DD_USHORT(header->compression) != ZFC_NO_COMPRESSION &&
               DD_USHORT(header->compression) != ZFC_DEFLATED)
            {
                if (pass != 0) continue;
                LOG_RES_WARNING("Zip %s:'%s' uses an unsupported compression algorithm")
                        << NativePath(composePath()).pretty() << NativePath(filePath).pretty();
            }

            if (DD_USHORT(header->flags) & ZFH_ENCRYPTED)
            {
                if (pass != 0) continue;
                LOG_RES_WARNING("Zip %s:'%s' is encrypted; encryption is not supported")
                        << NativePath(composePath()).pretty() << NativePath(filePath).pretty();
            }

            if (pass == 0)
            {
                // Another record will be needed.
                ++entryCount;
                continue;
            }

            // Read the local file header, which contains the extra field size (Info-ZIP!).
            handle_->seek(DD_ULONG(header->relOffset), SeekSet);
            handle_->read((uint8_t *)&localHeader, sizeof(localHeader));

            size_t baseOffset = DD_ULONG(header->relOffset) + sizeof(localfileheader_t)
                              + DD_USHORT(header->fileNameSize) + DD_USHORT(localHeader.extraFieldSize);

            size_t compressedSize;
            if (DD_USHORT(header->compression) == ZFC_DEFLATED)
            {
                // Compressed using the deflate algorithm.
                compressedSize = DD_ULONG(header->compressedSize);
            }
            else // No compression.
            {
                compressedSize = DD_ULONG(header->size);
            }

            if (!DoomsdayApp::game().isNull())
            {
                // In some cases the path to the file is mapped to some
                // other location in the virtual file system.
                String filePathCopy = filePath;
                if (applyGamePathMappings(filePathCopy))
                {
                    try
                    {
                        // Resolve all symbolic references in the path.
                        filePath = Uri(filePathCopy, RC_NULL).resolved();
                    }
                    catch (res::Uri::ResolveError const& er)
                    {
                        LOG_RES_WARNING(er.asText());
                    }
                }
            }

            // Make it absolute.
            filePath = App_BasePath() / filePath;
            Entry &entry = d->entries.insert(Path(filePath));

            entry.offset         = baseOffset;
            entry.size           = DD_ULONG(header->size);
            entry.compressedSize = compressedSize;

            LumpFile *lumpFile = new LumpFile(entry, nullptr, entry.path(),
                                              FileInfo(lastModified(), // Inherited from the file (note recursion).
                                              lumpIdx, entry.offset, entry.size, entry.compressedSize),
                                              this);
            entry.lumpFile.reset(lumpFile); // takes ownership

            catalogLump(*lumpFile);
            lumpIdx++;
        }
    }

    // The file central directory is no longer needed.
    M_Free(centralDirectory);
}

Zip::~Zip()
{}

void Zip::clearCachedLump(int lumpIndex, bool *retCleared)
{
    LOG_AS("Zip::clearCachedLump");

    if (retCleared) *retCleared = false;

    if (hasLump(lumpIndex))
    {
        if (d->dataCache)
        {
            d->dataCache->remove(lumpIndex, retCleared);
        }
    }
    else
    {
        LOGDEV_RES_WARNING(Zip_invalidIndexMessage(lumpIndex, lastIndex()));
    }
}

void Zip::clearLumpCache()
{
    LOG_AS("Zip::clearLumpCache");
    if (d->dataCache)
    {
        d->dataCache->clear();
    }
}

const uint8_t *Zip::cacheLump(int lumpIndex)
{
    LOG_AS("Zip::cacheLump");

    LumpFile &lumpFile = static_cast<LumpFile &>(lump(lumpIndex));
    LOGDEV_RES_XVERBOSE("\"%s:%s\" (%u bytes%s)",
                        NativePath(composePath()).pretty()
                        << NativePath(lumpFile.composePath()).pretty()
                        << lumpFile.info().size
                        << (lumpFile.info().isCompressed()? ", compressed" : ""));

    // Time to create the cache?
    if (!d->dataCache)
    {
        d->dataCache.reset(new LumpCache(lumpCount()));
    }

    const uint8_t *data = d->dataCache->data(lumpIndex);
    if (data) return data;

    uint8_t *region = (uint8_t *) Z_Malloc(lumpFile.info().size, PU_APPSTATIC, 0);
    if (!region) throw Error("Zip::cacheLump", stringf("Failed on allocation of %zu bytes for cache copy of lump #%i",
                                                       lumpFile.info().size, lumpIndex));

    readLump(lumpIndex, region, false);
    d->dataCache->insert(lumpIndex, region);

    return region;
}

void Zip::unlockLump(int lumpIndex)
{
    LOG_AS("Zip::unlockLump");
    LOGDEV_RES_XVERBOSE("\"%s:%s\"", NativePath(composePath()).pretty()
                        << NativePath(lump(lumpIndex).composePath()).pretty());

    if (hasLump(lumpIndex))
    {
        if (d->dataCache)
        {
            d->dataCache->unlock(lumpIndex);
        }
    }
    else
    {
        LOGDEV_RES_WARNING(Zip_invalidIndexMessage(lumpIndex, lastIndex()));
    }
}

size_t Zip::readLump(int lumpIndex, uint8_t *buffer, bool tryCache)
{
    LOG_AS("Zip::readLump");
    return readLump(lumpIndex, buffer, 0, lump(lumpIndex).size(), tryCache);
}

size_t Zip::readLump(int lumpIndex, uint8_t *buffer, size_t startOffset,
    size_t length, bool tryCache)
{
    LOG_AS("Zip::readLump");

    const LumpFile &lumpFile = static_cast<LumpFile &>(lump(lumpIndex));
    LOGDEV_RES_XVERBOSE("\"%s:%s\" (%u bytes%s) [%u +%u]",
               NativePath(composePath()).pretty()
            << NativePath(lumpFile.composePath()).pretty()
            << dsize(lumpFile.size())
            << (lumpFile.isCompressed()? ", compressed" : "")
            << startOffset
            << length);

    // Try to avoid a file system read by checking for a cached copy.
    if (tryCache)
    {
        const uint8_t *data = (d->dataCache ? d->dataCache->data(lumpIndex) : 0);
        LOGDEV_RES_XVERBOSE("Cache %s on #%i", (data? "hit" : "miss") << lumpIndex);
        if (data)
        {
            size_t readBytes = de::min(size_t(lumpFile.size()), length);
            std::memcpy(buffer, data + startOffset, readBytes);
            return readBytes;
        }
    }

    size_t readBytes = 0;
    if (!startOffset && length == lumpFile.size())
    {
        // Read it straight to the caller's data buffer.
        readBytes = d->bufferLump(lumpFile, buffer);
    }
    else
    {
        // Allocate a temporary buffer and read the whole lump into it(!).
        uint8_t *readBuf = (uint8_t *) M_Malloc(lumpFile.size());
        if (!readBuf) throw Error("Zip::readLump", stringf("Failed on allocation of %u bytes for work buffer", lumpFile.size()));

        if (d->bufferLump(lumpFile, readBuf))
        {
            readBytes = de::min(size_t(lumpFile.size()), length);
            std::memcpy(buffer, readBuf + startOffset, readBytes);
        }

        M_Free(readBuf);
    }

    /// @todo Do not check the read length here.
    if (readBytes < de::min(size_t(lumpFile.size()), length))
        throw Error("Zip::readLump", stringf("Only read %zu of %zu bytes of lump #%i", readBytes, length, lumpIndex));

    return readBytes;
}

bool Zip::recognise(FileHandle &file)
{
    localfileheader_t hdr;
    if (!readArchiveHeader(file, hdr)) return false;
    return hdr.signature == SIG_LOCAL_FILE_HEADER;
}

uint8_t *Zip::compress(uint8_t *in, size_t inSize, size_t *outSize)
{
    return compressAtLevel(in, inSize, outSize, Z_DEFAULT_COMPRESSION);
}

uint8_t *Zip::compressAtLevel(uint8_t *in, size_t inSize, size_t *outSize, int level)
{
#define CHUNK_SIZE 32768

    LOG_AS("Zip::compressAtLevel");

    z_stream stream;
    uint8_t chunk[CHUNK_SIZE];
    size_t allocSize = CHUNK_SIZE;
    uint8_t *output  = (uint8_t *) M_Malloc(allocSize); // some initial space
    int result;
    int have;

    DE_ASSERT(outSize);
    *outSize = 0;

    std::memset(&stream, 0, sizeof(stream));
    stream.next_in  = (Bytef*) in;
    stream.avail_in = (uInt) inSize;
    stream.zalloc   = Z_NULL;
    stream.zfree    = Z_NULL;
    stream.opaque   = Z_NULL;

    if (level < Z_NO_COMPRESSION)
    {
        level = Z_NO_COMPRESSION;
    }
    if (level > Z_BEST_COMPRESSION)
    {
        level = Z_BEST_COMPRESSION;
    }
    result = deflateInit(&stream, level);
    if (result != Z_OK)
    {
        M_Free(output);
        return 0;
    }

    // Compress until all the data has been exhausted.
    do
    {
        stream.next_out  = chunk;
        stream.avail_out = CHUNK_SIZE;
        result = deflate(&stream, Z_FINISH);
        if (result == Z_STREAM_ERROR)
        {
            M_Free(output);
            *outSize = 0;
            return 0;
        }
        have = CHUNK_SIZE - stream.avail_out;
        if (have)
        {
            // Need more memory?
            if (*outSize + have > allocSize)
            {
                // Need more memory.
                allocSize *= 2;
                output = (uint8_t *) M_Realloc(output, allocSize);
            }
            // Append.
            std::memcpy(output + *outSize, chunk, have);
            *outSize += have;
        }
    } while (!stream.avail_out); // output chunk full, more data may follow

    DE_ASSERT(result == Z_STREAM_END);
    DE_ASSERT(stream.total_out == *outSize);

    deflateEnd(&stream);
    return output;

#undef CHUNK_SIZE
}

uint8_t *Zip::uncompress(uint8_t *in, size_t inSize, size_t *outSize)
{
#define INF_CHUNK_SIZE 4096 // Uncompress in 4KB chunks.

    LOG_AS("Zip::uncompress");

    z_stream stream;
    uint8_t chunk[INF_CHUNK_SIZE];
    size_t allocSize = INF_CHUNK_SIZE;
    uint8_t *output  = (uint8_t *) M_Malloc(allocSize); // some initial space
    int result;
    int have;

    DE_ASSERT(outSize);
    *outSize = 0;

    std::memset(&stream, 0, sizeof(stream));
    stream.next_in  = (Bytef *) in;
    stream.avail_in = (uInt) inSize;

    result = inflateInit(&stream);
    if (result != Z_OK)
    {
        M_Free(output);
        return 0;
    }

    // Uncompress until all the input data has been exhausted.
    do
    {
        stream.next_out  = chunk;
        stream.avail_out = INF_CHUNK_SIZE;
        result = inflate(&stream, Z_FINISH);
        if (result == Z_STREAM_ERROR)
        {
            M_Free(output);
            *outSize = 0;
            return 0;
        }
        have = INF_CHUNK_SIZE - stream.avail_out;
        if (have)
        {
            // Need more memory?
            if (*outSize + have > allocSize)
            {
                // Need more memory.
                allocSize *= 2;
                output = (uint8_t *) M_Realloc(output, allocSize);
            }
            // Append.
            std::memcpy(output + *outSize, chunk, have);
            *outSize += have;
        }
    } while (!stream.avail_out); // output chunk full, more data may follow

    // We should now be at the end.
    DE_ASSERT(result == Z_STREAM_END);

    inflateEnd(&stream);
    return output;

#undef INF_CHUNK_SIZE
}

bool Zip::uncompressRaw(uint8_t *in, size_t inSize, uint8_t *out, size_t outSize)
{
    LOG_AS("Zip::uncompressRaw");
    z_stream stream;
    int result;

    std::memset(&stream, 0, sizeof(stream));
    stream.next_in   = (Bytef *) in;
    stream.avail_in  = (uInt) inSize;
    stream.zalloc    = Z_NULL;
    stream.zfree     = Z_NULL;
    stream.next_out  = (Bytef *) out;
    stream.avail_out = (uInt) outSize;

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return false;

    // Do the inflation in one call.
    result = inflate(&stream, Z_FINISH);

    if (stream.total_out != outSize)
    {
        inflateEnd(&stream);
        LOG_RES_WARNING("Failure due to %s (result code: %i)")
                << (result == Z_DATA_ERROR ? "corrupt data" : "zlib error")
                << result;
        return false;
    }

    // We're done.
    inflateEnd(&stream);
    return true;
}

const Zip::LumpTree &Zip::lumpTree() const
{
    return d->entries;
}

Zip::LumpFile &Zip::Entry::file() const
{
    DE_ASSERT(lumpFile);
    return *lumpFile;
}

} // namespace res
