/** @file scopestatement.h  Scope statement.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_SCOPESTATEMENT_H
#define LIBCORE_SCOPESTATEMENT_H

#include "statement.h"
#include "expression.h"
#include "compound.h"
#include "de/string.h"

namespace de {

/**
 * Class record declaration. The compound statement is executed in a specific
 * namespace.
 *
 * @ingroup script
 */
class ScopeStatement : public Statement
{
public:
    ScopeStatement();
    ScopeStatement(Expression *identifier, Expression *superRecords);

    /// Returns the compound of the statement.
    Compound &compound();

    void execute(Context &context) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_SCOPESTATEMENT_H
