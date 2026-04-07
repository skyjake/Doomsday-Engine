/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/libcore.h"
#include "de/Library"
#include "de/Log"
#include "de/LogBuffer"

#include <array>

namespace de {

#if defined (DE_STATIC_LINK)

struct ImportedStaticLibrary
{
    char const *name;
    StaticSymbolFunc symbolFunc;
};
static std::array<ImportedStaticLibrary, 16> importedLibraries;

ImportedStaticLibrary const *findStaticLibrary(char const *name)
{
    for (auto const &imp : importedLibraries)
    {
        if (!imp.name) break;
        if (!qstrcmp(imp.name, name))
        {
            return &imp;
        }
    }
    return nullptr;
}

#endif // DE_STATIC_LINK

#if defined (DE_STATIC_LINK)
typedef ImportedStaticLibrary const *Handle;
#else
//#  if defined(UNIX) && defined(DE_QT_4_7_OR_NEWER) && !defined(DE_QT_4_8_OR_NEWER)
#    define DE_USE_DLOPEN
#    include <dlfcn.h>
typedef void *Handle;
//#  else
//typedef QLibrary *Handle;
//#  endif
#endif

char const *Library::DEFAULT_TYPE = "library/generic";

DE_PIMPL(Library)
{
    /// Handle to the shared library.
    Handle library;

    typedef Hash<String, void *> Symbols;
    Symbols symbols;

    /// Type identifier for the library (e.g., "deng-plugin/generic").
    /// Queried by calling deng_LibraryType(), if one is exported in the library.
    String type;

    Impl(Public *i) : Base(i), library(0), type(DEFAULT_TYPE)
    {}

#if defined (DE_STATIC_LINK) || defined(DE_USE_DLOPEN)
    NativePath fileName;
    NativePath nativePath() { return fileName; }
    bool isLoaded() const { return library != nullptr; }
#else
//    NativePath nativePath() {
//        DE_ASSERT(library);
//        return library->fileName();
//    }
//    bool isLoaded() const { return library->isLoaded(); }
#endif
};

Library::Library(NativePath const &nativePath) : d(new Impl(this))
{
    LOG_AS("Library");
    LOG_TRACE("Loading \"%s\"", nativePath.pretty());

#if defined (DE_STATIC_LINK)
    d->fileName = nativePath;
    d->library = findStaticLibrary(nativePath);
#elif defined (DE_USE_DLOPEN)
    d->fileName = nativePath;
    d->library = dlopen(nativePath, RTLD_NOW);
#else
//    d->library = new QLibrary(nativePath);
//    d->library->setLoadHints(QLibrary::ResolveAllSymbolsHint);
//    d->library->load();
#endif

    if (!d->isLoaded())
    {
#if defined (DE_STATIC_LINK)
        String msg = "static library has not been imported";
#elif defined (DE_USE_DLOPEN)
        String msg = dlerror();
#else
//        QString msg = d->library->errorString();
//        delete d->library;
#endif
        d->library = 0;

        /// @throw LoadError Opening of the dynamic library failed.
        throw LoadError("Library::Library", msg);
    }

    if (hasSymbol("deng_LibraryType"))
    {
        // Query the type identifier.
        d->type = DE_SYMBOL(deng_LibraryType)();
    }

    // Automatically call the initialization function, if one exists.
    if (d->type.beginsWith("deng-plugin/") && hasSymbol("deng_InitializePlugin"))
    {
        DE_SYMBOL(deng_InitializePlugin)();
    }
}

Library::~Library()
{
    if (d->library)
    {
        LOG_AS("~Library");
        LOG_TRACE("Unloading \"%s\"", d->nativePath().pretty());

        // Automatically call the shutdown function, if one exists.
        if (d->type.beginsWith("deng-plugin/") && hasSymbol("deng_ShutdownPlugin"))
        {
            DE_SYMBOL(deng_ShutdownPlugin)();
        }

        // The log buffer may contain log entries built by the library; those
        // entries contain pointers to functions that are about to disappear.
        LogBuffer::get().clear();

#if defined (DE_STATIC_LINK)
        d->library = nullptr;
#elif defined (DE_USE_DLOPEN)
        dlclose(d->library);
#else
//        d->library->unload();
//        delete d->library;
#endif
    }
}

String const &Library::type() const
{
    return d->type;
}

void *Library::address(String const &name, SymbolLookupMode lookup)
{
    if (!d->library)
    {
        /// @throw SymbolMissingError There is no library loaded at the moment.
        throw SymbolMissingError("Library::symbol", "Library not loaded");
    }

    // Already looked up?
    Impl::Symbols::iterator found = d->symbols.find(name);
    if (found != d->symbols.end())
    {
        return found->second;
    }

#if defined (DE_STATIC_LINK)
    void *ptr = d->library->symbolFunc(name.toLatin1().constData());
    qDebug() << d->fileName.toString() << name << ptr;
#elif defined (DE_USE_DLOPEN)
    void *ptr = dlsym(d->library, name);
#else
//    void *ptr = de::function_cast<void *>(d->library->resolve(name.toLatin1().constData()));
#endif

    if (!ptr)
    {
        if (lookup == RequiredSymbol)
        {
            /// @throw SymbolMissingError There is no symbol @a name in the library.
            throw SymbolMissingError("Library::symbol", "Symbol '" + name + "' was not found");
        }
        return 0;
    }

    d->symbols.insert(name, ptr);
    return ptr;
}

bool Library::hasSymbol(String const &name) const
{
    // First check the symbols cache.
    if (d->symbols.find(name) != d->symbols.end()) return true;

#if defined (DE_STATIC_LINK)
    return d->library->symbolFunc(name.toLatin1().constData()) != 0;
#elif defined (DE_USE_DLOPEN)
    return dlsym(d->library, name) != 0;
#else
//    return d->library->resolve(name.toLatin1().constData()) != 0;
#endif
}

#if defined (DE_STATIC_LINK)
void Library::importStaticLibrary(char const *name, StaticSymbolFunc symbolFunc) // static
{
    for (uint i = 0; i < importedLibraries.size(); ++i)
    {
        auto &impLib = importedLibraries[i];
        if (!impLib.name)
        {
            impLib.name = name;
            impLib.symbolFunc = symbolFunc;
            return;
        }
    }
}

StringList Library::staticLibraries() // static
{
    StringList names;
    for (auto const &lib : importedLibraries)
    {
        if (lib.name)
        {
            names << lib.name;
        }
    }
    return names;
}
#endif

} // namespace de
