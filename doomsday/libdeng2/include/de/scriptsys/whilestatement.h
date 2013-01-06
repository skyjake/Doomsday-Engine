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

#ifndef LIBDENG2_WHILESTATEMENT_H
#define LIBDENG2_WHILESTATEMENT_H

#include "../Statement"
#include "../Compound"

namespace de {

class Expression;

/**
 * Keeps looping as long as the loop condition evaluates to True.
 *
 * @ingroup script
 */
class WhileStatement : public Statement
{
public:
    WhileStatement() : _loopCondition(0) {}
    ~WhileStatement();

    void setCondition(Expression *condition) {
        _loopCondition = condition;
    }

    Compound &compound() {
        return _compound;
    }

    void execute(Context &context) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Expression *_loopCondition;
    Compound _compound;
};

} // namespace de

#endif /* LIBDENG2_WHILESTATEMENT_H */
