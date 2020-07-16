/** @file udmflex.cpp  UDMF lexical analyzer.
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

#include "udmflex.h"

#include <de/set.h>

using namespace de;

const String UDMFLex::NAMESPACE("namespace");
const String UDMFLex::LINEDEF("linedef");
const String UDMFLex::SIDEDEF("sidedef");
const String UDMFLex::VERTEX("vertex");
const String UDMFLex::SECTOR("sector");
const String UDMFLex::THING("thing");
const String UDMFLex::T_TRUE("true");
const String UDMFLex::T_FALSE("false");
const String UDMFLex::ASSIGN("=");
const String UDMFLex::BRACKET_OPEN("{");
const String UDMFLex::BRACKET_CLOSE("}");
const String UDMFLex::SEMICOLON(";");

UDMFLex::UDMFLex(const String &input)
    : Lex(input, '/', '*', DoubleCharComment | NegativeNumbers)
{}

dsize UDMFLex::getExpressionFragment(TokenBuffer &output)
{
    output.clear();

    while (!atEnd())
    {
        skipWhite();

        if (atEnd() || (output.size() && peek() == '}')) break;

        // First character of the token.
        Char c = get();

        output.newToken(lineNumber());
        output.appendChar(c);

        // Single-character tokens.
        if (c == '{' || c == '}' || c == '=' || c == ';')
        {
            output.setType(c == '='? Token::OPERATOR : Token::LITERAL);
            output.endToken();

            if (output.latest().type() != Token::OPERATOR) break;
            continue;
        }

        if (c == '"')
        {
            // Parse the string into one token.
            output.setType(Token::LITERAL_STRING_QUOTED);
            parseString(output);
            output.endToken();
            continue;
        }

        // Number literal?
        if (parseLiteralNumber(c, output))
        {
            continue;
        }

        // Alphanumeric characters are joined into a token.
        if (c == '_' || isAlpha(c))
        {
            output.setType(Token::IDENTIFIER);

            while (isAlphaNumeric((c = peek())))
            {
                output.appendChar(get());
            }

            // It might be that this is a keyword.
            if (isKeyword(output.latest()))
            {
                output.setType(Token::KEYWORD);
            }

            output.endToken();
            continue;
        }
    }

    return output.size();
}

void UDMFLex::parseString(TokenBuffer &output)
{
    ModeSpan readingMode(*this, RetainComments);

    // The token already contains the first quote char.
    // This will throw an exception if the string is unterminated.
    for (;;)
    {
        Char c = get();
        output.appendChar(c);
        if (c == '"')
        {
            return;
        }
        if (c == '\\') // Escape.
        {
            output.appendChar(get());
        }
    }
}

bool UDMFLex::isKeyword(const Token &token)
{
    static const StringList keywords{
        NAMESPACE,
        LINEDEF,
        SIDEDEF,
        VERTEX,
        SECTOR,
        THING,
        T_TRUE,
        T_FALSE,
    };
    return String::contains(keywords, token.str(), CaseInsensitive);
}
