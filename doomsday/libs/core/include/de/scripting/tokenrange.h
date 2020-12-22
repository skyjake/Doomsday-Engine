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

#ifndef LIBCORE_TOKENRANGE_H
#define LIBCORE_TOKENRANGE_H

#include "de/string.h"
#include "tokenbuffer.h"

namespace de {

/**
 * Utility class for handling a range of tokens inside a token buffer.
 * Parsers use this for keeping track of which section of the tokens is
 * being analyzed.
 *
 * "Indices" refer to indices in the token buffer. "Positions" refer to
 * locations relative to the beginning of the range, starting from zero.
 *
 * @ingroup script
 */
class DE_PUBLIC TokenRange
{
public:
    /// The token range is unexpectedly empty. @ingroup errors
    DE_ERROR(EmptyRangeError);

    /// A position outside the range is accessed. @ingroup errors
    DE_ERROR(OutOfBoundsError);

    /// A matching bracket cannot be located within the range. @ingroup errors
    DE_ERROR(MismatchedBracketError);

public:
    TokenRange();

    /// Constructor that uses the entire range of tokens.
    TokenRange(const TokenBuffer &tokens);

    /// Constructor that uses a specific set of tokens.
    TokenRange(const TokenBuffer &tokens, dsize startIndex, dsize endIndex);

    const TokenBuffer &buffer() const { return *_tokens; }

    /// Determines the length of the range.
    /// @return Number of tokens in the range.
    dsize size() const { return _end - _start; }

    bool isEmpty() const { return !size(); }

    TokenRange undefinedRange() const;

    bool undefined() const;

    /// Converts a position within the range to an index in the buffer.
    dsize tokenIndex(dsize pos) const;

    dsize tokenPos(dsize index) const;

    /// Returns a specific token from the token buffer.
    /// @param pos Position of the token within the range.
    const Token &token(dsize pos) const;

    const Token &firstToken() const;

    const Token &lastToken() const;

    /// Determines whether the range begins with a specific token.
    bool beginsWith(const char *token) const;

    /**
     * Composes a subrange that starts from a specific position.
     *
     * @param pos  Start position of subrange.
     *
     * @return Subrange that starts from position @a pos and continues until
     *         the end of the range.
     */
    TokenRange startingFrom(dsize pos) const;

    /**
     * Composes a subrange that ends at a specific position.
     *
     * @param pos  End position of subrange.
     *
     * @return  Subrange that starts from the beginning of this range
     *          and ends to @a pos.
     */
    TokenRange endingTo(dsize pos) const;

    TokenRange between(dsize startPos, dsize endPos) const;

    TokenRange shrink(dsize count) const { return between(count, size() - count); }

    /**
     * Determines if the range contains a specific token.
     *
     * @param token Token to look for.
     *
     * @return @c true, if token was found, otherwise @c false.
     */
    bool has(const char *token) const { return find(token) >= 0; }

    /**
     * Determines if the range contains a specific token, but only if
     * it is outside any brackets.
     *
     * @param token Token to look for.
     *
     * @return @c true, if token was found, otherwise @c false.
     */
    bool hasBracketless(const char *token) const
    {
        return findIndexSkippingBrackets(token, _start) >= 0;
    }

    /**
     * Finds the position of a specific token within the range.
     *
     * @param token Token to find.
     * @param startPos Position where to start looking.
     *
     * @return Position of the token, or -1 if not found.
     */
    dint find(const char *token, dsize startPos = 0) const;

    dint findBracketless(const char *token, dsize startPos = 0) const;

    /**
     * Finds the index of a specific token within the range. When
     * an opening bracket is encountered, its contents are skipped.
     *
     * @param token Token to find.
     * @param startIndex Index where to start looking.
     *
     * @return Index of the token, or -1 if not found.
     */
    dint findIndexSkippingBrackets(const char *token, dsize startIndex) const;

    /**
     * Finds the next token subrange which is delimited with @c
     * delimiter. @c subrange is adjusted so that if @c true is
     * returned, it will contain the correct subrange. When calling
     * this the first time, set the subrange to <code>undefinedRange()</code>.
     *
     * @param delimiter Delimiting token.
     * @param subrange Token range that receives the delimiting subrange.
     *
     * @return  @c true, if the next delimited subrange found successfully.
     *          Otherwise @c false.
     */
    bool getNextDelimited(const char *delimiter, TokenRange &subrange) const;

    /**
     * Locates the matching closing bracket. If the matching bracket
     * is not found, an exception is thrown.
     *
     * @param openBracketPos Position of the opening bracket.
     *
     * @return Position of the closing bracket.
     */
    dsize closingBracket(dsize openBracketPos) const;

    /**
     * Locates the matching opening bracket. If the matching bracket
     * is not found, an exception is thrown.
     *
     * @param closeBracketPos Position of the closing bracket.
     *
     * @return Position of the opening bracket.
     */
    dsize openingBracket(dsize closeBracketPos) const;

    /**
     * Composes a string representation of the token range. Intended
     * for error reporting.
     *
     * @return String containing the text of the tokens.
     */
    String asText() const;

public:
    static void bracketTokens(const Token &openingToken,
                              const char *&opening,
                              const char *&closing);

private:
    const TokenBuffer *_tokens;

    /// Index of the start of the range. This is the first token in
    /// the range.
    dsize _start;

    /// Index of the end of the range, plus one. This is the token
    /// just after the last token of the range.
    dsize _end;
};

} // namespace de

#endif /* LIBCORE_TOKENRANGE_H */
