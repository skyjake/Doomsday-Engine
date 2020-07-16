/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBCORE_DICTIONARYEXPRESSION_H
#define LIBCORE_DICTIONARYEXPRESSION_H

#include "expression.h"

#include <vector>

namespace de {

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

    void push(Evaluator &evaluator, Value *scope = 0) const;

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

} // namespace de

#endif /* LIBCORE_DICTIONARYEXPRESSION_H */
