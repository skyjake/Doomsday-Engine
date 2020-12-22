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

#ifndef LIBCORE_LEX_H
#define LIBCORE_LEX_H

#include "../libcore.h"
#include "de/string.h"

namespace de {

class TokenBuffer;

/**
 * Base class for lexical analyzers. Provides the basic service of reading
 * characters one by one from an input text. It also classifies characters.
 *
 * @ingroup script
 */
class DE_PUBLIC Lex
{
public:
    /// Attempt to read characters when there are none left. @ingroup errors
    DE_ERROR(OutOfInputError);

    enum ModeFlag {
        DoubleCharComment = 0x1, // Comment start char must be used twice to begin comment.
        RetainComments    = 0x2,
        NegativeNumbers   = 0x4, // If set, '-' preceding a number is included in the literal.
        DefaultMode       = 0
    };
    using ModeFlags = Flags;

    /**
     * Utility for setting flags in a Lex instance. The flags specified
     * in the mode span are in effect during the lifetime of the ModeSpan instance.
     * When the ModeSpan goes out of scope, the original flags are restored
     * (the ones that were in use when the ModeSpan was constructed).
     */
    class ModeSpan
    {
    public:
        ModeSpan(Lex &lex, const ModeFlags &m)
            : _lex(lex)
            , _originalMode(lex._mode)
        {
            applyFlagOperation(_lex._mode, m, true);
        }

        ~ModeSpan() { _lex._mode = _originalMode; }

    private:
        Lex &     _lex;
        ModeFlags _originalMode;
    };

public:
    Lex(const String &input            = "",
        Char          lineCommentChar  = Char('#'),
        Char          multiCommentChar = Char('\0'),
        ModeFlags     initialMode      = DefaultMode);

    /// Returns the input string in its entirety.
    const String &input() const;

    /// Determines if the input string has been entirely read.
    bool atEnd() const;

    /// Returns the current position of the analyzer.
    String::const_iterator pos() const;

    /// Returns the next character, according to the position.
    /// Characters past the end of the input string are returned
    /// as zero.
    Char peek() const;

    /// Returns the next character and increments the position.
    /// Returns zero if the end of the input is reached.
    Char get();

    /// Skips until a non-whitespace character is found.
    void skipWhite();

    /// Skips until a non-whitespace character, or newline, is found.
    void skipWhiteExceptNewline();

    /// Skips until a new line begins.
    void skipToNextLine();

    Char peekComment() const;

    /// Returns the current line of the reading position. The character
    /// returned from get() will be on this line.
    dsize lineNumber() const { return _state.lineNumber; }

    /// Determines whether there is only whitespace (or nothing)
    /// remaining on the current line.
    bool onlyWhiteOnLine();

    bool atCommentStart() const;

    /// Counts the number of whitespace characters in the beginning of
    /// the current line.
    duint countLineStartSpace() const;

    /**
     * Attempts to parse the current reading position as a C-style number
     * literal (integer, float, or hexadecimal). It is assumed that a new
     * token has been started in the @a output buffer, and @a c has already
     * been added as the token's first character.
     *
     * @param c         Character that begins the number (from get()).
     * @param output    Token buffer.
     *
     * @return @c true, if a number token was parsed; otherwise @c false.
     */
    bool parseLiteralNumber(Char c, TokenBuffer &output);

public:
    /// Determines whether a character is whitespace.
    /// @param c Character to check.
    static bool isWhite(Char c);

    /// Determine whether a character is alphabetic.
    /// @param c Character to check.
    static bool isAlpha(Char c);

    /// Determine whether a character is numeric.
    /// @param c Character to check.
    static bool isNumeric(Char c);

    /// Determine whether a character is hexadecimal numeric.
    /// @param c Character to check.
    static bool isHexNumeric(Char c);

    /// Determine whether a character is alphanumeric.
    /// @param c Character to check.
    static bool isAlphaNumeric(Char c);

private:
    /// Input text being analyzed.
    const String *_input;

    struct State {
        String::const_iterator pos;          ///< Current reading position.
        String::const_iterator lineStartPos; ///< Position which begins the current line.
        dsize lineNumber = 1; ///< Keeps track of the line number on which the current position is.
    };
    State _state;

    mutable String::const_iterator _nextPos;

    /// Character that begins a line comment.
    Char _lineCommentChar;

    /// Character that begins a multiline comment, if it follows _lineCommentChar.
    /// In reversed order, the characters end a multiline comment.
    Char _multiCommentChar;

    ModeFlags _mode;
};

} // namespace de

#endif /* LIBCORE_LEX_H */
