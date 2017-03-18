/** @file archivefeed.h Archive Feed.
 *
 * @author Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ARCHIVEFEED_H
#define LIBDENG2_ARCHIVEFEED_H

#include "../Feed"
#include "../ByteArrayFile"
#include "../String"

namespace de {

class Archive;

/**
 * Produces files and folders that represent the contents of an Archive.
 *
 * ArchiveFeed gives populated files and folders write access mode if the
 * source file has write access when the ArchiveFeed is created. Note that
 * changing the source file's write access should not be done while the archive
 * is populated in the file system (populated files would retain the Write mode
 * and flushing the data would cause an exception).
 *
 * @ingroup fs
 */
class DENG2_PUBLIC ArchiveFeed : public Feed
{
public:
    /// Provided source file cannot be used as a feed source. @ingroup errors
    DENG2_ERROR(InvalidSourceError);

public:
    /**
     * Constructs a new archive feed.
     *
     * @param archiveFile  File where the data comes from.
     */
    ArchiveFeed(File &archiveFile);

    /**
     * Constructs an archive feed that populates the contents of a folder
     * in another feed's archive.
     *
     * @param parentFeed  Feed whose archive will be used.
     * @param basePath    Path within the archive for the new feed.
     */
    ArchiveFeed(ArchiveFeed &parentFeed, String const &basePath);

    virtual ~ArchiveFeed();

    String description() const override;

    PopulatedFiles populate(Folder const &folder) override;
    bool prune(File &file) const override;
    File *createFile(String const &name) override;
    void destroyFile(String const &name) override;
    Feed *newSubFeed(String const &name) override;

    /**
     * Returns the archive that the feed accesses.
     */
    Archive &archive();

    /**
     * Returns the archive that the feed accesses.
     */
    Archive const &archive() const;

    /**
     * Returns the base path within the archive.
     */
    String const &basePath() const;

    /**
     * Returns the file where the archive is stored (in a serialized format).
     */
    File const &archiveSourceFile() const;

    /**
     * Serializes the contents of the in-memory Archive to the File associated with
     * this feed. The entire source file is overwritten. If the Archive has not been
     * modified since the feed was created, nothing is done.
     */
    void rewriteFile();

    void uncache();

    /**
     * Uncaches all unmodified indexed archive entries from memory. Doing this at
     * specific suitable points in the application's lifetime is good so that unnecessary
     * data is not kept cached.
     */
    static void uncacheAllEntries(StringList folderTypes);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif /* LIBDENG2_ARCHIVEFEED_H */
