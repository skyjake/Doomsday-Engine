/*
 * The Doomsday Engine Project -- Hawthorn
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

#ifndef DEXPRESSIONSTATEMENT_HH
#define DEXPRESSIONSTATEMENT_HH

#include "dstatement.hh"

namespace de
{
    class Expression;
    
/**
 * ExpressionStatement is a Statement that evaluates an expression but
 * does not store the result anywhere. An example of this would be a 
 * statement that just does a single method call.
 */
    class ExpressionStatement : public Statement
    {
    public:
        ExpressionStatement(Expression* expression) : expression_(expression) {}
        
        ~ExpressionStatement();
        
        void execute(Context& context) const;
        
    private:
        Expression* expression_;
    };
}

#endif /* DEXPRESSIONSTATEMENT_HH */
