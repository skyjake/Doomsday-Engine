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

#ifndef LIBCORE_IPARSER_H
#define LIBCORE_IPARSER_H

#include "de/string.h"

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
    virtual ~IParser() = default;

    /**
     * Reads an input script in text format, and according to the
     * syntax specified by the parser, creates a set of statements
     * and related atoms in the output Script object.
     *
     * @param input  Input script in text format.
     * @param output  Output Script object.
     */
    virtual void parse(const String &input, Script &output) = 0;
};

} // namespace de

#endif /* LIBCORE_IPARSER_H */
