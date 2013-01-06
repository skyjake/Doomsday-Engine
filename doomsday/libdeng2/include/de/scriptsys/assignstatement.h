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
 
#ifndef LIBDENG2_ASSIGNSTATEMENT_H
#define LIBDENG2_ASSIGNSTATEMENT_H

#include "../libdeng2.h"
#include "../Statement"
#include "../ArrayExpression"
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
    DENG2_ERROR(LeftValueError);

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
    AssignStatement(Expression *target, Indices const &indices, Expression *value);

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

#endif /* LIBDENG2_ASSIGNSTATEMENT_H */
