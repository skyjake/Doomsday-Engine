/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ARCHIVEENTRYFILE_H
#define LIBCORE_ARCHIVEENTRYFILE_H

#include "de/bytearrayfile.h"

namespace de {

class Archive;

/**
 * Accesses data of an entry within an archive.
 *
 * @ingroup fs
 */
class DE_PUBLIC ArchiveEntryFile : public ByteArrayFile
{
public:
    /**
     * Constructs an archive file.
     *
     * @param name       Name of the file.
     * @param archive    Archive where the contents of the file are located.
     * @param entryPath  Path of the file's entry within the archive.
     */
    ArchiveEntryFile(const String &name, Archive &archive, const String &entryPath);

    ~ArchiveEntryFile() override;

    String describe() const override;
    String entryPath() const;

    void clear() override;

    /**
     * Flushes the entire archive that this file is part of into its source
     * file.
     *
     * Flushing may be needed when one needs to access the source file
     * containing the archive while the archive is still present in the tree as
     * Files and Folders.
     *
     * A flush is only done if the archive has been marked as changed. Without
     * manual flushing this occurs automatically when the root ArchiveFeed
     * instance is deleted.
     */
    void release() const override;

    Block metaId() const override;

    /// Returns the archive of the file.
    Archive &archive();

    /// Returns the archive of the file (non-modifiable).
    const Archive &archive() const;

    void uncache() const;

    // Implements IByteArray.
    Size size() const override;
    void get(Offset at, Byte *values, Size count) const override;

    /**
     * Modifies the content of an archive entry. Changes are made instantly
     * in the source archive, however nothing is written to the source file
     * containing the archive until a flush occurs.
     *
     * @param at      Offset where to write to.
     * @param values  Data.
     * @param count   Length of data.
     */
    void set(Offset at, const Byte *values, Size count) override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_ARCHIVEENTRYFILE_H */
