/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_DIRECTORYFEED_H
#define LIBDENG2_DIRECTORYFEED_H

#include <de/Feed>
#include <de/String>

namespace de
{
    /**
     * DirectoryFeed reads from and writes to directories in the native file system.
     *
     * @ingroup fs
     */
    class PUBLIC_API DirectoryFeed : public Feed
    {
    public:
        /// The native directory was not found. @ingroup errors
        DEFINE_ERROR(NotFoundError);
        
        /// An error occurred changing the working directory. @ingroup errors
        DEFINE_ERROR(WorkingDirError);
        
    public:
        /**
         * Constructs a DirectoryFeed that accesses a directory in the native file system.
         *
         * @param nativePath  Path of the native directory.
         */
        DirectoryFeed(const std::string& nativePath);
        
        virtual ~DirectoryFeed();
        
        void populate(Folder& folder);
        bool prune(File& file) const;

    public:
        /**
         * Changes the native working directory.
         *
         * @param nativePath  New path to use as the working directory.
         */
        static void changeWorkingDir(const std::string& nativePath);

    protected:
        void populateSubFolder(Folder& folder, const std::string& entryName);
        void populateFile(Folder& folder, const std::string& entryName);

    private:
        const String nativePath_;
    };
}

#endif /* LIBDENG2_DIRECTORYFEED_H */
