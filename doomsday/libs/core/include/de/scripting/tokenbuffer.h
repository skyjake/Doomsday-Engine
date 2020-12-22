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

#ifndef LIBCORE_TOKENBUFFER_H
#define LIBCORE_TOKENBUFFER_H

#include "../libcore.h"
#include "de/range.h"
#include "de/cstring.h"

#include <vector>

namespace de {

class String;

/**
 * Character sequence allocated out of the token buffer.
 */
class DE_PUBLIC Token
{
public:
    /// Types for tokens. This much can be analyzed without knowing
    /// anything about the context.
    enum Type {
        UNKNOWN,
        KEYWORD,
        OPERATOR,
        LITERAL,
        LITERAL_STRING_APOSTROPHE,
        LITERAL_STRING_QUOTED,
        LITERAL_STRING_LONG,
        LITERAL_NUMBER,
        IDENTIFIER,
    };

    // Token constants.
    static const char *PARENTHESIS_OPEN;
    static const char *PARENTHESIS_CLOSE;
    static const char *BRACKET_OPEN;
    static const char *BRACKET_CLOSE;
    static const char *CURLY_OPEN;
    static const char *CURLY_CLOSE;
    static const char *COLON;
    static const char *COMMA;
    static const char *SEMICOLON;

public:
    Token(const char *begin = nullptr, const char *end = nullptr, duint line = 0)
        : _type(UNKNOWN), _token{begin, end}, _line(line) {}

    void setType(Type type) { _type = type; }

    Type type() const { return _type; }

    /// Returns the address of the beginning of the token.
    /// @return Pointer to the first character of the token.
    const char *begin() const { return _token.start; }

    /// Returns the address of the end of the token.
    /// @return Pointer to the character just after the last
    /// character of the token.
    const char *end() const { return _token.end; }

    /// Returns the address of the beginning of the token.
    /// @return Pointer to the first character of the token.
//    const char *begin() { return _begin; }

    /// Returns the address of the end of the token.
    /// @return Pointer to the character just after the last
    /// character of the token. This is where a new character is
    /// appended when the token is being compiled.
//    const char *end() { return _end; }

    /// Determines the length of the token.
    /// @return Length of the token as number of bytes.
    size_t size() const {
        if (!_token.start) return 0;
        return _token.size();
    }

    bool isEmpty() const {
        return !size();
    }

    void appendChar(Char c);

    /// Determines whether the token equals @c str. Case sensitive.
    /// @return @c true if an exact match, otherwise @c false.
    bool equals(const char *str) const;

    /// Determines whether the token begins with @c str. Case
    /// sensitive. @return @c true if an exact match, otherwise @c false.
    bool beginsWith(const char *str) const;

    /// Determines the line on which the token begins in the source.
    duint line() const { return _line; }

    /// Converts the token into a String. This is human-readably output,
    /// with the line number included.
    String asText() const;

    /// Converts a substring of the token into a String.
    /// This includes nothing extra but the text of the token.
    String str() const;

    CString cStr() const { return CString(begin(), end()); }

    /// Unescapes a string literal into a String. The quotes/apostrophes in the
    /// beginning and end are also removed.
    String unescapeStringLiteral() const;

    bool isInteger() const;
    bool isFloat() const;

    /// Converts the token into a double-precision floating point number.
    ddouble toNumber() const;

    dint64 toInteger() const;
    ddouble toDouble() const;

    static const char *typeToText(Type type) {
        switch (type) {
        case UNKNOWN:
            return "UNKNOWN";
        case KEYWORD:
            return "KEYWORD";
        case OPERATOR:
            return "OPERATOR";
        case LITERAL:
            return "LITERAL";
        case LITERAL_STRING_APOSTROPHE:
            return "LITERAL_STRING_APOSTROPHE";
        case LITERAL_STRING_QUOTED:
            return "LITERAL_STRING_QUOTED";
        case LITERAL_STRING_LONG:
            return "LITERAL_STRING_LONG";
        case LITERAL_NUMBER:
            return "LITERAL_NUMBER";
        case IDENTIFIER:
            return "IDENTIFIER";
        }
        return "";
    }

private:
    Type    _type; ///< Type of the token.
    Rangecc _token;
    duint   _line; ///< On which line the token begins.
};

/**
 * Buffer of tokens, used as an efficient way to compile and store tokens.
 * Does its own memory management: tokens are allocated out of large blocks
 * of memory.
 *
 * @ingroup script
 */
class DE_PUBLIC TokenBuffer
{
public:
    /// Attempt to append characters while no token is being formed. @ingroup errors
    DE_ERROR(TokenNotStartedError);

    /// Parameter was out of range. @ingroup errors
    DE_ERROR(OutOfRangeError);

public:
    TokenBuffer();
    virtual ~TokenBuffer();

    /// Deletes all Tokens, but does not free the token pools.
    void clear();

    /// Begins forming a new Token.
    /// @param line Input source line number for the token.
    void newToken(duint line);

    /// Appends a character to the Token being formed.
    /// @param c Character to append.
    void appendChar(Char c);

    /// Sets the type of the token being formed.
    void setType(Token::Type type);

    /// Finishes the current Token.
    void endToken();

    /// Returns the number of tokens in the buffer.
    dsize size() const;

    bool empty() const { return !size(); }

    /// Returns a specific token in the buffer.
    const Token &at(dsize i) const;

    const Token &latest() const;

private:
    /**
     * Tokens are allocated out of Pools. If a pool runs out of
     * space, a new one is created with more space. Existing pools
     * are not resized, because existing Tokens will point to their
     * memory.
     */
    struct Pool {
        String chars;
        dsize size;
        dsize rover;

        Pool() : size(0), rover(0) {}
    };

    const char *advanceToPoolWithSpace(dsize minimum);

    typedef std::vector<Pool> Pools;
    Pools _pools;

    typedef std::vector<Token> Tokens;
    Tokens _tokens;

    /// Token being currently formed.
    Token *_forming;

    /// Index of pool used for token forming.
    duint _formPool;
};

} // namespace de

#endif /* LIBCORE_TOKENBUFFER_H */
