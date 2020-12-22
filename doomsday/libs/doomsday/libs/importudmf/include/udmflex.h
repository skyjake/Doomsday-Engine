/** @file udmflex.h  UDMF lexical analyzer.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef IMPORTUDMF_UDMFLEX_H
#define IMPORTUDMF_UDMFLEX_H

#include <de/scripting/lex.h>
#include <de/scripting/tokenbuffer.h>

class UDMFLex : public de::Lex
{
public:
    // Keywords.
    static de::String const NAMESPACE;
    static de::String const LINEDEF;
    static de::String const SIDEDEF;
    static de::String const VERTEX;
    static de::String const SECTOR;
    static de::String const THING;
    static de::String const T_TRUE;
    static de::String const T_FALSE;

    // Operators.
    static de::String const ASSIGN;

    // Literals.
    static de::String const BRACKET_OPEN;
    static de::String const BRACKET_CLOSE;
    static de::String const SEMICOLON;

public:
    UDMFLex(const de::String &input = "");

    /**
     * Reads tokens from the source until the end of an expression. The opening bracket
     * of a block ends an expression, and the closing bracket is treated as an expression
     * of its own.
     *
     * @param output  The read tokens.
     *
     * @return Number of tokens in the @a output buffer.
     */
    de::dsize getExpressionFragment(de::TokenBuffer &output);

    /**
     * Parse a string.
     *
     * @param output  Output token buffer.
     */
    void parseString(de::TokenBuffer &output);

    /// Determines whether a token is a keyword.
    static bool isKeyword(const de::Token &token);
};

#endif // IMPORTUDMF_UDMFLEX_H
