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

#include "de/scripting/tokenrange.h"
#include "de/scripting/tokenbuffer.h"

namespace de {

/// This position is used for marking an undefined position in the range.
static const duint UNDEFINED_POS = 0xffffffff;

TokenRange::TokenRange() : _tokens(nullptr), _start(0), _end(0)
{}

TokenRange::TokenRange(const TokenBuffer &tokens) : _tokens(&tokens), _start(0)
{
    _end = _tokens->size();
}

TokenRange::TokenRange(const TokenBuffer &tokens, dsize start, dsize end)
    : _tokens(&tokens), _start(start), _end(end)
{
}

dsize TokenRange::tokenIndex(dsize pos) const
{
    if (pos >= size())
    {
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::tokenIndex",
                               stringf("Position %zu is out of the range %zu...%zu (length %zu)",
                                       pos,
                                       _start,
                                       _end,
                                       size()));
    }
    return _start + pos;
}

dsize TokenRange::tokenPos(dsize index) const
{
    if (index < _start)
    {
        /// @throw OutOfBoundsError  @a index is out of range.
        throw OutOfBoundsError(
            "TokenRange::tokenPos",
            stringf("Index %zu is out of the range %zu...%zu", index, _start, _end));
    }
    return index - _start;
}

const Token &TokenRange::token(dsize pos) const
{
    if (pos >= size())
    {
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError("TokenRange::token", stringf("Position %zu is out of bounds", pos));
    }
    return _tokens->at(tokenIndex(pos));
}

const Token &TokenRange::firstToken() const
{
    if (!size())
    {
        /// @throw EmptyRangeError  The range is empty: there is no first token.
        throw EmptyRangeError("TokenRange::firstToken", "Token range has no first token");
    }
    return token(0);
}

const Token &TokenRange::lastToken() const
{
    if (!size())
    {
        /// @throw EmptyRangeError  The range is empty, there is no last token.
        throw EmptyRangeError("TokenRange::lastToken", "Token range has no last token");
    }
    return token(size() - 1);
}

bool TokenRange::beginsWith(const char *str) const
{
    if (size())
    {
        return token(0).cStr() == str;
    }
    return false;
}

TokenRange TokenRange::startingFrom(dsize pos) const
{
    return TokenRange(*_tokens, tokenIndex(pos), _end);
}

TokenRange TokenRange::endingTo(dsize pos) const
{
    if (pos > size())
    {
        /// @throw OutOfBoundsError  @a pos is out of range.
        throw OutOfBoundsError(
            "TokenRange::endingTo",
            stringf("Position %zu is not within the range (%zu, %zu)", pos, _start, _end));
    }
    return TokenRange(*_tokens, _start, tokenIndex(pos));
}

TokenRange TokenRange::between(dsize startPos, dsize endPos) const
{
    if (endPos > size())
    {
        return startingFrom(startPos);
    }
    return TokenRange(*_tokens, tokenIndex(startPos), tokenIndex(endPos));
}

dint TokenRange::find(const char *token, dsize startPos) const
{
    const dsize len = size();
    DE_ASSERT(startPos <= len);

    for (dsize i = startPos; i < len; ++i)
    {
        if (_tokens->at(_start + i).equals(token))
            return dint(i);
    }
    return -1;
}

dint TokenRange::findBracketless(const char *token, dsize startPos) const
{
    dint index = findIndexSkippingBrackets(token, tokenIndex(startPos));
    if (index >= 0)
    {
        return dint(tokenPos(index));
    }
    return -1;
}

dint TokenRange::findIndexSkippingBrackets(const char *token, dsize startIndex) const
{
    DE_ASSERT(startIndex >= _start && startIndex <= _end);

    for (dsize i = startIndex; i < _end; ++i)
    {
        const Token &t = _tokens->at(i);
        if (t.equals(Token::PARENTHESIS_OPEN) || t.equals(Token::BRACKET_OPEN) ||
            t.equals(Token::CURLY_OPEN))
        {
            i = tokenIndex(closingBracket(tokenPos(i)));
            continue;
        }
        if (t.equals(token))
            return dint(i);
    }
    return -1;
}

bool TokenRange::getNextDelimited(const char *delimiter, TokenRange &subrange) const
{
    if (subrange.undefined())
    {
        // This is the first range.
        subrange._start = subrange._end = _start;
    }
    else
    {
        // Start past the previous delimiter.
        subrange._start = subrange._end + 1;
    }

    if (subrange._start > _end)
    {
        // No more tokens available.
        return false;
    }

    dint index = findIndexSkippingBrackets(delimiter, subrange._start);
    if (index < 0)
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

void TokenRange::bracketTokens(const Token &openingToken, const char * &opening,
                               const char * &closing)
{
    opening = nullptr;
    closing = nullptr;

    if (openingToken.equals(Token::PARENTHESIS_OPEN))
    {
        opening = Token::PARENTHESIS_OPEN;
        closing = Token::PARENTHESIS_CLOSE;
    }
    else if (openingToken.equals(Token::BRACKET_OPEN))
    {
        opening = Token::BRACKET_OPEN;
        closing = Token::BRACKET_CLOSE;
    }
    else if (openingToken.equals(Token::CURLY_OPEN))
    {
        opening = Token::CURLY_OPEN;
        closing = Token::CURLY_CLOSE;
    }
}

dsize TokenRange::closingBracket(dsize openBracketPos) const
{
    const char *openingToken;
    const char *closingToken;

    bracketTokens(token(openBracketPos), openingToken, closingToken);

    int level = 1;
    for (dsize i = tokenIndex(openBracketPos + 1); i < _end; ++i)
    {
        const Token &token = _tokens->at(i);
        if (token.equals(closingToken))
        {
            --level;
        }
        else if (token.equals(openingToken))
        {
            ++level;
        }
        if (!level)
        {
            return tokenPos(i);
        }
    }
    /// @throw MismatchedBracketError  Cannot find a closing bracket.
    throw MismatchedBracketError("TokenRange::closingBracket",
                                 stringf("Could not find closing bracket for '%s' within '%s'",
                                         openingToken,
                                         asText().c_str()));
}

dsize TokenRange::openingBracket(dsize closeBracketPos) const
{
    const char *openingToken;
    const char *closingToken;

    for (dsize start = tokenIndex(closeBracketPos - 1); start < _end; --start)
    {
        bracketTokens(_tokens->at(start), openingToken, closingToken);
        if (!closingToken || !token(closeBracketPos).equals(closingToken))
        {
            // Not suitable.
            continue;
        }
        // This could be it.
        if (closingBracket(tokenPos(start)) == closeBracketPos)
        {
            return tokenPos(start);
        }
    }
    /// @throw MismatchedBracketError  Cannot find an opening bracket.
    throw MismatchedBracketError("TokenRange::openingBracket",
                                 "Could not find opening bracket for '" +
                                     token(closeBracketPos).str() + "' within '" + asText() + "'");
}

String TokenRange::asText() const
{
    String result;
    for (dsize i = _start; i < _end; ++i)
    {
        if (i > _start)
        {
            result += " ";
        }
        result += _tokens->at(i).str();
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

} // namespace de
