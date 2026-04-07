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

#ifndef LIBCORE_CONSTANTEXPRESSION_H
#define LIBCORE_CONSTANTEXPRESSION_H

#include "expression.h"
#include "de/value.h"

namespace de {

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
    ConstantExpression(Value *value);

    ~ConstantExpression();

    Value *evaluate(Evaluator &evaluator) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    static ConstantExpression *None();
    static ConstantExpression *True();
    static ConstantExpression *False();
    static ConstantExpression *Pi();

private:
    Value *_value;
};

} // namespace de

#endif /* LIBCORE_CONSTANTEXPRESSION_H */
