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

#ifndef LIBDENG2_LEX_H
#define LIBDENG2_LEX_H

#include "../libdeng2.h"
#include "../String"

#include <QFlags>

namespace de
{
    /**
     * Base class for lexical analyzers. It provides the basic service of reading 
     * characters one by one from an input text. It also classifies characters.
     *
     * @ingroup script
     */
    class Lex
    {
    public:
        /// Attempt to read characters when there are non left. @ingroup errors
        DENG2_ERROR(OutOfInputError);

        enum ModeFlag
        {
            SkipComments = 0x1
        };
        Q_DECLARE_FLAGS(ModeFlags, ModeFlag)
        
        /**
         * Utility for setting flags in a Lex instance. The flags specified
         * in the mode span are in effect during the lifetime of the ModeSpan instance.
         * When the ModeSpan goes out of scope, the original flags are restored
         * (the ones that were in use when the ModeSpan was constructed).
         */
        class ModeSpan
        {
        public:
            ModeSpan(Lex& lex, const ModeFlags& m) : _lex(lex), _originalMode(lex._mode) {
                lex._mode = m;
            }
            
            ~ModeSpan() {
                _lex._mode = _originalMode;
            }
            
        private:
            Lex& _lex;
            ModeFlags _originalMode;
        };
        
        // Constants.
        static const String T_PARENTHESIS_OPEN;
        static const String T_PARENTHESIS_CLOSE;
        static const String T_BRACKET_OPEN;
        static const String T_BRACKET_CLOSE;
        static const String T_CURLY_OPEN;
        static const String T_CURLY_CLOSE;

    public:
        Lex(const String& input = "");
        
        /// Returns the input string in its entirety.
        const String& input() const;
        
        /// Returns the current position of the analyzer.
        duint pos() const;
        
        /// Returns the next character, according to the position.
        /// Characters past the end of the input string are returned 
        /// as zero.
        QChar peek() const;
        
        /// Returns the next character and increments the position.
        /// Returns zero if the end of the input is reached.
        QChar get();
        
        /// Skips until a non-whitespace character is found.
        void skipWhite();
        
        /// Skips until a non-whitespace character, or newline, is found.
        void skipWhiteExceptNewline();
        
        /// Skips until a new line begins.
        void skipToNextLine();
        
        /// Returns the current line of the reading position. The character
        /// returned from get() will be on this line.
        duint lineNumber() const { 
            return _state.lineNumber; 
        }

        /// Determines whether there is only whitespace (or nothing) 
        /// remaining on the current line.
        bool onlyWhiteOnLine();
        
        /// Counts the number of whitespace characters in the beginning of
        /// the current line.
        duint countLineStartSpace() const;

    public:
        /// Determines whether a character is whitespace.
        /// @param c Character to check.
        static bool isWhite(QChar c);
        
        /// Determine whether a character is alphabetic.
        /// @param c Character to check.
        static bool isAlpha(QChar c);
        
        /// Determine whether a character is numeric.
        /// @param c Character to check.
        static bool isNumeric(QChar c);
        
        /// Determine whether a character is hexadecimal numeric.
        /// @param c Character to check.
        static bool isHexNumeric(QChar c);
        
        /// Determine whether a character is alphanumeric.
        /// @param c Character to check.
        static bool isAlphaNumeric(QChar c);

    private:
        /// Input text being analyzed.
        const String* _input;

        mutable duint _nextPos;

        struct State {
            duint pos;          ///< Current reading position.
            duint lineNumber;   ///< Keeps track of the line number on which the current position is.
            duint lineStartPos; ///< Position which begins the current line.
            
            State() : pos(0), lineNumber(1), lineStartPos(0) {}
        };
        
        State _state;
        
        /// Character that begins a line comment.
        duchar _lineCommentChar;
        
        ModeFlags _mode;
    };

    Q_DECLARE_OPERATORS_FOR_FLAGS(Lex::ModeFlags)
}

#endif /* LIBDENG2_LEX_H */
