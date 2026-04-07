/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#ifndef LIBCORE_DELETESTATEMENT_H
#define LIBCORE_DELETESTATEMENT_H

#include "../libcore.h"
#include "statement.h"
#include "arrayexpression.h"
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
    DE_ERROR(LeftValueError);

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

#endif /* LIBCORE_DELETESTATEMENT_H */
