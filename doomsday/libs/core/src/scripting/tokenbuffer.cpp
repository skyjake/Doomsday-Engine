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

#include "de/scripting/tokenbuffer.h"
#include "de/string.h"
#include "de/math.h"

#include <sstream>
#include <cstring>

using namespace de;

// Default size of one allocation pool.
static const size_t POOL_SIZE = 1024;

const char *Token::PARENTHESIS_OPEN  = "(";
const char *Token::PARENTHESIS_CLOSE = ")";
const char *Token::BRACKET_OPEN      = "[";
const char *Token::BRACKET_CLOSE     = "]";
const char *Token::CURLY_OPEN        = "{";
const char *Token::CURLY_CLOSE       = "}";
const char *Token::COLON             = ":";
const char *Token::COMMA             = ",";
const char *Token::SEMICOLON         = ";";

bool Token::equals(const char *str) const
{
    const iRangecc r{_token.start, _token.end};
    return cmpCStrSc_Rangecc(&r, str, &iCaseSensitive) == 0;
}

bool Token::beginsWith(const char *str) const
{
    const iRangecc r{_token.start, _token.end};
    return startsWithSc_Rangecc(&r, str, &iCaseSensitive) == 0;
}

void Token::appendChar(Char c)
{
    iMultibyteChar mb;
    init_MultibyteChar(&mb, c);
    char *end = const_cast<char *>(_token.end);
    for (const char *i = mb.bytes; *i; ++i)
    {
        *end++ = *i;
    }
    _token.end = end;
}

String Token::asText() const
{
    return Stringf("%s '%s' (on line %u)", typeToText(_type), str().c_str(), _line);
}

String Token::str() const
{
    return {_token.start, _token.end};
}

String Token::unescapeStringLiteral() const
{
    DE_ASSERT(_type == LITERAL_STRING_APOSTROPHE ||
              _type == LITERAL_STRING_QUOTED ||
              _type == LITERAL_STRING_LONG);

    String os;
    bool escaped = false;

    const char *begin = _token.start;
    const char *end   = _token.end;

    // A long string?
    if (_type == LITERAL_STRING_LONG)
    {
        DE_ASSERT(size() >= 6);
        begin += 3;
        end -= 3;
    }
    else
    {
        // Normal string token.
        ++begin;
        --end;
    }

    for (mb_iterator ptr = begin; ptr != end; ++ptr)
    {
        if (escaped)
        {
            Char c = '\\';
            escaped = false;
            if (*ptr == '\\')
            {
                c = '\\';
            }
            else if (*ptr == '\'')
            {
                c = '\'';
            }
            else if (*ptr == '\"')
            {
                c = '\"';
            }
            else if (*ptr == 'a')
            {
                c = '\a';
            }
            else if (*ptr == 'b')
            {
                c = '\b';
            }
            else if (*ptr == 'f')
            {
                c = '\f';
            }
            else if (*ptr == 'n')
            {
                c = '\n';
            }
            else if (*ptr == 'r')
            {
                c = '\r';
            }
            else if (*ptr == 't')
            {
                c = '\t';
            }
            else if (*ptr == 'v')
            {
                c = '\v';
            }
            else if (*ptr == 'x' && (end - ptr > 2))
            {
                const String num(ptr + 1, ptr + 3);
                c = Char(uint32_t(strtoul(num, nullptr, 16)));
                ptr += 2;
            }
            else
            {
                // Unknown escape sequence?
                os += '\\';
                os += *ptr;
                continue;
            }
            os += c;
        }
        else
        {
            if (*ptr == '\\')
            {
                escaped = true;
                continue;
            }
            os += *ptr;
        }
    }
    DE_ASSERT(!escaped);

    return os;
}

bool Token::isInteger() const
{
    if (_type != LITERAL_NUMBER) return false;

    const String string = str();
    if (string.beginsWith("0x") || string.beginsWith("0X"))
    {
        return true;
    }
    return !isFloat();
}

bool Token::isFloat() const
{
    if (_type != LITERAL_NUMBER) return false;
    for (Char c : *this)
    {
        if (c == '.') return true;
    }
    return false;
}

ddouble Token::toNumber() const
{
    const String s = str();
    if (s.beginsWith("0x") || s.beginsWith("0X"))
    {
        return ddouble(strtoull(s, nullptr, 16));
    }
    else
    {
        return s.toDouble();
    }
}

dint64 Token::toInteger() const
{
    return strtoll(str(), nullptr, 0);
}

ddouble Token::toDouble() const
{
    return str().toDouble();
}

//---------------------------------------------------------------------------------------

TokenBuffer::TokenBuffer() : _forming(nullptr), _formPool(0)
{}

TokenBuffer::~TokenBuffer()
{}

void TokenBuffer::clear()
{
    _tokens.clear();

    // Empty the allocated pools.
    for (Pools::iterator i = _pools.begin(); i != _pools.end(); ++i)
    {
        i->rover = 0;
    }

    // Reuse allocated pools.
    _formPool = 0;
}

const char *TokenBuffer::advanceToPoolWithSpace(dsize minimum)
{
    for (;; ++_formPool)
    {
        if (_pools.size() == _formPool)
        {
            // Need a new pool.
            _pools.push_back(Pool());
            Pool &newFp = _pools[_formPool];
            newFp.size = POOL_SIZE + minimum;
            newFp.chars.resize(newFp.size);
            return newFp.chars.data();
        }

        Pool &fp = _pools[_formPool];
        if (fp.rover + minimum < fp.size)
        {
            return &fp.chars.data()[fp.rover];
        }

        // Can we resize this pool?
        if (!fp.rover)
        {
            fp.size = std::max(POOL_SIZE + minimum, 2 * minimum);
            fp.chars.resize(fp.size);
            return fp.chars.data();
        }
    }
}

void TokenBuffer::newToken(duint line)
{
    if (_forming)
    {
        // Discard the currently formed token. Use the old start address.
        *_forming = Token(_forming->begin(), _forming->begin(), line);
        return;
    }

    // Determine which pool to use and the starting address.
    const char *begin = advanceToPoolWithSpace(0);
    _tokens.push_back(Token(begin, begin, line));
    _forming = &_tokens.back();
}

void TokenBuffer::appendChar(Char c)
{
    DE_ASSERT(_forming);

    // There is room for at least one character available in the pool.
    _forming->appendChar(c);

    // If we run out of space in the pool, we'll need to relocate the
    // token to a new pool. If the pool is new, or there are no tokens
    // in it yet, we can resize the pool in place.
    Pool &fp = _pools[_formPool];
    if (_forming->end() - fp.chars.data() >= int(fp.size) - int(iMultibyteCharMaxSize))
    {
        // The pool is full. Find a new pool and move the token.
        const String tok = _forming->str();
        const char *newBegin = advanceToPoolWithSpace(tok.size());
        std::memmove(const_cast<char *>(newBegin), tok.data(), tok.size());
        *_forming = Token(newBegin, newBegin + tok.size(), _forming->line());
    }
}

void TokenBuffer::setType(Token::Type type)
{
    DE_ASSERT(_forming);
    _forming->setType(type);
}

void TokenBuffer::endToken()
{
    if (_forming)
    {
        // Update the pool.
        _pools[_formPool].rover += _forming->size();

        _forming = nullptr;
    }
}

dsize TokenBuffer::size() const
{
    return _tokens.size();
}

const Token &TokenBuffer::at(dsize i) const
{
    if (i >= _tokens.size())
    {
        /// @throw OutOfRangeError  Index @a i is out of range.
        throw OutOfRangeError("TokenBuffer::at", "Index out of range");
    }
    return _tokens[i];
}

const Token &TokenBuffer::latest() const
{
    return _tokens.back();
}
