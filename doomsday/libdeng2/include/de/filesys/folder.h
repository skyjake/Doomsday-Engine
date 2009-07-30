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

#ifndef LIBDENG2_FOLDER_H
#define LIBDENG2_FOLDER_H

#include "../deng.h"
#include "../File"

#include <map>
#include <list>

namespace de
{
    class Feed;
    
    /**
     * A folder contains a set of files. It is used for building a tree of files
     * in the file system (de::FS). This is the base class for all types of folders.
     *
     * @see Feed
     *
     * @ingroup fs
     */
    class LIBDENG2_API Folder : public File
    {
    public:
        /// A folder cannot contain two or more files with the same name. @ingroup errors
        DEFINE_ERROR(DuplicateNameError);
        
        /// File path did not point to a file. @ingroup errors
        DEFINE_ERROR(NotFoundError);
        
        typedef std::list<Feed*> Feeds;
        
    public:
        Folder(const String& name = "");
        
        virtual ~Folder();
    
        /**
         * Populates the folder with a set of File instances. Each feed attached to 
         * the folder will contribute. Every populated file will also be
         * added to the file system's main index. 
         *
         * Repopulation is nondestructive as long as the source data has not changed.
         * Population may be performed more than once during the lifetime of the folder,
         * for example when it's necessary to synchronize it with the contents of a 
         * native hard drive directory.
         */
        virtual void populate();

        /**
         * Attach a feed to the folder. The feed will provide content for the folder.
         *
         * @param feed  Feed to attach to the folder. The folder gets ownership of the feed.
         */
        void attach(Feed* feed);
        
        /**
         * Detaches a feed from the folder. The feed object is not deleted.
         *
         * @param feed  Feed to detach from the folder.
         *
         * @return  The Feed object. Ownership is returned to the caller.
         */
        Feed* detach(Feed& feed);

        /**
         * Provides access to the list of Feeds for this folder. The feeds are responsible
         * for creating File and Folder instances in the folder.
         */
        const Feeds& feeds() const { return feeds_; }
        
        /**
         * Destroys the contents of the folder. All contained file objects are deleted.
         */
        virtual void clear();

        /**
         * Checks whether the folder contains a file.
         *
         * @param name  File to check for. The name is not case sensitive.
         */
        bool has(const String& name) const;

        /**
         * Adds an object to the folder. The object must be an instance of a class
         * derived from File.
         *
         * @param fileObject  Object to add to the folder. The folder takes 
         *      ownership of this object. Cannot be NULL.
         *
         * @return  Reference to @a fileObject, for convenience.
         */
        template <typename Type>
        Type& add(Type* fileObject) {
            assert(fileObject != 0);
            add(static_cast<File*>(fileObject));
            return *fileObject;
        }
        
        /**
         * Adds a file instance to the contents of the folder. 
         *
         * @param file  File to add. The folder takes ownership of this instance.
         *
         * @return  Reference to the file, for convenience.
         */
        virtual File& add(File* file);

        /**
         * Removes a file from the folder, by name. The file is not deleted. The
         * ownership of the file is given to the caller.
         *
         * @return  The removed file object. Ownership of the object is given to
         * the caller.
         */
        File* remove(const String& name);

        template <typename Type>
        Type* remove(Type* fileObject) {
            assert(fileObject != 0);
            remove(static_cast<File*>(fileObject));
            return fileObject;
        }

        /**
         * Removes a file from the folder. The file is not deleted. The ownership 
         * of the file is given to the caller.
         *
         * @return  The removed file object. Ownership of the object is given to
         * the caller.
         */
        virtual File* remove(File* file);

        /**
         * Locates a file in this folder or in one of its subfolders. Looks recursively
         * through subfolders.
         *
         * @param path  Path to look for. Relative to this folder.
         * 
         * @return  The located file, or @c NULL if the path was not found.
         */
        virtual File* locateFile(const String& path) const;
        
        template <typename Type>
        Type* locate(const String& path) const {
            return dynamic_cast<Type*>(locateFile(path));
        }

        /**
         * Locates a file in this folder or in one of its subfolders. Looks recusively
         * through subfolders.
         *
         * @param path  Path to look for. Relative to this folder.
         *
         * @return  The found file.
         */
        template <typename Type>
        Type& find(const String& path) const {
            Type* found = locate<Type>(path);
            if(!found) {
                /// @throw NotFoundError  Path didn't exist, or the located file had
                /// an incompatible type.
                throw NotFoundError("FS::find", path +": path was not found or had incompatible type");
            }
            return *found;
        }
                    
    private:
        /// A map of file names to file instances.
        typedef std::map<String, File*> Contents;
        Contents contents_;
        
        /// Feeds provide content for the folder.
        Feeds feeds_;
    };
}

#endif /* LIBDENG2_FOLDER_H */
