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

#include "de/scripting/scriptlex.h"
#include "de/scripting/tokenbuffer.h"
#include "de/vector.h"
#include "de/set.h"

using namespace de;

const char *ScriptLex::AND("and");
const char *ScriptLex::OR("or");
const char *ScriptLex::NOT("not");
const char *ScriptLex::IF("if");
const char *ScriptLex::ELSIF("elsif");
const char *ScriptLex::ELSE("else");
const char *ScriptLex::END("end");
const char *ScriptLex::THROW("throw");
const char *ScriptLex::CATCH("catch");
const char *ScriptLex::IN("in");
const char *ScriptLex::WHILE("while");
const char *ScriptLex::FOR("for");
const char *ScriptLex::DEF("def");
const char *ScriptLex::TRY("try");
const char *ScriptLex::IMPORT("import");
const char *ScriptLex::RECORD("record");
const char *ScriptLex::SCOPE("->");
const char *ScriptLex::DEL("del");
const char *ScriptLex::PASS("pass");
const char *ScriptLex::CONTINUE("continue");
const char *ScriptLex::BREAK("break");
const char *ScriptLex::RETURN("return");
const char *ScriptLex::CONST("const");
const char *ScriptLex::PRINT("print");
const char *ScriptLex::T_TRUE("True");
const char *ScriptLex::T_FALSE("False");
const char *ScriptLex::NONE("None");
const char *ScriptLex::PI("Pi");
const char *ScriptLex::ASSIGN("=");
const char *ScriptLex::SCOPE_ASSIGN(":=");
const char *ScriptLex::WEAK_ASSIGN("?=");

ScriptLex::ScriptLex(const String &input) : Lex(input)
{}

duint ScriptLex::getStatement(TokenBuffer &output, const Behaviors &behavior)
{
    // Get rid of the previous contents of the token buffer.
    output.clear();

    duint counter = 0; // How many tokens have we added?

    enum BracketType {
        BRACKET_PARENTHESIS,
        BRACKET_SQUARE,
        BRACKET_CURLY,
        MAX_BRACKETS
    };
    dint bracketLevel[MAX_BRACKETS] = { 0, 0, 0 };

    // Skip any whitespace before the beginning of the statement.
    skipWhite();

    // We have arrived at a non-white token. What is our indentation
    // for this statement?
    duint indentation = countLineStartSpace();

    // Now we can start forming tokens until we arrive at a non-escaped
    // newline. Also, the statement does not end until all braces and
    // parenthesis have been closed.
    while (!atEnd())
    {
        // Tokens are primarily separated by whitespace.
        skipWhiteExceptNewline();

        if ((behavior & StopAtMismatchedCloseBrace) &&
            !bracketLevel[BRACKET_CURLY] &&
            peek() == '}')
        {
            // Don't read past the bracket.
            break;
        }

        if (!peek()) break;

        // This will be the first character of the token.
        Char c = get();

        if (c == '\n' || c == ';')
        {
            // A statement ending character? Open brackets prevent the statement
            // from ending here.
            if (Vec3i(bracketLevel).max() > 0)
                continue;
            else
                break;
        }

        output.newToken(lineNumber());

        if (c == '\\')
        {
            // An escaped newline?
            if (onlyWhiteOnLine())
            {
                skipToNextLine();
                continue;
            }
        }

        output.appendChar(c);

        if (c == '"' || c == '\'')
        {
            // Read an entire string constant into the token.
            // The type of the token is also determined.
            output.setType(parseString(c, indentation, output));

            // The string token is complete.
            output.endToken();
            counter++;
            continue;
        }

        // Is it a number literal?
        if (parseLiteralNumber(c, output))
        {
            counter++;
            continue;
        }

        // Alphanumeric characters are joined into a token.
        if (isAlphaNumeric(c))
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
            counter++;
            continue;
        }

        // The scope keyword.
        if (c == '-' && peek() == '>')
        {
            output.setType(Token::KEYWORD);
            output.appendChar(get());
            output.endToken();
            counter++;
            continue;
        }

        if (isOperator(c))
        {
            output.setType(Token::OPERATOR);

            if (combinesWith(c, peek()))
            {
                output.appendChar(get());
                /// @todo  Three-character tokens: >>=, <<=
            }
            else
            {
                // Keep score of bracket levels, since they prevent
                // newlines from ending the statement.
                if (c == '(') bracketLevel[BRACKET_PARENTHESIS]++;
                if (c == ')') bracketLevel[BRACKET_PARENTHESIS]--;
                if (c == '[') bracketLevel[BRACKET_SQUARE]++;
                if (c == ']') bracketLevel[BRACKET_SQUARE]--;
                if (c == '{') bracketLevel[BRACKET_CURLY]++;
                if (c == '}') bracketLevel[BRACKET_CURLY]--;

                if (Vec3i(bracketLevel).min() < 0)
                {
                    // Very unusual!
                    throw MismatchedBracketError("ScriptLex::getStatement",
                                                 "Mismatched bracket '" + String(1, c) +
                                                 "' on line " +
                                                 String::asText(lineNumber()));
                }
            }

            // Many operators are just one character long.
            output.endToken();
            counter++;
            continue;
        }

        // Unexpected character!
        throw UnexpectedCharacterError("ScriptLex::getStatement",
                                       "Character '" + String(1, c) + "' was unexpected");
    }

    // Open brackets left?
    if (atEnd())
    {
        for (int i = 0; i < MAX_BRACKETS; ++i)
        {
            if (bracketLevel[i] > 0)
            {
                throw MismatchedBracketError(
                    "ScriptLex::getStatement",
                    stringf("Unclosed bracket '%s'",
                            i == BRACKET_PARENTHESIS ? ")" : i == BRACKET_SQUARE ? "]" : "}"));
            }
        }
    }

    return counter; // Number of tokens added.
}

Token::Type
ScriptLex::parseString(Char startChar, duint startIndentation, TokenBuffer &output)
{
    Token::Type type =
        ( startChar == '\''? Token::LITERAL_STRING_APOSTROPHE :
          Token::LITERAL_STRING_QUOTED );
    bool longString = false;
    duint charLineNumber = lineNumber();

    ModeSpan readingMode(*this, RetainComments);

    // The token already contains the startChar.
    Char c = get();

    if (c == '\n')
    {
        // This can't be good.
        throw UnterminatedStringError("ScriptLex::parseString",
                                      "String on line " + String::asText(charLineNumber) +
                                      " is not terminated");
    }

    output.appendChar(c);

    if (c == startChar)
    {
        // Already over?
        if (c == '"' && peek() == '"')
        {
            // 3-quoted string (allow newlines).
            longString = true;
            output.appendChar(get());
        }
        else
        {
            // The string is empty.
            return type;
        }
    }

    // Read chars until something interesting is found.
    for (;;)
    {
        charLineNumber = lineNumber();

        output.appendChar(c = get());
        if (c == '\\') // Escape?
        {
            output.appendChar(get()); // Don't care what it is.
            continue;
        }
        if (c == '\n') // Newline?
        {
            if (!longString)
            {
                throw UnterminatedStringError("ScriptLex::parseString",
                                              "String on line " +
                                              String::asText(charLineNumber) +
                                              " is not terminated");
            }
            // Skip whitespace according to the indentation.
            duint skipCount = startIndentation;
            while (skipCount-- && isWhite(c = peek()) && c != '\n')
            {
                // Skip the white.
                get();
            }
            continue;
        }
        if (c == startChar)
        {
            // This will end the string?
            if (longString)
            {
                if (peek() == '"')
                {
                    output.appendChar(get());
                    if (peek() == '"')
                    {
                        output.appendChar(get());
                        break;
                    }
                }
                // Not actually a terminating """.
                continue;
            }
            break;
        }
    }

    return (longString? Token::LITERAL_STRING_LONG : type);
}

bool ScriptLex::isOperator(Char c)
{
    return (c == '=' || c == ',' || c == '.'
         || c == '-' || c == '+' || c == '/' || c == '*' || c == '%'
         || c == '&' || c == '|' || c == '!' || c == '^' || c == '~'
         || c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']'
         || c == ':' || c == '<' || c == '>' || c == '?');
}

bool ScriptLex::combinesWith(Char a, Char b)
{
    if (b == '=')
    {
        return (a == '=' || a == '+' || a == '-' || a == '/'
             || a == '*' || a == '%' || a == '!' || a == '|' || a == '&'
             || a == '^' || a == '~' || a == '<' || a == '>' || a == ':' || a == '?');
    }
    else if ((a == '<' && b == '<') || (a == '>' && b == '>'))
    {
        return true;
    }
    return false;
}

const Set<CString> ScriptLex::KEYWORDS = {
    AND,    BREAK, CATCH,  CONST,  CONTINUE, DEF,  DEL,     ELSE,   ELSIF, END,
    FOR,    IF,    IMPORT, IN,     NOT,      OR,   PASS,    PRINT,  RECORD,
    RETURN, SCOPE, THROW,  TRY,    WHILE,    NONE, T_FALSE, T_TRUE, PI,
};

bool ScriptLex::isKeyword(const Token &token) // static
{
    return KEYWORDS.contains(token.cStr());
}

StringList ScriptLex::keywords() // static
{
    return compose<StringList>(KEYWORDS.begin(), KEYWORDS.end());
}
