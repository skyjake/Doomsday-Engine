/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_LIBRARY_H
#define LIBDENG2_LIBRARY_H

#include "../libdeng2.h"
#include "../NativePath"

#include <QLibrary>
#include <QMap>

/**
 * Convenience macro for accessing symbols that have a type defined in de::Library
 * with the type name matching the symbol name.
 */
#define DENG2_SYMBOL(Name) symbol<de::Library::Name>(#Name)

namespace de
{
    class Audio;
    class Map;
    class Object;
    class User;
    class World;
    
    /**
     * The Library class allows loading shared library files
     * (DLL/so/bundle/dylib) and looks up exported symbols in the libraries.
     *
     * Library type identifiers;
     * - <code>library/generic</code>: A shared library with no special function.
     * - <code>deng-plugin/generic</code>: Generic libdeng2 plugin. Loaded always.
     * - <code>deng-plugin/game</code>: The game plugin. Only one of these can be loaded.
     * - <code>deng-plugin/audio</code>: Audio driver. Optional. Loaded on demand by
     *   the audio subsystem.
     *
     * @ingroup core
     */
    class DENG2_PUBLIC Library
    {
    public:
        /// Loading the shared library failed. @ingroup errors
        DENG2_ERROR(LoadError);
        
        /// A symbol was not found. @ingroup errors
        DENG2_ERROR(SymbolMissingError);
        
        /// Default type identifier.
        static const char* DEFAULT_TYPE;
        
        // Common function profiles.
        /**
         * Queries the plugin for a type identifier string. If this function is not
         * defined, the identifier defaults to DEFAULT_TYPE.
         *
         * @return  Type identifier string.
         */
        typedef const char* (*deng_LibraryType)(void);
        
        /**
         * Performs any one-time initialization necessary for the usage of the plugin.
         * If this symbol is exported from a shared library, it gets called automatically
         * when the library is loaded. 
         */
        typedef void (*deng_InitializePlugin)(void);
        
        /**
         * Frees resources reserved by the plugin. If this symbol is exported from a 
         * shared library, it gets called automatically when the library is unloaded.
         */
        typedef void (*deng_ShutdownPlugin)(void);
        
        /**
         * Constructs a new instance of an audio subsystem.
         *
         * @return  Audio subsystem. 
         */
        typedef Audio* (*deng_NewAudio)(void);
        
        /**
         * Constructs a new game world.
         */
        typedef World* (*deng_NewWorld)(void);
        
        /**
         * Constructs a new game map.
         */
        typedef Map* (*deng_NewMap)();
        
        /**
         * Constructs a new object.
         */
        typedef Object* (*deng_NewObject)(void);
        
        /**
         * Constructs a new user.
         */
        typedef User* (*deng_NewUser)(void);
        
        typedef dint (*deng_GetInteger)(dint id);
        typedef const char* (*deng_GetString)(dint id);
        typedef void* (*deng_GetAddress)(dint id);
        typedef void (*deng_Ticker)(ddouble tickLength);
        
    public:
        /**
         * Constructs a new Library by loading a native shared library.
         * 
         * @param nativePath  Path of the shared library to load.
         */
        Library(const NativePath& nativePath);
        
        /**
         * Unloads the shared library.
         */
        virtual ~Library();

        /**
         * Returns the type identifier of the library. This affects how
         * libdeng2 will treat the library. The type is determined
         * automatically when the library is first loaded, and can then be
         * queried at any time even after the library has been unloaded.
         */
        const String& type() const;

        enum SymbolLookupMode {
            RequiredSymbol, ///< Symbol must be exported.
            OptionalSymbol  ///< Symbol can be missing.
        };

        /**
         * Gets the address of an exported symbol. Throws an exception if a required
         * symbol is not found.
         *
         * @param name    Name of the exported symbol.
         * @param lookup  Lookup mode (required or optional).
         *
         * @return  A pointer to the symbol. Returns @c NULL if an optional symbol is
         * not found.
         */
        void* address(const String& name, SymbolLookupMode lookup = RequiredSymbol);

        /**
         * Checks if the library exports a specific symbol.
         * @param name  Name of the exported symbol.
         * @return @c true if the symbol is exported, @c false if not.
         */
        bool hasSymbol(const String& name) const;

        /**
         * Gets the address of a symbol. Throws an exception if a required symbol
         * is not found.
         *
         * @param name    Name of the symbol.
         * @param lookup  Symbol lookup mode (required or optional).
         *
         * @return Pointer to the symbol as type @a Type.
         */
        template <typename Type>
        Type symbol(const String& name, SymbolLookupMode lookup = RequiredSymbol) {
            /**
             * @note Casting to a pointer-to-function type: see
             * http://www.trilithium.com/johan/2004/12/problem-with-dlsym/
             */
            // This is not 100% portable to all possible memory architectures; thus:
            DENG2_ASSERT(sizeof(void*) == sizeof(Type));

            union { void* original; Type target; } forcedCast;
            forcedCast.original = address(name, lookup);
            return forcedCast.target;
        }

        /**
         * Utility template for acquiring pointers to symbols. Throws an exception
         * if a required symbol is not found.
         *
         * @param ptr     Pointer that will be set to point to the symbol's address.
         * @param name    Name of the symbol whose address to get.
         * @param lookup  Symbol lookup mode (required or optional).
         *
         * @return @c true, if the symbol was found. Otherwise @c false.
         */
        template <typename Type>
        bool setSymbolPtr(Type& ptr, const String& name, SymbolLookupMode lookup = RequiredSymbol) {
            ptr = symbol<Type>(name, lookup);
            return ptr != 0;
        }

    private:  
        struct Instance;
        Instance* d;
    };
}

#endif /* LIBDENG2_LIBRARY_H */
