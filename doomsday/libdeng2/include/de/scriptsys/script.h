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

#ifndef LIBDENG2_SCRIPT_H
#define LIBDENG2_SCRIPT_H

/**
 * @defgroup script Script Engine
 *
 * The script engine parses and executes scripts.
 */

#include "../Compound"
#include "../String"

#include <vector>

namespace de
{
    class File;
    
    /**
     * Contains statements and expressions which are ready to be executed.
     * A Script instance is built from source code text, which is parsed and
     * converted to statements and expressions. 
     *
     * @see Process (used to execute scripts)
     *
     * @ingroup script
     */
    class LIBDENG2_API Script
    {
    public:
        /// Constructs an empty script with no statements.
        Script();
        
        /**
         * Parses the source into statements.
         *
         * @param source  Script source.
         */
        Script(const String& source);
        
        /**
         * Parses the source file info statements. The path of the source file
         * is saved and used when importing modules.
         *
         * @param file  Source file.
         */
        Script(const File& file);
        
        virtual ~Script();

        const String& path() const { return _path; }

        /// Returns the statement that begins the script. This is where
        /// a process begins the execution of a script.
        const Statement* firstStatement() const;

        /// Returns a modifiable reference to the main statement compound
        /// of the script.
        Compound& compound() {
            return _compound;
        }
        
    private:
        Compound _compound;
        
        /// File path where the script was loaded. Will be visible in the namespace
        /// of the process executing the script.
        String _path;
    };
}

#endif
