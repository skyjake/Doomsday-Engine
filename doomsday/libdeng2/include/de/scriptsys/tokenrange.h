/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_TOKENRANGE_H
#define LIBDENG2_TOKENRANGE_H

#include "../String"
#include "../TokenBuffer"

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
class TokenRange
{
public:
    /// The token range is unexpectedly empty. @ingroup errors
    DENG2_ERROR(EmptyRangeError);

    /// A position outside the range is accessed. @ingroup errors
    DENG2_ERROR(OutOfBoundsError);

    /// A matching bracket cannot be located within the range. @ingroup errors
    DENG2_ERROR(MismatchedBracketError);

public:
    TokenRange();

    /// Constructor that uses the entire range of tokens.
    TokenRange(TokenBuffer const &tokens);

    /// Constructor that uses a specific set of tokens.
    TokenRange(TokenBuffer const &tokens, duint startIndex, duint endIndex);

    TokenBuffer const &buffer() const {
        return *_tokens;
    }

    /// Determines the length of the range.
    /// @return Number of tokens in the range.
    duint size() const {
        return _end - _start;
    }

    bool empty() const { return !size(); }

    TokenRange undefinedRange() const;

    bool undefined() const;

    /// Converts a position within the range to an index in the buffer.
    duint tokenIndex(duint pos) const;

    duint tokenPos(duint index) const;

    /// Returns a specific token from the token buffer.
    /// @param pos Position of the token within the range.
    Token const &token(duint pos) const;

    Token const &firstToken() const;

    Token const &lastToken() const;

    /// Determines whether the range begins with a specific token.
    bool beginsWith(QChar const *token) const;

    /**
     * Composes a subrange that starts from a specific position.
     *
     * @param pos  Start position of subrange.
     *
     * @return Subrange that starts from position @a pos and continues until
     *         the end of the range.
     */
    TokenRange startingFrom(duint pos) const;

    /**
     * Composes a subrange that ends at a specific position.
     *
     * @param pos  End position of subrange.
     *
     * @return  Subrange that starts from the beginning of this range
     *          and ends to @a pos.
     */
    TokenRange endingTo(duint pos) const;

    TokenRange between(duint startPos, duint endPos) const;

    TokenRange shrink(duint count) const {
        return between(count, size() - count);
    }

    /**
     * Determines if the range contains a specific token.
     *
     * @param token Token to look for.
     *
     * @return @c true, if token was found, otherwise @c false.
     */
    bool has(QChar const *token) const {
        return find(token) >= 0;
    }

    /**
     * Determines if the range contains a specific token, but only if
     * it is outside any brackets.
     *
     * @param token Token to look for.
     *
     * @return @c true, if token was found, otherwise @c false.
     */
    bool hasBracketless(QChar const *token) const {
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
    dint find(QChar const *token, dint startPos = 0) const;

    dint findBracketless(QChar const *token, dint startPos = 0) const;

    /**
     * Finds the index of a specific token within the range. When
     * an opening bracket is encountered, its contents are skipped.
     *
     * @param token Token to find.
     * @param startIndex Index where to start looking.
     *
     * @return Index of the token, or -1 if not found.
     */
    dint findIndexSkippingBrackets(QChar const *token, dint startIndex) const;

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
    bool getNextDelimited(QChar const *delimiter, TokenRange &subrange) const;

    /**
     * Locates the matching closing bracket. If the matching bracket
     * is not found, an exception is thrown.
     *
     * @param openBracketPos Position of the opening bracket.
     *
     * @return Position of the closing bracket.
     */
    duint closingBracket(duint openBracketPos) const;

    /**
     * Locates the matching opening bracket. If the matching bracket
     * is not found, an exception is thrown.
     *
     * @param closeBracketPos Position of the closing bracket.
     *
     * @return Position of the opening bracket.
     */
    duint openingBracket(duint closeBracketPos) const;

    /**
     * Composes a string representation of the token range. Intended
     * for error reporting.
     *
     * @return String containing the text of the tokens.
     */
    String asText() const;

public:
    static void bracketTokens(Token const &openingToken,
        QChar const * &opening, QChar const * &closing);

private:
    TokenBuffer const *_tokens;

    /// Index of the start of the range. This is the first token in
    /// the range.
    duint _start;

    /// Index of the end of the range, plus one. This is the token
    /// just after the last token of the range.
    duint _end;
};

} // namespace de

#endif /* LIBDENG2_TOKENRANGE_H */
