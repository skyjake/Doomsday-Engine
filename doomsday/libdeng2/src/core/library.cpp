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

#include "de/Library"
#include "de/Log"

using namespace de;

const char* Library::DEFAULT_TYPE = "library/generic";

Library::Library(const String& nativePath)
    : _library(0), _type(DEFAULT_TYPE)
{
    LOG_AS("Library::Library");

#ifdef MACOSX
    // If the library happens to be in a bundle, just use the bundle path.
    String path = nativePath;
    if(path.fileNamePath().fileNameExtension() == ".bundle")
    {
        path = path.fileNamePath();
    }
    LOG_TRACE("%s") << path;
    _library = new QLibrary(path);
#else
    LOG_TRACE("%s") << nativePath;
    _library = new QLibrary(nativePath);
#endif
    _library->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    _library->load();

    if(!_library->isLoaded())
    {
        QString msg = _library->errorString();

        delete _library;
        _library = 0;

        /// @throw LoadError Opening of the dynamic library failed.
        throw LoadError("Library::Library", msg);
    }

    if(hasSymbol("deng_LibraryType"))
    {
        // Query the type identifier.
        _type = DENG2_SYMBOL(deng_LibraryType)();
    }
    
    // Automatically call the initialization function, if one exists.
    if(_type.beginsWith("deng-plugin/") && hasSymbol("deng_InitializePlugin"))
    {
        DENG2_SYMBOL(deng_InitializePlugin)();
    }
}

Library::~Library()
{
    if(_library)
    {
        // Automatically call the shutdown function, if one exists.
        if(_type.beginsWith("deng-plugin/") && hasSymbol("deng_ShutdownPlugin"))
        {
            DENG2_SYMBOL(deng_ShutdownPlugin)();
        }

        delete _library;
    }
}

void* Library::address(const String& name, SymbolLookupMode lookup)
{
    if(!_library)
    {
        /// @throw SymbolMissingError There is no library loaded at the moment.
        throw SymbolMissingError("Library::symbol", "Library not loaded");
    }
    
    // Already looked up?
    Symbols::iterator found = _symbols.find(name);
    if(found != _symbols.end())
    {
        return found.value();
    }
    
    void* ptr = _library->resolve(name.toAscii().constData());

    if(!ptr)
    {
        if(lookup == RequiredSymbol)
        {
            /// @throw SymbolMissingError There is no symbol @a name in the library.
            throw SymbolMissingError("Library::symbol", "Symbol '" + name + "' was not found");
        }
        return 0;
    }

    _symbols[name] = ptr;
    return ptr;
}

bool Library::hasSymbol(const String &name) const
{
    // First check the symbols cache.
    if(_symbols.find(name) != _symbols.end()) return true;
    return _library->resolve(name.toAscii().constData()) != 0;
}
