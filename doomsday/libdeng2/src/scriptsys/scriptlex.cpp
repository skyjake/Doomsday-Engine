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

duint ScriptLex::getStatement(TokenBuffer &output)
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
    try
    {
        forever
        {
            // Tokens are primarily separated by whitespace.
            skipWhiteExceptNewline();

            // This will be the first character of the token.
            QChar c = get();
            
            if(c == '\n' || c == ';')
            {
                // A statement ending character? Open brackets prevent the statement
                // from ending here.
                if(Vector3i(bracketLevel).max() > 0)
                    continue;
                else
                    break;
            }

            output.newToken(lineNumber());    

            if(c == '\\')
            {
                // An escaped newline?
                if(onlyWhiteOnLine())
                {
                    skipToNextLine();
                    continue;
                }
            }

            output.appendChar(c);

            if(c == '"' || c == '\'')
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
            if((c == '.' && isNumeric(peek())) || isNumeric(c))
            {
                bool gotPoint = (c == '.');
                bool isHex = (c == '0' && (peek() == 'x' || peek() == 'X'));
                bool gotX = false;
                
                output.setType(Token::LITERAL_NUMBER);

                // Read until a non-numeric character is found.
                while(isNumeric((c = peek())) || (isHex && isHexNumeric(c)) || 
                    (!isHex && !gotPoint && c == '.') || 
                    (isHex && !gotX && (c == 'x' || c == 'X')))
                {
                    // Just one decimal point.
                    if(c == '.') gotPoint = true;
                    // Just one 'x'.
                    if(c == 'x' || c == 'X') gotX = true;
                    output.appendChar(get());
                }
                output.endToken();
                counter++;
                continue;
            }

            // Alphanumeric characters are joined into a token.
            if(isAlphaNumeric(c))
            {
                output.setType(Token::IDENTIFIER);
                
                while(isAlphaNumeric((c = peek())))
                {
                    output.appendChar(get());
                }
                
                // It might be that this is a keyword.
                if(isKeyword(output.latest()))
                {
                    output.setType(Token::KEYWORD);
                }
                
                output.endToken();
                counter++;
                continue;
            }

            if(isOperator(c))
            {
                output.setType(Token::OPERATOR);
                
                if(combinesWith(c, peek()))
                {
                    output.appendChar(get());
                    /// @todo  Three-character tokens: >>=, <<=
                }
                else
                {
                    // Keep score of bracket levels, since they prevent 
                    // newlines from ending the statement.
                    if(c == '(') bracketLevel[BRACKET_PARENTHESIS]++;
                    if(c == ')') bracketLevel[BRACKET_PARENTHESIS]--;
                    if(c == '[') bracketLevel[BRACKET_SQUARE]++;
                    if(c == ']') bracketLevel[BRACKET_SQUARE]--;
                    if(c == '{') bracketLevel[BRACKET_CURLY]++;
                    if(c == '}') bracketLevel[BRACKET_CURLY]--;

                    if(Vector3i(bracketLevel).min() < 0)
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
    }
    catch(OutOfInputError const &)
    {
        // Open brackets left?
        for(int i = 0; i < MAX_BRACKETS; ++i)
        {
            if(bracketLevel[i] > 0)
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
    
    ModeSpan readingMode(*this, SkipComments);
    
    // The token already contains the startChar.
    QChar c = get();
    
    if(c == '\n')
    {
        // This can't be good.
        throw UnterminatedStringError("ScriptLex::parseString",
                                      "String on line " + QString::number(charLineNumber) +
                                      " is not terminated");
    }

    output.appendChar(c);
    
    if(c == startChar)
    {
        // Already over?
        if(c == '"' && peek() == '"')
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
    for(;;)
    {
        charLineNumber = lineNumber();
        
        output.appendChar(c = get());
        if(c == '\\') // Escape?
        {
            output.appendChar(get()); // Don't care what it is.
            continue;
        }
        if(c == '\n') // Newline?
        {
            if(!longString)
            {
                throw UnterminatedStringError("ScriptLex::parseString",
                                              "String on line " +
                                              QString::number(charLineNumber) +
                                              " is not terminated");
            }
            // Skip whitespace according to the indentation.
            duint skipCount = startIndentation;
            while(skipCount-- && isWhite(c = peek()) && c != '\n')
            {
                // Skip the white.
                get();
            }
            continue;
        }
        if(c == startChar)
        {
            // This will end the string.
            if(longString)
            {
                c = get();
                QChar d = get();
                if(c != '"') 
                {
                    throw UnexpectedCharacterError("ScriptLex::parseString",
                        "Character '" + String(1, c) + "' was unexpected");
                }
                if(d != '"') 
                {
                    throw UnexpectedCharacterError("ScriptLex::parseString",
                        "Character '" + String(1, d) + "' was unexpected");
                }
                output.appendChar(c);
                output.appendChar(d);
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
    if(b == '=')
    {
        return (a == '=' || a == '+' || a == '-' || a == '/' 
            || a == '*' || a == '%' || a == '!' || a == '|' || a == '&'
            || a == '^' || a == '~' || a == '<' || a == '>' || a == ':' || a == '?');
    }
    else if((a == '<' && b == '<') || (a == '>' && b == '>'))
    {
        return true;
    }
    return false;
}

bool ScriptLex::isKeyword(Token const &token)
{
    QChar const *keywords[] =
    {
        AND,
        BREAK,
        CATCH,
        CONST,
        CONTINUE,
        DEF,
        DEL,
        ELSE,
        ELSIF,
        END,
        FOR,
        IF,
        IMPORT,
        EXPORT,
        IN,
        NOT,
        OR,
        PASS,
        PRINT,
        RECORD,
        RETURN,
        THROW,
        TRY,
        WHILE,
        NONE,
        T_FALSE,
        T_TRUE,
        PI,
        0
    };
    
    for(int i = 0; keywords[i]; ++i)
    {
        if(token.equals(keywords[i]))
        {
            return true;
        }
    }
    return false;
}

String ScriptLex::unescapeStringToken(Token const &token)
{
    DENG2_ASSERT(token.type() == Token::LITERAL_STRING_APOSTROPHE ||
             token.type() == Token::LITERAL_STRING_QUOTED ||
             token.type() == Token::LITERAL_STRING_LONG);
    
    String result;
    QTextStream os(&result);
    bool escaped = false;
    
    QChar const *begin = token.begin();
    QChar const *end = token.end();
    
    // A long string?
    if(token.type() == Token::LITERAL_STRING_LONG)
    {
        DENG2_ASSERT(token.size() >= 6);
        begin += 3;
        end -= 3;
    }
    else
    {
        // Normal string token.
        ++begin;
        --end;
    }
    
    for(QChar const *ptr = begin; ptr != end; ++ptr)
    {
        if(escaped)
        {
            QChar c = '\\';
            escaped = false;
            if(*ptr == '\\')
            {
                c = '\\';
            }
            else if(*ptr == '\'')
            {
                c = '\'';
            }
            else if(*ptr == '\"')
            {
                c = '\"';
            }
            else if(*ptr == 'a')
            {
                c = '\a';
            }
            else if(*ptr == 'b')
            {
                c = '\b';
            }
            else if(*ptr == 'f')
            {
                c = '\f';
            }
            else if(*ptr == 'n')
            {
                c = '\n';
            }
            else if(*ptr == 'r')
            {
                c = '\r';
            }
            else if(*ptr == 't')
            {
                c = '\t';
            }
            else if(*ptr == 'v')
            {
                c = '\v';
            }
            else if(*ptr == 'x' && (end - ptr > 2))
            {
                QString num(const_cast<QChar const *>(ptr + 1), 2);
                duint code = num.toInt(0, 16);
                c = QChar(code);
                ptr += 2; 
            }
            else
            {
                // Unknown escape sequence?
                os << '\\' << *ptr;
                continue;
            }            
            os << c;
        }
        else
        {
            if(*ptr == '\\')
            {
                escaped = true;
                continue;
            }
            os << *ptr; 
        }
    }
    DENG2_ASSERT(!escaped);
    
    return result;
}

ddouble ScriptLex::tokenToNumber(Token const &token)
{
    String str(token.str());

    if(token.beginsWith(String("0x")) || token.beginsWith(String("0X")))
    {
        return ddouble(str.toLongLong(0, 16));
    }
    else
    {
        return str.toDouble();
    }
}
