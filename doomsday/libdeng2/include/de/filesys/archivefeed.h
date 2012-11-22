/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ARCHIVEFEED_H
#define LIBDENG2_ARCHIVEFEED_H

#include "../Feed"
#include "../ByteArrayFile"
#include "../String"

namespace de
{
    class Archive;
    
    /**
     * Produces files and folders that represent the contents of an Archive.
     *
     * @ingroup fs
     */
    class ArchiveFeed : public Feed
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
        ArchiveFeed(File& archiveFile);
        
        /**
         * Constructs an archive feed that populates the contents of a folder
         * in another feed's archive.
         *
         * @param parentFeed  Feed whose archive will be used.
         * @param basePath    Path within the archive for the new feed.
         */
        ArchiveFeed(ArchiveFeed& parentFeed, const String& basePath);

        virtual ~ArchiveFeed();

        void populate(Folder& folder);
        bool prune(File& file) const;
        File* newFile(const String& name);
        void removeFile(const String& name);

        /**
         * Returns the archive that the feed accesses.
         */
        Archive& archive();
                    
        /**
         * Returns the base path within the archive.
         */
        const String& basePath() const;
                    
    private:
        struct Instance;
        Instance* d;
    };
}

#endif /* LIBDENG2_ARCHIVEFEED_H */
