/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#ifndef LIBDENG2_MODULE_H
#define LIBDENG2_MODULE_H

#include "../String"

namespace de
{
    class File;
    class Process;
    class Record;
    class Script;
    
    /**
     * Modules are scripts that are loaded from the file system and then stay
     * in memory for other scripts to use. For instance, a module could contain
     * functions that are useful for many other scripts. Scripts can use the 
     * import statement to gain access to a module's namespace. The module's
     * source script is executed only the first time it gets imported -- 
     * subsequent calls simply provide a reference to the existing module's namespace.
     *
     * @ingroup script
     */
    class Module
    {
    public:
        /**
         * Constructs a new module. The source script is loaded from a 
         * file and executed.
         *
         * @param sourcePath  Path of the source file.
         */
        Module(String const &sourcePath);
        
        /**
         * Constructs a new module. The source script is loaded from a 
         * file and executed.
         *
         * @param sourceFile  Script source file.
         */
        Module(File const &sourceFile);
        
        virtual ~Module();
        
        /// Returns the module's source script's absolute path.
        String const &sourcePath() const { return _sourcePath; }
        
        /// Returns the namespace of the module. The import statement gives access
        /// to this.
        Record &names();

    protected:
        void initialize(Script const &script);
        
    private:
        /// Path of the script source file. Used to identify whether a script has
        /// already been loaded or not. Imported scripts are run only once.
        String _sourcePath;
        
        /// Process where the source script of the module was executed. Owns
        /// the namespace of the module.
        Process *_process;
    };
}

#endif /* LIBDENG2_MODULE_H */
