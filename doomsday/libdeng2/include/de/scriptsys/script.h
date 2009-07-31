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
    /**
     * Contains statements and expressions which are ready to be executed.
     * A Script instance is built from a the source code text, which is parsed and
     * converted to statements and expressions. Process objects are used to 
     * execute scripts.
     *
     * @ingroup script
     */
    class Script
    {
    public:
        /// Constructs an empty script with no statements.
        Script();
        
        /// Parses the source into statements.
        Script(const String& source);
        
        virtual ~Script();

        /// Returns the statement that begins the script. This is where
        /// a process begins the execution of a script.
        const Statement* firstStatement() const;

        /// Returns a modifiable reference to the main statement compound
        /// of the script.
        Compound& compound() {
            return compound_;
        }
        
    private:
        Compound compound_;
    };
}

#endif
