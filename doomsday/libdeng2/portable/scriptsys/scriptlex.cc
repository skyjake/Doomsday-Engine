/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <sstream>
#include <iomanip>

using namespace de;

ScriptLex::ScriptLex(const String& input) : Lex(input)
{}

duint ScriptLex::getStatement(TokenBuffer& output)
{
    // Get rid of the previous contents of the token buffer.
    output.clear();
    
    duint counter = 0; // How many have we added?

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
        for(;;)
        {
            // Tokens are primarily separated by whitespace.
            skipWhiteExceptNewline();

            // This will be the first character of the token.
            duchar c = get();
            
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
            if(c == '.' && isNumeric(peek()) || isNumeric(c))
            {
                bool gotPoint = (c == '.');
                bool isHex = (c == '0' && (peek() == 'x' || peek() == 'X'));
                bool gotX = false;
                
                output.setType(TokenBuffer::Token::LITERAL_NUMBER);

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
                output.setType(TokenBuffer::Token::IDENTIFIER);
                
                while(isAlphaNumeric((c = peek())))
                {
                    output.appendChar(get());
                }
                
                // It might be that this is a keyword.
                if(isKeyword(output.latest()))
                {
                    output.setType(TokenBuffer::Token::KEYWORD);
                }
                
                output.endToken();
                counter++;
                continue;
            }

            if(isOperator(c))
            {
                output.setType(TokenBuffer::Token::OPERATOR);
                
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
                        std::ostringstream os;
                        os << "Mismatched bracket '" << c << "' on line " << lineNumber();
                        throw MismatchedBracketError("ScriptLex::getStatement", os.str());
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
    catch(const OutOfInputError& error)
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
        
    return counter;
}

TokenBuffer::Token::Type 
ScriptLex::parseString(duchar startChar, duint startIndentation, TokenBuffer& output)
{
    TokenBuffer::Token::Type type = 
        ( startChar == '\''? TokenBuffer::Token::LITERAL_STRING_APOSTROPHE : 
          TokenBuffer::Token::LITERAL_STRING_QUOTED );
    bool longString = false;
    duint charLineNumber = lineNumber();
    
    ModeSpan readingMode(*this, SKIP_COMMENTS);
    
    // The token already contains the startChar.
    duchar c = get();
    
    if(c == '\n')
    {
        // This can't be good.
        std::ostringstream os;
        os << "String on line " << charLineNumber << " is not terminated";
        throw UnterminatedStringError("ScriptLex::parseString", os.str());
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
                std::ostringstream os;
                os << "String on line " << charLineNumber << " is not terminated";
                throw UnterminatedStringError("ScriptLex::parseString", os.str());
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
                duchar d = get();
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
    
    return (longString? TokenBuffer::Token::LITERAL_STRING_LONG : type);
}

bool ScriptLex::isOperator(duchar c)
{
    return (c == '=' || c == ',' || c == '.' 
        || c == '-' || c == '+' || c == '/' || c == '*' || c == '%' 
        || c == '&' || c == '|' || c == '!' || c == '^' || c == '~'
        || c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' 
        || c == ':' || c == '<' || c == '>' );
}

bool ScriptLex::combinesWith(duchar a, duchar b)
{
    if(b == '=')
    {
        return (a == '=' || a == '+' || a == '-' || a == '/' 
            || a == '*' || a == '%' || a == '!' || a == '|' || a == '&'
            || a == '^' || a == '~' || a == '<' || a == '>' || a == ':');
    }
    else if((a == '<' && b == '<') || (a == '>' && b == '>'))
    {
        return true;
    }
    return false;
}

bool ScriptLex::isKeyword(const TokenBuffer::Token& token)
{
    const char* keywords[] = 
    {
        "end",
        "def",
        "record",
        "del",
        "if", 
        "elsif",
        "else",
        "for",
        "while",
        "break",
        "continue",
        "return",
        "print",
        "in",
        "and",
        "or",
        "not",
        "None",
        "True",
        "False",
        "Pi",
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

String ScriptLex::unescapeStringToken(const TokenBuffer::Token& token)
{
    assert(token.type() == TokenBuffer::Token::LITERAL_STRING_APOSTROPHE
        || token.type() == TokenBuffer::Token::LITERAL_STRING_QUOTED
        || token.type() == TokenBuffer::Token::LITERAL_STRING_LONG);
    
    std::ostringstream os;
    bool escaped = false;
    
    const duchar* begin = token.begin();
    const duchar* end = token.end();
    
    // A long string?
    if(token.type() == TokenBuffer::Token::LITERAL_STRING_LONG)
    {
        assert(token.size() >= 6);
        begin += 3;
        end -= 3;
    }
    else
    {
        // Normal string token.
        ++begin;
        --end;
    }
    
    for(const duchar* ptr = begin; ptr != end; ++ptr)
    {
        if(escaped)
        {
            duchar c = '\\';
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
                std::istringstream is(std::string(reinterpret_cast<const char*>(ptr + 1), 2));
                duint code = ' ';
                is >> std::setbase(16) >> code;
                c = code;
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
    assert(!escaped);
    
    return os.str();
}

ddouble ScriptLex::tokenToNumber(const TokenBuffer::Token& token)
{
    std::istringstream is(token.str());

    if(token.beginsWith("0x") || token.beginsWith("0X"))
    {
        duint64 number = 0;
        is >> std::hex >> number;
        return number;
    }
    else
    {
        ddouble number = 0;
        is >> number;
        return number;
    }
}
