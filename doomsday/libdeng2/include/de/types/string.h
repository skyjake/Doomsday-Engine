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

#ifndef LIBDENG2_STRING_H
#define LIBDENG2_STRING_H

#include <string>

#include "../deng.h"
#include "../Error"
#include "../IByteArray"
#include "../IBlock"

namespace de
{
    /**
     * The String class extends the STL string class with the IByteArray
     * interface.
     *
     * @ingroup types
     */
    class LIBDENG2_API String : public std::string, public IByteArray, public IBlock
    {
    public:
        /// Error related to String operations (note: shadows de::Error). @ingroup errors
        DEFINE_ERROR(Error);
        
        /// Encoding conversion failed. @ingroup errors
        DEFINE_SUB_ERROR(Error, ConversionError);

        /// An error was encountered in string pattern replacement. @ingroup errors
        DEFINE_SUB_ERROR(Error, IllegalPatternError);
        
        /// Invalid record member name. @ingroup errors
        DEFINE_SUB_ERROR(Error, InvalidMemberError);

        /**
         * Data argument for the pattern formatter.
         * @see String::patternFormat()
         */
        class IPatternArg 
        {
        public:
            /// An incompatible type is requested in asText() or asNumber(). @ingroup errors
            DEFINE_ERROR(TypeError);
            
        public:
            virtual ~IPatternArg() {}
            
            /// Returns the value of the argument as a text string.
            virtual String asText() const = 0;
            
            /// Returns the value of the argument as a number.
            virtual ddouble asNumber() const = 0;
        };
        
    public:
        String(const std::string& text = "");
        String(const String& other);
        String(const IByteArray& array);
        String(const char* cStr);
        String(const char* cStr, size_type length);
        String(size_type length, const char& ch);
        String(const std::string& str, size_type index, size_type length);
        String(iterator start, iterator end);
        String(const_iterator start, const_iterator end);

        /// Checks if the string begins with the substring @a s.
        bool beginsWith(const String& s) const;

        /// Checks if the string ends with the substring @a s.
        bool endsWith(const String& s) const;

        /// Checks if the string contains the substring @a s.
        bool contains(const String& s) const;

        /// Does a path concatenation on this string and the argument.
        String concatenatePath(const String& path, char dirChar = '/') const;

        /// Does a path concatenation on a native path. The directory separator 
        /// character depends on the platform.
        String concatenateNativePath(const String& nativePath) const;

        /// Does a record member concatenation on a variable name.
        String concatenateMember(const String& member) const;

        /// Removes whitespace from the beginning and end of the string.
        /// @return Copy of the string without whitespace.
        String strip() const;

        /// Removes whitespace from the beginning of the string.
        /// @return Copy of the string without whitespace.
        String leftStrip() const;

        /// Removes whitespace from the end of the string.
        /// @return Copy of the string without whitespace.
        String rightStrip() const;

        /// Returns a lower-case version of the string.
        String lower() const;

        /// Returns an upper-case version of the string.
        String upper() const;

        /// Converts the string to a wide-character STL wstring.
        std::wstring wide() const;

        /// Extracts the base name from the string (includes extension).
        String fileName() const;
        
        /// Extracts the base name from the string (does not include extension).
        String fileNameWithoutExtension() const;
        
        /**
         * Extracts the file name extension from a path. A valid extension
         * is the part of a file name after a period where the file name
         * itself is at least one character long. For instance:
         * with "a.ext" the extension is ".ext", but with ".ext" there is
         * no extension. 
         * 
         * @return The extension, including the period in the beginning. 
         * An empty string is returned if the string contains no period.
         */
        String fileNameExtension() const;
        
        /// Extracts the path of the string.
        String fileNamePath(char dirChar = '/') const;

        /// Extracts the path of the string, using native directory separators.
        String fileNameNativePath() const;

        /**
         * Compare two strings (case sensitive).
         *
         * @return 0, if @a a and @a b are identical. Positive, if @a a > @a b.
         *         Negative, if @a a < @a b.
         */
        dint compareWithCase(const String& str) const;
        
        /**
         * Compare two strings (case insensitive).
         *
         * @return 0, if @a a and @a b are identical. Positive, if @a a > @a b.
         *         Negative, if @a a < @a b.
         */
        dint compareWithoutCase(const String& str) const;
        
        // Implements IByteArray.
        Size size() const;
        void get(Offset at, Byte* values, Size count) const;
        void set(Offset at, const Byte* values, Size count);    
        
        // Implements IBlock.
        void clear() { std::string::clear(); }
        void copyFrom(const IByteArray& array, Offset at, Size count);
        void resize(Size size) { std::string::resize(size); }
        const Byte* data() const { return reinterpret_cast<const Byte*>(std::string::data()); }

    public:
        static dint compareWithCase(const char* a, const char* b, dsize count);
            
        /// wstring to String conversion.
        static String wideToString(const std::wstring& str);
        
        /// String to wstring conversion. @see wide()
        static std::wstring stringToWide(const String& str);
        
        /**
         * Advances the iterator until a nonspace character is encountered.
         * 
         * @param i  Iterator to advance.
         * @param end  End of the string. Will not advance past this.
         */
        static void skipSpace(String::const_iterator& i, const String::const_iterator& end);
        
        /**
         * Formats data according to formatting instructions. Outputs a
         * string containing the formatted data.
         *
         * @param formatIter  Will be moved past the formatting instructions.
         *                    After the call, will remain past the end of
         *                    the formatting characters.
         * @param formatEnd   Iterator to the end of the format string.
         * @param arg         Argument to format.
         *
         * @return  Formatted argument as a string.
         */
        static String patternFormat(String::const_iterator& formatIter, 
            const String::const_iterator& formatEnd, 
            const IPatternArg& arg);
            
        static void advanceFormat(String::const_iterator& i, 
            const String::const_iterator& end);
    };
}

#endif /* LIBDENG2_STRING_H */
