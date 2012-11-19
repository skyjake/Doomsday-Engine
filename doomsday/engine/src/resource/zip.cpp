/**
 * @file zip.cpp
 * Zip archives. @ingroup resource
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

#include <vector>

#include "de_base.h"
#include "de_filesys.h"
#include "pathtree.h"
#include "resource/lumpcache.h"
#include "resource/zip.h"

#include <de/App>
#include <de/ByteOrder>
#include <de/Error>
#include <de/Log>
#include <de/NativePath>
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

static String invalidIndexMessage(int invalidIdx, int lastValidIdx);
static bool applyGamePathMappings(String& path);

class ZipFile : public File1
{
public:
    ZipFile(FileHandle& hndl, String path, FileInfo const& info, File1* container)
        : File1(hndl, path, info, container)
    {}

    /// @return  Name of this file.
    String const& name() const
    {
        return directoryNode().name();
    }

    /**
     * Compose an absolute URI to this file.
     *
     * @param delimiter     Delimit directory using this character.
     *
     * @return The absolute URI.
     */
    Uri composeUri(QChar delimiter = '/') const
    {
        return directoryNode().composeUri(delimiter);
    }

    /**
     * Retrieve the directory node for this file.
     *
     * @return  Directory node for this file.
     */
    PathTree::Node const& directoryNode() const
    {
        return dynamic_cast<Zip&>(container()).lumpDirectoryNode(info_.lumpIdx);
    }

    /**
     * Read the file data into @a buffer.
     *
     * @param buffer        Buffer to read into. Must be at least large enough to
     *                      contain the whole file.
     * @param tryCache      @c true= try the lump cache first.
     *
     * @return Number of bytes read.
     *
     * @see size() or info() to determine the size of buffer needed.
     */
    size_t read(uint8_t* buffer, bool tryCache = true)
    {
        return dynamic_cast<Zip&>(container()).readLump(info_.lumpIdx, buffer, tryCache);
    }

    /**
     * Read a subsection of the file data into @a buffer.
     *
     * @param buffer        Buffer to read into. Must be at least @a length bytes.
     * @param startOffset   Offset from the beginning of the file to start reading.
     * @param length        Number of bytes to read.
     * @param tryCache      If @c true try the local data cache first.
     *
     * @return Number of bytes read.
     */
    size_t read(uint8_t* buffer, size_t startOffset, size_t length, bool tryCache = true)
    {
        return dynamic_cast<Zip&>(container()).readLump(info_.lumpIdx, buffer, startOffset, length, tryCache);
    }

    /**
     * Read this lump into the local cache.

     * @return Pointer to the cached copy of the associated data.
     */
    uint8_t const* cache()
    {
        return dynamic_cast<Zip&>(container()).cacheLump(info_.lumpIdx);
    }

    /**
     * Remove a lock on the locally cached data.
     *
     * @return This instance.
     */
    ZipFile& unlock()
    {
        dynamic_cast<Zip&>(container()).unlockLump(info_.lumpIdx);
        return *this;
    }
};

struct Zip::Instance
{
    Zip* self;

    /// Directory containing structure and info records for all lumps.
    PathTree* lumpDirectory;

    /// LUT which maps logical lump indices to PathTreeNodes.
    typedef std::vector<PathTree::Node*> LumpNodeLut;
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
            lumpDirectory->traverse(PCF_NO_BRANCH, NULL, PathTree::no_hash, clearZipFileWorker);
            delete lumpDirectory;
        }

        if(lumpNodeLut) delete lumpNodeLut;
        if(lumpCache) delete lumpCache;
    }

    static int clearZipFileWorker(PathTree::Node& node, void* /*parameters*/)
    {
        ZipFile* rec = reinterpret_cast<ZipFile*>(node.userPointer());
        if(rec)
        {
            // Detach our user data from this node.
            node.setUserPointer(0);
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
            throw FormatError("Zip::readLumpDirectory", "Central directory in \"" + NativePath(self->composePath()).pretty() + "\" not found");

        // Read the central directory end record.
        centralend_t summary;
        readCentralEnd(*self->handle_, summary);

        // Does the summary say something we don't like?
        if(summary.diskEntryCount != summary.totalEntryCount)
            throw FormatError("Zip::readLumpDirectory", "Multipart zip file \"" + NativePath(self->composePath()).pretty() + "\" not supported");

        // We'll load the file directory using one continous read into a temporary
        // local buffer before we process it into our runtime representation.
        // Read the entire central directory into memory.
        void* centralDirectory = M_Malloc(summary.size);
        if(!centralDirectory) throw FormatError("Zip::readLumpDirectory", String("Failed on allocation of %1 bytes for temporary copy of the central centralDirectory").arg(summary.size));

        self->handle_->seek(summary.offset, SeekSet);
        self->handle_->read((uint8_t*)centralDirectory, summary.size);

        /**
         * Pass 1: Validate support and count the number of lump records we need.
         * Pass 2: Read all zip entries and populate the lump directory.
         */
        char* pos;
        int entryCount = 0;
        for(int pass = 0; pass < 2; ++pass)
        {
            if(pass == 1)
            {
                if(entryCount == 0) break;

                // Intialize the directory.
                lumpDirectory = new PathTree(PATHTREE_MULTI_LEAF);
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

                String filePath = NativePath(nameStart, USHORT(header->fileNameSize)).withSeparators('/');

                // Skip directories (we don't presently model these).
                if(ULONG(header->size) == 0 && filePath.last() == '/') continue;

                // Do we support the format of this lump?
                if(USHORT(header->compression) != ZFC_NO_COMPRESSION &&
                   USHORT(header->compression) != ZFC_DEFLATED)
                {
                    if(pass != 0) continue;
                    LOG_WARNING("Zip %s:'%s' uses an unsupported compression algorithm, ignoring.")
                        << NativePath(self->composePath()).pretty() << NativePath(filePath).pretty();
                }

                if(USHORT(header->flags) & ZFH_ENCRYPTED)
                {
                    if(pass != 0) continue;
                    LOG_WARNING("Zip %s:'%s' is encrypted.\n  Encryption is not supported, ignoring.")
                        << NativePath(self->composePath()).pretty() << NativePath(filePath).pretty();
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

                if(DD_GameLoaded())
                {
                    // In some cases the path to the file is mapped to some
                    // other location in the virtual file system.
                    String filePathCopy = filePath;
                    if(applyGamePathMappings(filePathCopy))
                    {
                        try
                        {
                            // Resolve all symbolic references in the path.
                            filePath = Uri(filePathCopy, FC_NONE).resolved();
                        }
                        catch(de::Uri::ResolveError const& er)
                        {
                            LOG_WARNING(er.asText());
                        }
                    }
                }

                // Make it absolute.
                filePath = App_BasePath() / filePath;

                QByteArray filePathUtf8 = filePath.toUtf8();

                FileHandle* dummy = 0; /// @todo Fixme!
                ZipFile* record =
                    new ZipFile(*dummy, filePathUtf8.constData(),
                                FileInfo(self->lastModified(), // Inherited from the file (note recursion).
                                         lumpIdx, baseOffset, ULONG(header->size),
                                         compressedSize),
                                self);
                PathTree::Node* node = lumpDirectory->insert(Uri(filePath, FC_NONE));
                node->setUserPointer(record);

                lumpIdx++;
            }
        }

        // The file central directory is no longer needed.
        M_Free(centralDirectory);
    }

    static int buildLumpNodeLutWorker(PathTree::Node& node, void* parameters)
    {
        Instance* zipInst = (Instance*)parameters;
        ZipFile* lumpRecord = reinterpret_cast<ZipFile*>(node.userPointer());
        DENG2_ASSERT(lumpRecord && zipInst->self->isValidIndex(lumpRecord->info().lumpIdx)); // Sanity check.
        (*zipInst->lumpNodeLut)[lumpRecord->info().lumpIdx] = &node;
        return 0; // Continue iteration.
    }

    void buildLumpNodeLut()
    {
        LOG_AS("Zip");
        // Been here already?
        if(lumpNodeLut) return;

        lumpNodeLut = new LumpNodeLut(self->lumpCount());
        if(!lumpDirectory) return;

        lumpDirectory->traverse(PCF_NO_BRANCH, NULL, PathTree::no_hash, buildLumpNodeLutWorker, (void*)this);
    }

    /**
     * @param lump      Lump/file to be buffered.
     * @param buffer    Must be large enough to hold the entire uncompressed data lump.
     */
    size_t bufferLump(ZipFile const& lump, uint8_t* buffer)
    {
        DENG2_ASSERT(buffer);
        LOG_AS("Zip");

        FileInfo const& lumpInfo = lump.info();
        self->handle_->seek(lumpInfo.baseOffset, SeekSet);

        if(lumpInfo.isCompressed())
        {
            bool result;
            uint8_t* compressedData = (uint8_t*) M_Malloc(lumpInfo.compressedSize);
            if(!compressedData) throw Error("Zip::bufferLump", QString("Failed on allocation of %1 bytes for decompression buffer").arg(lumpInfo.compressedSize));

            // Read the compressed data into a temporary buffer for decompression.
            self->handle_->read(compressedData, lumpInfo.compressedSize);

            // Uncompress into the buffer provided by the caller.
            result = uncompressRaw(compressedData, lumpInfo.compressedSize, buffer, lumpInfo.size);

            M_Free(compressedData);
            if(!result) return 0; // Inflate failed.
        }
        else
        {
            // Read the uncompressed data directly to the buffer provided by the caller.
            self->handle_->read(buffer, lumpInfo.size);
        }
        return lumpInfo.size;
    }
};

Zip::Zip(FileHandle& hndl, String path, FileInfo const& info, File1* container)
    : File1(hndl, path, info, container)
{
    d = new Instance(this);
}

Zip::~Zip()
{
    clearLumpCache();
    delete d;
}

bool Zip::isValidIndex(int lumpIdx) const
{
    return lumpIdx >= 0 && lumpIdx < lumpCount();
}

int Zip::lastIndex() const
{
    return lumpCount() - 1;
}

int Zip::lumpCount() const
{
    d->readLumpDirectory();
    return d->lumpDirectory? d->lumpDirectory->size() : 0;
}

bool Zip::empty()
{
    return !lumpCount();
}

PathTree::Node& Zip::lumpDirectoryNode(int lumpIdx) const
{
    if(!isValidIndex(lumpIdx)) throw NotFoundError("Zip::lumpDirectoryNode", invalidIndexMessage(lumpIdx, lastIndex()));
    d->buildLumpNodeLut();
    return *((*d->lumpNodeLut)[lumpIdx]);
}

File1& Zip::lump(int lumpIdx)
{
    LOG_AS("Zip");
    if(!isValidIndex(lumpIdx)) throw NotFoundError("Zip::lump", invalidIndexMessage(lumpIdx, lastIndex()));
    d->buildLumpNodeLut();
    return *reinterpret_cast<ZipFile*>((*d->lumpNodeLut)[lumpIdx]->userPointer());
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

    if(!isValidIndex(lumpIdx)) throw NotFoundError("Zip::cacheLump", invalidIndexMessage(lumpIdx, lastIndex()));

    ZipFile& file = reinterpret_cast<ZipFile&>(lump(lumpIdx));
    LOG_TRACE("\"%s:%s\" (%u bytes%s)")
        << de::NativePath(composePath()).pretty()
        << de::NativePath(file.composePath()).pretty()
        << (unsigned long) file.info().size
        << (file.info().isCompressed()? ", compressed" : "");

    // Time to create the cache?
    if(!d->lumpCache)
    {
        d->lumpCache = new LumpCache(lumpCount());
    }

    uint8_t const* data = d->lumpCache->data(lumpIdx);
    if(data) return data;

    uint8_t* region = (uint8_t*) Z_Malloc(file.info().size, PU_APPSTATIC, 0);
    if(!region) throw Error("Zip::cacheLump", QString("Failed on allocation of %1 bytes for cache copy of lump #%2").arg(file.info().size).arg(lumpIdx));

    readLump(lumpIdx, region, false);
    d->lumpCache->insert(lumpIdx, region);

    return region;
}

Zip& Zip::unlockLump(int lumpIdx)
{
    LOG_AS("Zip::unlockLump");
    LOG_TRACE("\"%s:%s\"") << de::NativePath(composePath()).pretty() << lump(lumpIdx).composePath();

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
    return readLump(lumpIdx, buffer, 0, lump(lumpIdx).size(), tryCache);
}

size_t Zip::readLump(int lumpIdx, uint8_t* buffer, size_t startOffset,
    size_t length, bool tryCache)
{
    LOG_AS("Zip::readLump");
    ZipFile const& file = reinterpret_cast<ZipFile&>(lump(lumpIdx));

    LOG_TRACE("\"%s:%s\" (%u bytes%s) [%lu +%lu]")
        << de::NativePath(composePath()).pretty()
        << de::NativePath(file.composePath()).pretty()
        << (unsigned long) file.size()
        << (file.isCompressed()? ", compressed" : "")
        << (unsigned long) startOffset
        << (unsigned long) length;

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache)
    {
        uint8_t const* data = d->lumpCache? d->lumpCache->data(lumpIdx) : 0;
        LOG_DEBUG("Cache %s on #%i") << (data? "hit" : "miss") << lumpIdx;
        if(data)
        {
            size_t readBytes = MIN_OF(file.size(), length);
            memcpy(buffer, data + startOffset, readBytes);
            return readBytes;
        }
    }

    size_t readBytes;
    if(!startOffset && length == file.size())
    {
        // Read it straight to the caller's data buffer.
        readBytes = d->bufferLump(file, buffer);
    }
    else
    {
        // Allocate a temporary buffer and read the whole lump into it(!).
        uint8_t* lumpData = (uint8_t*) M_Malloc(file.size());
        if(!lumpData) throw Error("Zip::readLumpSection", QString("Failed on allocation of %1 bytes for work buffer").arg(file.size()));

        if(d->bufferLump(file, lumpData))
        {
            readBytes = MIN_OF(file.size(), length);
            memcpy(buffer, lumpData + startOffset, readBytes);
        }
        else
        {
            readBytes = 0;
        }
        M_Free(lumpData);
    }

    /// @todo Do not check the read length here.
    if(readBytes < MIN_OF(file.size(), length))
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
 * @return  @c true= iff @a path was mapped to another location.
 *
 * @todo This is clearly implemented in the wrong place. Path mapping
 *       should be done at a higher level.
 */
static bool applyGamePathMappings(String& path)
{
    // Manually mapped to Defs?
    if(path.beginsWith('@'))
    {
        path.remove(0, 1);
        if(path.at(0) == '/') path.remove(0, 1);

        path = String("$(App.DefsPath)/$(GamePlugin.Name)/auto/") / path;
        return true;
    }

    // Manually mapped to Data?
    if(path.beginsWith('#'))
    {
        path.remove(0, 1);
        if(path.at(0) == '/') path.remove(0, 1);

        // Is there a prefix to be omitted in the name?
        if(int slash = path.lastIndexOf('/'))
        {
            // The slash must not be too early in the string.
            if(slash >= 2)
            {
                // Good old negative indices.
                if(path.at(slash - 2) == '.' && path.at(slash - 1) >= '1' && path.at(slash - 1) <= '9')
                    path.remove(slash - 2, 2);
            }
        }

        path = String("$(App.DataPath)/$(GamePlugin.Name)/auto/") / path;
        return true;
    }

    // Implicitly mapped to another location?
    if(!path.contains('/'))
    {
        // No directory separators; i.e., a root file.
        FileType const& ftype = F_GuessFileTypeFromFileName(path.fileName());

        switch(ftype.defaultClass())
        {
        case FC_PACKAGE:
            // Mapped to the Data directory.
            path = String("$(App.DataPath)/$(GamePlugin.Name)/auto/") / path;
            return true;

        case FC_DEFINITION:
            // Mapped to the Defs directory?
            path = String("$(App.DefsPath)/$(GamePlugin.Name)/auto/") / path;
            return true;

        default:
            return false;
        }
    }

    // Key-named directories in the root might be mapped to another location.
    FileNamespaces const& namespaces = F_FileNamespaces();
    DENG2_FOR_EACH_CONST(FileNamespaces, i, namespaces)
    {
        if((*i)->applyPathMappings(path))
        {
            return true;
        }
    }
    return false;
}

static String invalidIndexMessage(int invalidIdx, int lastValidIdx)
{
    String msg = String("Invalid lump index %1").arg(invalidIdx);
    if(lastValidIdx < 0) msg += " (file is empty)";
    else                 msg += String(", valid range: [0..%2)").arg(lastValidIdx);
    return msg;
}

} // namespace de
