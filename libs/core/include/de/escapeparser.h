/** @file escapeparser.h  Text escape sequence parser.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ESCAPEPARSER_H
#define LIBCORE_ESCAPEPARSER_H

#include "de/libcore.h"
#include "de/string.h"
#include "de/observers.h"

namespace de {

/**
 * Escape sequence parser for text strings. @ingroup data
 *
 * @see DE_ESC() macro
 */
class DE_PUBLIC EscapeParser
{
public:
    /**
     * Called during parsing when a plain text range has been parsed.
     *
     * @param range  Range in the original text.
     */
    DE_AUDIENCE(PlainText, void handlePlainText(const CString &))

    /**
     * Called during parsing when an escape sequence has been parsed.
     * Does not include the Esc (0x1b) in the beginning.
     *
     * @param range  Range in the original text.
     */
    DE_AUDIENCE(EscapeSequence, void handleEscapeSequence(const CString &))

public:
    EscapeParser();

    void parse(const String &textWithEscapes);

    /**
     * Returns the original string that was parsed.
     */
    String originalText() const;

    /**
     * Returns the plain text string. Available after parsing.
     */
    String plainText() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_ESCAPEPARSER_H
