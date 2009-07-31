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

#include "de/TokenRange"
#include "de/TokenBuffer"

#include <sstream>

using namespace de;

/// This position is used for marking an undefined position in the range.
const duint UNDEFINED_POS = 0xffffffff;

TokenRange::TokenRange() : tokens_(0), start_(0), end_(0)
{}

TokenRange::TokenRange(const TokenBuffer& tokens) : tokens_(&tokens), start_(0)
{
    end_ = tokens_->size();
}

TokenRange::TokenRange(const TokenBuffer& tokens, duint start, duint end)
    : tokens_(&tokens), start_(start), end_(end)
{    
}

duint TokenRange::tokenIndex(duint pos) const
{
    if(pos < 0 || pos >= size())
    {
        std::ostringstream message;
        message << "Position " << pos << " is out of the range ("
            << start_ << ", " << end_ << "), length " << size();
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::tokenIndex", message.str());
    }
    return start_ + pos;
}

duint TokenRange::tokenPos(duint index) const
{
    if(index < start_)
    {
        std::ostringstream message;
        message << "Index " << index << " is out of the range ("
            << start_ << ", " << end_ << ")";
        /// @throw OutOfBoundsError  @a index is out of range.
        throw OutOfBoundsError("TokenRange::tokenPos", message.str());
    }
    return index - start_;
}

const TokenBuffer::Token& TokenRange::token(duint pos) const
{
    if(pos >= size())
    {
        std::ostringstream message;
        message << "Position " << pos << " is out of the range ("
            << start_ << ", " << end_ << ")";
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::token", message.str());
    }
    return tokens_->at(tokenIndex(pos));
}

const TokenBuffer::Token& TokenRange::firstToken() const 
{
    if(!size())
    {
        /// @throw EmptyRangeError  The range is empty: there is no first token.
        throw EmptyRangeError("TokenRange::firstToken", "Token range has no first token");
    }
    return token(0);
}

const TokenBuffer::Token& TokenRange::lastToken() const 
{
    if(!size())
    {
        /// @throw EmptyRangeError  The range is empty, there is no last token.
        throw EmptyRangeError("TokenRange::lastToken", "Token range has no last token");
    }
    return token(size() - 1);
}

bool TokenRange::beginsWith(const char* str) const
{
    if(size())
    {
        return token(0).equals(str);
    }
    return false;
}

TokenRange TokenRange::startingFrom(duint pos) const
{
    return TokenRange(*tokens_, tokenIndex(pos), end_);
}

TokenRange TokenRange::endingTo(duint pos) const
{
    if(pos < 0 || pos > size())
    {
        std::ostringstream message;
        message << "Position " << pos << " is not within the range ("
            << start_ << ", " << end_ << ")";
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::endingTo", message.str());
    }
    return TokenRange(*tokens_, start_, tokenIndex(pos));
}

TokenRange TokenRange::between(duint startPos, duint endPos) const
{
    if(endPos > size())
    {
        return startingFrom(startPos);
    }
    return TokenRange(*tokens_, tokenIndex(startPos), tokenIndex(endPos));
}

dint TokenRange::find(const char* token, dint startPos) const
{
    duint len = size();
    assert(startPos >= 0 && startPos <= len);

    for(dint i = startPos; i < len; ++i)
    {
        if(tokens_->at(start_ + i).equals(token))
            return i;
    }
    return -1;
}

dint TokenRange::findBracketless(const char* token, dint startPos) const
{
    dint index = findIndexSkippingBrackets(token, tokenIndex(startPos));
    if(index >= 0)
    {
        return tokenPos(index);
    }
    return -1;
}

dint TokenRange::findIndexSkippingBrackets(const char* token, dint startIndex) const
{
    assert(startIndex >= start_ && startIndex <= end_);
    
    for(dint i = startIndex; i < end_; ++i)
    {
        const TokenBuffer::Token& t = tokens_->at(i);
        if(t.equals("(") || t.equals("[") || t.equals("{"))
        {
            i = tokenIndex(closingBracket(tokenPos(i)));
            continue;
        }
        if(t.equals(token))
            return i;
    }
    return -1;
}

bool TokenRange::getNextDelimited(const char* delimiter, TokenRange& subrange) const
{
    if(subrange.undefined())
    {
        // This is the first range.
        subrange.start_ = subrange.end_ = start_;
    }
    else
    {
        // Start past the previous delimiter.
        subrange.start_ = subrange.end_ + 1;
    }
    
    if(subrange.start_ > end_)
    {
        // No more tokens available.
        return false;
    }
    
    dint index = findIndexSkippingBrackets(delimiter, subrange.start_);
    if(index < 0)
    {
        // Not found. Use the entire range.
        subrange.end_ = end_;
    }
    else
    {
        // Everything up to the delimiting token (not included).
        subrange.end_ = index;
    }
    return true;
}

void TokenRange::bracketTokens(const TokenBuffer::Token& openingToken, const char*& opening, 
    const char*& closing)
{
    opening = NULL;
    closing = NULL;
    
    if(openingToken.equals("("))
    {
        opening = "(";
        closing = ")";
    }
    else if(openingToken.equals("["))
    {
        opening = "[";
        closing = "]";
    }
    else if(openingToken.equals("{"))
    {
        opening = "{";
        closing = "}";
    }
}

duint TokenRange::closingBracket(duint openBracketPos) const
{
    const char* openingToken;
    const char* closingToken;
    
    bracketTokens(token(openBracketPos), openingToken, closingToken);
    
    int level = 1;
    for(int i = tokenIndex(openBracketPos + 1); i < end_; ++i)
    {
        const TokenBuffer::Token& token = tokens_->at(i);
        if(token.equals(closingToken))
        {
            --level;
        }
        else if(token.equals(openingToken))
        {
            ++level;
        }
        if(!level)
        {
            return tokenPos(i);
        }
    }
    /// @throw MismatchedBracketError  Cannot find a closing bracket.
    throw MismatchedBracketError("TokenRange::closingBracket",
        "Could not find closing bracket for '" + std::string(openingToken) +
        "' within '" + str() + "'");
}

duint TokenRange::openingBracket(duint closeBracketPos) const
{
    const char* openingToken;
    const char* closingToken;
    
    for(dint start = tokenIndex(closeBracketPos - 1); start >= 0; --start)
    {
        bracketTokens(tokens_->at(start), openingToken, closingToken);
        if(!closingToken || !token(closeBracketPos).equals(closingToken))
        {
            // Not suitable.
            continue;
        }
        // This could be it.
        if(closingBracket(tokenPos(start)) == closeBracketPos)
        {
            return tokenPos(start);
        }
    }
    /// @throw MismatchedBracketError  Cannot find an opening bracket.
    throw MismatchedBracketError("TokenRange::openingBracket",
        "Could not find opening bracket for '" + std::string(token(closeBracketPos).str()) +
        "' within '" + str() + "'");
}

String TokenRange::asText() const
{
    std::ostringstream os;
    for(int i = start_; i < end_; ++i)
    {
        if(i > start_) 
        {
            os << " ";
        }
        os << tokens_->at(i).str();
    }    
    return os.str();
}

TokenRange TokenRange::undefinedRange() const
{
    return TokenRange(*tokens_, UNDEFINED_POS, UNDEFINED_POS);
}

bool TokenRange::undefined() const
{
    return (start_ == UNDEFINED_POS && end_ == UNDEFINED_POS);    
}
