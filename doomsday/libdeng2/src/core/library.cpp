/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/libdeng2.h"
#include "de/Library"
#include "de/Log"

#if defined(UNIX) && defined(DENG2_QT_4_7_OR_NEWER) && !defined(DENG2_QT_4_8_OR_NEWER)
#  define DENG2_USE_DLOPEN
#  include <dlfcn.h>
typedef void *Handle;
#else
typedef QLibrary *Handle;
#endif

using namespace de;

char const *Library::DEFAULT_TYPE = "library/generic";

struct Library::Instance
{
    /// Handle to the shared library.
    Handle library;

    typedef QMap<String, void *> Symbols;
    Symbols symbols;

    /// Type identifier for the library (e.g., "deng-plugin/generic").
    /// Queried by calling deng_LibraryType(), if one is exported in the library.
    String type;

    Instance() : library(0), type(DEFAULT_TYPE)
    {}

#ifdef DENG2_USE_DLOPEN
    NativePath fileName;
    NativePath nativePath() { return fileName; }
    bool isLoaded() const { return library != 0; }
#else
    NativePath nativePath() {
        DENG2_ASSERT(library);
        return library->fileName();
    }
    bool isLoaded() const { return library->isLoaded(); }
#endif
};

Library::Library(NativePath const &nativePath) : d(0)
{
    d = new Instance;

    LOG_AS("Library::Library");
    LOG_TRACE("Loading ") << nativePath;

#ifndef DENG2_USE_DLOPEN
    d->library = new QLibrary(nativePath);
    d->library->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    d->library->load();
#else
    d->fileName = nativePath;
    d->library = dlopen(nativePath.toUtf8().constData(), RTLD_NOW);
#endif

    if(!d->isLoaded())
    {
#ifndef DENG2_USE_DLOPEN
        QString msg = d->library->errorString();
        delete d->library;
#else
        QString msg = dlerror();
#endif
        d->library = 0;

        /// @throw LoadError Opening of the dynamic library failed.
        throw LoadError("Library::Library", msg);
    }

    if(hasSymbol("deng_LibraryType"))
    {
        // Query the type identifier.
        d->type = DENG2_SYMBOL(deng_LibraryType)();
    }
    
    // Automatically call the initialization function, if one exists.
    if(d->type.beginsWith("deng-plugin/") && hasSymbol("deng_InitializePlugin"))
    {
        DENG2_SYMBOL(deng_InitializePlugin)();
    }
}

Library::~Library()
{
    if(d->library)
    {
        LOG_AS("~Library");
        LOG_TRACE("Unloading ") << d->nativePath();

        // Automatically call the shutdown function, if one exists.
        if(d->type.beginsWith("deng-plugin/") && hasSymbol("deng_ShutdownPlugin"))
        {
            DENG2_SYMBOL(deng_ShutdownPlugin)();
        }

#ifndef DENG2_USE_DLOPEN
        d->library->unload();
        delete d->library;
#else
        dlclose(d->library);
#endif
    }

    delete d;
}

String const &Library::type() const
{
    return d->type;
}

void *Library::address(String const &name, SymbolLookupMode lookup)
{
    if(!d->library)
    {
        /// @throw SymbolMissingError There is no library loaded at the moment.
        throw SymbolMissingError("Library::symbol", "Library not loaded");
    }
    
    // Already looked up?
    Instance::Symbols::iterator found = d->symbols.find(name);
    if(found != d->symbols.end())
    {
        return found.value();
    }
    
#ifndef DENG2_USE_DLOPEN
    void *ptr = d->library->resolve(name.toAscii().constData());
#else
    void *ptr = dlsym(d->library, name.toAscii().constData());
#endif

    if(!ptr)
    {
        if(lookup == RequiredSymbol)
        {
            /// @throw SymbolMissingError There is no symbol @a name in the library.
            throw SymbolMissingError("Library::symbol", "Symbol '" + name + "' was not found");
        }
        return 0;
    }

    d->symbols[name] = ptr;
    return ptr;
}

bool Library::hasSymbol(String const &name) const
{
    // First check the symbols cache.
    if(d->symbols.find(name) != d->symbols.end()) return true;

#ifndef DENG2_USE_DLOPEN
    return d->library->resolve(name.toAscii().constData()) != 0;
#else
    return dlsym(d->library, name.toAscii().constData()) != 0;
#endif
}
