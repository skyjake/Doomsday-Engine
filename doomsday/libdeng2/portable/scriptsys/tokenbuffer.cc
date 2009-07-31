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

#include "de/TokenBuffer"
#include "de/String"
#include "de/math.h"

#include <sstream>
#include <cstdlib>
#include <cstring>

using namespace de;

// Default size of one allocation pool.
static const duint POOL_SIZE = 1024;

bool TokenBuffer::Token::equals(const char* str) const
{
    if(size() < std::strlen(str))
    {
        // No possibility of a match.
        return false;
    }
    return !String::compareWithCase(str, reinterpret_cast<const char*>(begin_), size());
}

bool TokenBuffer::Token::beginsWith(const char* str) const
{
    duint length = std::strlen(str);
    if(length > size())
    {
        // No way.
        return false;
    }
    return !String::compareWithCase(str, reinterpret_cast<const char*>(begin_), length);
}

String TokenBuffer::Token::asText() const
{
    std::ostringstream os;
    os << "\"" << String(reinterpret_cast<const char*>(begin_), end_ - begin_) << 
        "\" (on line " << line_ << ")"; 
    return os.str();
}

String TokenBuffer::Token::str() const
{
    return String(reinterpret_cast<const char*>(begin_), end_ - begin_);
}

TokenBuffer::TokenBuffer() : forming_(0), formPool_(0)
{}
 
TokenBuffer::~TokenBuffer()
{
    for(Pools::iterator i = pools_.begin(); i != pools_.end(); ++i)
    {
        std::free(i->chars);
    }
}

void TokenBuffer::clear()
{
    tokens_.clear();

    // Empty the allocated pools.
    for(Pools::iterator i = pools_.begin(); i != pools_.end(); ++i)
    {
        i->rover = 0;
    }

    // Reuse allocated pools.
    formPool_ = 0;
}

duchar* TokenBuffer::advanceToPoolWithSpace(duint minimum)
{
    for(;; ++formPool_)
    {
        if(pools_.size() == formPool_)
        {
            // Need a new pool.
            pools_.push_back(Pool());
            Pool& newFp = pools_[formPool_];
            newFp.size = POOL_SIZE + minimum;
            newFp.chars = reinterpret_cast<duchar*>(std::malloc(newFp.size));
            return newFp.chars;
        }

        Pool& fp = pools_[formPool_];
        if(fp.rover + minimum < fp.size)
        {
            return &fp.chars[fp.rover];
        }
        
        // Can we resize this pool?
        if(!fp.rover)
        {
            fp.size = max(POOL_SIZE + minimum, 2 * minimum);
            fp.chars = reinterpret_cast<duchar*>(std::realloc(fp.chars, fp.size));
            return fp.chars;
        }
    }
}

void TokenBuffer::newToken(duint line)
{
    if(forming_)
    {
        // Discard the currently formed token. Use the old start address.
        *forming_ = Token(forming_->begin(), forming_->begin(), line);
        return;
    }

    // Determine which pool to use and the starting address.
    duchar* begin = advanceToPoolWithSpace(0);
    
    tokens_.push_back(Token(begin, begin, line));
    forming_ = &tokens_.back();
}

void TokenBuffer::cancelToken()
{
    
}

void TokenBuffer::appendChar(duchar c)
{
    assert(forming_ != 0);
        
    // There is at least one character available in the pool.
    forming_->appendChar(c);

    // If we run out of space in the pool, we'll need to relocate the
    // token to a new pool. If the pool is new, or there are no tokens
    // in it yet, we can resize the pool in place.
    Pool& fp = pools_[formPool_];
    if(forming_->end() - fp.chars >= fp.size)
    {
        // The pool is full. Find a new pool and move the token.
        std::string tok = forming_->str();
        duchar* newBegin = advanceToPoolWithSpace(tok.size());
        memmove(newBegin, tok.c_str(), tok.size());
        *forming_ = Token(newBegin, newBegin + tok.size(), forming_->line());
    }
}

void TokenBuffer::setType(Token::Type type)
{
    assert(forming_ != 0);
    forming_->setType(type);
}

void TokenBuffer::endToken()
{
    if(forming_)
    {
        // Update the pool.
        pools_[formPool_].rover += forming_->size();
        
        forming_ = 0;
    }
}

duint TokenBuffer::size() const
{
    return tokens_.size();
}

const TokenBuffer::Token& TokenBuffer::at(duint i) const
{
    if(i >= tokens_.size())
    {
        /// @throw OutOfRangeError  Index @a i is out of range.
        throw OutOfRangeError("TokenBuffer::at", "Index out of range");
    }
    return tokens_[i];
}

const TokenBuffer::Token& TokenBuffer::latest() const
{
    return tokens_.back();
}
