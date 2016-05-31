/** @file library.cpp  Dynamic libraries.
 * @ingroup base
 *
 * @authors Copyright © 2006-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/library.h"
#include "doomsday/doomsdayapp.h"

#include <de/App>
#include <de/NativeFile>
#include <de/Library>
#include <de/str.h>

#include <QList>

struct library_s  // typedef Library
{
    Str *path = nullptr;              ///< de::FS path of the library (e.g., "/bin/doom.dll").
    de::LibraryFile *file = nullptr;  ///< File where the plugin has been loaded from.
    bool isGamePlugin = false;        ///< Is this a game plugin? (only one should be in use at a time)
    std::string typeId;               ///< Library type ID e.g., "deng-plugin/game".
};

static ddstring_t *lastError;

typedef QList<Library *> LoadedLibs;
static LoadedLibs loadedLibs;

void Library_Init()
{
    lastError = Str_NewStd();
}

void Library_Shutdown()
{
    Str_Delete(lastError); lastError = nullptr;

    /// @todo  Unload all remaining libraries?
}

void Library_ReleaseGames()
{
#ifdef UNIX
    LOG_AS("Library_ReleaseGames");
    for (Library *lib : loadedLibs)
    {
        if (lib->isGamePlugin)
        {
            LOGDEV_RES_VERBOSE("Closing '%s'") << Str_Text(lib->path);

            // Close the Library.
            DENG2_ASSERT(lib->file);
            lib->file->clear();
        }
    }
#endif
}

#ifdef UNIX
static void reopenLibraryIfNeeded(Library *lib)
{
    DENG2_ASSERT(lib);

    if (!lib->file->loaded())
    {
        LOGDEV_RES_XVERBOSE("Re-opening '%s'") << Str_Text(lib->path);

        // Make sure the Library gets opened again now.
        lib->file->library();

        DENG2_ASSERT(lib->file->loaded());

        DoomsdayApp::plugins().publishAPIs(lib);
    }
}
#endif

Library *Library_New(char const *filePath)
{
    try
    {
        Str_Clear(lastError);

        auto &libFile = de::App::rootFolder().locate<de::LibraryFile>(filePath);
        if (libFile.library().type() == de::Library::DEFAULT_TYPE)
        {
            // This is just a shared library, not a plugin.
            // We don't have to keep it loaded.
            libFile.clear();
            Str_Set(lastError, "not a Doomsday plugin");
            return nullptr;
        }

        // Create the Library instance.
        Library *lib = new Library;
        lib->file   = &libFile;
        lib->path   = Str_Set(Str_NewStd(), filePath);
        lib->typeId = libFile.library().type().toStdString();
        loadedLibs.append(lib);

        // Symbols from game plugins conflict with each other, so we have to
        // keep track of games.
        if (libFile.library().type() == "deng-plugin/game")
        {
            lib->isGamePlugin = true;
        }

        DoomsdayApp::plugins().publishAPIs(lib);
        return lib;
    }
    catch (de::Error const &er)
    {
        Str_Set(lastError, er.asText().toLatin1().constData());
        LOG_RES_WARNING("Library_New: Error opening \"%s\": ") << filePath << er.asText();
    }
    return nullptr;
}

void Library_Delete(Library *lib)
{
    if (!lib) return;

    // Unload the library from memory.
    lib->file->clear();

    Str_Delete(lib->path);
    loadedLibs.removeOne(lib);
    delete lib;
}

char const *Library_Type(Library const *lib)
{
    DENG2_ASSERT(lib);
    return &lib->typeId[0];
}

de::LibraryFile &Library_File(Library *lib)
{
    DENG2_ASSERT(lib);
    return *lib->file;
}

void *Library_Symbol(Library *lib, char const *symbolName)
{
    try
    {
        LOG_AS("Library_Symbol");
        DENG2_ASSERT(lib);
#ifdef UNIX
        reopenLibraryIfNeeded(lib);
#endif
        return lib->file->library().address(symbolName);
    }
    catch (de::Library::SymbolMissingError const &er)
    {
        Str_Set(lastError, er.asText().toLatin1().constData());
        return nullptr;
    }
}

char const *Library_LastError()
{
    return Str_Text(lastError);
}

de::LoopResult Library_ForAll(std::function<de::LoopResult (de::LibraryFile &)> func)
{
    de::FS::Index const &libs = de::App::fileSystem().indexFor(DENG2_TYPE_NAME(de::LibraryFile));
    DENG2_FOR_EACH_CONST(de::FS::Index, i, libs)
    {
        auto &libraryFile = i->second->as<de::LibraryFile>();
        if (libraryFile.path().beginsWith("/bin/"))
        {
            if (auto result = func(libraryFile))
                return result;
        }
    }
    return de::LoopContinue;
}
