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

#ifndef LIBDENG2_LEX_H
#define LIBDENG2_LEX_H

#include "../deng.h"
#include "../String"
#include "../Flag"

namespace de
{
    /**
     * Lex is the base class for lexical analyzers. It provides the basic
     * service of reading characters one by one from an input text. It also
     * classifies characters.
     *
     * @ingroup script
     */
    class Lex
    {
    public:
        /// Attempt to read characters when there are non left. @ingroup errors
        DEFINE_ERROR(OutOfInputError);
        
        DEFINE_FINAL_FLAG(SKIP_COMMENTS, 0, Mode);
        
        /**
         * Utility for setting flags in a Lex instance. The flags specified
         * in the mode span are in effect during the lifetime of the ModeSpan instance.
         * When the ModeSpan goes out of scope, the original flags are restored
         * (the ones that were in use when the ModeSpan was constructed).
         */
        class ModeSpan
        {
        public:
            ModeSpan(Lex& lex, const Mode& m) : lex_(lex), originalMode_(lex.mode_) {
                lex.mode_ = m;
            }
            
            ~ModeSpan() {
                lex_.mode_ = originalMode_;
            }
            
        private:
            Lex& lex_;
            duint originalMode_;
        };
        
    public:
        Lex(const String& input = "");
        
        /// Returns the input string in its entirety.
        const String& input() const;
        
        /// Returns the current position of the analyzer.
        duint pos() const;
        
        /// Returns the next character, according to the position.
        /// Characters past the end of the input string are returned 
        /// as zero.
        duchar peek() const;
        
        /// Returns the next character and increments the position.
        /// Returns zero if the end of the input is reached.
        duchar get();
        
        /// Skips until a non-whitespace character is found.
        void skipWhite();
        
        /// Skips until a non-whitespace character, or newline, is found.
        void skipWhiteExceptNewline();
        
        /// Skips until a new line begins.
        void skipToNextLine();
        
        /// Returns the current line of the reading position. The character
        /// returned from get() will be on this line.
        duint lineNumber() const { 
            return state_.lineNumber; 
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
        static bool isWhite(duchar c);      
        
        /// Determine whether a character is alphabetic.
        /// @param c Character to check.
        static bool isAlpha(duchar c);
        
        /// Determine whether a character is numeric.
        /// @param c Character to check.
        static bool isNumeric(duchar c);
        
        /// Determine whether a character is alphanumeric.
        /// @param c Character to check.
        static bool isAlphaNumeric(duchar c);
            
    private:
        /// Input text being analyzed.
        const String* input_;

        mutable duint nextPos_;

        struct State {
            duint pos;          ///< Current reading position.
            duint lineNumber;   ///< Keeps track of the line number on which the current position is.
            duint lineStartPos; ///< Position which begins the current line.
            
            State() : pos(0), lineNumber(1), lineStartPos(0) {}
        };
        
        State state_;
        
        /// Character that begins a line comment.
        duchar lineCommentChar_;
        
        Mode mode_;
    };
}

#endif /* LIBDENG2_LEX_H */
