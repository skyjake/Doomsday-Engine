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
 
 
#ifndef LIBDENG2_COMPOUND_H
#define LIBDENG2_COMPOUND_H

#include "../deng.h"

#include <list>

namespace de
{
    class Statement;
    
    /**
     * A series of statements.
     */
    class Compound
    {
    public:
        Compound();

        virtual ~Compound();

        const Statement* firstStatement() const;
        
        /// Determines the size of the compound.
        /// @return Number of statements in the compound.
        duint size() const {
            return statements_.size();
        }
        
        /**
         * Adds a new statement to the end of the compound. The previous
         * final statement is updated to use this statement as its 
         * successor. 
         *
         * @param statement  Statement object. The Compound takes ownership
         *                   of the object.
         */
        void add(Statement* statement);
        
    private:
        typedef std::list<Statement*> Statements;
        Statements statements_;
    };
}

#endif /* LIBDENG2_COMPOUND_H */
 