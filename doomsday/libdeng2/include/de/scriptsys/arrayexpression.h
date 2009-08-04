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

#ifndef LIBDENG2_ARRAYEXPRESSION_H
#define LIBDENG2_ARRAYEXPRESSION_H

#include "../Expression"

#include <vector>

namespace de  
{
    class Evaluator;
    class Value;
    
    /**
     * Evaluates into an ArrayValue.
     *
     * @ingroup script
     */
    class ArrayExpression : public Expression
    {
    public:
        ArrayExpression();
        ~ArrayExpression();

        void clear();

        /**
         * Adds an argument expression to the array expression.
         *
         * @param arg  Argument expression to add. Ownership transferred
         *             to the array expression.
         */
        void add(Expression* arg);

        void push(Evaluator& evaluator, Record* names = 0) const;

        /**
         * Collects the result values of the arguments and puts them 
         * into an array.
         *
         * @return ArrayValue with the results of the argument evaluations.
         */
        Value* evaluate(Evaluator& evaluator) const;
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);         
        
    private:
        typedef std::vector<Expression*> Arguments;
        Arguments arguments_;
    };
}

#endif /* LIBDENG2_ARRAYEXPRESSION_H */
