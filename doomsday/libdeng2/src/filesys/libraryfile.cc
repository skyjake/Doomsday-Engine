/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QLibrary>

using namespace de;

LibraryFile::LibraryFile(File* source)
    : File(source->name()), _library(0)
{
    Q_ASSERT(source != 0);
    setSource(source); // takes ownership
}

LibraryFile::~LibraryFile()
{
    FOR_AUDIENCE(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion.clear();
    
    deindex();    
    delete _library;
}

Library& LibraryFile::library()
{
    if(_library)
    {
        return *_library;
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
    
    _library = new Library(native->nativePath());
    return *_library;
}

void LibraryFile::clear()
{
    if(_library)
    {
        delete _library;
        _library = 0;
    }
}

bool LibraryFile::hasUnderscoreName(const String& nameAfterUnderscore) const
{
    return name().contains("_" + nameAfterUnderscore + ".");
}

bool LibraryFile::recognize(const File& file)
{
    // Check the extension first.
    if(!QLibrary::isLibrary(file.name()))
    {
        return false;
    }

#if defined(Q_OS_MAC)
    if(file.name().beginsWith("libdengplugin_"))
    {
        return true;
    }
#elif defined(Q_OS_UNIX)
    if(file.name().beginsWith("libdengplugin_"))
    {
        return true;
    }
#elif defined(Q_OS_WIN32)
    if(file.name().beginsWith("dengplugin_"))
    {
        return true;
    }
#endif
    return false;
}
