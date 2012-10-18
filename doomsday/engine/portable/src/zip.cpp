/**
 * @file zip.cpp
 *
 * Zip archives. @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <zlib.h>

#include "de_base.h"
#include "de_filesys.h"
#include "game.h"
#include "lumpcache.h"
#include "lumpindex.h"
#include "pathdirectory.h"

#include "zip.h"

#include <vector>

#include <de/ByteOrder>
#include <de/Error>
#include <de/Log>
#include <de/memory.h>
#include <de/memoryzone.h>

namespace de {

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

static void ApplyPathMappings(ddstring_t* dest, const ddstring_t* src);

struct ZipLumpRecord
{
public:
    explicit ZipLumpRecord(FileInfo const& _info) : info_(_info)
    {}

    FileInfo const& info() const {
        return info_;
    }

private:
    FileInfo info_;
};

struct Zip::Instance
{
    Zip* self;

    /// Directory containing structure and info records for all lumps.
    PathDirectory* lumpDirectory;

    /// LUT which maps logical lump indices to PathDirectoryNodes.
    typedef std::vector<PathDirectoryNode*> LumpNodeLut;
    LumpNodeLut* lumpNodeLut;

    /// Lump data cache.
    LumpCache* lumpCache;

    Instance(Zip* d)
        : self(d), lumpDirectory(0), lumpNodeLut(0), lumpCache(0)
    {}

    ~Instance()
    {
        if(lumpDirectory)
        {
            PathDirectory_Iterate(reinterpret_cast<pathdirectory_s*>(lumpDirectory), PCF_NO_BRANCH,
                                  NULL, PATHDIRECTORY_NOHASH, clearLumpRecordWorker);
            delete lumpDirectory;
        }

        if(lumpNodeLut) delete lumpNodeLut;
        if(lumpCache) delete lumpCache;
    }

    ZipLumpRecord* lumpRecord(int lumpIdx)
    {
        if(!self->isValidIndex(lumpIdx)) return NULL;
        buildLumpNodeLut();
        return reinterpret_cast<ZipLumpRecord*>((*lumpNodeLut)[lumpIdx]->userData());
    }

    static int clearLumpRecordWorker(pathdirectorynode_s* _node, void* /*parameters*/)
    {
        PathDirectoryNode* node = reinterpret_cast<PathDirectoryNode*>(_node);
        ZipLumpRecord* rec = reinterpret_cast<ZipLumpRecord*>(node->userData());
        if(rec)
        {
            // Detach our user data from this node.
            node->setUserData(0);
            delete rec;
        }
        return 0; // Continue iteration.
    }

    /// @todo Do not position the stream here.
    static bool readArchiveHeader(FileHandle& file, localfileheader_t& hdr)
    {
        size_t readBytes, initPos = file.tell();
        // Seek to the start of the header.
        file.seek(0, SeekSet);
        readBytes = file.read((uint8_t*)&hdr, sizeof(localfileheader_t));
        // Return the stream to its original position.
        file.seek(initPos, SeekSet);
        if(!(readBytes < sizeof(localfileheader_t)))
        {
            hdr.signature       = littleEndianByteOrder.toNative(hdr.signature);
            hdr.requiredVersion = littleEndianByteOrder.toNative(hdr.requiredVersion);
            hdr.flags           = littleEndianByteOrder.toNative(hdr.flags);
            hdr.compression     = littleEndianByteOrder.toNative(hdr.compression);
            hdr.lastModTime     = littleEndianByteOrder.toNative(hdr.lastModTime);
            hdr.lastModDate     = littleEndianByteOrder.toNative(hdr.lastModDate);
            hdr.crc32           = littleEndianByteOrder.toNative(hdr.crc32);
            hdr.compressedSize  = littleEndianByteOrder.toNative(hdr.compressedSize);
            hdr.size            = littleEndianByteOrder.toNative(hdr.size);
            hdr.fileNameSize    = littleEndianByteOrder.toNative(hdr.fileNameSize);
            hdr.extraFieldSize  = littleEndianByteOrder.toNative(hdr.extraFieldSize);
            return true;
        }
        return false;
    }

    static bool readCentralEnd(FileHandle& file, centralend_t& end)
    {
        size_t readBytes = file.read((uint8_t*)&end, sizeof(centralend_t));
        if(!(readBytes < sizeof(centralend_t)))
        {
            end.disk            = littleEndianByteOrder.toNative(end.disk);
            end.centralStartDisk= littleEndianByteOrder.toNative(end.centralStartDisk);
            end.diskEntryCount  = littleEndianByteOrder.toNative(end.diskEntryCount);
            end.totalEntryCount = littleEndianByteOrder.toNative(end.totalEntryCount);
            end.size            = littleEndianByteOrder.toNative(end.size);
            end.offset          = littleEndianByteOrder.toNative(end.offset);
            end.commentSize     = littleEndianByteOrder.toNative(end.commentSize);
            return true;
        }
        return false;
    }

    /**
     * Finds the central directory end record in the end of the file.
     *
     * @note: This gets awfully slow if the comment is long.
     *
     * @return  @c true= successful.
     */
    bool locateCentralDirectory()
    {
        uint32_t signature;
        // Start from the earliest location where the signature might be.
        int pos = CENTRAL_END_SIZE; // Offset from the end.
        while(pos < MAXIMUM_COMMENT_SIZE)
        {
            self->handle_->seek(-pos, SeekEnd);

            // Is this the signature?
            self->handle_->read((uint8_t*)&signature, sizeof(signature));
            if(littleEndianByteOrder.toNative(signature) == SIG_END_OF_CENTRAL_DIR)
                return true; // Yes, this is it.

            // Move backwards.
            pos++;
        }
        return false;
    }

    void readLumpDirectory()
    {
        LOG_AS("Zip");
        // Already been here?
        if(lumpDirectory) return;

        // Scan the end of the file for the central directory end record.
        if(!locateCentralDirectory())
            throw Error("Zip::readLumpDirectory", QString("Central directory in %1 not found").arg(Str_Text(self->path())));

        // Read the central directory end record.
        centralend_t summary;
        readCentralEnd(*self->handle_, summary);

        // Does the summary say something we don't like?
        if(summary.diskEntryCount != summary.totalEntryCount)
            throw Error("Zip::readLumpDirectory", QString("Multipart zip file \"%1\" not supported").arg(Str_Text(self->path())));

        // We'll load the file directory using one continous read into a temporary
        // local buffer before we process it into our runtime representation.
        // Read the entire central directory into memory.
        void* centralDirectory = M_Malloc(summary.size);
        if(!centralDirectory) throw Error("Zip::readLumpDirectory", QString("Failed on allocation of %1 bytes for temporary copy of the central centralDirectory").arg(summary.size));

        self->handle_->seek(summary.offset, SeekSet);
        self->handle_->read((uint8_t*)centralDirectory, summary.size);

        /**
         * Pass 1: Validate support and count the number of lump records we need.
         * Pass 2: Read all zip entries and populate the lump directory.
         */
        char* pos;
        int entryCount = 0;
        ddstring_t entryPath;
        Str_Init(&entryPath);
        for(int pass = 0; pass < 2; ++pass)
        {
            if(pass == 1)
            {
                if(entryCount == 0) break;

                // Intialize the directory.
                lumpDirectory = new PathDirectory(PDF_ALLOW_DUPLICATE_LEAF);
            }

            // Position the read cursor at the start of the buffered central centralDirectory.
            pos = (char*)centralDirectory;

            // Read all the entries.
            uint lumpIdx = 0;
            for(int index = 0; index < summary.totalEntryCount; ++index, pos += sizeof(centralfileheader_t))
            {
                centralfileheader_t const* header = (centralfileheader_t*) pos;
                char const* nameStart = pos + sizeof(centralfileheader_t);
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
                    LOG_WARNING("Zip %s:'%s' uses an unsupported compression algorithm, ignoring.")
                        << Str_Text(self->path()) << Str_Text(&entryPath);
                }

                if(USHORT(header->flags) & ZFH_ENCRYPTED)
                {
                    if(pass != 0) continue;
                    LOG_WARNING("Zip %s:'%s' is encrypted.\n  Encryption is not supported, ignoring.")
                        << Str_Text(self->path()) << Str_Text(&entryPath);
                }

                if(pass == 0)
                {
                    // Another record will be needed.
                    ++entryCount;
                    continue;
                }

                // Read the local file header, which contains the extra field size (Info-ZIP!).
                self->handle_->seek(ULONG(header->relOffset), SeekSet);
                self->handle_->read((uint8_t*)&localHeader, sizeof(localHeader));

                size_t baseOffset = ULONG(header->relOffset) + sizeof(localfileheader_t)
                                  + USHORT(header->fileNameSize) + USHORT(localHeader.extraFieldSize);

                size_t compressedSize;
                if(USHORT(header->compression) == ZFC_DEFLATED)
                {
                    // Compressed using the deflate algorithm.
                    compressedSize = ULONG(header->compressedSize);
                }
                else // No compression.
                {
                    compressedSize = ULONG(header->size);
                }

                // Convert all slashes to our internal separator.
                F_FixSlashes(&entryPath, &entryPath);

                // In some cases the path inside the file is mapped to another virtual location.
                ApplyPathMappings(&entryPath, &entryPath);

                // Make it absolute.
                F_PrependBasePath(&entryPath, &entryPath);

                ZipLumpRecord* record =
                    new ZipLumpRecord(FileInfo(self->lastModified(), // Inherited from the file (note recursion).
                                               lumpIdx++, baseOffset, ULONG(header->size),
                                               compressedSize, self));
                PathDirectoryNode* node = lumpDirectory->insert(Str_Text(&entryPath));
                node->setUserData(record);
            }
        }

        // The file central directory is no longer needed.
        M_Free(centralDirectory);
        Str_Free(&entryPath);
    }

    static int buildLumpNodeLutWorker(pathdirectorynode_s* _node, void* parameters)
    {
        PathDirectoryNode* node = reinterpret_cast<PathDirectoryNode*>(_node);
        Instance* zipInst = (Instance*)parameters;
        ZipLumpRecord* lumpRecord = reinterpret_cast<ZipLumpRecord*>(node->userData());
        DENG2_ASSERT(lumpRecord && zipInst->self->isValidIndex(lumpRecord->info().lumpIdx)); // Sanity check.
        (*zipInst->lumpNodeLut)[lumpRecord->info().lumpIdx] = node;
        return 0; // Continue iteration.
    }

    void buildLumpNodeLut()
    {
        LOG_AS("Zip");
        // Been here already?
        if(lumpNodeLut) return;

        lumpNodeLut = new LumpNodeLut(self->lumpCount());
        PathDirectory_Iterate2(reinterpret_cast<pathdirectory_s*>(lumpDirectory), PCF_NO_BRANCH,
                               NULL, PATHDIRECTORY_NOHASH, buildLumpNodeLutWorker, (void*)this);
    }

    /**
     * @param buffer  Must be large enough to hold the entire uncompressed data lump.
     */
    size_t bufferLump(ZipLumpRecord const* lumpRecord, uint8_t* buffer)
    {
        DENG2_ASSERT(lumpRecord && buffer);
        LOG_AS("Zip");

        self->handle_->seek(lumpRecord->info().baseOffset, SeekSet);

        if(lumpRecord->info().isCompressed())
        {
            bool result;
            uint8_t* compressedData = (uint8_t*) M_Malloc(lumpRecord->info().compressedSize);
            if(!compressedData) throw Error("Zip::bufferLump", QString("Failed on allocation of %1 bytes for decompression buffer").arg(lumpRecord->info().compressedSize));

            // Read the compressed data into a temporary buffer for decompression.
            self->handle_->read(compressedData, lumpRecord->info().compressedSize);

            // Uncompress into the buffer provided by the caller.
            result = uncompressRaw(compressedData, lumpRecord->info().compressedSize,
                                   buffer, lumpRecord->info().size);

            M_Free(compressedData);
            if(!result) return 0; // Inflate failed.
        }
        else
        {
            // Read the uncompressed data directly to the buffer provided by the caller.
            self->handle_->read(buffer, lumpRecord->info().size);
        }
        return lumpRecord->info().size;
    }
};

Zip::Zip(FileHandle& hndl, char const* path, FileInfo const& info)
    : File1(FT_ZIP, path, hndl, info)
{
    d = new Instance(this);
}

Zip::~Zip()
{
    clearLumpCache();
    delete d;
}

bool Zip::isValidIndex(int lumpIdx)
{
    return lumpIdx >= 0 && lumpIdx < lumpCount();
}

int Zip::lastIndex()
{
    return lumpCount() - 1;
}

int Zip::lumpCount()
{
    d->readLumpDirectory();
    return d->lumpDirectory? d->lumpDirectory->size() : 0;
}

bool Zip::empty()
{
    return !lumpCount();
}

static QString invalidIndexMessage(int invalidIdx, int lastValidIdx)
{
    QString msg = QString("Invalid lump index %1 ").arg(invalidIdx);
    if(lastValidIdx < 0) msg += "(file is empty)";
    else                 msg += QString("(valid range: [0..%2])").arg(lastValidIdx);
    return msg;
}

de::PathDirectoryNode& Zip::lumpDirectoryNode(int lumpIdx)
{
    if(!isValidIndex(lumpIdx)) throw Error("Zip::lumpDirectoryNode", invalidIndexMessage(lumpIdx, lastIndex()));
    d->buildLumpNodeLut();
    return *((*d->lumpNodeLut)[lumpIdx]);
}

FileInfo const& Zip::lumpInfo(int lumpIdx)
{
    LOG_AS("Zip");
    ZipLumpRecord* lrec = d->lumpRecord(lumpIdx);
    if(!lrec) throw Error("Zip::lumpInfo", invalidIndexMessage(lumpIdx, lastIndex()));
    return lrec->info();
}

size_t Zip::lumpSize(int lumpIdx)
{
    LOG_AS("Zip");
    ZipLumpRecord* lrec = d->lumpRecord(lumpIdx);
    if(!lrec) throw Error("Zip::lumpSize", invalidIndexMessage(lumpIdx, lastIndex()));
    return lrec->info().size;
}

AutoStr* Zip::composeLumpPath(int lumpIdx, char delimiter)
{
    if(!isValidIndex(lumpIdx)) return AutoStr_NewStd();
    PathDirectoryNode& node = lumpDirectoryNode(lumpIdx);
    return node.composePath(AutoStr_NewStd(), NULL, delimiter);
}

Zip& Zip::clearCachedLump(int lumpIdx, bool* retCleared)
{
    LOG_AS("Zip::clearCachedLump");

    if(retCleared) *retCleared = false;

    if(isValidIndex(lumpIdx))
    {
        if(d->lumpCache)
        {
            d->lumpCache->remove(lumpIdx, retCleared);
        }
        else
        {
            LOG_DEBUG("LumpCache not in use, ignoring.");
        }
    }
    else
    {
        QString msg = invalidIndexMessage(lumpIdx, lastIndex());
        LOG_DEBUG(msg + ", ignoring.");
    }
    return *this;
}

Zip& Zip::clearLumpCache()
{
    LOG_AS("Zip::clearLumpCache");
    if(d->lumpCache) d->lumpCache->clear();
    return *this;
}

uint8_t const* Zip::cacheLump(int lumpIdx)
{
    LOG_AS("Zip::cacheLump");

    if(!isValidIndex(lumpIdx)) throw Error("Zip::cacheLump", invalidIndexMessage(lumpIdx, lastIndex()));

    FileInfo const& info = lumpInfo(lumpIdx);
    LOG_TRACE("\"%s:%s\" (%lu bytes%s)")
        << F_PrettyPath(Str_Text(path()))
        << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')))
        << (unsigned long) info.size
        << (info.isCompressed()? ", compressed" : "");

    // Time to create the cache?
    if(!d->lumpCache)
    {
        d->lumpCache = new LumpCache(lumpCount());
    }

    uint8_t const* data = d->lumpCache->data(lumpIdx);
    if(data) return data;

    uint8_t* region = (uint8_t*) Z_Malloc(info.size, PU_APPSTATIC, 0);
    if(!region) throw Error("Zip::cacheLump", QString("Failed on allocation of %1 bytes for cache copy of lump #%2").arg(info.size).arg(lumpIdx));

    readLump(lumpIdx, region, false);
    d->lumpCache->insert(lumpIdx, region);

    return region;
}

Zip& Zip::unlockLump(int lumpIdx)
{
    LOG_AS("Zip::unlockLump");
    LOG_TRACE("\"%s:%s\"")
        << F_PrettyPath(Str_Text(path())) << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')));

    if(isValidIndex(lumpIdx))
    {
        if(d->lumpCache)
        {
            d->lumpCache->unlock(lumpIdx);
        }
        else
        {
            LOG_DEBUG("LumpCache not in use, ignoring.");
        }
    }
    else
    {
        QString msg = invalidIndexMessage(lumpIdx, lastIndex());
        LOG_DEBUG(msg + ", ignoring.");
    }
    return *this;
}

size_t Zip::readLump(int lumpIdx, uint8_t* buffer, bool tryCache)
{
    LOG_AS("Zip::readLump");
    if(!isValidIndex(lumpIdx)) return 0;
    return readLump(lumpIdx, buffer, 0, lumpInfo(lumpIdx).size, tryCache);
}

size_t Zip::readLump(int lumpIdx, uint8_t* buffer, size_t startOffset,
    size_t length, bool tryCache)
{
    LOG_AS("Zip::readLump");
    ZipLumpRecord const* lrec = d->lumpRecord(lumpIdx);
    if(!lrec) return 0;

    LOG_TRACE("\"%s:%s\" (%lu bytes%s) [%lu +%lu]")
        << F_PrettyPath(Str_Text(path()))
        << F_PrettyPath(Str_Text(composeLumpPath(lumpIdx, '/')))
        << (unsigned long) lrec->info().size
        << (lrec->info().isCompressed()? ", compressed" : "")
        << (unsigned long) startOffset
        << (unsigned long) length;

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache)
    {
        uint8_t const* data = d->lumpCache? d->lumpCache->data(lumpIdx) : 0;
        LOG_DEBUG("Cache %s on #%i") << (data? "hit" : "miss") << lumpIdx;
        if(data)
        {
            size_t readBytes = MIN_OF(lrec->info().size, length);
            memcpy(buffer, data + startOffset, readBytes);
            return readBytes;
        }
    }

    size_t readBytes;
    if(!startOffset && length == lrec->info().size)
    {
        // Read it straight to the caller's data buffer.
        readBytes = d->bufferLump(lrec, buffer);
    }
    else
    {
        // Allocate a temporary buffer and read the whole lump into it(!).
        uint8_t* lumpData = (uint8_t*) M_Malloc(lrec->info().size);
        if(!lumpData) throw Error("Zip::readLumpSection", QString("Failed on allocation of %1 bytes for work buffer").arg(lrec->info().size));

        if(d->bufferLump(lrec, lumpData))
        {
            readBytes = MIN_OF(lrec->info().size, length);
            memcpy(buffer, lumpData + startOffset, readBytes);
        }
        else
        {
            readBytes = 0;
        }
        M_Free(lumpData);
    }

    /// @todo Do not check the read length here.
    if(readBytes < MIN_OF(lrec->info().size, length))
        throw Error("Zip::readLumpSection", QString("Only read %1 of %2 bytes of lump #%3").arg(readBytes).arg(length).arg(lumpIdx));

    return readBytes;
}

bool Zip::recognise(FileHandle& file)
{
    localfileheader_t hdr;
    if(!Zip::Instance::readArchiveHeader(file, hdr)) return false;
    return hdr.signature == SIG_LOCAL_FILE_HEADER;
}

uint8_t* Zip::compress(uint8_t* in, size_t inSize, size_t* outSize)
{
    return compressAtLevel(in, inSize, outSize, Z_DEFAULT_COMPRESSION);
}

uint8_t* Zip::compressAtLevel(uint8_t* in, size_t inSize, size_t* outSize, int level)
{
#define CHUNK_SIZE 32768

    LOG_AS("Zip::compressAtLevel");

    z_stream stream;
    uint8_t chunk[CHUNK_SIZE];
    size_t allocSize = CHUNK_SIZE;
    uint8_t* output = (uint8_t*) M_Malloc(allocSize); // some initial space
    int result;
    int have;

    DENG2_ASSERT(outSize);
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
        M_Free(output);
        return 0;
    }

    // Compress until all the data has been exhausted.
    do
    {
        stream.next_out = chunk;
        stream.avail_out = CHUNK_SIZE;
        result = deflate(&stream, Z_FINISH);
        if(result == Z_STREAM_ERROR)
        {
            M_Free(output);
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
                output = (uint8_t*) M_Realloc(output, allocSize);
            }
            // Append.
            memcpy(output + *outSize, chunk, have);
            *outSize += have;
        }
    } while(!stream.avail_out); // output chunk full, more data may follow

    DENG2_ASSERT(result == Z_STREAM_END);
    DENG2_ASSERT(stream.total_out == *outSize);

    deflateEnd(&stream);
    return output;

#undef CHUNK_SIZE
}

uint8_t* Zip::uncompress(uint8_t* in, size_t inSize, size_t* outSize)
{
#define INF_CHUNK_SIZE 4096 // Uncompress in 4KB chunks.

    LOG_AS("Zip::uncompress");

    z_stream stream;
    uint8_t chunk[INF_CHUNK_SIZE];
    size_t allocSize = INF_CHUNK_SIZE;
    uint8_t* output = (uint8_t*) M_Malloc(allocSize); // some initial space
    int result;
    int have;

    DENG2_ASSERT(outSize);
    *outSize = 0;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef*) in;
    stream.avail_in = (uInt) inSize;

    result = inflateInit(&stream);
    if(result != Z_OK)
    {
        M_Free(output);
        return 0;
    }

    // Uncompress until all the input data has been exhausted.
    do
    {
        stream.next_out = chunk;
        stream.avail_out = INF_CHUNK_SIZE;
        result = inflate(&stream, Z_FINISH);
        if(result == Z_STREAM_ERROR)
        {
            M_Free(output);
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
                output = (uint8_t*) M_Realloc(output, allocSize);
            }
            // Append.
            memcpy(output + *outSize, chunk, have);
            *outSize += have;
        }
    } while(!stream.avail_out); // output chunk full, more data may follow

    // We should now be at the end.
    DENG2_ASSERT(result == Z_STREAM_END);

    inflateEnd(&stream);
    return output;

#undef INF_CHUNK_SIZE
}

bool Zip::uncompressRaw(uint8_t* in, size_t inSize, uint8_t* out, size_t outSize)
{
    LOG_AS("Zip::uncompressRaw");
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
        LOG_WARNING("Failure due to %s (result code %i).")
            << (result == Z_DATA_ERROR ? "corrupt data" : "zlib error")
            << result;
        return false;
    }

    // We're done.
    inflateEnd(&stream);
    return true;
}

/**
 * The path inside the zip might be mapped to another virtual location.
 *
 * @todo This is clearly implemented in the wrong place. Path mapping
 *       should be done at a higher level.
 *
 * Data files (pk3, zip, lmp, wad, deh) in the root are mapped to Data/Game/Auto.
 * Definition files (ded) in the root are mapped to Defs/Game/Auto.
 * Paths that begin with a '@' are mapped to Defs/Game/Auto.
 * Paths that begin with a '#' are mapped to Data/Game/Auto.
 * Key-named directories at the root are mapped to another location.
 */
static void ApplyPathMappings(ddstring_t* dest, const ddstring_t* src)
{
    // Manually mapped to Defs?
    if(Str_At(src, 0) == '@')
    {
        ddstring_t* out = (dest == src? Str_New() : dest);
        int dist;

        Str_Appendf(out, "%sauto/", Str_Text(&reinterpret_cast<de::Game*>(App_CurrentGame())->defsPath()));
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

        Str_Appendf(out, "%sauto/", Str_Text(&reinterpret_cast<de::Game*>(App_CurrentGame())->dataPath()));
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

        // Certain resource files require special handling.
        // Something of a kludge, at this level.
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
        // Kludge end

        Str_Init(&mapped);
        switch(rclass)
        {
        case RC_PACKAGE: // Mapped to the Data directory.
            Str_Appendf(&mapped, "%sauto/", Str_Text(&reinterpret_cast<de::Game*>(App_CurrentGame())->dataPath()));
            break;
        case RC_DEFINITION: // Mapped to the Defs directory.
            Str_Appendf(&mapped, "%sauto/", Str_Text(&reinterpret_cast<de::Game*>(App_CurrentGame())->defsPath()));
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

} // namespace de
