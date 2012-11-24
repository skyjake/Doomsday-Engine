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

#ifndef LIBDENG2_IFSTATEMENT_H
#define LIBDENG2_IFSTATEMENT_H

#include "../Statement"
#include "../Compound"

#include <list>

namespace de
{
    class Expression;
    
    /**
     * Branching statement for conditionally executing one or more compounds.
     *
     * @ingroup script
     */
    class IfStatement : public Statement
    {
    public:
        ~IfStatement();
        
        void clear();

        /** 
         * Add a new branch to the statement.
         */
        void newBranch();
        
        /** 
         * Sets the condition expression of the latest branch.
         */
        void setBranchCondition(Expression *expression);

        /**
         * Returns the compound of the latest branch.
         */
        Compound &branchCompound();

        /** 
         * Returns the else-compound of the statement.
         */
        Compound &elseCompound() {
            return _elseCompound;
        }

        void execute(Context &context) const;

        // Implements ISerializable.
        void operator >> (Writer &to) const;
        void operator << (Reader &from);         

    private:
        struct Branch {
            Expression *condition;
            Compound *compound;
            Branch(Compound *c = 0) : condition(0), compound(c) {}
        };
        typedef std::list<Branch> Branches;

        Branches _branches;
        Compound _elseCompound;
    };
}

#endif /* LIBDENG2_IFSTATEMENT_H */
