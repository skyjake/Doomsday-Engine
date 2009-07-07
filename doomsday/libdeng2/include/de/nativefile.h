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

#ifndef LIBDENG2_NATIVEFILE_H
#define LIBDENG2_NATIVEFILE_H

#include <de/File>
#include <de/Flag>
#include <de/String>

namespace de
{
    /**
     * NativeFile reads from and writes to files in the native file system.
     *
     * @ingroup fs
     */
    class PUBLIC_API NativeFile : public File
    {
    public:
        // Mode flags.
        DEFINE_FLAG(WRITE, 0);
        DEFINE_FLAG(TRUNCATE, 1);
        DEFINE_FINAL_FLAG(APPEND, 2, Mode);
        
    public:
        /**
         * Constructs a NativeFile that accesses a file in the native file system.
         * 
         * @param name  Name of the file object.
         * @param nativePath  Path in the native file system to access. Relative to the
         *      current working directory.
         * @param mode  Mode for accessing the file. Defaults to read-only.
         */
        NativeFile(const std::string& name, const std::string& nativePath, const Mode& mode = Mode());
        
        virtual ~NativeFile();

        /**
         * Returns the native path of the file.
         */
        const String& nativePath() const { return nativePath_; }

        /**
         * Sets the size of the file.
         */
        virtual void setSize(Size newSize);

        Size size() const;
		void get(Offset at, Byte* values, Size count) const;
		void set(Offset at, const Byte* values, Size count);
        
    private:
        /// Path of the native file in the OS file system.
        String nativePath_;
        
        /// Mode flags.
        Mode mode_;    

        /// Size of the file.
        Size size_;
    };
}

#endif /* LIBDENG2_NATIVEFILE_H */
