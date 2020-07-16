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

#ifndef LIBCORE_ARRAYEXPRESSION_H
#define LIBCORE_ARRAYEXPRESSION_H

#include "expression.h"

#include <vector>

namespace de {

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

    dsize size() const { return _arguments.size(); }

    /**
     * Adds an argument expression to the array expression.
     *
     * @param arg  Argument expression to add. Ownership transferred
     *             to the array expression.
     */
    void add(Expression *arg);

    void push(Evaluator &evaluator, Value *scope = 0) const;

    /**
     * Returns one of the expressions in the array.
     *
     * @param pos  Index.
     *
     * @return  Expression.
     */
    const Expression &at(dint pos) const;

    const Expression &front() const { return at(0); }

    const Expression &back() const { return at(size() - 1); }

    /**
     * Collects the result values of the arguments and puts them
     * into an array.
     *
     * @return ArrayValue with the results of the argument evaluations.
     */
    Value *evaluate(Evaluator &evaluator) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    typedef std::vector<Expression *> Arguments;
    Arguments _arguments;
};

} // namespace de

#endif /* LIBCORE_ARRAYEXPRESSION_H */
