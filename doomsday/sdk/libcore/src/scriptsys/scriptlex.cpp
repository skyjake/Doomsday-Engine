/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ScriptLex"
#include "de/TokenBuffer"
#include "de/Vector"

using namespace de;

String const ScriptLex::AND("and");
String const ScriptLex::OR("or");
String const ScriptLex::NOT("not");
String const ScriptLex::IF("if");
String const ScriptLex::ELSIF("elsif");
String const ScriptLex::ELSE("else");
String const ScriptLex::END("end");
String const ScriptLex::THROW("throw");
String const ScriptLex::CATCH("catch");
String const ScriptLex::IN("in");
String const ScriptLex::WHILE("while");
String const ScriptLex::FOR("for");
String const ScriptLex::DEF("def");
String const ScriptLex::TRY("try");
String const ScriptLex::IMPORT("import");
String const ScriptLex::EXPORT("export");
String const ScriptLex::RECORD("record");
String const ScriptLex::SCOPE("->");
String const ScriptLex::DEL("del");
String const ScriptLex::PASS("pass");
String const ScriptLex::CONTINUE("continue");
String const ScriptLex::BREAK("break");
String const ScriptLex::RETURN("return");
String const ScriptLex::CONST("const");
String const ScriptLex::PRINT("print");
String const ScriptLex::T_TRUE("True");
String const ScriptLex::T_FALSE("False");
String const ScriptLex::NONE("None");
String const ScriptLex::PI("Pi");

String const ScriptLex::ASSIGN("=");
String const ScriptLex::SCOPE_ASSIGN(":=");
String const ScriptLex::WEAK_ASSIGN("?=");

ScriptLex::ScriptLex(String const &input) : Lex(input)
{}

duint ScriptLex::getStatement(TokenBuffer &output, Behaviors const &behavior)
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

        if (behavior.testFlag(StopAtMismatchedCloseBrace) &&
            !bracketLevel[BRACKET_CURLY] &&
            peek() == '}')
        {
            // Don't read past the bracket.
            break;
        }

        if (peek() == 0) break;

        // This will be the first character of the token.
        QChar c = get();

        if (c == '\n' || c == ';')
        {
            // A statement ending character? Open brackets prevent the statement
            // from ending here.
            if (Vector3i(bracketLevel).max() > 0)
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

                if (Vector3i(bracketLevel).min() < 0)
                {
                    // Very unusual!
                    throw MismatchedBracketError("ScriptLex::getStatement",
                                                 "Mismatched bracket '" + QString(c) +
                                                 "' on line " +
                                                 QString::number(lineNumber()));
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
                throw MismatchedBracketError("ScriptLex::getStatement", "Unclosed bracket '" +
                    String(i == BRACKET_PARENTHESIS? ")" :
                           i == BRACKET_SQUARE? "]" : "}") + "'");
            }
        }
    }

    return counter; // Number of tokens added.
}

Token::Type
ScriptLex::parseString(QChar startChar, duint startIndentation, TokenBuffer &output)
{
    Token::Type type =
        ( startChar == '\''? Token::LITERAL_STRING_APOSTROPHE :
          Token::LITERAL_STRING_QUOTED );
    bool longString = false;
    duint charLineNumber = lineNumber();

    ModeSpan readingMode(*this, RetainComments);

    // The token already contains the startChar.
    QChar c = get();

    if (c == '\n')
    {
        // This can't be good.
        throw UnterminatedStringError("ScriptLex::parseString",
                                      "String on line " + QString::number(charLineNumber) +
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
                                              QString::number(charLineNumber) +
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

bool ScriptLex::isOperator(QChar c)
{
    return (c == '=' || c == ',' || c == '.'
         || c == '-' || c == '+' || c == '/' || c == '*' || c == '%'
         || c == '&' || c == '|' || c == '!' || c == '^' || c == '~'
         || c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']'
         || c == ':' || c == '<' || c == '>' || c == '?');
}

bool ScriptLex::combinesWith(QChar a, QChar b)
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

static QSet<QString> const keywordStr
{
    ScriptLex::AND,
    ScriptLex::BREAK,
    ScriptLex::CATCH,
    ScriptLex::CONST,
    ScriptLex::CONTINUE,
    ScriptLex::DEF,
    ScriptLex::DEL,
    ScriptLex::ELSE,
    ScriptLex::ELSIF,
    ScriptLex::END,
    ScriptLex::FOR,
    ScriptLex::IF,
    ScriptLex::IMPORT,
    ScriptLex::EXPORT,
    ScriptLex::IN,
    ScriptLex::NOT,
    ScriptLex::OR,
    ScriptLex::PASS,
    ScriptLex::PRINT,
    ScriptLex::RECORD,
    ScriptLex::RETURN,
    ScriptLex::SCOPE,
    ScriptLex::THROW,
    ScriptLex::TRY,
    ScriptLex::WHILE,
    ScriptLex::NONE,
    ScriptLex::T_FALSE,
    ScriptLex::T_TRUE,
    ScriptLex::PI,
};

bool ScriptLex::isKeyword(Token const &token)
{
    return keywordStr.contains(token.str());

/*    for (int i = 0; keywordStr[i]; ++i)
    {
        if (token.equals(keywordStr[i]))
        {
            return true;
        }
    }
    return false;*/
}

StringList ScriptLex::keywords()
{
    StringList list;
    foreach (QString const &kw, keywordStr)
    {
        list << kw;
    }
    return list;
}
