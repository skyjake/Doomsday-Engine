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

#ifndef LIBDENG2_DIRECTORYFEED_H
#define LIBDENG2_DIRECTORYFEED_H

#include "../File"
#include "../Feed"
#include "../NativePath"

#include <QFlags>

namespace de
{
    /**
     * Reads from and writes to directories in the native file system.
     *
     * @ingroup fs
     */
    class DENG2_PUBLIC DirectoryFeed : public Feed
    {
    public:
        /// The native directory was not found. @ingroup errors
        DENG2_ERROR(NotFoundError);
        
        /// Failed attempt to find out the status of a file. @ingroup errors
        DENG2_ERROR(StatusError);
        
        /// An error occurred changing the working directory. @ingroup errors
        DENG2_ERROR(WorkingDirError);
        
        /// Creating a directory failed. @ingroup errors
        DENG2_ERROR(CreateDirError);
                
        enum Flag
        {
            /// Opens all files and folders in write mode.
            AllowWrite = 0x1,

            /// Creates the native directory if not does not exist.
            CreateIfMissing = 0x2
        };
        Q_DECLARE_FLAGS(Flags, Flag)
        
    public:
        /**
         * Constructs a DirectoryFeed that accesses a directory in the native file system.
         *
         * @param nativePath  Path of the native directory.
         * @param mode        Feed mode.
         */
        DirectoryFeed(NativePath const &nativePath, Flags const &mode = 0);
        
        virtual ~DirectoryFeed();
        
        String description() const;

        void populate(Folder &folder);
        bool prune(File &file) const;
        File *newFile(String const &name);
        void removeFile(String const &name);

    public:
        /**
         * Changes the native working directory.
         *
         * @param nativePath  New path to use as the working directory.
         */
        static void changeWorkingDir(NativePath const &nativePath);

        /**
         * Creates a native directory relative to the current working directory.
         *
         * @param nativePath  Native directory to create.
         */
        static void createDir(NativePath const &nativePath);

        /**
         * Determines whether a native path exists.
         *
         * @param nativePath  Path to check.
         *
         * @return @c true if the path exists, @c false otherwise.
         */
        static bool exists(NativePath const &nativePath);

        /**
         * Determines the status of a file in the directory.
         * StatusError is thrown if the file status cannot be determined
         * due to any reason.
         *
         * @param nativePath  Path of the file.
         *
         * @return  Status of the file.
         */
        static File::Status fileStatus(NativePath const &nativePath);

    protected:
        void populateSubFolder(Folder &folder, String const &entryName);
        void populateFile(Folder &folder, String const &entryName);

    private:
        NativePath const _nativePath;
        Flags _mode;
    };

    Q_DECLARE_OPERATORS_FOR_FLAGS(DirectoryFeed::Flags)
}

#endif /* LIBDENG2_DIRECTORYFEED_H */
