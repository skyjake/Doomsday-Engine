/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_TOKENBUFFER_H
#define LIBDENG2_TOKENBUFFER_H

#include "../libdeng2.h"

#include <vector>

namespace de
{
    class String;
    
    /**
     * Character sequence allocated out of the token buffer.
     */
    class Token
    {
    public:
        /// Types for tokens. This much can be analyzed without knowing
        /// anything about the context.
        enum Type {
            UNKNOWN,
            KEYWORD,
            OPERATOR,
            LITERAL_STRING_APOSTROPHE,
            LITERAL_STRING_QUOTED,
            LITERAL_STRING_LONG,
            LITERAL_NUMBER,
            IDENTIFIER
        };

        // Token constants.
        static String const PARENTHESIS_OPEN;
        static String const PARENTHESIS_CLOSE;
        static String const BRACKET_OPEN;
        static String const BRACKET_CLOSE;
        static String const CURLY_OPEN;
        static String const CURLY_CLOSE;
        static String const COLON;
        static String const COMMA;
        static String const SEMICOLON;

    public:
        Token(QChar *begin = 0, QChar *end = 0, duint line = 0)
            : _type(UNKNOWN), _begin(begin), _end(end), _line(line) {}

        void setType(Type type) { _type = type; }

        Type type() const { return _type; }

        /// Returns the address of the beginning of the token.
        /// @return Pointer to the first character of the token.
        QChar const *begin() const { return _begin; }

        /// Returns the address of the end of the token.
        /// @return Pointer to the character just after the last
        /// character of the token.
        QChar const *end() const { return _end; }

        /// Returns the address of the beginning of the token.
        /// @return Pointer to the first character of the token.
        QChar *begin() { return _begin; }

        /// Returns the address of the end of the token.
        /// @return Pointer to the character just after the last
        /// character of the token. This is where a new character is
        /// appended when the token is being compiled.
        QChar *end() { return _end; }

        /// Determines the length of the token.
        /// @return Length of the token as number of characters.
        int size() const {
            if(!_begin || !_end) return 0;
            return _end - _begin;
        }

        bool isEmpty() const {
            return !size();
        }

        void appendChar(QChar c) {
            *_end++ = c;
        }

        /// Determines whether the token equals @c str. Case sensitive.
        /// @return @c true if an exact match, otherwise @c false.
        bool equals(QChar const *str) const;

        /// Determines whether the token begins with @c str. Case
        /// sensitive. @return @c true if an exact match, otherwise @c false.
        bool beginsWith(QChar const *str) const;

        /// Determines the line on which the token begins in the source.
        duint line() const { return _line; }

        /// Converts the token into a String. This is human-readably output,
        /// with the line number included.
        String asText() const;

        /// Converts a substring of the token into a String.
        /// This includes nothing extra but the text of the token.
        String str() const;

        static char const *typeToText(Type type) {
            switch(type)
            {
            case UNKNOWN:
                return "UNKNOWN";
            case KEYWORD:
                return "KEYWORD";
            case OPERATOR:
                return "OPERATOR";
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
        Type _type;     ///< Type of the token.
        QChar *_begin;  ///< Points to the first character.
        QChar *_end;    ///< Points to the last character + 1.
        duint _line;    ///< On which line the token begins.
    };

    /** 
     * Buffer of tokens, used as an efficient way to compile and store tokens.
     * Does its own memory management: tokens are allocated out of large blocks 
     * of memory.
     *
     * @ingroup script
     */
    class TokenBuffer
    {  
    public:
        /// Attempt to append characters while no token is being formed. @ingroup errors
        DENG2_ERROR(TokenNotStartedError);
        
        /// Parameter was out of range. @ingroup errors
        DENG2_ERROR(OutOfRangeError);
        
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
        void appendChar(QChar c);
        
        /// Sets the type of the token being formed.
        void setType(Token::Type type);
        
        /// Finishes the current Token.
        void endToken();
        
        /// Returns the number of tokens in the buffer.
        duint size() const;
        
        bool empty() const { return !size(); }
        
        /// Returns a specific token in the buffer.
        Token const &at(duint i) const;
        
        Token const &latest() const;
        
    private:
        /**
         * Tokens are allocated out of Pools. If a pool runs out of
         * space, a new one is created with more space. Existing pools
         * are not resized, because existing Tokens will point to their
         * memory.
         */
        struct Pool {
            QString chars;
            duint size;
            duint rover;
            
            Pool() : size(0), rover(0) {}
        };
        
        QChar *advanceToPoolWithSpace(duint minimum);
                
        typedef std::vector<Pool> Pools;
        Pools _pools;
        
        typedef std::vector<Token> Tokens;
        Tokens _tokens;
        
        /// Token being currently formed.
        Token *_forming;
        
        /// Index of pool used for token forming.
        duint _formPool;
    };
}

#endif /* LIBDENG2_TOKENBUFFER_H */
