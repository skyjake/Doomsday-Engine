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

#ifndef LIBDENG2_CONSTANTEXPRESSION_H
#define LIBDENG2_CONSTANTEXPRESSION_H

#include "../Expression"
#include "../Value"

namespace de
{
    /**
     * An expression that always evaluates to a constant value. It is used for
     * storing constants in scripts. 
     *
     * @ingroup script
     */
    class ConstantExpression : public Expression
    {
    public:
        ConstantExpression();
        
        /**
         * Constructor. 
         *
         * @param value  Value of the expression. The expression takes 
         *               ownership of the value object.
         */ 
        ConstantExpression(Value* value);
        
        ~ConstantExpression();
        
        Value* evaluate(Evaluator& evaluator) const;

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);         
        
    public:
        static ConstantExpression* None();        
        static ConstantExpression* True();
        static ConstantExpression* False();
        static ConstantExpression* Pi();

    private:
        Value* _value;
    };
}

#endif /* LIBDENG2_CONSTANTEXPRESSION_H */
