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

#ifndef LIBDENG2_PRINTSTATEMENT_H
#define LIBDENG2_PRINTSTATEMENT_H

#include "../Statement"

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

#endif /* LIBDENG2_PRINTSTATEMENT_H */
