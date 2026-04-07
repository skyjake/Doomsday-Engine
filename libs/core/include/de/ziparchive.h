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

#ifndef LIBCORE_ZIPARCHIVE_H
#define LIBCORE_ZIPARCHIVE_H

#include "de/archive.h"
#include "de/nativepath.h"
#include "de/filesys/iinterpreter.h"

namespace de {

/**
 * Archive whose serialization uses the ZIP file format.
 * @ingroup data
 *
 * All the features of the ZIP format are not supported:
 * - Deflate is the only supported compression method.
 * - Multipart ZIP files are not supported.
 *
 * @see http://en.wikipedia.org/wiki/Zip_(file_format)
 */
class DE_PUBLIC ZipArchive : public Archive
{
public:
    /// The central directory of the ZIP archive cannot be located. Maybe it's not
    /// a ZIP archive after all? @ingroup errors
    DE_SUB_ERROR(FormatError, MissingCentralDirectoryError);

    /// The source archive belongs to a multipart archive. @ingroup errors
    DE_SUB_ERROR(FormatError, MultiPartError);

    /// An entry in the archive uses a compression algorithm not supported by the
    /// implementation. @ingroup errors
    DE_SUB_ERROR(FormatError, UnknownCompressionError);

    /// An entry is encrypted. Decrypting is not supported. @ingroup errors
    DE_SUB_ERROR(FormatError, EncryptionError);

    /// There is an error during decompression. @ingroup errors
    DE_SUB_ERROR(ContentError, InflateError);

    /// There is an error during compression. @ingroup errors
    DE_SUB_ERROR(ContentError, DeflateError);

public:
    /**
     * Constructs an empty ZIP archive.
     */
    ZipArchive();

    /**
     * Constructs a new ZIP archive instance. The content index contained
     * in @a data is read during construction.
     *
     * @param data  Data of the source archive. No copy of the
     *              data is made, so the caller must make sure the
     *              byte array remains in existence for the lifetime
     *              of the Archive instance.
     * @param dirCacheId  ID of cached ZIP directory data.
     */
    ZipArchive(const IByteArray &data, const Block &dirCacheId = Block());

    void operator >> (Writer &to) const;

public:
    /**
     * Determines whether a File looks like it could be accessed using ZipArchive.
     *
     * @param file  File to check.
     *
     * @return @c true, if the file looks like an archive.
     */
    static bool recognize(const File &file);

    /**
     * Determines whether a native file looks like it could be in ZIP format.
     *
     * @param path  Native path of the file to check.
     *
     * @return @c true, if the file looks like an archive.
     */
    static bool recognize(const NativePath &path);

    struct DE_PUBLIC Interpreter : public filesys::IInterpreter {
        File *interpretFile(File *sourceData) const override;
    };

protected:
    void readFromSource(const Entry &entry, const Path &path, IBlock &uncompressedData) const;

    struct ZipEntry : public Entry
    {
        duint16 compression;        ///< Type of compression employed by the entry.
        duint32 crc32;              ///< CRC32 checksum.
        dsize localHeaderOffset;    ///< Offset of the local file header.

        ZipEntry(const PathTree::NodeArgs &args) : Entry(args),
              compression(0), crc32(0), localHeaderOffset(0) {}

        /// Recalculates CRC32 of the entry.
        void update();
    };

    typedef PathTreeT<ZipEntry> Index;

    const Index &index() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_ZIPARCHIVE_H
