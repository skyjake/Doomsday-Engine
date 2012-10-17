/**
 * @file library.cpp
 * Dynamic libraries. @ingroup base
 *
 * @authors Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <de/App>
#include <de/NativeFile>
#include <de/LibraryFile>
#include <de/Library>

#include "de_base.h"
#include "m_misc.h"

struct library_s { // typedef Library
    Str* path;
    de::LibraryFile* libFile;
    bool isGamePlugin;

    library_s() : path(0), libFile(0), isGamePlugin(false) {}
};

static ddstring_t* lastError;

typedef QList<Library*> LoadedLibs;
static LoadedLibs loadedLibs;

void Library_Init(void)
{
    lastError = Str_NewStd();
}

void Library_Shutdown(void)
{
    Str_Delete(lastError); lastError = 0;

    /// @todo  Unload all remaining libraries?
}

void Library_ReleaseGames(void)
{
#ifdef UNIX
    foreach(Library* lib, loadedLibs)
    {
        if(lib->isGamePlugin)
        {
            LegacyCore_PrintfLogFragmentAtLevel(DE2_LOG_DEBUG,
                    "Library_ReleaseGames: Closing '%s'\n", Str_Text(lib->path));

            // Close the Library.
            DENG_ASSERT(lib->libFile);
            lib->libFile->clear();
        }
    }
#endif
}

#ifdef UNIX
static void reopenLibraryIfNeeded(Library* lib)
{
    DENG_ASSERT(lib);

    if(!lib->libFile->loaded())
    {
        LegacyCore_PrintfLogFragmentAtLevel(DE2_LOG_DEBUG,
                "reopenLibraryIfNeeded: Opening '%s'\n", Str_Text(lib->path));

        // Make sure the Library gets opened again now.
        lib->libFile->library();

        DENG_ASSERT(lib->libFile->loaded());
    }
}
#endif

Library* Library_New(const char* filePath)
{
    try
    {
        Str_Clear(lastError);

        de::LibraryFile& libFile = DENG2_APP->rootFolder().locate<de::LibraryFile>(filePath);
        if(libFile.library().type() == de::Library::DEFAULT_TYPE)
        {
            // This is just a shared library, not a plugin.
            // We don't have to keep it loaded.
            libFile.clear();
            Str_Set(lastError, "not a Doomsday plugin");
            return 0;
        }

        // Create the Library instance.
        Library* lib = new Library;
        lib->libFile = &libFile;
        lib->path = Str_Set(Str_NewStd(), filePath);
        loadedLibs.append(lib);

        // Symbols from game plugins conflict with each other, so we have to
        // keep track of games.
        if(libFile.library().type() == "deng-plugin/game")
        {
            lib->isGamePlugin = true;
        }

        return lib;
    }
    catch(const de::Error& er)
    {
        Str_Set(lastError, er.asText().toAscii().constData());
        LOG_WARNING("Library_New: Error opening \"%s\": ") << filePath << er.asText();
        return 0;
    }
}

void Library_Delete(Library *lib)
{
    DENG_ASSERT(lib);
    lib->libFile->clear();
    Str_Delete(lib->path);
    loadedLibs.removeOne(lib);
    delete lib;
}

void* Library_Symbol(Library* lib, const char* symbolName)
{
    try
    {
        DENG_ASSERT(lib);
#ifdef UNIX
        reopenLibraryIfNeeded(lib);
#endif
        return lib->libFile->library().address(symbolName);
    }
    catch(const de::Library::SymbolMissingError& er)
    {
        Str_Set(lastError, er.asText().toAscii().constData());
        return 0;
    }
}

const char* Library_LastError(void)
{
    return Str_Text(lastError);
}

int Library_IterateAvailableLibraries(int (*func)(const char*, const char *, void *), void *data)
{
    const de::FS::Index& libs = DENG2_APP->fileSystem().indexFor(DENG2_TYPE_NAME(de::LibraryFile));

    DENG2_FOR_EACH_CONST(de::FS::Index, i, libs)
    {
        // For now we are not using libdeng2 to actually load the library.
        de::LibraryFile* lib = static_cast<de::LibraryFile*>(i->second);
        const de::NativeFile* src = dynamic_cast<const de::NativeFile*>(lib->source());
        if(src)
        {
            int result = func(src->name().toUtf8().constData(),
                              lib->path().toUtf8().constData(), data);
            if(result) return result;
        }
    }

    return 0;
}
