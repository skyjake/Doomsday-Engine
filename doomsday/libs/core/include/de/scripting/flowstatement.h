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
 
#ifndef LIBCORE_FLOWSTATEMENT_H
#define LIBCORE_FLOWSTATEMENT_H

#include "statement.h"

namespace de {

class Expression;

/**
 * Controls the script's flow of execution.
 *
 * @ingroup script
 */
class FlowStatement : public Statement
{
public:
    /// Type of control flow operation.
    enum Type {
        PASS,
        CONTINUE,
        BREAK,
        RETURN,
        THROW
    };

public:
    FlowStatement();

    FlowStatement(Type type, Expression *countArgument = 0);

    ~FlowStatement();

    void execute(Context &context) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Type _type;
    Expression *_arg;
};

} // namespace de

#endif /* LIBCORE_FLOWSTATEMENT_H */
