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

#ifndef LIBDENG2_FUNCTIONSTATEMENT_H
#define LIBDENG2_FUNCTIONSTATEMENT_H

#include "de/Statement"
#include "de/Function"
#include "de/Compound"
#include "de/DictionaryExpression"
#include "de/String"

#include <list>

namespace de
{
    class Expression;
    
    /**
     * Creates a new function.
     *
     * @ingroup script
     */
    class FunctionStatement : public Statement
    {
    public:
        FunctionStatement(Expression *identifier = 0);
        
        ~FunctionStatement();

        void addArgument(String const &argName, Expression *defaultValue = 0);

        /// Returns the statement compound of the function.
        Compound &compound();
        
        void execute(Context &context) const;
        
        // Implements ISerializable.
        void operator >> (Writer &to) const;
        void operator << (Reader &from);         

    private:
        Expression *_identifier;
        
        // The statement holds one reference to the function.
        Function *_function;
    
        /// Expression that evaluates into the default values of the method.
        DictionaryExpression _defaults;
    };
}

#endif /* LIBDENG2_FUNCTIONSTATEMENT_H */
