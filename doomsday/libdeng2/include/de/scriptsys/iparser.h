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

#ifndef LIBDENG2_IPARSER_H
#define LIBDENG2_IPARSER_H

#include "../String"

namespace de {

class Script;

/**
 * Interface for objects responsible for reading a script in text form and
 * producing an executable Script object, with Statements and related
 * atoms.
 *
 * This is an interface for parsers. A concrete Parser implementation
 * uses a specific syntax for the input text.
 *
 * @ingroup script
 */
class IParser
{
public:
    virtual ~IParser() {}

    /**
     * Reads an input script in text format, and according to the
     * syntax specified by the parser, creates a set of statements
     * and related atoms in the output Script object.
     *
     * @param input  Input script in text format.
     * @param output  Output Script object.
     */
    virtual void parse(String const &input, Script &output) = 0;
};

} // namespace de

#endif /* LIBDENG2_IPARSER_H */
