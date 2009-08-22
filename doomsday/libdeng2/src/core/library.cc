/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifdef UNIX
#   include <dlfcn.h>
#endif

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

using namespace de;

const char* Library::DEFAULT_TYPE = "library/generic";

Library::Library(const String& nativePath)
    : handle_(0), type_(DEFAULT_TYPE)
{
    LOG_AS("Library::Library");
    LOG_VERBOSE("Loading library: ") << nativePath;
    
#ifdef UNIX
    if((handle_ = dlopen(nativePath.c_str(), RTLD_LAZY)) == 0)
    {
        /// @throw LoadError Opening of the dynamic library failed.
        throw LoadError("Library::Library", dlerror());
    }
#endif 

#ifdef WIN32
    if(!(handle_ = reinterpret_cast<void*>(LoadLibrary(nativePath.c_str()))))
    {
        throw LoadError("Library::Library", "LoadLibrary failed: '" + nativePath + "'");
    }
#endif

    if(address("deng_LibraryType")) 
    {
        // Query the type identifier.
        type_ = SYMBOL(deng_LibraryType)();
    }
    
    if(type_.beginsWith("deng-plugin/") && address("deng_InitializePlugin"))
    {
        // Automatically call the initialization function, if one exists.
        SYMBOL(deng_InitializePlugin)();
    }
}

Library::~Library()
{
    if(handle_)
    {
        if(type_.beginsWith("deng-plugin/") && address("deng_ShutdownPlugin")) 
        {
            // Automatically call the shutdown function, if one exists.
            SYMBOL(deng_ShutdownPlugin)();
        }

#ifdef UNIX
        // Close the library.
        dlclose(handle_);
#endif

#ifdef WIN32
        FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#endif
    }
}

void* Library::address(const String& name)
{
    if(!handle_)
    {
        return 0;
    }
    
    // Already looked up?
    Symbols::iterator found = symbols_.find(name);
    if(found != symbols_.end())
    {
        return found->second;
    }
    
    void* ptr = 0;

#ifdef UNIX
    ptr = dlsym(handle_, name.c_str());
#endif

#ifdef WIN32
    ptr = reinterpret_cast<void*>(
        GetProcAddress(reinterpret_cast<HMODULE>(handle_), name.c_str()));
#endif 

    if(!ptr)
    {
        /// @throw SymbolMissingError There is no symbol @a name in the library.
        throw SymbolMissingError("Library::symbol", "Symbol '" + name + "' was not found");
    }

    symbols_[name] = ptr;
    return ptr;
}
