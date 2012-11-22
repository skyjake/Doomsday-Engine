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

#ifndef LIBDENG2_ZIPARCHIVE_H
#define LIBDENG2_ZIPARCHIVE_H

#include "../Archive"

namespace de
{
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
    class DENG2_PUBLIC ZipArchive : public Archive
    {
    public:
        /// The central directory of the ZIP archive cannot be located. Maybe it's not
        /// a ZIP archive after all? @ingroup errors
        DENG2_SUB_ERROR(FormatError, MissingCentralDirectoryError);

        /// The source archive belongs to a multipart archive. @ingroup errors
        DENG2_SUB_ERROR(FormatError, MultiPartError);

        /// An entry in the archive uses a compression algorithm not supported by the
        /// implementation. @ingroup errors
        DENG2_SUB_ERROR(FormatError, UnknownCompressionError);

        /// An entry is encrypted. Decrypting is not supported. @ingroup errors
        DENG2_SUB_ERROR(FormatError, EncryptionError);

        /// There is an error during decompression. @ingroup errors
        DENG2_SUB_ERROR(ContentError, InflateError);

        /// There is an error during compression. @ingroup errors
        DENG2_SUB_ERROR(ContentError, DeflateError);

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
         */
        ZipArchive(const IByteArray& data);

        virtual ~ZipArchive();

        void operator >> (Writer& to) const;

    public:
        /**
         * Determines whether a File looks like it could be accessed using
         * Archive.
         *
         * @param file  File to check.
         *
         * @return @c true, if the file looks like an archive.
         */
        static bool recognize(const File& file);

    protected:
        Entry* newEntry();

        void readFromSource(const Entry* entry, const String& path, IBlock& uncompressedData) const;

        struct ZipEntry : public Entry
        {
            duint16 compression;        ///< Type of compression employed by the entry.
            duint32 crc32;              ///< CRC32 checksum.
            dsize localHeaderOffset;    ///< Offset of the local file header.

            ZipEntry() : Entry(), compression(0), crc32(0), localHeaderOffset(0)
            {}

            /// Recalculates CRC32 of the entry.
            void update();
        };
    };
}

#endif // LIBDENG2_ZIPARCHIVE_H
