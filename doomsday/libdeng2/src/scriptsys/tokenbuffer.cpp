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

#include "de/TokenBuffer"
#include "de/String"
#include "de/math.h"

#include <cstring>

using namespace de;

// Default size of one allocation pool.
static const duint POOL_SIZE = 1024;

const String Token::PARENTHESIS_OPEN = "(";
const String Token::PARENTHESIS_CLOSE = ")";
const String Token::BRACKET_OPEN = "[";
const String Token::BRACKET_CLOSE = "]";
const String Token::CURLY_OPEN = "{";
const String Token::CURLY_CLOSE = "}";
const String Token::COLON(":");
const String Token::COMMA(",");
const String Token::SEMICOLON(";");

bool Token::equals(const QChar* str) const
{
    if(size() < QString(str).size())
    {
        // No possibility of a match.
        return false;
    }
    return !String::compareWithCase(str, _begin, size());
}

bool Token::beginsWith(const QChar* str) const
{
    int length = QString(str).size();
    if(length > size())
    {
        // No way.
        return false;
    }
    return !String::compareWithCase(str, _begin, length);
}

String Token::asText() const
{
    return "'" + QString(_begin, _end - _begin) + "' (on line " + QString::number(_line) + ")";
}

String Token::str() const
{
    return String(_begin, _end - _begin);
}

TokenBuffer::TokenBuffer() : _forming(0), _formPool(0)
{}
 
TokenBuffer::~TokenBuffer()
{}

void TokenBuffer::clear()
{
    _tokens.clear();

    // Empty the allocated pools.
    for(Pools::iterator i = _pools.begin(); i != _pools.end(); ++i)
    {
        i->rover = 0;
    }

    // Reuse allocated pools.
    _formPool = 0;
}

QChar* TokenBuffer::advanceToPoolWithSpace(duint minimum)
{
    for(;; ++_formPool)
    {
        if(_pools.size() == _formPool)
        {
            // Need a new pool.
            _pools.push_back(Pool());
            Pool& newFp = _pools[_formPool];
            newFp.size = POOL_SIZE + minimum;
            newFp.chars.resize(newFp.size);
            return newFp.chars.data();
        }

        Pool& fp = _pools[_formPool];
        if(fp.rover + minimum < fp.size)
        {
            return &fp.chars.data()[fp.rover];
        }
        
        // Can we resize this pool?
        if(!fp.rover)
        {
            fp.size = max(POOL_SIZE + minimum, 2 * minimum);
            fp.chars.resize(fp.size);
            return fp.chars.data();
        }
    }
}

void TokenBuffer::newToken(duint line)
{
    if(_forming)
    {
        // Discard the currently formed token. Use the old start address.
        *_forming = Token(_forming->begin(), _forming->begin(), line);
        return;
    }

    // Determine which pool to use and the starting address.
    QChar* begin = advanceToPoolWithSpace(0);
    
    _tokens.push_back(Token(begin, begin, line));
    _forming = &_tokens.back();
}

void TokenBuffer::appendChar(QChar c)
{
    DENG2_ASSERT(_forming != 0);
        
    // There is at least one character available in the pool.
    _forming->appendChar(c);

    // If we run out of space in the pool, we'll need to relocate the
    // token to a new pool. If the pool is new, or there are no tokens
    // in it yet, we can resize the pool in place.
    Pool& fp = _pools[_formPool];
    if(_forming->end() - fp.chars.data() >= dint(fp.size))
    {
        // The pool is full. Find a new pool and move the token.
        String tok = _forming->str();
        QChar* newBegin = advanceToPoolWithSpace(tok.size());
        memmove(newBegin, tok.data(), tok.size() * sizeof(QChar));
        *_forming = Token(newBegin, newBegin + tok.size(), _forming->line());
    }
}

void TokenBuffer::setType(Token::Type type)
{
    DENG2_ASSERT(_forming != 0);
    _forming->setType(type);
}

void TokenBuffer::endToken()
{
    if(_forming)
    {
        // Update the pool.
        _pools[_formPool].rover += _forming->size();
        
        _forming = 0;
    }
}

duint TokenBuffer::size() const
{
    return _tokens.size();
}

const Token& TokenBuffer::at(duint i) const
{
    if(i >= _tokens.size())
    {
        /// @throw OutOfRangeError  Index @a i is out of range.
        throw OutOfRangeError("TokenBuffer::at", "Index out of range");
    }
    return _tokens[i];
}

const Token& TokenBuffer::latest() const
{
    return _tokens.back();
}
