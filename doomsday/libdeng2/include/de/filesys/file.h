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

#ifndef LIBDENG2_FILE_H
#define LIBDENG2_FILE_H

#include "../IByteArray"
#include "../String"
#include "../Time"
#include "../Record"
#include "../AccessorValue"
#include "../Audience"

#include <QFlags>

namespace de
{
    class FS;
    class Folder;
    class Feed;
    
    /**
     * Base class for all files stored in the file system.
     * 
     * Subclasses have some special requirements for their destructors:
     * - deindex() must be called in all subclass destructors so that the instances 
     *   indexed under the subclasses' type are removed from the file system's index also.
     * - The file must be automatically flushed before it gets destroyed (see flush()).
     * - The deletion audience must be notified and @c audienceForDeletion must be cleared
     *   afterwards.
     *
     * @ingroup fs
     */
    class DENG2_PUBLIC File : public IByteArray
    {
    public:
        /// I/O to the native file failed. @ingroup errors
        DENG2_ERROR(IOError);

        /// Only reading is allowed from the file. @ingroup errors
        DENG2_SUB_ERROR(IOError, ReadOnlyError);
        
        // Mode flags.
        enum Flag
        {
            Write = 0x1,
            Truncate = 0x2
        };
        Q_DECLARE_FLAGS(Flags, Flag)

        /**
         * The file object is about to be deleted. This may be, e.g., due to pruning or
         * because the parent is being deleted.
         *
         * @param file  The file object being deleted.
         */
        DENG2_DEFINE_AUDIENCE(Deletion, void fileBeingDeleted(const File& file))

        /**
         * Stores the status of a file (size, time of last modification).
         */
        class Status
        {
        public:
            /// Type of file.
            enum Type {
                FILE = 0,           
                FOLDER = 1
            };
            
        public:
            Status(dsize s = 0, const Time& modTime = Time()) 
                : size(s), modifiedAt(modTime), _type(FILE) {}
                
            Status(Type t, dsize s = 0, const Time& modTime = Time())
                : size(s), modifiedAt(modTime), _type(t) {}

            Type type() const { return _type; }
            
            bool operator == (const Status& s) const {
                return size == s.size && modifiedAt == s.modifiedAt;
            }
            
            bool operator != (const Status& s) const { return !(*this == s); }

        public:
            dsize size;
            Time modifiedAt;
            
        private:
            Type _type;
        };
        
        /**
         * Accesses the properties of a File. Allows using properties of a file
         * (like its name, path or size) as a Value, for instance in script
         * expressions.
         *
         * @ingroup fs
         */
        class Accessor : public AccessorValue
        {
        public:
            /// Property of the file to access.
            enum Property {
                NAME,
                PATH,
                TYPE,
                SIZE,
                MODIFIED_AT
            };
            
        public:
            Accessor(File& owner, Property prop);
            
            /// Update the text content of the accessor.
            void update() const;
            
            /// Returns a TextValue with the text content of the accessor,
            /// except for the SIZE property, which is duplicated as a NumberValue.
            Value* duplicateContent() const;
            
        private:
            File& _owner;
            Property _prop;
        };

    public:
        /**
         * When destroyed, a file is automatically removed from its parent folder 
         * and deindexed from the file system.
         *
         * The source file of this file will be destroyed also.
         *
         * @note  Subclasses must call deindex() in their destructors so that
         *        the instances indexed under the subclasses' type are removed
         *        from the index also. Flushing should also be done automatically
         *        as necessary in the subclass.
         */
        virtual ~File();

        /**
         * Remove this file from its file system's index.
         */
        virtual void deindex();
        
        /**
         * Commits any buffered changes to the content of the file. All subclasses
         * of File must make sure they flush themselves right before they get deleted.
         */
        virtual void flush();
        
        /**
         * Empties the contents of the file.
         */
        virtual void clear();

        /// Returns a reference to the application's file system.
        static FS& fileSystem();
        
        /// Returns the name of the file.
        const String& name() const { return _name; }

        /**
         * Sets the parent folder of this file.
         */
        void setParent(Folder* parent) { _parent = parent; }

        /**
         * Returns the parent folder. May be NULL.
         */
        Folder* parent() const { return _parent; }
        
        /**
         * Sets the origin Feed of the File. The origin feed is the feed that is able
         * to singlehandedly decide whether the File needs to be pruned. Typically
         * this is the Feed that generated the File. 
         *
         * @note  Folder instances should not have an origin feed as the folder may
         *        be shared by many feeds.
         *
         * @param feed  The origin feed.
         */ 
        void setOriginFeed(Feed* feed);
        
        /**
         * Returns the origin Feed of the File. @see setOriginFeed()
         */
        Feed* originFeed() const { return _originFeed; }
        
        /**
         * Sets the source file of this file. The source is where this file is
         * getting its data from. File interpreters use this to access their
         * uninterpreted data. By default all files use themselves as the
         * source, so there is always a valid source for every file. If another
         * file is being used as the source, the source is not typically
         * indexed to the file system.
         *
         * @param source  Source file. The file takes ownership of @a source.
         */ 
        void setSource(File* source);
        
        /**
         * Returns the source file. @see setSource()
         *
         * @return  Source file. Always returns a valid pointer.
         */
        const File* source() const;
        
        /**
         * Returns the source file. @see setSource()
         *
         * @return  Source file. Always returns a valid pointer.
         */
        File* source();
        
        /**
         * Updates the status of the file.
         *
         * @param status  New status.
         */
        virtual void setStatus(const Status& status);

        /**
         * Returns the status of the file.
         */
        const Status& status() const;
                        
        /**
         * Forms the complete path of this file object.
         *
         * @return Path of the object. This is not a native path, but instead 
         *         intended for de::FS.
         */
        const String path() const;

        /**
         * Returns the mode of the file.
         */
        const Flags& mode() const;
        
        /**
         * Changes the mode of the file. For example, using
         * <code>WRITE|TRUNCATE</code> as the mode would empty the contents of
         * the file and open it in writing mode.
         *
         * @param newMode  Mode.
         */
        virtual void setMode(const Flags& newMode);

        /// Returns the file information (const).
        const Record& info() const { return _info; }

        /// Returns the file information.
        Record& info() { return _info; }
        
        /**
         * Makes sure that the file has write access.
         */
        void verifyWriteAccess();

        // Implements IByteArray.
        Size size() const;
        void get(Offset at, Byte* values, Size count) const;
        void set(Offset at, const Byte* values, Size count);
    
    protected:
        /**
         * Constructs a new file. By default files are in read-only mode.
         *
         * @param name  Name of the file.
         */
        File(const String& name = "");
    
    private:
        /// The parent folder.
        Folder* _parent;
        
        /// The source file (NULL for non-interpreted files).
        File* _source;
                
        /// Feed that generated the file. This feed is called upon when the file needs 
        /// to be pruned. May also be NULL.
        Feed* _originFeed;
        
        /// Name of the file.
        String _name;
        
        /// Status of the file.
        Status _status;
        
        /// Mode flags.
        Flags _mode;
        
        /// File information.
        Record _info;
    }; 

    Q_DECLARE_OPERATORS_FOR_FLAGS(File::Flags)
}

#endif /* LIBDENG2_FILE_H */
