/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#ifndef LIBDENG2_DELETESTATEMENT_H
#define LIBDENG2_DELETESTATEMENT_H

#include "../libdeng2.h"
#include "../Statement"
#include "../ArrayExpression"
#include <string>
#include <vector>

namespace de {

/**
 * Deletes variables.
 *
 * @ingroup script
 */
class DeleteStatement : public Statement
{
public:
    /// Trying to delete something other than a reference (RefValue). @ingroup errors
    DENG2_ERROR(LeftValueError);

public:
    DeleteStatement();

    /**
     * Constructor.
     *
     * @param targets  Expression that resolves to a reference (array of RefValues).
     *                 Statement gets ownership.
     */
    DeleteStatement(ArrayExpression *targets);

    ~DeleteStatement();

    void execute(Context &context) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    ArrayExpression *_targets;
};

} // namespace de

#endif /* LIBDENG2_DELETESTATEMENT_H */
