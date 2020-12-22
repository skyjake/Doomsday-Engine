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

#ifndef LIBCORE_FUNCTIONSTATEMENT_H
#define LIBCORE_FUNCTIONSTATEMENT_H

#include "statement.h"
#include "function.h"
#include "compound.h"
#include "dictionaryexpression.h"
#include "de/string.h"

#include <list>

namespace de {

class Expression;

/**
 * Creates a new function.
 *
 * @ingroup script
 */
class FunctionStatement : public Statement
{
public:
    FunctionStatement(Expression *identifier = 0);

    ~FunctionStatement();

    void addArgument(const String &argName, Expression *defaultValue = 0);

    /// Returns the statement compound of the function.
    Compound &compound();

    void execute(Context &context) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Expression *_identifier;

    // The statement holds one reference to the function.
    Function *_function;

    /// Expression that evaluates into the default values of the method.
    DictionaryExpression _defaults;
};

} // namespace de

#endif /* LIBCORE_FUNCTIONSTATEMENT_H */
