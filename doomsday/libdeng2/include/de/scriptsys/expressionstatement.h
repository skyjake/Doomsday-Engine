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

#ifndef LIBDENG2_EXPRESSIONSTATEMENT_H
#define LIBDENG2_EXPRESSIONSTATEMENT_H

#include "../Statement"

namespace de
{
    class Expression;
    
    /**
     * Evaluates an expression but does not store the result anywhere. An example of 
     * this would be a statement that just does a single method call.
     *
     * @ingroup script
     */
    class ExpressionStatement : public Statement
    {
    public:
        /**
         * Constructs a new expression statement.
         *
         * @param expression  Statement gets ownership.
         */ 
        ExpressionStatement(Expression* expression = 0) : _expression(expression) {}
        
        ~ExpressionStatement();
        
        void execute(Context& context) const;
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);         
        
    private:
        Expression* _expression;
    };
}

#endif /* LIBDENG2_EXPRESSIONSTATEMENT_H */
