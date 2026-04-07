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
 
#ifndef LIBCORE_ASSIGNSTATEMENT_H
#define LIBCORE_ASSIGNSTATEMENT_H

#include "../libcore.h"
#include "statement.h"
#include "arrayexpression.h"
#include <string>
#include <vector>

namespace de {

/**
 * Assigns a value to a variable.
 *
 * @ingroup script
 */
class AssignStatement : public Statement
{
public:
    /// Trying to assign into something other than a reference (RefValue). @ingroup errors
    DE_ERROR(LeftValueError);

    typedef std::vector<Expression *> Indices;

public:
    AssignStatement();

    /**
     * Constructor. Statement takes ownership of the expressions
     * @c target and @c value.
     *
     * @param target  Expression that resolves to a reference (RefValue).
     * @param indices Expressions that determine element indices into existing
     *                element-based values. Empty, if there is no indices for
     *                the assignment.
     * @param value   Expression that determines the value of the variable.
     */
    AssignStatement(Expression *target, const Indices &indices, Expression *value);

    ~AssignStatement();

    void execute(Context &context) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    ArrayExpression _args;
    dint _indexCount;
};

} // namespace de

#endif /* LIBCORE_ASSIGNSTATEMENT_H */
