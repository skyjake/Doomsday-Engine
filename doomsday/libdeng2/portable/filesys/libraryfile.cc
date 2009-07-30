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

#include "de/LibraryFile"
#include "de/NativeFile"
#include "de/Library"

using namespace de;

LibraryFile::LibraryFile(File* source)
    : File(source->name()), library_(0)
{
    assert(source != 0);
    setSource(source); // takes ownership
}

LibraryFile::~LibraryFile()
{
    deindex();    
    delete library_;
}

Library& LibraryFile::library()
{
    if(library_)
    {
        return *library_;
    }
    
    /// @todo A method for File for making a NativeFile out of any File.
    NativeFile* native = dynamic_cast<NativeFile*>(source());
    if(!native)
    {
        /// @throw UnsupportedSourceError Currently shared libraries are only loaded directly
        /// from native files. Other kinds of files would require a temporary native file.
        throw UnsupportedSourceError("LibraryFile::library", source()->path() + 
            ": can only load from NativeFile");
    }
    
    library_ = new Library(native->nativePath());
    return *library_;
}

void LibraryFile::clear()
{
    if(library_)
    {
        delete library_;
        library_ = 0;
    }
}

bool LibraryFile::hasUnderscoreName(const String& nameAfterUnderscore) const
{
    return name().contains("_" + nameAfterUnderscore + ".");
}

bool LibraryFile::recognize(const File& file)
{
#if defined(MACOSX)
    if(file.name().beginsWith("libdengplugin_") &&
        file.name().fileNameExtension() == ".dylib")
    {
        return true;
    }
#elif defined(UNIX)
    if(file.name().beginsWith("libdengplugin_") &&
        file.name().fileNameExtension() == ".so")
    {
        return true;
    }
#elif defined(WIN32)
    if(file.name().beginsWith("dengplugin_") &&
        file.name().fileNameExtension() == ".dll")
    {
        return true;
    }
#endif
    return false;
}
