/** @file escapeparser.h  Text escape sequence parser.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_ESCAPEPARSER_H
#define LIBDENG2_ESCAPEPARSER_H

#include "../libdeng2.h"
#include "../String"
#include "../Observers"

namespace de {

/**
 * Escape sequence parser for text strings.
 *
 * @see DENG2_ESC() macro
 */
class DENG2_PUBLIC EscapeParser
{
public:
    /**
     * Called during parsing when a plain text range has been parsed.
     *
     * @param range  Range in the original text.
     */
    DENG2_DEFINE_AUDIENCE(PlainText, void handlePlainText(Rangei const &range))

    /**
     * Called during parsing when an escape sequence has been parsed.
     * Does not include the Esc (0x1b) in the beginning.
     *
     * @param range  Range in the original text.
     */
    DENG2_DEFINE_AUDIENCE(EscapeSequence, void handleEscapeSequence(Rangei const &range))

public:
    EscapeParser();

    void parse(String const &textWithEscapes);

    /**
     * Returns the original string that was parsed.
     */
    String originalText() const;

    /**
     * Returns the plain text string. Available after parsing.
     */
    String plainText() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_ESCAPEPARSER_H
