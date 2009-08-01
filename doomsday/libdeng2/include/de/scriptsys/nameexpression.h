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

#ifndef LIBDENG2_NAMEEXPRESSION_H
#define LIBDENG2_NAMEEXPRESSION_H

#include "../Expression"
#include "../Flag"

namespace de
{
    /**
     * Locates a variable in the namespaces, and creates an appropriate value to
     * refer to that node.
     */
    class NameExpression : public Expression
    {
    public:
        /// Identifier is not text. @ingroup errors
        DEFINE_ERROR(IdentifierError);

        /// Variable already exists when it was required not to. @ingroup errors
        DEFINE_ERROR(AlreadyExistsError);

        /// The identifier does not specify an existing variable. @ingroup errors
        DEFINE_ERROR(NotFoundError);

        /// Evaluates to a value.
        DEFINE_FLAG(BY_VALUE, 0);
        
        /// Evaluates to a reference.
        DEFINE_FLAG(BY_REFERENCE, 1);
        
        /// Look for object in local namespace only. 
        DEFINE_FLAG(LOCAL_ONLY, 2);
        
        /// If missing, create a new variable.
        DEFINE_FLAG(NEW_VARIABLE, 3);

        /// Must create a new variable.
        DEFINE_FINAL_FLAG(NOT_IN_SCOPE, 4, Flags);
        
    public:
        NameExpression(Expression* identifier, const Flags& flags = BY_VALUE);
        ~NameExpression();

        void push(Evaluator& evaluator, Record* names = 0);
        
        Value* evaluate(Evaluator& evaluator) const;
        
    private:
        Expression* identifier_;
        Flags flags_;
    };
}

#endif /* LIBDENG2_NAMEEXPRESSION_H */
