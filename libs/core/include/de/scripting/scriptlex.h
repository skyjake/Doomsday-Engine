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

#ifndef LIBCORE_SCRIPTLEX_H
#define LIBCORE_SCRIPTLEX_H

#include "lex.h"
#include "tokenbuffer.h"
#include "de/set.h"

namespace de {

#ifdef WIN32
#  undef IN
#  undef CONST
#endif

/**
 * Lexical analyzer specific to Doomsday scripts.
 *
 * @ingroup script
 */
class DE_PUBLIC ScriptLex : public Lex
{
public:
    /// Base error for syntax errors at the level of lexical analysis (e.g.,
    /// a non-terminated string constant). @ingroup errors
    DE_ERROR(SyntaxError);

    /// A unexpected character is encountered. @ingroup errors
    DE_SUB_ERROR(SyntaxError, UnexpectedCharacterError);

    /// An unterminated string token is encountered. @ingroup errors
    DE_SUB_ERROR(SyntaxError, UnterminatedStringError);

    /// The bracket level goes below zero, i.e., when more brackets are closed
    /// than opened, or when the input ends before all brackets are closed. @ingroup errors
    DE_SUB_ERROR(SyntaxError, MismatchedBracketError);

    // Keywords.
    static const char *AND;
    static const char *OR;
    static const char *NOT;
    static const char *ELSIF;
    static const char *ELSE;
    static const char *THROW;
    static const char *CATCH;
    static const char *IN;
    static const char *END;
    static const char *IF;
    static const char *WHILE;
    static const char *FOR;
    static const char *DEF;
    static const char *TRY;
    static const char *IMPORT;
    static const char *RECORD;
    static const char *SCOPE;
    static const char *DEL;
    static const char *PASS;
    static const char *CONTINUE;
    static const char *BREAK;
    static const char *RETURN;
    static const char *PRINT;
    static const char *CONST;
    static const char *T_TRUE;
    static const char *T_FALSE;
    static const char *NONE;
    static const char *PI;

    // Operators.
    static const char *ASSIGN;
    static const char *SCOPE_ASSIGN;
    static const char *WEAK_ASSIGN;

    enum Behavior
    {
        DefaultBehavior = 0,
        StopAtMismatchedCloseBrace = 0x1 ///< Mismatched } is treated as end of input.
    };
    using Behaviors = Behavior;

public:
    ScriptLex(const String &input = "");

    /**
     * Analyze one complete statement from the input.
     *
     * @param output    Buffer for output tokens.
     * @param behavior  Parsing behavior.
     *
     * @return  The number of tokens added to the output token buffer.
     */
    duint getStatement(TokenBuffer &output, const Behaviors &behavior = DefaultBehavior);

    /**
     * Parse a string.
     *
     * @param startChar         Character that begun the string. This is already
     *                          in the token.
     * @param startIndentation  Indentation level for the line that starts the token.
     * @param output            Output token buffer.
     *
     * @return Type of the parsed string.
     */
    Token::Type parseString(Char startChar, duint startIndentation, TokenBuffer &output);

public:
    /// Determines whether a character is an operator character.
    static bool isOperator(Char c);

    /// Determines whether a token is a Haw script keyword.
    static bool isKeyword(const Token &token);

    /// Returns a list of all the keywords.
    static StringList keywords();

    /// Determines whether one character should join another to
    /// form a longer token.
    static bool combinesWith(Char a, Char b);

private:
    static const Set<CString> KEYWORDS;
};

} // namespace de

#endif /* LIBCORE_SCRIPTLEX_H */
