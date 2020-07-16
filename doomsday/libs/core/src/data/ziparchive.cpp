/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/ziparchive.h"
#include "de/block.h"
#include "de/bytearrayfile.h"
#include "de/bytesubarray.h"
#include "de/date.h"
#include "de/file.h"
#include "de/fixedbytearray.h"
#include "de/iserializable.h"
#include "de/byteorder.h"
#include "de/logbuffer.h"
#include "de/metadatabank.h"
#include "de/reader.h"
#include "de/writer.h"
#include "de/zeroed.h"

// Interpretations:
#include "de/archivefolder.h"

#include <cstring>
#include <zlib.h>

#ifdef max
#  undef max
#endif

namespace de {
namespace internal {

// Marker signatures.
#define SIG_LOCAL_FILE_HEADER   0x04034b50
#define SIG_CENTRAL_FILE_HEADER 0x02014b50
#define SIG_END_OF_CENTRAL_DIR  0x06054b50
#define SIG_DIGITAL_SIGNATURE   0x05054b50

// Maximum tolerated size of the comment.
#define MAXIMUM_COMMENT_SIZE    2048

// This is the length of the central directory end record (without the
// comment, but with the signature).
#define CENTRAL_END_SIZE        22

// Deflate minimum compression. Worse than this will be stored uncompressed.
#define REQUIRED_DEFLATE_PERCENTAGE .98

// File header flags.
#define ZFH_ENCRYPTED           0x1
#define ZFH_COMPRESSION_OPTS    0x6
#define ZFH_DESCRIPTOR          0x8
#define ZFH_COMPRESS_PATCHED    0x20    // Not supported.

// Compression methods.
enum Compression {
    NO_COMPRESSION = 0,     // Supported format.
    SHRUNK,
    REDUCED_1,
    REDUCED_2,
    REDUCED_3,
    REDUCED_4,
    IMPLODED,
    DEFLATED = 8,           // The only supported compression (via zlib).
    DEFLATED_64,
    PKWARE_DCL_IMPLODED
};

/**
 * MS-DOS time.
 * - 0..4:   Second/2
 * - 5..10:  Minute
 * - 11..15: Hour
 */
struct DOSTime {
    duint16 seconds;
    duint16 minutes;
    duint16 hours;

    DOSTime(duint16 h, duint16 m, duint16 s) : seconds(s), minutes(m), hours(h) {}
    DOSTime(const duint16 &i) {
        seconds = (i & 0x1f) * 2;
        minutes = (i >> 5) & 0x3f;
        hours = i >> 11;
    }
    operator duint16() const {
        return (hours << 11) | ((minutes & 0x3f) << 5) | ((seconds/2) & 0x1f);
    }
};

/**
 * MS-DOS date.
 * - 0..4:  Day of the month (1-31)
 * - 5..8:  Month (1=Jan)
 * - 9..15: Year since 1980
 */
struct DOSDate {
    duint16 dayOfMonth;
    duint16 month;
    duint16 year;

    DOSDate(duint16 y, duint16 m, duint16 d) : dayOfMonth(d), month(m), year(y) {}
    DOSDate(const duint16 &i) {
        dayOfMonth = i & 0x1f;
        month = (i >> 5) & 0xf;
        year = i >> 9;
    }
    operator duint16() const {
        return (year << 9) | ((month & 0xf) << 5) | (dayOfMonth & 0x1f);
    }
};

struct LocalFileHeader : public ISerializable {
    Uint32 signature;
    Uint16 requiredVersion;
    Uint16 flags;
    Uint16 compression;
    Uint16 lastModTime;
    Uint16 lastModDate;
    Uint32 crc32;
    Uint32 compressedSize;
    Uint32 size;
    Uint16 fileNameSize;
    Uint16 extraFieldSize;

    void operator >> (Writer &to) const override {
        to  << signature
            << requiredVersion
            << flags
            << compression
            << lastModTime
            << lastModDate
            << crc32
            << compressedSize
            << size
            << fileNameSize
            << extraFieldSize;
    }
    void operator << (Reader &from) override {
        from >> signature
             >> requiredVersion
             >> flags
             >> compression
             >> lastModTime
             >> lastModDate
             >> crc32
             >> compressedSize
             >> size
             >> fileNameSize
             >> extraFieldSize;
    }
};

struct CentralFileHeader : public ISerializable {
    Uint32 signature;
    Uint16 version;
    Uint16 requiredVersion;
    Uint16 flags;
    Uint16 compression;
    Uint16 lastModTime;
    Uint16 lastModDate;
    Uint32 crc32;
    Uint32 compressedSize;
    Uint32 size;
    Uint16 fileNameSize;
    Uint16 extraFieldSize;
    Uint16 commentSize;
    Uint16 diskStart;
    Uint16 internalAttrib;
    Uint32 externalAttrib;
    Uint32 relOffset;

    /*
     * Followed by:
     * file name (variable size)
     * extra field (variable size)
     * file comment (variable size)
     */

    void operator >> (Writer &to) const override {
        to  << signature
            << version
            << requiredVersion
            << flags
            << compression
            << lastModTime
            << lastModDate
            << crc32
            << compressedSize
            << size
            << fileNameSize
            << extraFieldSize
            << commentSize
            << diskStart
            << internalAttrib
            << externalAttrib
            << relOffset;
    }
    void operator << (Reader &from) override {
        from >> signature
             >> version
             >> requiredVersion
             >> flags
             >> compression
             >> lastModTime
             >> lastModDate
             >> crc32
             >> compressedSize
             >> size
             >> fileNameSize
             >> extraFieldSize
             >> commentSize
             >> diskStart
             >> internalAttrib
             >> externalAttrib
             >> relOffset;
    }
};

struct CentralEnd : public ISerializable {
    Uint16 disk;
    Uint16 centralStartDisk;
    Uint16 diskEntryCount;
    Uint16 totalEntryCount;
    Uint32 size;
    Uint32 offset;
    Uint16 commentSize;

    void operator >> (Writer &to) const override {
        to  << disk
            << centralStartDisk
            << diskEntryCount
            << totalEntryCount
            << size
            << offset
            << commentSize;
    }
    void operator << (Reader &from) override {
        from >> disk
             >> centralStartDisk
             >> diskEntryCount
             >> totalEntryCount
             >> size
             >> offset
             >> commentSize;
    }
};

} // namespace internal

using namespace internal;

DE_STATIC_STRING(ZIPARCHIVE_META_CATEGORY, "ZipArchive");

DE_PIMPL(ZipArchive)
{
    Block directoryCacheId;
    CentralEnd zipSummary;
    List<std::pair<Block, CentralFileHeader>> centralHeaders;

    Impl(Public *i) : Base(i) {}

    /**
     * Locates the central directory. Start from the earliest location where
     * the signature might be.
     *
     * @param reader  Source.
     */
    bool seekToCentralEnd(Reader &reader)
    {
        for (duint pos = CENTRAL_END_SIZE; pos < MAXIMUM_COMMENT_SIZE + CENTRAL_END_SIZE; pos++)
        {
            reader.setOffset(reader.source()->size() - pos);
            duint32 signature;
            reader >> signature;
            if (signature == SIG_END_OF_CENTRAL_DIR)
            {
                // This is it!
                return true;
            }
        }
        return false;
    }

    /**
     * Reads the central directory. THe reader must currently be at the start of the
     * CentralEnd block.
     *
     * @param reader      Source.
     * @param updateFromLocalHeaders  Read the local file headers, too.
     * @param overrideLocalHeaderStart  Offset to apply to all positions within the source data.
     *                    Used for pretending that cached metadata is at its original
     *                    offset instead of the beginning of the cache file.
     *                    Ignored if @a updateFromLocalHeaders is true.
     */
    void readCentralDirectory(Reader &reader, bool readingFromOriginal,
                              IByteArray::Offset localHeaderStartOffset = std::numeric_limits<dsize>::max())
    {
        reader >> zipSummary;

        const duint entryCount = zipSummary.totalEntryCount;

        // The ZIP must have only one part, all entries in the same archive.
        if (entryCount != zipSummary.diskEntryCount)
        {
            /// @throw MultiPartError  ZIP archives in more than one part are not supported
            /// by the implementation.
            throw MultiPartError("ZipArchive::readCentralDirectory", "Multipart archives are not supported");
        }

        // Read all the entries of the central directory.
        reader.setOffset(readingFromOriginal? IByteArray::Offset(zipSummary.offset)
                                            : localHeaderStartOffset);
        for (duint index = 0; index < entryCount; ++index)
        {
            CentralFileHeader header;
            reader >> header;

            // Check the signature.
            if (header.signature != SIG_CENTRAL_FILE_HEADER)
            {
                /// @throw FormatError  Invalid signature in a central directory entry.
                throw FormatError("ZipArchive::readCentralDirectory",
                                  stringf("Corrupt central directory (at file %u)", index));
            }

            Block latin1Name;
            reader.readBytes(header.fileNameSize, latin1Name);
            const String fileName = String::fromLatin1(latin1Name); /// @todo UTF-8?

            // Advance the cursor past the variable sized fields.
            reader.seek(header.extraFieldSize + header.commentSize);

            ZipEntry *entry = nullptr;

            // Skip folders.
            if (!fileName.endsWith("/") || header.size)
            {
                // Check for unsupported features.
                if (header.compression != NO_COMPRESSION && header.compression != DEFLATED)
                {
                    /// @throw UnknownCompressionError  Deflation is the only compression
                    /// algorithm supported by the implementation.
                    throw UnknownCompressionError("ZipArchive::readCentralDirectory",
                                                  "Entry '" + fileName + "' uses an unsupported compression algorithm");
                }
                if (header.flags & ZFH_ENCRYPTED)
                {
                    /// @throw EncryptionError  Archive is encrypted, which is not supported
                    /// by the implementation.
                    throw EncryptionError("ZipArchive::readCentralDirectory",
                                          "Entry '" + fileName + "' is encrypted and thus cannot be read");
                }

                // Make an index entry for this.
                entry = static_cast<ZipEntry *>(&self().insertEntry(fileName));

                entry->size              = header.size;
                entry->sizeInArchive     = header.compressedSize;
                entry->compression       = header.compression;
                entry->crc32             = header.crc32;
                entry->localHeaderOffset = header.relOffset;

                // Unpack the last modified time from the ZIP entry header.
                DOSDate lastModDate(header.lastModDate);
                DOSTime lastModTime(header.lastModTime);
                entry->modifiedAt = Time(lastModDate.year + 1980, lastModDate.month, lastModDate.dayOfMonth,
                                         lastModTime.hours, lastModTime.minutes, lastModTime.seconds);
                                             
                if (readingFromOriginal)
                {
                    LocalFileHeader localHeader;

                    // Check where the entry's data is located by reading the local header.
                    reader.mark();
                    reader.setOffset(header.relOffset);
                    reader >> localHeader;

                    entry->offset = reader.offset() +
                                    localHeader.fileNameSize +
                                    localHeader.extraFieldSize;

                    // Back to the central directory.
                    reader.rewind();
                }
                else
                {
                    // Cached headers' entries point directly to the data.
                    entry->offset = header.relOffset;
                }
            }

            if (readingFromOriginal)
            {
                // We'll keep the central header for caching. However, the cached
                // directory never includes additional fields.
                header.extraFieldSize = 0;
                header.commentSize = 0;

                // Cache's relOffset is actually the entry data starting offset.
                header.relOffset = (entry? duint32(entry->offset) : 0);
                centralHeaders << std::make_pair(latin1Name, header);
            }
        }
    }

    /**
     * Write a copy of the unmodified central directory as it was read from the source
     * archive. This is saved in the metadata cache.
     *
     * @param writer  Destination.
     */
    void writeCachedCentralDirectory(Writer &writer)
    {
        for (const auto &name_header : centralHeaders)
        {
            writer << name_header.second
                   << FixedByteArray(name_header.first);
        }
        writer << duint32(SIG_END_OF_CENTRAL_DIR) << zipSummary;
    }

    /**
     * Writes a new central directory for a new ZIP archive as it will be written by
     * ZipArchive.
     *
     * @param writer  Destination.
     */
    void writeCentralDirectory(Writer &writer)
    {
        CentralEnd summary;
        summary.diskEntryCount = summary.totalEntryCount = self().index().leafNodes().size();

        // This is where the central directory begins.
        summary.offset = writer.offset();

        // Write the central directory.
        for (PathTreeIterator<Index> iter(self().index().leafNodes()); iter.hasNext(); )
        {
            const ZipEntry &entry = iter.next();
            const String fullPath = entry.path();

            CentralFileHeader header;
            header.signature = SIG_CENTRAL_FILE_HEADER;
            header.version = 20;
            header.requiredVersion = 20;
            Date at(entry.modifiedAt);
            header.lastModTime = DOSTime(at.hours(), at.minutes(), at.seconds());
            header.lastModDate = DOSDate(at.year() - 1980, at.month(), at.dayOfMonth());
            header.compression = entry.compression;
            header.crc32 = entry.crc32;
            header.compressedSize = entry.sizeInArchive;
            header.size = entry.size;
            header.fileNameSize = fullPath.size();
            header.relOffset = entry.localHeaderOffset;

            writer << header << FixedByteArray(fullPath.toLatin1());
        }

        // Size of the central directory.
        summary.size = writer.offset() - summary.offset;

        // End of central directory.
        writer << duint32(SIG_END_OF_CENTRAL_DIR) << summary;

        writer << duint32(SIG_DIGITAL_SIGNATURE)
               << duint16(0); // No signature data.
    }

    void updateCachedDirectory()
    {
        if (directoryCacheId)
        {
            Block meta;
            Writer writer(meta);
            writeCachedCentralDirectory(writer);
            MetadataBank::get().setMetadata(ZIPARCHIVE_META_CATEGORY(), directoryCacheId, meta);
        }
    }

    bool restoreFromCache()
    {
        if (!directoryCacheId) return false;

        auto &bank = MetadataBank::get();

        try
        {
            if (const Block meta = bank.check(ZIPARCHIVE_META_CATEGORY(), directoryCacheId))
            {
                Reader reader(meta);
                if (!seekToCentralEnd(reader))
                {
                    return false;
                }
                readCentralDirectory(reader, false, 0);
                return true;
            }
        }
        catch (const Error &er)
        {
            LOGDEV_RES_WARNING("Corrupt cached metadata: %s") << er.asText();
        }
        return false;
    }
};

ZipArchive::ZipArchive() : d(new Impl(this))
{
    setIndex(new Index);
}

ZipArchive::ZipArchive(const IByteArray &archive, const Block &dirCacheId)
    : Archive(archive)
    , d(new Impl(this))
{
    setIndex(new Index);

    d->directoryCacheId = dirCacheId;

    if (d->restoreFromCache())
    {
        // No need to check the file itself.
        return;
    }

    Reader reader(archive, littleEndianByteOrder);
    if (!d->seekToCentralEnd(reader))
    {
        /// @throw MissingCentralDirectoryError The ZIP central directory was not found
        /// in the end of the source data.
        throw MissingCentralDirectoryError("ZipArchive::Archive",
            "Could not locate the central directory of the archive");
    }
    d->readCentralDirectory(reader, true);
    d->updateCachedDirectory();

    // No need to keep these any more.
    d->centralHeaders.clear();
}

void ZipArchive::readFromSource(const Entry &e, const Path &, IBlock &uncompressedData) const
{
    const ZipEntry &entry = static_cast<const ZipEntry &>(e);

    if (entry.compression == NO_COMPRESSION)
    {
        // Data is not compressed so we can just read it.
        if (entry.dataInArchive)
        {
            uncompressedData.copyFrom(*entry.dataInArchive, 0, entry.size);
        }
        else
        {
            DE_ASSERT(source() != NULL);
            uncompressedData.copyFrom(*source(), entry.offset, entry.size);
        }
    }
    else // Data is compressed.
    {
        // Prepare the output buffer for the decompressed data.
        uncompressedData.resize(entry.size);

        // Take a copy of the compressed data for zlib.
        if (!entry.dataInArchive)
        {
            DE_ASSERT(source() != NULL);
            entry.dataInArchive.reset(new Block(*source(), entry.offset, entry.sizeInArchive));
        }

        z_stream stream;
        zap(stream);
        stream.next_in = const_cast<IByteArray::Byte *>(entry.dataInArchive->data());
        stream.avail_in = entry.sizeInArchive;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.next_out = const_cast<IByteArray::Byte *>(uncompressedData.data());
        stream.avail_out = entry.size;

        /*
         * Set up a raw inflate with a window of -15 bits.
         *
         * From zlib documentation:
         *
         * "windowBits can also be –8..–15 for raw inflate. In this case,
         * -windowBits determines the window size. inflate() will then process
         * raw deflate data, not looking for a zlib or gzip header, not
         * generating a check value, and not looking for any check values for
         * comparison at the end of the stream. This is for use with other
         * formats that use the deflate compressed data format such as 'zip'."
         */
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        {
            /// @throw InflateError Problem with zlib: inflateInit2 failed.
            throw InflateError("ZipArchive::readEntry",
                               "Inflation failed because initialization failed");
        }

        // Do the inflation in one call.
        dint result = inflate(&stream, Z_FINISH);

        if (stream.total_out != entry.size)
        {
            /// @throw InflateError The actual decompressed size is not equal to the
            /// size listed in the central directory.
            throw InflateError("ZipArchive::readEntry",
                               "Failure due to " +
                               String((result == Z_DATA_ERROR ? "corrupt data in archive"
                                                              : "zlib error")) + ": " + stream.msg);
        }

        // We're done.
        inflateEnd(&stream);
        entry.dataInArchive.reset(); // Now have the decompressed version.
    }
}

const ZipArchive::Index &ZipArchive::index() const
{
    return static_cast<const Index &>(Archive::index());
}

void ZipArchive::operator >> (Writer &to) const
{
    /**
     * ZIP archives will use little-endian byte order regardless of the byte
     * order employed by the supplied Writer @a to.
     */
    Writer writer(to, littleEndianByteOrder);

    // First write the local headers and entry contents.
    for (PathTreeIterator<Index> iter(index().leafNodes()); iter.hasNext(); )
    {
        // We will be updating relevant members of the entry.
        ZipEntry &entry = iter.next();
        entry.update();

        const String fullPath = entry.path();

        // This is where the local file header is located.
        entry.localHeaderOffset = writer.offset();

        LocalFileHeader header;
        header.signature = SIG_LOCAL_FILE_HEADER;
        header.requiredVersion = 20;
        header.compression = entry.compression;
        Date at(entry.modifiedAt);
        header.lastModTime = DOSTime(at.hours(), at.minutes(), at.seconds());
        header.lastModDate = DOSDate(at.year() - 1980, at.month(), at.dayOfMonth());
        header.crc32 = entry.crc32;
        header.compressedSize = entry.sizeInArchive;
        header.size = entry.size;
        header.fileNameSize = fullPath.size();

        // Can we use the data already in the source archive?
        if ((entry.dataInArchive || source()) && !entry.maybeChanged)
        {
            // Yes, we can.
            writer << header << FixedByteArray(fullPath.toLatin1());
            IByteArray::Offset newOffset = writer.offset();
            if (entry.dataInArchive)
            {
                writer << FixedByteArray(*entry.dataInArchive);
            }
            else
            {
                // Re-use the data in the source.
                writer << FixedByteArray(*source(), entry.offset, entry.sizeInArchive);
            }
            // Written to new location.
            entry.offset = newOffset;
        }
        else
        {
            DE_ASSERT(entry.data != NULL);

            // Let's try and compress.
            Block archived(Block::Size(REQUIRED_DEFLATE_PERCENTAGE * entry.data->size()));

            z_stream stream;
            zap(stream);
            stream.next_in = const_cast<IByteArray::Byte *>(entry.data->data());
            stream.avail_in = entry.data->size();
            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.next_out = const_cast<IByteArray::Byte *>(archived.data());
            stream.avail_out = archived.size();

            /*
             * The deflation is done in raw mode. From zlib documentation:
             *
             * "windowBits can also be –8..–15 for raw deflate. In this case,
             * -windowBits determines the window size. deflate() will then
             * generate raw deflate data with no zlib header or trailer, and
             * will not compute an adler32 check value."
             */
            if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                            -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            {
                /// @throw DeflateError  zlib error: could not initialize deflate operation.
                throw DeflateError("ZipArchive::operator >>", "Deflate init failed");
            }

            int result = deflate(&stream, Z_FINISH);
            if (result == Z_STREAM_END)
            {
                // Compression was ok.
                header.compression = entry.compression = DEFLATED;
                header.compressedSize = entry.sizeInArchive = stream.total_out;
                writer << header << FixedByteArray(fullPath.toLatin1());
                entry.offset = writer.offset();
                writer << FixedByteArray(archived, 0, entry.sizeInArchive);
            }
            else
            {
                // We won't compress.
                header.compression = entry.compression = NO_COMPRESSION;
                header.compressedSize = entry.sizeInArchive = entry.data->size();
                writer << header << FixedByteArray(fullPath.toLatin1());
                entry.offset = writer.offset();
                writer << FixedByteArray(*entry.data);
            }

            // Clean up.
            deflateEnd(&stream);
        }
    }

    d->writeCentralDirectory(writer);

    // Since we used our own writer, seek the writer that was given to us
    // by the amount of data we wrote.
    to.seek(writer.offset());
}

static bool recognizeZipExtension(const String &ext)
{    
    for (const char *e : {".pack", ".demo", ".save", ".addon", ".pk3", ".zip"})
    {
        if (!iCmpStr(ext, e)) return true;
    }
    return false;
}

bool ZipArchive::recognize(const File &file)
{
    if (file.status().type() == File::Type::File)
    {
        // For now, just check the name.
        return recognizeZipExtension(file.extension());
    }
    return false;
}

bool ZipArchive::recognize(const NativePath &path)
{
    return recognizeZipExtension(path.toString().fileNameExtension().lower());
}

void ZipArchive::ZipEntry::update()
{
    if (data)
    {
        size  = data->size();
        crc32 = ::crc32(0L, data->data(), data->size());
    }
}

File *ZipArchive::Interpreter::interpretFile(File *sourceData) const
{
    if (recognize(*sourceData))
    {
        try
        {
            // It is a ZIP archive: we will represent it as a folder.
            LOG_RES_XVERBOSE("Interpreted %s as a ZIP format archive", sourceData->description());

            std::unique_ptr<ArchiveFolder> package(new ArchiveFolder(*sourceData, sourceData->name()));

            // Archive opened successfully, give ownership of the source to the folder.
            package->setSource(sourceData);
            return package.release();
        }
        catch (const Archive::FormatError &)
        {
            // Even though it was recognized as an archive, the file
            // contents may still prove to be corrupted.
            LOG_RES_WARNING("Archive in %s is invalid") << sourceData->description();
        }
        catch (const IByteArray::OffsetError &)
        {
            LOG_RES_WARNING("Archive in %s is truncated") << sourceData->description();
        }
        catch (const IIStream::InputError &er)
        {
            LOG_RES_WARNING("Failed to read %s") << sourceData->description();
            LOGDEV_RES_WARNING("%s") << er.asText();
        }
    }
    return nullptr;
}

} // namespace de
