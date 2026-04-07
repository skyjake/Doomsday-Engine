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

#ifndef LIBCORE_NAMEEXPRESSION_H
#define LIBCORE_NAMEEXPRESSION_H

#include "expression.h"
#include "de/string.h"

namespace de {

/**
 * Responsible for referencing, creating, and deleting variables and record
 * references based an textual identifier.
 *
 * @ingroup script
 */
class NameExpression : public Expression
{
public:
    /// Identifier is not text. @ingroup errors
    DE_ERROR(IdentifierError);

    /// Variable already exists when it was required not to. @ingroup errors
    DE_ERROR(AlreadyExistsError);

    /// The identifier does not specify an existing variable. @ingroup errors
    DE_ERROR(NotFoundError);

    /// Special scope that can be specified in the constructor to tell the
    /// expression to start looking in the context's local namespace.
    static const char *LOCAL_SCOPE;

public:
    NameExpression();
    NameExpression(const String &identifier, Flags flags = ByValue);
    NameExpression(const StringList &identifierSeqeunce, Flags flags = ByValue);

    /// Returns the identifier in the name expression.
    const String &identifier() const;

    Value *evaluate(Evaluator &evaluator) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_NAMEEXPRESSION_H */
