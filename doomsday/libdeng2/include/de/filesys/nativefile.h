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

#include "../deng.h"
#include "../File"
#include "../Flag"
#include "../String"

#include <fstream>

namespace de
{
    /**
     * Reads from and writes to files in the native file system.
     *
     * @ingroup fs
     */
    class LIBDENG2_API NativeFile : public File
    {
    public:
        /// Input from the native file failed. @ingroup errors
        DEFINE_SUB_ERROR(IOError, InputError);
        
        /// Output to the native file failed. @ingroup errors
        DEFINE_SUB_ERROR(IOError, OutputError);
        
    public:
        /**
         * Constructs a NativeFile that accesses a file in the native file system
         * in read-only mode.
         * 
         * @param name        Name of the file object.
         * @param nativePath  Path in the native file system to access. Relative to the
         *                    current working directory.
         */
        NativeFile(const String& name, const String& nativePath);
        
        virtual ~NativeFile();

        void clear();

        /**
         * Returns the native path of the file.
         */
        const String& nativePath() const { return nativePath_; }

        void setMode(const Mode& newMode);

        // Implements IByteArray.
        Size size() const;
		void get(Offset at, Byte* values, Size count) const;
		void set(Offset at, const Byte* values, Size count);
        
    protected:
        /// Returns the input stream.
        std::ifstream& input() const;

        /// Returns the output stream.
        std::ofstream& output();

        /// Close any open streams.
        void close();
        
    private:
        /// Path of the native file in the OS file system.
        String nativePath_;
        
        /// Input stream.
        mutable std::ifstream* in_;
        
        /// Output stream.
        std::ofstream* out_;
    };
}

#endif /* LIBDENG2_NATIVEFILE_H */
