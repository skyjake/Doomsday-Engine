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

#ifndef DCONSTANTEXPRESSION_HH
#define DCONSTANTEXPRESSION_HH

#include "dexpression.hh"
#include "dvalue.hh"

namespace de
{
/**
 * ConstantExpression is an expression that always evaluates to a constant
 * value. It is used for storing the constant values in scripts. 
 */
    class ConstantExpression : public Expression
    {
    public:
        /**
         * Constructor. 
         * @param value Value of the expression. The expression takes 
         * ownership of the value object.
         */ 
        ConstantExpression(Value* value);
        
        ~ConstantExpression();
        
        Value* evaluate(Evaluator& evaluator) const;
        
    public:
        static ConstantExpression* None();        
        static ConstantExpression* True();
        static ConstantExpression* False();

    private:
        Value* value_;
    };
}

#endif /* DCONSTANTEXPRESSION_HH */
