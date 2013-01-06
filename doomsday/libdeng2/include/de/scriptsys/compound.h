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
 
 
#ifndef LIBDENG2_COMPOUND_H
#define LIBDENG2_COMPOUND_H

#include "../libdeng2.h"
#include "../ISerializable"

#include <list>

namespace de {

class Statement;

/**
 * A series of statements.
 *
 * @ingroup script
 */
class Compound : public ISerializable
{
public:
    Compound();

    virtual ~Compound();

    Statement const *firstStatement() const;

    /// Determines the size of the compound.
    /// @return Number of statements in the compound.
    duint size() const {
        return _statements.size();
    }

    /**
     * Adds a new statement to the end of the compound. The previous
     * final statement is updated to use this statement as its
     * successor.
     *
     * @param statement  Statement object. The Compound takes ownership
     *                   of the object.
     */
    void add(Statement *statement);

    /**
     * Deletes all statements.
     */
    void clear();

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    typedef std::list<Statement *> Statements;
    Statements _statements;
};

} // namespace de

#endif /* LIBDENG2_COMPOUND_H */
 
