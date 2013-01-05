/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_DICTIONARYEXPRESSION_H
#define LIBDENG2_DICTIONARYEXPRESSION_H

#include "../Expression"

#include <vector>

namespace de
{
    /**
     * Evaluates arguments and forms a dictionary out of the results.
     *
     * @ingroup script
     */
    class DictionaryExpression : public Expression
    {
    public:
        DictionaryExpression();
        ~DictionaryExpression();
        
        void clear();

        /**
         * Adds an key/value pair to the array expression. Ownership of
         * the expressions is transferred to the expression.
         *
         * @param key  Evaluates the key.
         * @param value  Evaluates the value. 
         */
        void add(Expression *key, Expression *value);

        void push(Evaluator &evaluator, Record *names = 0) const;

        /**
         * Collects the result keys and values of the arguments and puts them 
         * into a dictionary.
         *
         * @param evaluator  Evaluator.
         *
         * @return DictionaryValue with the results of the argument evaluations.
         */
        Value *evaluate(Evaluator &evaluator) const;

        // Implements ISerializable.
        void operator >> (Writer &to) const;
        void operator << (Reader &from);         

    private:
        typedef std::pair<Expression *, Expression *> ExpressionPair;
        typedef std::vector<ExpressionPair> Arguments;
        Arguments _arguments;
    };
}

#endif /* LIBDENG2_DICTIONARYEXPRESSION_H */
