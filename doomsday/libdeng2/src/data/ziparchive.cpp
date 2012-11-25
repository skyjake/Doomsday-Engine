/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/ZipArchive"
#include "de/ISerializable"
#include "de/ByteSubArray"
#include "de/FixedByteArray"
#include "de/ByteArrayFile"
#include "de/Reader"
#include "de/Writer"
#include "de/LittleEndianByteOrder"
#include "de/Block"
#include "de/File"
#include "de/Date"
#include "de/Zeroed"

#include <cstring>
#include <zlib.h>

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
    DOSTime(duint16 const &i) {
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
    DOSDate(duint16 const &i) {
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

    void operator >> (Writer &to) const {
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
    void operator << (Reader &from) {
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

    void operator >> (Writer &to) const {
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
    void operator << (Reader &from) {
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

    void operator >> (Writer &to) const {
        to  << disk
            << centralStartDisk
            << diskEntryCount
            << totalEntryCount
            << size
            << offset
            << commentSize;
    }
    void operator << (Reader &from) {
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

ZipArchive::ZipArchive() : Archive()
{}

ZipArchive::ZipArchive(IByteArray const &archive) : Archive(archive)
{
    Reader reader(archive, littleEndianByteOrder);

    // Locate the central directory. Start from the earliest location where
    // the signature might be.
    duint centralEndPos = 0;
    for(duint pos = CENTRAL_END_SIZE; pos < MAXIMUM_COMMENT_SIZE; pos++)
    {
        reader.setOffset(archive.size() - pos);
        duint32 signature;
        reader >> signature;
        if(signature == SIG_END_OF_CENTRAL_DIR)
        {
            // This is it!
            centralEndPos = archive.size() - pos;
            break;
        }
    }
    if(!centralEndPos)
    {
        /// @throw MissingCentralDirectoryError The ZIP central directory was not found
        /// in the end of the source data.
        throw MissingCentralDirectoryError("ZipArchive::Archive",
            "Could not locate the central directory of the archive");
    }

    // The central directory end record is at the signature we found.
    CentralEnd summary;
    reader >> summary;

    duint const entryCount = summary.totalEntryCount;

    // The ZIP must have only one part, all entries in the same archive.
    if(entryCount != summary.diskEntryCount)
    {
        /// @throw MultiPartError  ZIP archives in more than one part are not supported
        /// by the implementation.
        throw MultiPartError("ZipArchive::Archive", "Multipart archives are not supported");
    }

    // Read all the entries of the central directory.
    reader.setOffset(summary.offset);
    for(duint index = 0; index < entryCount; ++index)
    {
        CentralFileHeader header;
        reader >> header;

        // Check the signature.
        if(header.signature != SIG_CENTRAL_FILE_HEADER)
        {
            /// @throw FormatError  Invalid signature in a central directory entry.
            throw FormatError("ZipArchive::Archive", "Corrupt central directory");
        }

        String fileName = String::fromLatin1(ByteSubArray(archive, reader.offset(), header.fileNameSize));

        // Advance the cursor past the variable sized fields.
        reader.seek(header.fileNameSize + header.extraFieldSize + header.commentSize);

        // Skip folders.
        if(fileName.endsWith("/") && !header.size)
        {
            continue;
        }

        // Check for unsupported features.
        if(header.compression != NO_COMPRESSION && header.compression != DEFLATED)
        {
            /// @throw UnknownCompressionError  Deflation is the only compression
            /// algorithm supported by the implementation.
            throw UnknownCompressionError("ZipArchive::Archive",
                "Entry '" + fileName + "' uses an unsupported compression algorithm");
        }
        if(header.flags & ZFH_ENCRYPTED)
        {
            /// @throw EncryptionError  Archive is encrypted, which is not supported
            /// by the implementation.
            throw EncryptionError("ZipArchive::Archive",
                "Entry '" + fileName + "' is encrypted and thus cannot be read");
        }

        // Make an index entry for this.
        QScopedPointer<ZipEntry> entry(new ZipEntry);
        entry->size              = header.size;
        entry->sizeInArchive     = header.compressedSize;
        entry->compression       = header.compression;
        entry->crc32             = header.crc32;
        entry->localHeaderOffset = header.relOffset;

        // Unpack the last modified time from the ZIP entry header.
        DOSDate lastModDate(header.lastModDate);
        DOSTime lastModTime(header.lastModTime);
        entry->modifiedAt = QDateTime(QDate(lastModDate.year + 1980, lastModDate.month, lastModDate.dayOfMonth),
                                      QTime(lastModTime.hours, lastModTime.minutes, lastModTime.seconds));

        // Read the local file header, which contains the correct extra
        // field size (Info-ZIP!).
        reader.mark();
        reader.setOffset(header.relOffset);

        LocalFileHeader localHeader;
        reader >> localHeader;

        entry->offset = reader.offset() + header.fileNameSize + localHeader.extraFieldSize;

        insertToIndex(fileName, entry.take());

        // Back to the central directory.
        reader.rewind();
    }
}

ZipArchive::~ZipArchive()
{}

void ZipArchive::readFromSource(Entry const *e, String const &, IBlock &uncompressedData) const
{
    ZipEntry const &entry = *static_cast<ZipEntry const *>(e);

    if(entry.compression == NO_COMPRESSION)
    {
        // Data is not compressed so we can just read it.
        if(entry.dataInArchive)
        {
            uncompressedData.copyFrom(*entry.dataInArchive, 0, entry.size);
        }
        else
        {
            DENG2_ASSERT(source() != NULL);
            uncompressedData.copyFrom(*source(), entry.offset, entry.size);
        }
    }
    else // Data is compressed.
    {
        // Prepare the output buffer for the decompressed data.
        uncompressedData.resize(entry.size);

        // Take a copy of the compressed data for zlib.
        if(!entry.dataInArchive)
        {
            DENG2_ASSERT(source() != NULL);
            entry.dataInArchive = new Block(*source(), entry.offset, entry.sizeInArchive);
        }

        z_stream stream;
        std::memset(&stream, 0, sizeof(stream));
        stream.next_in = const_cast<IByteArray::Byte *>(entry.dataInArchive->data());
        stream.avail_in = entry.sizeInArchive;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.next_out = const_cast<IByteArray::Byte *>(uncompressedData.data());
        stream.avail_out = entry.size;

        if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        {
            /// @throw InflateError Problem with zlib: inflateInit2 failed.
            throw InflateError("ZipArchive::readEntry",
                "Inflation failed because initialization failed");
        }

        // Do the inflation in one call.
        dint result = inflate(&stream, Z_FINISH);

        if(stream.total_out != entry.size)
        {
            /// @throw InflateError The actual decompressed size is not equal to the
            /// size listed in the central directory.
            throw InflateError("ZipArchive::readEntry",
                "Failure due to " +
                String((result == Z_DATA_ERROR ? "corrupt data in archive" :
                "zlib error")) + ": " + stream.msg);
        }

        // We're done.
        inflateEnd(&stream);
    }
}

void ZipArchive::operator >> (Writer &to) const
{
    /**
     * ZIP archives will use little-endian byte order regardless of the byte
     * order employed by the supplied Writer @a to.
     */
    Writer writer(to, littleEndianByteOrder);

    // First write the local headers.
    DENG2_FOR_EACH_CONST(Index, i, index())
    {
        // We will be updating relevant members of the entry.
        ZipEntry &entry = *static_cast<ZipEntry *>(const_cast<Entry *>(i.value()));
        entry.update();

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
        header.fileNameSize = i.key().size();

        // Can we use the data already in the source archive?
        if((entry.dataInArchive || source()) && !entry.maybeChanged)
        {
            // Yes, we can.
            writer << header << FixedByteArray(i.key().toLatin1());
            IByteArray::Offset newOffset = writer.offset();
            if(entry.dataInArchive)
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
            DENG2_ASSERT(entry.data != NULL);

            // Let's try and compress.
            Block archived(Block::Size(REQUIRED_DEFLATE_PERCENTAGE * entry.data->size()));

            z_stream stream;
            std::memset(&stream, 0, sizeof(stream));
            stream.next_in = const_cast<IByteArray::Byte *>(entry.data->data());
            stream.avail_in = entry.data->size();
            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.next_out = const_cast<IByteArray::Byte *>(archived.data());
            stream.avail_out = archived.size();

            if(deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                            -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            {
                /// @throw DeflateError  zlib error: could not initialize deflate operation.
                throw DeflateError("ZipArchive::operator >>", "Deflate init failed");
            }

            deflate(&stream, Z_FINISH);
            int result = deflateEnd(&stream);
            if(result == Z_OK)
            {
                // Compression was ok.
                header.compression = entry.compression = DEFLATED;
                header.compressedSize = entry.sizeInArchive = stream.total_out;
                writer << header << FixedByteArray(i.key().toLatin1());
                entry.offset = writer.offset();
                writer << FixedByteArray(archived, 0, entry.sizeInArchive);
            }
            else
            {
                // We won't compress.
                header.compression = entry.compression = NO_COMPRESSION;
                header.compressedSize = entry.sizeInArchive = entry.data->size();
                writer << header << FixedByteArray(i.key().toLatin1());
                entry.offset = writer.offset();
                writer << FixedByteArray(*entry.data);
            }
        }
    }

    CentralEnd summary;
    summary.diskEntryCount = summary.totalEntryCount = index().size();

    // This is where the central directory begins.
    summary.offset = writer.offset();

    // Write the central directory.
    DENG2_FOR_EACH_CONST(Index, i, index())
    {
        ZipEntry const &entry = *static_cast<ZipEntry const *>(i.value());

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
        header.fileNameSize = i.key().size();
        header.relOffset = entry.localHeaderOffset;

        writer << header << FixedByteArray(i.key().toLatin1());
    }

    // Size of the central directory.
    summary.size = writer.offset() - summary.offset;

    // End of central directory.
    writer << duint32(SIG_END_OF_CENTRAL_DIR) << summary;

    // No signature data.
    writer << duint32(SIG_DIGITAL_SIGNATURE) << duint16(0);

    // Since we used our own writer, seek the writer that was given to us
    // by the amount of data we wrote.
    to.seek(writer.offset());
}

bool ZipArchive::recognize(File const &file)
{
    // For now, just check the name.
    String ext = file.name().fileNameExtension().lower();
    return (ext == ".pack" || ext == ".demo" || ext == ".save" || ext == ".addon" ||
            ext == ".box" || ext == ".pk3" || ext == ".zip");
}

Archive::Entry *ZipArchive::newEntry()
{
    ZipEntry *entry = new ZipEntry;
    entry->compression = NO_COMPRESSION; // Will be updated.
    return entry;
}

void ZipArchive::ZipEntry::update()
{
    if(data)
    {
        size = data->size();
        crc32 = ::crc32(0L, data->data(), data->size());
    }
}

} // namespace de
