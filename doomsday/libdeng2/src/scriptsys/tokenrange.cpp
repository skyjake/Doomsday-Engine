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

#include "de/TokenRange"
#include "de/TokenBuffer"

#include <QTextStream>

using namespace de;

/// This position is used for marking an undefined position in the range.
duint const UNDEFINED_POS = 0xffffffff;

TokenRange::TokenRange() : _tokens(0), _start(0), _end(0)
{}

TokenRange::TokenRange(TokenBuffer const &tokens) : _tokens(&tokens), _start(0)
{
    _end = _tokens->size();
}

TokenRange::TokenRange(TokenBuffer const &tokens, duint start, duint end)
    : _tokens(&tokens), _start(start), _end(end)
{    
}

duint TokenRange::tokenIndex(duint pos) const
{
    if(pos >= size())
    {
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::tokenIndex",
                               "Position " + QString::number(pos) + " is out of the range (" +
                               QString::number(_start) + ", " + QString::number(_end) +
                               "), length " + QString::number(size()));
    }
    return _start + pos;
}

duint TokenRange::tokenPos(duint index) const
{
    if(index < _start)
    {
        /// @throw OutOfBoundsError  @a index is out of range.
        throw OutOfBoundsError("TokenRange::tokenPos",
                               "Index " + QString::number(index) + " is out of the range (" +
                               QString::number(_start) + ", " + QString::number(_end) + ")");
    }
    return index - _start;
}

Token const &TokenRange::token(duint pos) const
{
    if(pos >= size())
    {
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::token",
                               "Position " + QString::number(pos) + " is out of the range (" +
                               QString::number(_start) + ", " + QString::number(_end) + ")");
    }
    return _tokens->at(tokenIndex(pos));
}

Token const &TokenRange::firstToken() const
{
    if(!size())
    {
        /// @throw EmptyRangeError  The range is empty: there is no first token.
        throw EmptyRangeError("TokenRange::firstToken", "Token range has no first token");
    }
    return token(0);
}

Token const &TokenRange::lastToken() const
{
    if(!size())
    {
        /// @throw EmptyRangeError  The range is empty, there is no last token.
        throw EmptyRangeError("TokenRange::lastToken", "Token range has no last token");
    }
    return token(size() - 1);
}

bool TokenRange::beginsWith(QChar const *str) const
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
    if(pos > size())
    {
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::endingTo",
                               "Position " + QString::number(pos) + " is not within the range (" +
                               QString::number(_start) + ", " + QString::number(_end) + ")");
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

dint TokenRange::find(QChar const *token, dint startPos) const
{
    duint len = size();
    DENG2_ASSERT(startPos >= 0 && startPos <= dint(len));

    for(dint i = startPos; i < dint(len); ++i)
    {
        if(_tokens->at(_start + i).equals(token))
            return i;
    }
    return -1;
}

dint TokenRange::findBracketless(QChar const *token, dint startPos) const
{
    dint index = findIndexSkippingBrackets(token, tokenIndex(startPos));
    if(index >= 0)
    {
        return tokenPos(index);
    }
    return -1;
}

dint TokenRange::findIndexSkippingBrackets(QChar const *token, dint startIndex) const
{
    DENG2_ASSERT(startIndex >= dint(_start) && startIndex <= dint(_end));
    
    for(duint i = startIndex; i < _end; ++i)
    {
        Token const &t = _tokens->at(i);
        if(t.equals(Token::PARENTHESIS_OPEN) || t.equals(Token::BRACKET_OPEN) ||
           t.equals(Token::CURLY_OPEN))
        {
            i = tokenIndex(closingBracket(tokenPos(i)));
            continue;
        }
        if(t.equals(token))
            return i;
    }
    return -1;
}

bool TokenRange::getNextDelimited(QChar const *delimiter, TokenRange &subrange) const
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

void TokenRange::bracketTokens(Token const &openingToken, QChar const * &opening,
                               QChar const * &closing)
{
    opening = NULL;
    closing = NULL;
    
    if(openingToken.equals(Token::PARENTHESIS_OPEN))
    {
        opening = Token::PARENTHESIS_OPEN;
        closing = Token::PARENTHESIS_CLOSE;
    }
    else if(openingToken.equals(Token::BRACKET_OPEN))
    {
        opening = Token::BRACKET_OPEN;
        closing = Token::BRACKET_CLOSE;
    }
    else if(openingToken.equals(Token::CURLY_OPEN))
    {
        opening = Token::CURLY_OPEN;
        closing = Token::CURLY_CLOSE;
    }
}

duint TokenRange::closingBracket(duint openBracketPos) const
{
    QChar const *openingToken;
    QChar const *closingToken;
    
    bracketTokens(token(openBracketPos), openingToken, closingToken);
    
    int level = 1;
    for(dint i = tokenIndex(openBracketPos + 1); i < dint(_end); ++i)
    {
        Token const &token = _tokens->at(i);
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
    QChar const *openingToken;
    QChar const *closingToken;
    
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
        "Could not find opening bracket for '" + token(closeBracketPos).str() +
        "' within '" + asText() + "'");
}

String TokenRange::asText() const
{
    String result;
    QTextStream os(&result);
    for(duint i = _start; i < _end; ++i)
    {
        if(i > _start) 
        {
            os << " ";
        }
        os << _tokens->at(i).str();
    }    
    return result;
}

TokenRange TokenRange::undefinedRange() const
{
    return TokenRange(*_tokens, UNDEFINED_POS, UNDEFINED_POS);
}

bool TokenRange::undefined() const
{
    return (_start == UNDEFINED_POS && _end == UNDEFINED_POS);    
}
