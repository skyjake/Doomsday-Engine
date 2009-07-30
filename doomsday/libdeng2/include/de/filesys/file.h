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

#ifndef LIBDENG2_FILE_H
#define LIBDENG2_FILE_H

#include "../IByteArray"
#include "../String"
#include "../Time"
#include "../Record"
#include "../TextValue"

namespace de
{
    class FS;
    class Folder;
    class Feed;
    
    /**
     * Base class for all files stored in the file system.
     * 
     * @note Subclasses must call deindex() in their destructors so that
     *       the instances indexed under the subclasses' type are removed
     *       from the index also.
     *
     * @ingroup fs
     */
    class LIBDENG2_API File : public IByteArray
    {
    public:
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
                : size(s), modifiedAt(modTime), type_(FILE) {}
                
            Status(Type t, dsize s = 0, const Time& modTime = Time())
                : size(s), modifiedAt(modTime), type_(t) {}

            Type type() const { return type_; }
            
            bool operator == (const Status& s) const {
                return size == s.size && modifiedAt == s.modifiedAt;
            }
            
            bool operator != (const Status& s) const { return !(*this == s); }

        public:
            dsize size;
            Time modifiedAt;
            
        private:
            Type type_;
        };
        
        /**
         * Special text value that accesses the properties of the file.
         *
         * @ingroup fs
         */
        class AccessorValue : public TextValue
        {
        public:
            /// Accessor values cannot be serialized as they just mirror the 
            /// information of the owning file. @ingroup errors
            DEFINE_ERROR(CannotSerializeError);
            
            /// Propert of the file to access.
            enum Property {
                NAME,
                PATH,
                TYPE,
                SIZE,
                MODIFIED_AT
            };
            
        public:
            AccessorValue(File& owner, Property prop);
            
            /// Update the text content of the accessor.
            void update() const;
            
            /// Returns a TextValue with the text content of the accessor,
            /// except for the SIZE property, which is duplicated as a NumberValue.
            Value* duplicate() const;

            Number asNumber() const;
            Text asText() const;
            dsize size() const;
            bool isTrue() const;
            dint compare(const Value& value) const;
            void sum(const Value& value);
            void multiply(const Value& value);
            void divide(const Value& value);
            void modulo(const Value& divisor);
            
            // Implements ISerializable.
            void operator >> (Writer& to) const;
            void operator << (Reader& from);
            
        private:
            File& owner_;
            Property prop_;
        };
        
    public:
        /// An attempt was made to write to a read-only file.  @ingroup errors
        DEFINE_ERROR(ReadOnlyError);
        
    public:
        File(const std::string& name = "");
        
        /**
         * When destroyed, a file is automatically removed from its parent folder 
         * and deindexed from the file system.
         *
         * @note  Subclasses must call deindex() in their destructors so that
         *        the instances indexed under the subclasses' type are removed
         *        from the index also.
         */
        virtual ~File();

        /**
         * Remove this file from its file system's index.
         */
        virtual void deindex();
        
        /**
         * Commits any buffered changes to the content of the file. Called automatically
         * before the File instance is deleted.
         */
        virtual void flush();

        /// Returns a reference to the application's file system.
        static FS& fileSystem();
        
        /// Returns the name of the file.
        const String& name() const { return name_; }

        /**
         * Sets the parent folder of this file.
         */
        void setParent(Folder* parent) { parent_ = parent; }

        /**
         * Returns the parent folder. May be NULL.
         */
        Folder* parent() const { return parent_; }
        
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
        Feed* originFeed() const { return originFeed_; }
        
        /**
         * Sets the source file of this file. The source is where this file is getting
         * its data from. File interpreters use this to access their uninterpreted data.
         * By default all files use themselves as the source, so there is always a
         * valid source for every file.
         *
         * @param source  Source file.
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

        /// Returns the file information (const).
        const Record& info() const { return info_; }

        /// Returns the file information.
        Record& info() { return info_; }

        // Implements ISerializable.
        Size size() const;
		void get(Offset at, Byte* values, Size count) const;
		void set(Offset at, const Byte* values, Size count);
        
    private:
        /// The parent folder.
        Folder* parent_;
        
        /// The source file (NULL for non-interpreted files).
        File* source_;
                
        /// Feed that generated the file. This feed is called upon when the file needs 
        /// to be pruned. May also be NULL.
        Feed* originFeed_;
        
        /// Name of the file.
        String name_;
        
        /// Status of the file.
        Status status_;
        
        /// File information.
        Record info_;
    }; 
}

#endif /* LIBDENG2_FILE_H */
