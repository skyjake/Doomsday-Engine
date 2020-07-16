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

#ifndef LIBCORE_PRINTSTATEMENT_H
#define LIBCORE_PRINTSTATEMENT_H

#include "statement.h"

namespace de {

class ArrayExpression;

/**
 * Prints arguments to standard output.
 *
 * @ingroup script
 */
class PrintStatement : public Statement
{
public:
    /**
     * Constructor.
     *
     * @param arguments  Array expression that contains all the arguments
     *                   of the print statement. Ownership transferred to the statement.
     */
    PrintStatement(ArrayExpression *arguments = 0);

    ~PrintStatement();

    void execute(Context &context) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    ArrayExpression *_arg;
};

} // namespace de

#endif /* LIBCORE_PRINTSTATEMENT_H */
