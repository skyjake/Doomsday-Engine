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

TokenRange::TokenRange() : _tokens(0), _start(0), _end(0)
{}

TokenRange::TokenRange(const TokenBuffer& tokens) : _tokens(&tokens), _start(0)
{
    _end = _tokens->size();
}

TokenRange::TokenRange(const TokenBuffer& tokens, duint start, duint end)
    : _tokens(&tokens), _start(start), _end(end)
{    
}

duint TokenRange::tokenIndex(duint pos) const
{
    if(pos < 0 || pos >= size())
    {
        std::ostringstream message;
        message << "Position " << pos << " is out of the range ("
            << _start << ", " << _end << "), length " << size();
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::tokenIndex", message.str());
    }
    return _start + pos;
}

duint TokenRange::tokenPos(duint index) const
{
    if(index < _start)
    {
        std::ostringstream message;
        message << "Index " << index << " is out of the range ("
            << _start << ", " << _end << ")";
        /// @throw OutOfBoundsError  @a index is out of range.
        throw OutOfBoundsError("TokenRange::tokenPos", message.str());
    }
    return index - _start;
}

const TokenBuffer::Token& TokenRange::token(duint pos) const
{
    if(pos >= size())
    {
        std::ostringstream message;
        message << "Position " << pos << " is out of the range ("
            << _start << ", " << _end << ")";
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::token", message.str());
    }
    return _tokens->at(tokenIndex(pos));
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
    return TokenRange(*_tokens, tokenIndex(pos), _end);
}

TokenRange TokenRange::endingTo(duint pos) const
{
    if(pos < 0 || pos > size())
    {
        std::ostringstream message;
        message << "Position " << pos << " is not within the range ("
            << _start << ", " << _end << ")";
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::endingTo", message.str());
    }
    return TokenRange(*_tokens, _start, tokenIndex(pos));
}

TokenRange TokenRange::between(duint startPos, duint endPos) const
{
    if(endPos > size())
    {
        return startingFrom(startPos);
    }
    return TokenRange(*_tokens, tokenIndex(startPos), tokenIndex(endPos));
}

dint TokenRange::find(const char* token, dint startPos) const
{
    duint len = size();
    assert(startPos >= 0 && startPos <= dint(len));

    for(dint i = startPos; i < dint(len); ++i)
    {
        if(_tokens->at(_start + i).equals(token))
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
    assert(startIndex >= dint(_start) && startIndex <= dint(_end));
    
    for(duint i = startIndex; i < _end; ++i)
    {
        const TokenBuffer::Token& t = _tokens->at(i);
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
        subrange._start = subrange._end = _start;
    }
    else
    {
        // Start past the previous delimiter.
        subrange._start = subrange._end + 1;
    }
    
    if(subrange._start > _end)
    {
        // No more tokens available.
        return false;
    }
    
    dint index = findIndexSkippingBrackets(delimiter, subrange._start);
    if(index < 0)
    {
        // Not found. Use the entire range.
        subrange._end = _end;
    }
    else
    {
        // Everything up to the delimiting token (not included).
        subrange._end = index;
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
    for(dint i = tokenIndex(openBracketPos + 1); i < dint(_end); ++i)
    {
        const TokenBuffer::Token& token = _tokens->at(i);
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
        "Could not find closing bracket for '" + String(openingToken) +
        "' within '" + asText() + "'");
}

duint TokenRange::openingBracket(duint closeBracketPos) const
{
    const char* openingToken;
    const char* closingToken;
    
    for(dint start = tokenIndex(closeBracketPos - 1); start >= 0; --start)
    {
        bracketTokens(_tokens->at(start), openingToken, closingToken);
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
        "' within '" + asText() + "'");
}

String TokenRange::asText() const
{
    std::ostringstream os;
    for(duint i = _start; i < _end; ++i)
    {
        if(i > _start) 
        {
            os << " ";
        }
        os << _tokens->at(i).str();
    }    
    return os.str();
}

TokenRange TokenRange::undefinedRange() const
{
    return TokenRange(*_tokens, UNDEFINED_POS, UNDEFINED_POS);
}

bool TokenRange::undefined() const
{
    return (_start == UNDEFINED_POS && _end == UNDEFINED_POS);    
}
