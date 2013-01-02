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

#include "api_def.h"
#include "api_console.h"
#include "api_filesys.h"
#include "api_mpe.h"

struct library_s { // typedef Library
    Str* path;              ///< de::FS path of the library (e.g., "/bin/doom.dll").
    de::LibraryFile* file;  ///< File where the plugin has been loaded from.
    bool isGamePlugin;      ///< Is this a game plugin? (only one should be in use at a time)
    std::string typeId;     ///< deng2 library type ID e.g., "deng-plugin/game".

    library_s() : path(0), file(0), isGamePlugin(false) {}
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
            DENG_ASSERT(lib->file);
            lib->file->clear();
        }
    }
#endif
}

#ifdef UNIX
static void reopenLibraryIfNeeded(Library* lib)
{
    DENG_ASSERT(lib);

    if(!lib->file->loaded())
    {
        LegacyCore_PrintfLogFragmentAtLevel(DE2_LOG_DEBUG,
                "reopenLibraryIfNeeded: Opening '%s'\n", Str_Text(lib->path));

        // Make sure the Library gets opened again now.
        lib->file->library();

        DENG_ASSERT(lib->file->loaded());

        Library_PublishAPIs(lib);
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
        lib->file = &libFile;
        lib->path = Str_Set(Str_NewStd(), filePath);
        lib->typeId = libFile.library().type().toStdString();
        loadedLibs.append(lib);

        // Symbols from game plugins conflict with each other, so we have to
        // keep track of games.
        if(libFile.library().type() == "deng-plugin/game")
        {
            lib->isGamePlugin = true;
        }

        Library_PublishAPIs(lib);

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

    // Unload the library from memory.
    lib->file->clear();

    Str_Delete(lib->path);
    loadedLibs.removeOne(lib);
    delete lib;
}

const char* Library_Type(const Library* lib)
{
    DENG_ASSERT(lib);
    return &lib->typeId[0];
}

de::LibraryFile& Library_File(Library* lib)
{
    DENG_ASSERT(lib);
    return *lib->file;
}

void Library_PublishAPIs(Library *lib)
{
    de::Library &library = lib->file->library();
    if(library.hasSymbol("deng_API"))
    {
        de::Library::deng_API setAPI = library.DENG2_SYMBOL(deng_API);

        setAPI(DE_API_BASE_v1,        &_api_Base);
        setAPI(DE_API_CONSOLE_v1,     &_api_Con);
        setAPI(DE_API_DEFINITIONS_v1, &_api_Def);
        setAPI(DE_API_FILE_SYSTEM_v1, &_api_F);
        setAPI(DE_API_MAP_EDIT_v1,    &_api_MPE);
        setAPI(DE_API_PLUGIN_v1,      &_api_Plug);
        setAPI(DE_API_URI_v1,         &_api_Uri);
        setAPI(DE_API_WAD_v1,         &_api_W);
    }
}

void* Library_Symbol(Library* lib, const char* symbolName)
{
    try
    {
        DENG_ASSERT(lib);
#ifdef UNIX
        reopenLibraryIfNeeded(lib);
#endif
        return lib->file->library().address(symbolName);
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

int Library_IterateAvailableLibraries(int (*func)(void *, const char *, const char *, void *), void *data)
{
    const de::FS::Index& libs = DENG2_APP->fileSystem().indexFor(DENG2_TYPE_NAME(de::LibraryFile));

    DENG2_FOR_EACH_CONST(de::FS::Index, i, libs)
    {
        de::LibraryFile* lib = static_cast<de::LibraryFile*>(i->second);
        const de::NativeFile* src = dynamic_cast<const de::NativeFile*>(lib->source());
        if(src)
        {
            int result = func(lib, src->name().toUtf8().constData(),
                              lib->path().toUtf8().constData(), data);
            if(result) return result;
        }
    }

    return 0;
}
