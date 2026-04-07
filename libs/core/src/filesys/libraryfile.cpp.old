/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/LibraryFile"
#include "de/Library"
#include "de/NativeFile"
#include "de/NativePath"
#include "de/LogBuffer"

using namespace de;

DE_PIMPL_NOREF(LibraryFile)
{
    Library *library = nullptr;
    NativePath nativePath;
};

LibraryFile::LibraryFile(File *source)
    : File(source->name())
    , d(new Impl)
{
    DE_ASSERT(source != 0);
    setSource(source); // takes ownership
}

LibraryFile::LibraryFile(NativePath const &nativePath)
    : File(nativePath.fileName())
    , d(new Impl)
{
    d->nativePath = nativePath;
}

LibraryFile::~LibraryFile()
{
    DE_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion().clear();

    deindex();
    delete d->library;
}

String LibraryFile::describe() const
{
    String desc = DE_STR("shared library");
    if (loaded()) desc += " [" + library().type() + "]";
    return desc;
}

bool LibraryFile::loaded() const
{
    return d->library != 0;
}

Library &LibraryFile::library()
{
    if (d->library)
    {
        return *d->library;
    }

    if (!d->nativePath.isEmpty())
    {
        d->library = new Library(d->nativePath);
    }
    else
    {
        /// @todo A mechanism to make a NativeFile out of any File via caching?

        NativeFile *native = maybeAs<NativeFile>(source());
        if (!native)
        {
            /// @throw UnsupportedSourceError Currently shared libraries are only loaded directly
            /// from native files. Other kinds of files would require a temporary native file.
            throw UnsupportedSourceError("LibraryFile::library",
                                         source()->description() +
                                             ": can only load from NativeFile");
        }
        d->library = new Library(native->nativePath());
    }
    return *d->library;
}

Library const &LibraryFile::library() const
{
    if (d->library) return *d->library;

    /// @throw NotLoadedError Library is presently not loaded.
    throw NotLoadedError("LibraryFile::library", "Library is not loaded: " + description());
}

void LibraryFile::clear()
{
    if (d->library)
    {
        delete d->library;
        d->library = nullptr;
    }
}

bool LibraryFile::hasUnderscoreName(String const &nameAfterUnderscore) const
{
    return name().contains("_" + nameAfterUnderscore + ".") ||
           name().endsWith("_" + nameAfterUnderscore);
}

bool LibraryFile::recognize(File const &file)
{
    #if defined (DE_APPLE)
    {
        // On macOS/iOS, plugins are in the .bundle format. The LibraryFile will point
        // to the actual binary inside the bundle. Libraries must be loaded from
        // native files.
        if (NativeFile const *native = maybeAs<NativeFile>(file))
        {
            // Check if this in the executable folder with a matching bundle name.
            if (native->nativePath().fileNamePath().toString()
                .endsWith(file.name() + ".bundle/Contents/MacOS"))
            {
                return true;
            }
        }
    }
    #else // not Apple
    {
        // Check the extension first.
        if (QLibrary::isLibrary(file.name()))
        {
            #if defined(UNIX)
            {
                // Only actual .so files should be considered.
                if (!file.name().endsWith(".so")) // just checks the file name
                {
                    return false;
                }
            }
            #endif

            // Looks like a library.
            return true;
        }
    }
    #endif

    return false;
}

File *LibraryFile::Interpreter::interpretFile(File *sourceData) const
{
    if (recognize(*sourceData))
    {
        LOG_RES_XVERBOSE("Interpreted %s as a shared library", sourceData->description());

        // It is a shared library intended for Doomsday.
        return new LibraryFile(sourceData);
    }
    return nullptr;
}
